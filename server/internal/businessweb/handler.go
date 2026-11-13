package businessweb

import (
	"context"
	"embed"
	"encoding/json"
	"html/template"
	"io"
	"log"
	"net"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/business"
	"daymark.local/server/internal/diagnostics"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"daymark.local/server/internal/telemetry"
)

//go:embed templates.html admin.css
var assets embed.FS

type Handler struct {
	Admin              *business.AdminService
	Metrics            *telemetry.Store
	Auth               *application.Auth
	Origin             string
	Secure             bool
	Clock              ports.Clock
	AdminEnabled       bool
	TelemetryEnabled   bool
	mux                *http.ServeMux
	pages              *template.Template
	slots              chan struct{}
	mu                 sync.Mutex
	attempts           map[string]attempt
	requests, failures atomic.Uint64
	started            time.Time
}
type attempt struct {
	at time.Time
	n  int
}

func New(admin *business.AdminService, metrics *telemetry.Store, auth *application.Auth, clock ports.Clock, origin string, enabled bool) (*Handler, error) {
	u, e := url.Parse(origin)
	if e != nil || u.Host == "" || u.Path != "" && u.Path != "/" {
		return nil, domain.Invalid("/public_origin")
	}
	h := &Handler{Admin: admin, Metrics: metrics, Auth: auth, Clock: clock, Origin: strings.TrimRight(origin, "/"), Secure: u.Scheme == "https", AdminEnabled: enabled, TelemetryEnabled: metrics != nil, slots: make(chan struct{}, 16), attempts: map[string]attempt{}, started: clock.Now()}
	h.pages, e = template.New("root").Funcs(template.FuncMap{"allowed": business.Allowed, "chartRows": func(rows []telemetry.Metric) []telemetry.Metric {
		if len(rows) > 12 {
			return rows[:12]
		}
		return rows
	}, "dimension": metricDimension, "metricName": func(name string) string {
		if label := map[string]string{"app.startup": "启动耗时", "surface.used": "使用界面", "update.outcome": "更新结果", "performance.sample": "私有提交内存"}[name]; label != "" {
			return label
		}
		return name
	}, "maxCount": func(rows []telemetry.Metric) int64 {
		max := int64(1)
		for _, row := range rows {
			if row.Count > max {
				max = row.Count
			}
		}
		return max
	}, "operation": func() (string, error) { id, e := domain.NewID(); return string(id), e }, "date": func(ms int64) string {
		if ms == 0 {
			return "—"
		}
		return time.UnixMilli(ms).UTC().Format("2006-01-02 15:04 UTC")
	}, "phase": func(n domain.Notice) string {
		if n.State == "revoked" {
			return "已撤回"
		}
		if n.State == "draft" {
			return "草稿"
		}
		if n.ExpireMS <= clock.Now().UnixMilli() {
			return "已过期"
		}
		if n.PublishMS > clock.Now().UnixMilli() {
			return "定时发布"
		}
		return "已发布"
	}, "joinRoles": func(roles []string) string { return strings.Join(roles, ",") }}).ParseFS(assets, "templates.html")
	if e != nil {
		return nil, e
	}
	mux := http.NewServeMux()
	h.mux = mux
	if enabled {
		mux.HandleFunc("GET /admin/assets/admin.css", func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "text/css; charset=utf-8")
			content, _ := assets.ReadFile("admin.css")
			w.Write(content)
		})
		mux.HandleFunc("GET /admin/session/login", h.loginPage)
		mux.HandleFunc("POST /admin/session/login", h.login)
		mux.HandleFunc("POST /admin/session/logout", h.logout)
		mux.HandleFunc("GET /admin", h.page)
		mux.HandleFunc("GET /admin/", h.page)
		mux.HandleFunc("GET /admin/api/v1/users", h.users)
		mux.HandleFunc("GET /admin/api/v1/admins", h.admins)
		mux.HandleFunc("GET /admin/api/v1/notices", h.notices)
		mux.HandleFunc("GET /admin/api/v1/audit", h.audits)
		mux.HandleFunc("GET /admin/api/v1/metrics", h.metrics)
		for _, prefix := range []string{"/admin/api/v1/", "/admin/actions/"} {
			mux.HandleFunc("POST "+prefix+"users/{id}/status", h.userCommand)
			mux.HandleFunc("POST "+prefix+"users/{id}/revoke-sessions", h.userCommand)
			mux.HandleFunc("POST "+prefix+"admins/{id}", h.adminCommand)
			mux.HandleFunc("POST "+prefix+"notices", h.noticeCommand)
			mux.HandleFunc("POST "+prefix+"notices/{id}/{action}", h.noticeCommand)
		}
		mux.HandleFunc("PATCH /admin/api/v1/notices/{id}", h.noticeCommand)
	}
	// Register even when storage failed: explicit enablement must report a
	// retryable unavailable error instead of masquerading as an old server.
	mux.HandleFunc("GET /telemetry/v1/consent", h.consent)
	mux.HandleFunc("PUT /telemetry/v1/consent", h.consent)
	mux.HandleFunc("POST /telemetry/v1/batches", h.ingest)
	return h, nil
}

