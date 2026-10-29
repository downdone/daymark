package httpapi

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"mime"
	"net"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/protocol"
)

type Handler struct {
	RegistrationEnabled bool
	Auth                *application.Auth
	Sync                *application.Sync
	Codec               *protocol.Codec
	MaxBody             int64
	Timeout             time.Duration
	semaphore           chan struct{}
	mu                  sync.Mutex
	attempts            map[string]window
	status              map[string]int
	Transfer            *application.Transfer
	Account             *application.Account
}
type window struct {
	Start time.Time
	Count int
}
type requestKey struct{}

func New(auth *application.Auth, syncer *application.Sync, codec *protocol.Codec) *Handler {
	return &Handler{Auth: auth, Sync: syncer, Codec: codec, MaxBody: 1048576, Timeout: 10 * time.Second, semaphore: make(chan struct{}, 64), attempts: map[string]window{}, status: protocol.ErrorStatuses()}
}
func (h *Handler) Router() http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("GET /health/live", func(w http.ResponseWriter, r *http.Request) {
		h.write(w, r, 200, "Health", map[string]any{"status": "ok"})
	})
	mux.HandleFunc("GET /health/ready", func(w http.ResponseWriter, r *http.Request) {
		if e := h.Sync.Store.Ready(); e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Health", map[string]any{"status": "ok"})
	})
	mux.HandleFunc("GET /v1/capabilities", h.capabilities)
	mux.HandleFunc("POST /v1/auth/login", h.login)
	mux.HandleFunc("POST /v1/auth/register", h.register)
	mux.HandleFunc("POST /v1/auth/refresh", h.refresh)
	mux.HandleFunc("POST /v1/auth/logout", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		if e := h.Auth.Revoke(r.Context(), p, p.SessionID); e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 204, "", nil)
	}))
	mux.HandleFunc("POST /v1/auth/password", h.authorized(h.password))
	mux.HandleFunc("GET /v1/me", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		out, e := h.Auth.Me(r.Context(), p)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Me", out)
	}))
	mux.HandleFunc("GET /v1/devices", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		out, e := h.Auth.Devices(r.Context(), p)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Sessions", map[string]any{"protocol_version": 1, "sessions": out})
	}))
	mux.HandleFunc("DELETE /v1/devices/{session_id}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		id := domain.ID(r.PathValue("session_id"))
		if !domain.ValidID(string(id), false) {
			h.problem(w, r, domain.Invalid("/session_id"))
			return
		}
		if e := h.Auth.Revoke(r.Context(), p, id); e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 204, "", nil)
	}))
	mux.HandleFunc("POST /v1/datasets/{dataset_id}/sync/push", h.authorized(h.push))
	mux.HandleFunc("GET /v1/datasets/{dataset_id}/sync/receipts/{mutation_id}", h.authorized(h.receipt))
	mux.HandleFunc("GET /v1/datasets/{dataset_id}/sync/pull", h.authorized(h.pull))
	mux.HandleFunc("POST /v1/datasets/{dataset_id}/sync/progress", h.authorized(h.progress))
	if h.Transfer != nil {
		h.registerTransfer(mux)
	}
	if h.Account != nil {
		h.registerAccount(mux)
	}
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		allowed := []string{}
		for _, method := range []string{"GET", "POST", "PUT", "DELETE"} {
			probe := r.Clone(r.Context())
			probe.Method = method
			_, pattern := mux.Handler(probe)
			if pattern != "/" && pattern != "" {
				allowed = append(allowed, method)
			}
		}
		if len(allowed) > 0 {
			w.Header().Set("Allow", strings.Join(allowed, ", "))
			h.problem(w, r, domain.Fail("method_not_allowed"))
			return
		}
		h.problem(w, r, domain.Fail("not_found"))
	})
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		w.Header().Set("X-Content-Type-Options", "nosniff")
		id, e := domain.NewID()
		if e != nil {
			h.problem(w, r, e)
			return
		}
		r = r.WithContext(context.WithValue(r.Context(), requestKey{}, string(id)))
		w.Header().Set("X-Request-ID", string(id))
		defer func() {
			if recover() != nil {
				h.problem(w, r, domain.Fail("internal_error"))
			}
		}()
		select {
		case h.semaphore <- struct{}{}:
			defer func() { <-h.semaphore }()
		default:
			h.problem(w, r, domain.Fail("busy"))
			return
		}
		ctx, cancel := context.WithTimeout(r.Context(), h.Timeout)
		defer cancel()
		r = r.WithContext(ctx)
		query, parseError := url.ParseQuery(r.URL.RawQuery)
		if parseError != nil {
			h.problem(w, r, domain.Fail("malformed_request"))
			return
		}
		for _, values := range query {
			if len(values) != 1 {
				h.problem(w, r, domain.Fail("malformed_request"))
				return
			}
		}
		if strings.HasPrefix(r.URL.Path, "/v1/") && r.URL.Path != "/v1/capabilities" {
			if len(r.Header.Values("X-Daymark-DTO-Version")) != 1 || r.Header.Get("X-Daymark-DTO-Version") != "1" {
				h.problem(w, r, domain.Fail("protocol_upgrade_required"))
				return
			}
			w.Header().Set("X-Daymark-DTO-Version", "1")
		}
		if r.Header.Get("Content-Encoding") != "" && r.Header.Get("Content-Encoding") != "identity" {
			h.problem(w, r, domain.Fail("unsupported_media_type"))
			return
		}
		mux.ServeHTTP(w, r)
	})
}
func (h *Handler) authorized(next func(http.ResponseWriter, *http.Request, domain.Principal)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		values := r.Header.Values("Authorization")
		if len(values) != 1 {
			h.problem(w, r, domain.Fail("auth_expired"))
			return
		}
		parts := strings.Fields(values[0])
		if len(parts) != 2 || parts[0] != "Bearer" {
			h.problem(w, r, domain.Fail("auth_expired"))
			return
		}
		p, e := h.Auth.Authenticate(r.Context(), parts[1])
		if e != nil {
			h.problem(w, r, e)
			return
		}
		next(w, r, p)
	}
}
func (h *Handler) body(w http.ResponseWriter, r *http.Request) ([]byte, error) {
	kind, _, e := mime.ParseMediaType(r.Header.Get("Content-Type"))
	if e != nil || kind != "application/json" {
		return nil, domain.Fail("unsupported_media_type")
	}
	r.Body = http.MaxBytesReader(w, r.Body, h.MaxBody)
	b, e := io.ReadAll(r.Body)
	if e != nil {
		var tooLarge *http.MaxBytesError
		if errors.As(e, &tooLarge) {
			return nil, domain.Fail("payload_too_large")
		}
		return nil, domain.Wrap("malformed_request", e)
	}
	return b, nil
}
func (h *Handler) decode(w http.ResponseWriter, r *http.Request, name string, target any) error {
	b, e := h.body(w, r)
	if e != nil {
		return e
	}
	return h.Codec.Decode(name, b, target)
}
func (h *Handler) limited(r *http.Request) bool {
	addr, _, e := net.SplitHostPort(r.RemoteAddr)
	if e != nil {
		addr = r.RemoteAddr
	}
	return h.limitKey("auth:"+addr, 20)
}
func (h *Handler) limitKey(addr string, maximum int) bool {
	now := h.Sync.Clock.Now()
	h.mu.Lock()
	defer h.mu.Unlock()
	v := h.attempts[addr]
	if now.Sub(v.Start) >= time.Minute {
		v = window{Start: now}
	}
	if v.Start.IsZero() {
		v.Start = now
	}
	if _, ok := h.attempts[addr]; !ok && len(h.attempts) >= 4096 {
		for k, x := range h.attempts {
			if now.Sub(x.Start) >= time.Minute {
				delete(h.attempts, k)
			}
		}
		if len(h.attempts) >= 4096 {
			return true
		}
	}
	v.Count++
	h.attempts[addr] = v
	return v.Count > maximum
}
func (h *Handler) register(w http.ResponseWriter, r *http.Request) {
	if !h.RegistrationEnabled {
		h.problem(w, r, domain.Fail("not_found"))
		return
	}
	addr, _, e := net.SplitHostPort(r.RemoteAddr)
	if e != nil {
		addr = r.RemoteAddr
	}
	if h.limited(r) || h.limitKey("register:global", 20) || h.limitKey("register:ip:"+addr, 5) {
		h.problem(w, r, domain.Fail("rate_limited"))
		return
	}
	var in struct {
		ProtocolVersion int    `json:"protocol_version"`
		Login           string `json:"login"`
		Password        string `json:"password"`
	}
	if e := h.decode(w, r, "RegistrationRequest", &in); e != nil {
		h.problem(w, r, e)
		return
	}
	if e := h.Auth.Register(r.Context(), in.Login, []byte(in.Password)); e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 204, "", nil)
}
func (h *Handler) login(w http.ResponseWriter, r *http.Request) {
	if h.limited(r) {
		h.problem(w, r, domain.Fail("rate_limited"))
		return
	}
	var in application.LoginInput
	if e := h.decode(w, r, "LoginRequest", &in); e != nil {
		h.problem(w, r, e)
		return
	}
	out, e := h.Auth.Login(r.Context(), in)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 200, "TokenPair", out)
}
func (h *Handler) refresh(w http.ResponseWriter, r *http.Request) {
	if h.limited(r) {
		h.problem(w, r, domain.Fail("rate_limited"))
		return
	}
	var in struct {
		Token string `json:"refresh_token"`
	}
	if e := h.decode(w, r, "RefreshRequest", &in); e != nil {
		h.problem(w, r, e)
		return
	}
	out, e := h.Auth.Refresh(r.Context(), in.Token)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 200, "TokenPair", out)
}
func (h *Handler) password(w http.ResponseWriter, r *http.Request, p domain.Principal) {
	var in struct {
		Old string `json:"current_password"`
		New string `json:"new_password"`
	}
	if e := h.decode(w, r, "PasswordChange", &in); e != nil {
		h.problem(w, r, e)
		return
	}
	if e := h.Auth.ChangePassword(r.Context(), p, []byte(in.Old), []byte(in.New)); e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 204, "", nil)
}
func (h *Handler) scope(r *http.Request, p domain.Principal, epoch domain.ID) (domain.Scope, error) {
	dataset := domain.ID(r.PathValue("dataset_id"))
	if !domain.ValidID(string(dataset), false) || !domain.ValidID(string(epoch), false) {
		return domain.Scope{}, domain.Invalid("/dataset_or_epoch")
	}
	return domain.Scope{Principal: p, DatasetID: dataset, DataEpoch: epoch}, nil
}
func (h *Handler) push(w http.ResponseWriter, r *http.Request, p domain.Principal) {
	b, e := h.body(w, r)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	dataset := domain.ID(r.PathValue("dataset_id"))
	if !domain.ValidID(string(dataset), false) {
		h.problem(w, r, domain.Invalid("/dataset_id"))
		return
	}
	m, e := h.Codec.DecodeMutation(b, dataset)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	scope, e := h.scope(r, p, m.DataEpoch)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	out, e := h.Sync.Push(r.Context(), scope, m)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 200, "Receipt", out)
}
func (h *Handler) receipt(w http.ResponseWriter, r *http.Request, p domain.Principal) {
	scope, e := h.scope(r, p, domain.ID(r.URL.Query().Get("data_epoch")))
	if e != nil {
		h.problem(w, r, e)
		return
	}
	id := domain.ID(r.PathValue("mutation_id"))
	if !domain.ValidID(string(id), false) {
		h.problem(w, r, domain.Invalid("/mutation_id"))
		return
	}
	out, e := h.Sync.GetReceipt(r.Context(), scope, id)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	if out == nil {
		h.problem(w, r, domain.Fail("not_found"))
		return
	}
	h.write(w, r, 200, "Receipt", out)
}
func (h *Handler) pull(w http.ResponseWriter, r *http.Request, p domain.Principal) {
	dataset := domain.ID(r.PathValue("dataset_id"))
	if !domain.ValidID(string(dataset), false) {
		h.problem(w, r, domain.Invalid("/dataset_id"))
		return
	}
	var scope domain.Scope
	var found bool
	me, e := h.Auth.Me(r.Context(), p)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	for _, d := range me["datasets"].([]domain.Dataset) {
		if d.ID == dataset {
			scope = domain.Scope{Principal: p, DatasetID: dataset, DataEpoch: d.DataEpoch}
			found = true
			break
		}
	}
	if !found {
		h.problem(w, r, domain.Fail("not_found"))
		return
	}
	limit := 128
	if raw := r.URL.Query().Get("max_batches"); raw != "" {
		limit, e = strconv.Atoi(raw)
		if e != nil || limit < 1 || limit > 128 {
			h.problem(w, r, domain.Invalid("/max_batches"))
			return
		}
	}
	out, e := h.Sync.Pull(r.Context(), scope, r.URL.Query().Get("cursor"), limit)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 200, "PullResponse", out)
}
func (h *Handler) progress(w http.ResponseWriter, r *http.Request, p domain.Principal) {
	var in struct {
		Epoch   domain.ID      `json:"data_epoch"`
		Applied domain.Counter `json:"applied_seq"`
	}
	if e := h.decode(w, r, "Progress", &in); e != nil {
		h.problem(w, r, e)
		return
	}
	scope, e := h.scope(r, p, in.Epoch)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	if e = h.Sync.ReportProgress(r.Context(), scope, in.Applied); e != nil {
		h.problem(w, r, e)
		return
	}
	h.write(w, r, 204, "", nil)
}
func (h *Handler) capabilities(w http.ResponseWriter, r *http.Request) {
	h.write(w, r, 200, "Capabilities", map[string]any{"protocol_version": 1, "server_instance_id": h.Sync.Cursor.ServerID, "supported_protocol_versions": []int{1}, "supported_dto_versions": []int{1}, "dto_version": 1, "command_types": domain.CommandTypes, "max_inline_bytes": h.MaxBody, "max_inline_commands": h.Sync.MaxCommands, "max_inline_entities": h.Sync.MaxInlineEntities, "max_chunk_bytes": 262144, "max_job_bytes": domain.Counter(67108864), "max_batch_reassembly_bytes": domain.Counter(268435456), "metadata": map[string]any{"registration_enabled": h.RegistrationEnabled}, "features": map[string]bool{"staged_payloads": h.Sync.Resolver != nil, "snapshots": h.Transfer != nil, "imports": h.Transfer != nil, "account_export": h.Transfer != nil, "account_deletion": h.Account != nil}})
}
func (h *Handler) write(w http.ResponseWriter, r *http.Request, status int, schema string, out any) {
	if status == 204 {
		w.WriteHeader(status)
		return
	}
	b, e := json.Marshal(out)
	if e != nil {
		h.problem(w, r, domain.Fail("internal_error"))
		return
	}
	v, e := protocol.StrictValue(b)
	if e != nil || h.Codec.Validate(schema, v) != nil {
		h.problem(w, r, domain.Fail("internal_error"))
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_, _ = w.Write(b)
}
func (h *Handler) problem(w http.ResponseWriter, r *http.Request, e error) {
	code := domain.Code(e)
	status, ok := h.status[code]
	if !ok {
		code = "internal_error"
		status = 500
	}
	retry := code == "busy" || code == "rate_limited" || code == "temporarily_unavailable" || code == "commit_result_unknown" || code == "timeout"
	id, _ := r.Context().Value(requestKey{}).(string)
	out := map[string]any{"type": "urn:daymark:problem:" + strings.ReplaceAll(code, "_", "-"), "title": strings.ReplaceAll(code, "_", " "), "status": status, "instance": "urn:daymark:request:" + id, "code": code, "retryable": retry}
	var f *domain.Fault
	if errors.As(e, &f) {
		if f.Field != "" {
			out["field_path"] = f.Field
		}
		if len(f.Conflicts) > 0 {
			out["conflicts"] = f.Conflicts
		}
		if len(f.Structures) > 0 {
			out["structure_conflicts"] = f.Structures
		}
	}
	if code == "busy" || code == "rate_limited" {
		w.Header().Set("Retry-After", "1")
		out["retry_after_ms"] = 1000
	}
	w.Header().Set("Content-Type", "application/problem+json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(out)
}