type recorder struct {
	http.ResponseWriter
	status int
}

func (w *recorder) WriteHeader(status int) {
	if w.status == 0 {
		w.status = status
		w.ResponseWriter.WriteHeader(status)
	}
}
func (w *recorder) Write(p []byte) (int, error) {
	if w.status == 0 {
		w.WriteHeader(200)
	}
	return w.ResponseWriter.Write(p)
}
func (w *recorder) Unwrap() http.ResponseWriter { return w.ResponseWriter }
func (h *Handler) Wrap(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		h.requests.Add(1)
		record := &recorder{ResponseWriter: w}
		defer func() {
			if record.status >= 500 {
				h.failures.Add(1)
			}
		}()
		w = record
		if !strings.HasPrefix(r.URL.Path, "/admin") && !strings.HasPrefix(r.URL.Path, "/telemetry/") {
			next.ServeHTTP(w, r)
			return
		}
		id, e := domain.NewID()
		if e != nil {
			http.Error(w, "Service unavailable", 503)
			return
		}
		w.Header().Set("X-Request-ID", string(id))
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		// no-referrer can turn a browser navigation POST's Origin into "null".
		// Keep same-origin forms verifiable without leaking cross-origin referrers.
		w.Header().Set("Referrer-Policy", "same-origin")
		w.Header().Set("Content-Security-Policy", "default-src 'none'; style-src 'self'; img-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'")
		if h.AdminEnabled && h.Admin == nil && (r.URL.Path == "/admin" || strings.HasPrefix(r.URL.Path, "/admin/")) {
			h.problem(w, r, domain.Fail("temporarily_unavailable"))
			return
		}
		select {
		case h.slots <- struct{}{}:
			defer func() { <-h.slots }()
		default:
			h.problem(w, r, domain.Fail("busy"))
			return
		}
		ctx, cancel := context.WithTimeout(r.Context(), 10*time.Second)
		defer cancel()
		r = r.WithContext(ctx)
		if r.Header.Get("Content-Encoding") != "" && r.Header.Get("Content-Encoding") != "identity" {
			h.problem(w, r, domain.Fail("unsupported_media_type"))
			return
		}
		query, e := url.ParseQuery(r.URL.RawQuery)
		if e != nil {
			h.problem(w, r, domain.Fail("malformed_request"))
			return
		}
		for _, values := range query {
			if len(values) != 1 {
				h.problem(w, r, domain.Fail("malformed_request"))
				return
			}
		}
		h.mux.ServeHTTP(w, r)
	})
}
func (h *Handler) problem(w http.ResponseWriter, r *http.Request, e error) {
	code := domain.Code(e)
	status := diagnostics.Status(code)
	if status == 500 && diagnostics.Code(code) == "S999" {
		code = "internal_error"
	}
	if status >= 500 {
		log.Printf("business_request_failed diagnostic_code=%s request_id=%s", diagnostics.Code(code), w.Header().Get("X-Request-ID"))
	}
	w.Header().Set("X-Daymark-Error-Code", diagnostics.Code(code))
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(map[string]any{"version": 1, "code": code, "module": "business", "request_id": w.Header().Get("X-Request-ID")})
}
func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(value)
}
func decode(w http.ResponseWriter, r *http.Request, out any, limit int64) error {
	if strings.Split(r.Header.Get("Content-Type"), ";")[0] != "application/json" {
		return domain.Fail("unsupported_media_type")
	}
	r.Body = http.MaxBytesReader(w, r.Body, limit)
	b, e := io.ReadAll(r.Body)
	if e != nil {
		return domain.Fail("payload_too_large")
	}
	if _, e = protocol.StrictValue(b); e != nil {
		return e
	}
	d := json.NewDecoder(strings.NewReader(string(b)))
	d.DisallowUnknownFields()
	if e = d.Decode(out); e != nil {
		return domain.Fail("malformed_request")
	}
	return nil
}
func (h *Handler) cookie(w http.ResponseWriter, name, value string, maxAge int) {
	http.SetCookie(w, &http.Cookie{Name: name, Value: value, Path: "/admin", MaxAge: maxAge, HttpOnly: true, Secure: h.Secure, SameSite: http.SameSiteStrictMode})
}
func cookie(r *http.Request, name string) string {
	all := r.CookiesNamed(name)
	if len(all) != 1 {
		return ""
	}
	return all[0].Value
}
func (h *Handler) csrf(w http.ResponseWriter, r *http.Request, login bool) error {
	if r.Header.Get("Origin") != h.Origin {
		return domain.Fail("admin_forbidden")
	}
	token := cookie(r, "daymark_admin")
	if login {
		token = cookie(r, "daymark_admin_login")
	}
	value := r.Header.Get("X-CSRF-Token")
	if strings.Split(r.Header.Get("Content-Type"), ";")[0] == "application/x-www-form-urlencoded" {
		r.Body = http.MaxBytesReader(w, r.Body, 262144)
		if e := r.ParseForm(); e != nil {
			return domain.Fail("malformed_request")
		}
		for _, values := range r.PostForm {
			if len(values) != 1 {
				return domain.Fail("malformed_request")
			}
		}
		value = r.PostForm.Get("csrf")
	}
	if !h.Admin.ValidCSRF(token, value) {
		return domain.Fail("admin_forbidden")
	}
	return nil
}
func (h *Handler) rate(r *http.Request) bool {
	h.mu.Lock()
	defer h.mu.Unlock()
	now := h.Clock.Now()
	host, _, _ := net.SplitHostPort(r.RemoteAddr)
	if host == "" {
		host = "unknown"
	}
	if len(h.attempts) >= 2048 {
		for k, v := range h.attempts {
			if now.Sub(v.at) > time.Minute {
				delete(h.attempts, k)
			}
		}
		if len(h.attempts) >= 2048 {
			return false
		}
	}
	v := h.attempts[host]
	if now.Sub(v.at) > time.Minute {
		v = attempt{at: now}
	}
	v.n++
	h.attempts[host] = v
	return v.n <= 10
}
func (h *Handler) loginPage(w http.ResponseWriter, r *http.Request) {
	token, e := business.RandomToken()
	if e != nil {
		h.problem(w, r, e)
		return
	}
	h.cookie(w, "daymark_admin_login", token, 600)
	h.render(w, r, pageData{Page: "login", CSRF: h.Admin.CSRF(token), Title: "管理登录"})
}
func (h *Handler) login(w http.ResponseWriter, r *http.Request) {
	if !h.rate(r) {
		w.Header().Set("Retry-After", "60")
		h.problem(w, r, domain.Fail("rate_limited"))
		return
	}
	if e := h.csrf(w, r, true); e != nil {
		h.problem(w, r, e)
		return
	}
	password := []byte(r.PostForm.Get("password"))
	defer clear(password)
	token, e := h.Admin.Login(r.Context(), r.PostForm.Get("login"), password, r.PostForm.Get("otp"), w.Header().Get("X-Request-ID"))
	r.PostForm.Del("password")
	r.PostForm.Del("otp")
	if e != nil {
		h.render(w, r, pageData{Page: "login", Title: "管理登录", Login: r.PostForm.Get("login"), CSRF: h.Admin.CSRF(cookie(r, "daymark_admin_login")), Error: userError(e), Status: diagnostics.Status(domain.Code(e))})
		return
	}
	h.cookie(w, "daymark_admin", token, 8*3600)
	h.cookie(w, "daymark_admin_login", "", -1)
	http.Redirect(w, r, "/admin", 303)
}
func (h *Handler) logout(w http.ResponseWriter, r *http.Request) {
	if e := h.csrf(w, r, false); e != nil {
		h.problem(w, r, e)
		return
	}
	if e := h.Admin.Logout(r.Context(), cookie(r, "daymark_admin")); e != nil {
		h.problem(w, r, e)
		return
	}
	h.cookie(w, "daymark_admin", "", -1)
	http.Redirect(w, r, "/admin/session/login", 303)
}
func (h *Handler) principal(w http.ResponseWriter, r *http.Request) (domain.Principal, bool) {
	parts := strings.Fields(r.Header.Get("Authorization"))
	if len(r.Header.Values("Authorization")) != 1 || len(parts) != 2 || parts[0] != "Bearer" {
		h.problem(w, r, domain.Fail("auth_expired"))
		return domain.Principal{}, false
	}
	p, e := h.Auth.Authenticate(r.Context(), parts[1])
	if e != nil {
		h.problem(w, r, e)
		return p, false
	}
	return p, true
}
func (h *Handler) consent(w http.ResponseWriter, r *http.Request) {
	if h.Metrics == nil {
		code := "not_found"
		if h.TelemetryEnabled {
			code = "telemetry_storage_failed"
		}
		h.problem(w, r, domain.Fail(code))
		return
	}
	p, ok := h.principal(w, r)
	if !ok {
		return
	}
	if r.Method == "GET" {
		out, e := h.Metrics.Consent(r.Context(), p)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		writeJSON(w, 200, out)
		return
	}
	var in telemetry.ConsentChange
	if e := decode(w, r, &in, 4096); e != nil {
		h.problem(w, r, e)
		return
	}
	if in.Enabled && !h.TelemetryEnabled {
		h.problem(w, r, domain.Fail("telemetry_consent_required"))
		return
	}
	out, e := h.Metrics.ChangeConsent(r.Context(), p, in)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 200, out)
}
func (h *Handler) ingest(w http.ResponseWriter, r *http.Request) {
	if h.Metrics == nil {
		code := "not_found"
		if h.TelemetryEnabled {
			code = "telemetry_storage_failed"
		}
		h.problem(w, r, domain.Fail(code))
		return
	}
	if !h.TelemetryEnabled {
		h.problem(w, r, domain.Fail("telemetry_busy"))
		return
	}
	p, ok := h.principal(w, r)
	if !ok {
		return
	}
	var in telemetry.Batch
	if e := decode(w, r, &in, 32768); e != nil {
		h.problem(w, r, e)
		return
	}
	out, e := h.Metrics.Ingest(r.Context(), p, in)
	if e != nil {
		if domain.Code(e) == "rate_limited" {
			w.Header().Set("Retry-After", "900")
		}
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 202, out)
}
func pageLimit(r *http.Request) (int, error) {
	limit := 50
	if text := r.URL.Query().Get("limit"); text != "" {
		n, e := strconv.Atoi(text)
		if e != nil || n < 1 || n > 50 {
			return 0, domain.Invalid("/limit")
		}
		limit = n
	}
	return limit, nil
}
func userQuery(r *http.Request) (domain.UserQuery, error) {
	n, e := pageLimit(r)
	q := domain.UserQuery{After: domain.ID(r.URL.Query().Get("after")), Status: r.URL.Query().Get("status"), Search: strings.ToLower(r.URL.Query().Get("q")), Limit: n}
	if e != nil || q.After != "" && !domain.ValidID(string(q.After), false) || len(q.Search) > 64 || q.Status != "" && q.Status != "ACTIVE" && q.Status != "DISABLED" && q.Status != "DELETING" && q.Status != "DELETED" {
		return q, domain.Invalid("/query")
	}
	return q, nil
}
func (h *Handler) users(w http.ResponseWriter, r *http.Request) {
	q, e := userQuery(r)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	var out []domain.UserSummary
	e = h.Admin.Read(r.Context(), cookie(r, "daymark_admin"), "users", func(tx ports.BusinessTx) error {
		var e error
		out, e = tx.Users(r.Context(), q, h.Clock.Now().UnixMilli())
		return e
	})
	if e != nil {
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 200, map[string]any{"items": out})
}
func (h *Handler) admins(w http.ResponseWriter, r *http.Request) {
	var out []domain.Admin
	e := h.Admin.Read(r.Context(), cookie(r, "daymark_admin"), "owner", func(tx ports.BusinessTx) error { var e error; out, e = tx.Admins(r.Context()); return e })
	if e != nil {
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 200, map[string]any{"items": out})
}
func (h *Handler) notices(w http.ResponseWriter, r *http.Request) {
	limit, e := pageLimit(r)
	after := domain.ID(r.URL.Query().Get("after"))
	if e != nil || after != "" && !domain.ValidID(string(after), false) {
		h.problem(w, r, domain.Invalid("/query"))
		return
	}
	var out []domain.Notice
	e = h.Admin.Read(r.Context(), cookie(r, "daymark_admin"), "notices", func(tx ports.BusinessTx) error { var e error; out, e = tx.Notices(r.Context(), after, limit); return e })
	if e != nil {
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 200, map[string]any{"items": out})
}
func (h *Handler) audits(w http.ResponseWriter, r *http.Request) {
	before := int64(0)
	if value := r.URL.Query().Get("before"); value != "" {
		var e error
		before, e = strconv.ParseInt(value, 10, 64)
		if e != nil || before < 0 {
			h.problem(w, r, domain.Invalid("/before"))
			return
		}
	}
	var out []domain.Audit
	e := h.Admin.Read(r.Context(), cookie(r, "daymark_admin"), "audit", func(tx ports.BusinessTx) error { var e error; out, e = tx.Audits(r.Context(), before, 50); return e })
	if e != nil {
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 200, map[string]any{"items": out})
}
func (h *Handler) metrics(w http.ResponseWriter, r *http.Request) {
	if _, e := h.Admin.Me(r.Context(), cookie(r, "daymark_admin"), "metrics"); e != nil {
		h.problem(w, r, e)
		return
	}
	days := 7
	if value := r.URL.Query().Get("days"); value != "" {
		n, e := strconv.Atoi(value)
		if e != nil {
			h.problem(w, r, domain.Invalid("/days"))
			return
		}
		days = n
	}
	if h.Metrics == nil {
		writeJSON(w, 200, map[string]any{"enabled": false})
		return
	}
	out, e := h.Metrics.Report(r.Context(), days)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	writeJSON(w, 200, map[string]any{"enabled": true, "report": out, "service": map[string]any{"requests": h.requests.Load(), "failures": h.failures.Load(), "uptime_seconds": int64(h.Clock.Now().Sub(h.started).Seconds())}})
}
func formInt(r *http.Request, key string) int64 {
	n, _ := strconv.ParseInt(r.PostForm.Get(key), 10, 64)
	return n
}
func (h *Handler) userCommand(w http.ResponseWriter, r *http.Request) {
	if e := h.csrf(w, r, false); e != nil {
		h.problem(w, r, e)
		return
	}
	var in business.UserCommand
	if strings.HasPrefix(r.URL.Path, "/admin/actions/") {
		in = business.UserCommand{OperationID: domain.ID(r.PostForm.Get("operation_id")), Version: formInt(r, "version"), Status: r.PostForm.Get("status"), Reason: r.PostForm.Get("reason")}
	} else if e := decode(w, r, &in, 4096); e != nil {
		h.problem(w, r, e)
		return
	}
	revoke := strings.HasSuffix(r.URL.Path, "revoke-sessions")
	out, e := h.Admin.UserCommand(r.Context(), cookie(r, "daymark_admin"), domain.ID(r.PathValue("id")), in, revoke, w.Header().Get("X-Request-ID"))
	h.finish(w, r, out, e, "/admin/users")
}
func (h *Handler) adminCommand(w http.ResponseWriter, r *http.Request) {
	if e := h.csrf(w, r, false); e != nil {
		h.problem(w, r, e)
		return
	}
	var in business.AdminCommand
	if strings.HasPrefix(r.URL.Path, "/admin/actions/") {
		in = business.AdminCommand{OperationID: domain.ID(r.PostForm.Get("operation_id")), Version: formInt(r, "version"), Status: r.PostForm.Get("status"), Roles: strings.Split(r.PostForm.Get("roles"), ",")}
	} else if e := decode(w, r, &in, 4096); e != nil {
		h.problem(w, r, e)
		return
	}
	out, e := h.Admin.AdminCommand(r.Context(), cookie(r, "daymark_admin"), domain.ID(r.PathValue("id")), in, w.Header().Get("X-Request-ID"))
	h.finish(w, r, out, e, "/admin/admins")
}
func (h *Handler) noticeCommand(w http.ResponseWriter, r *http.Request) {
	if e := h.csrf(w, r, false); e != nil {
		h.problem(w, r, e)
		return
	}
	id := domain.ID(r.PathValue("id"))
	action := r.PathValue("action")
	if action == "" {
		action = "create"
		if r.Method == "PATCH" {
			action = "edit"
		}
	}
	var in business.NoticeCommand
	if strings.HasPrefix(r.URL.Path, "/admin/actions/") {
		in = business.NoticeCommand{OperationID: domain.ID(r.PostForm.Get("operation_id")), Version: formInt(r, "version"), Title: r.PostForm.Get("title"), Body: r.PostForm.Get("body"), Action: r.PostForm.Get("action"), Platform: r.PostForm.Get("platform"), MinVersion: r.PostForm.Get("min_version"), MaxVersion: r.PostForm.Get("max_version")}
		for _, v := range strings.Fields(r.PostForm.Get("targets")) {
			in.Targets = append(in.Targets, domain.ID(v))
		}
		for key, target := range map[string]*int64{"publish_at": &in.PublishMS, "expire_at": &in.ExpireMS} {
			if value := r.PostForm.Get(key); value != "" {
				at, e := time.Parse(time.RFC3339, value)
				if e != nil {
					h.formError(w, r, domain.Invalid("/"+key))
					return
				}
				*target = at.UnixMilli()
			}
		}
	} else if e := decode(w, r, &in, 262144); e != nil {
		h.problem(w, r, e)
		return
	}
	if match := r.Header.Get("If-Match"); match != "" && strings.Trim(match, "\"") != strconv.FormatInt(in.Version, 10) {
		h.problem(w, r, domain.Fail("admin_conflict"))
		return
	}
	out, e := h.Admin.NoticeCommand(r.Context(), cookie(r, "daymark_admin"), id, in, action, w.Header().Get("X-Request-ID"))
	h.finish(w, r, out, e, "/admin/notices")
}
func (h *Handler) finish(w http.ResponseWriter, r *http.Request, out domain.CommandReceipt, e error, location string) {
	if e != nil {
		if strings.HasPrefix(r.URL.Path, "/admin/actions/") {
			h.formError(w, r, e)
		} else {
			h.problem(w, r, e)
		}
		return
	}
	if strings.HasPrefix(r.URL.Path, "/admin/actions/") {
		http.Redirect(w, r, location, 303)
		return
	}
	status := 200
	if strings.HasSuffix(r.URL.Path, "/notices") {
		status = 201
	}
	writeJSON(w, status, out)
}
func userError(e error) string {
	code := domain.Code(e)
	message := map[string]string{"admin_conflict": "内容已经变化，请返回刷新后重试。", "admin_last_owner": "必须保留至少一位有效的所有者。", "admin_forbidden": "当前管理员没有这项操作权限。", "admin_session_expired": "管理登录已过期，请重新登录。", "admin_reauthentication_required": "此操作需要重新验证，请先退出并重新登录。", "admin_mfa_invalid": "动态验证码或恢复码不正确。", "admin_mfa_replayed": "此动态验证码已经使用，请等待下一组。", "invalid_credentials": "登录信息不正确。", "validation_failed": "请检查字段内容、时间和发布范围。", "quota_exceeded": "已达到当前容量限制，请先清理过期记录。"}[code]
	if message == "" {
		message = "操作未完成，可检查连接后重试。"
	}
	return message + "（" + diagnostics.Code(code) + "-C000）"
}
