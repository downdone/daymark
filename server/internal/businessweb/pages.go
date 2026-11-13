package businessweb

import (
	"bytes"
	"encoding/json"
	"net/http"
	"slices"
	"strconv"
	"strings"
	"time"

	"daymark.local/server/internal/business"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/telemetry"
)

type pageData struct {
	Page, Title, CSRF, Error, Login string
	Status                          int
	Admin                           domain.Admin
	Overview                        domain.BusinessOverview
	Users                           []domain.UserSummary
	Admins                          []domain.Admin
	Notices                         []domain.Notice
	Audits                          []domain.Audit
	Report                          telemetry.Report
	MetricsEnabled                  bool
	Requests, Failures              uint64
	Query                           domain.UserQuery
	Next                            string
	Draft                           business.NoticeCommand
	DraftID                         domain.ID
	PublishAt, ExpireAt, Targets    string
	ConfirmUser                     *domain.UserSummary
	ConfirmNotice                   *domain.Notice
	ConfirmAction                   string
}

func (h *Handler) render(w http.ResponseWriter, r *http.Request, data pageData) {
	var output bytes.Buffer
	if e := h.pages.ExecuteTemplate(&output, "page", data); e != nil {
		h.problem(w, r, domain.Fail("internal_error"))
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	if data.Status > 0 {
		w.WriteHeader(data.Status)
	}
	w.Write(output.Bytes())
}
func (h *Handler) page(w http.ResponseWriter, r *http.Request) { h.loadPage(w, r, "") }
func (h *Handler) loadPage(w http.ResponseWriter, r *http.Request, message string) {
	page := strings.Trim(strings.TrimPrefix(r.URL.Path, "/admin"), "/")
	if strings.HasPrefix(page, "actions/") {
		if strings.Contains(page, "/notices") {
			page = "notices"
		} else if strings.Contains(page, "/admins") {
			page = "admins"
		} else {
			page = "users"
		}
	}
	if page == "" {
		page = "overview"
	}
	permission := map[string]string{"overview": "", "users": "users", "users/confirm": "users", "notices": "notices", "notices/preview": "notices", "notices/revoke": "notices", "admins": "owner", "audit": "audit", "metrics": "metrics"}
	p, ok := permission[page]
	if !ok {
		h.problem(w, r, domain.Fail("not_found"))
		return
	}
	a, e := h.Admin.Me(r.Context(), cookie(r, "daymark_admin"), p)
	if e != nil {
		if domain.Code(e) == "admin_session_expired" {
			http.Redirect(w, r, "/admin/session/login", 303)
		} else {
			h.problem(w, r, e)
		}
		return
	}
	data := pageData{Page: page, Title: map[string]string{"overview": "概览", "users": "用户", "users/confirm": "确认用户操作", "notices": "公告管理", "notices/preview": "发布预览", "notices/revoke": "撤回公告", "admins": "管理员", "audit": "操作审计", "metrics": "使用统计"}[page], Admin: a, CSRF: h.Admin.CSRF(cookie(r, "daymark_admin")), Error: message, MetricsEnabled: h.Metrics != nil, Requests: h.requests.Load(), Failures: h.failures.Load()}
	ctx := r.Context()
	e = h.Admin.Read(ctx, cookie(r, "daymark_admin"), p, func(tx ports.BusinessTx) error {
		var e error
		switch page {
		case "overview":
			data.Overview, e = tx.Overview(ctx, h.Clock.Now().UnixMilli())
		case "users":
			data.Query, e = userQuery(r)
			if e != nil {
				return e
			}
			data.Users, e = tx.Users(ctx, data.Query, h.Clock.Now().UnixMilli())
			if len(data.Users) == data.Query.Limit {
				data.Next = string(data.Users[len(data.Users)-1].ID)
			}
		case "users/confirm":
			id := domain.ID(r.URL.Query().Get("id"))
			if !domain.ValidID(string(id), false) {
				return domain.Invalid("/id")
			}
			u, e := tx.User(ctx, id)
			if e != nil {
				return e
			}
			if u == nil {
				return domain.Fail("not_found")
			}
			data.ConfirmUser = &domain.UserSummary{ID: u.ID, Login: u.Login, Status: u.Status, Version: u.CredentialVersion}
			data.ConfirmAction = r.URL.Query().Get("action")
			if !slices.Contains([]string{"ACTIVE", "DISABLED", "revoke"}, data.ConfirmAction) {
				return domain.Invalid("/action")
			}
		case "notices":
			data.Notices, e = tx.Notices(ctx, domain.ID(r.URL.Query().Get("after")), 50)
			if e != nil {
				return e
			}
			if len(data.Notices) == 50 {
				data.Next = string(data.Notices[49].ID)
			}
			data.Draft.Action = "none"
			data.Draft.Platform = "all"
			data.ExpireAt = h.Clock.Now().AddDate(0, 0, 7).UTC().Format(time.RFC3339)
			if id := domain.ID(r.URL.Query().Get("id")); id != "" {
				if !domain.ValidID(string(id), false) {
					return domain.Invalid("/id")
				}
				n, e := tx.Notice(ctx, id)
				if e != nil {
					return e
				}
				if n == nil || n.State != "draft" {
					return domain.Fail("admin_conflict")
				}
				data.DraftID = n.ID
				data.Draft = business.NoticeCommand{Version: n.Version, Title: n.Title, Body: n.Body, Action: n.Action, Platform: n.Platform, MinVersion: n.MinVersion, MaxVersion: n.MaxVersion, Targets: n.Targets}
				if n.PublishMS > 0 {
					data.PublishAt = time.UnixMilli(n.PublishMS).UTC().Format(time.RFC3339)
				}
				data.ExpireAt = time.UnixMilli(n.ExpireMS).UTC().Format(time.RFC3339)
				targetStrings := []string{}
				for _, target := range n.Targets {
					targetStrings = append(targetStrings, string(target))
				}
				data.Targets = strings.Join(targetStrings, " ")
			}
		case "notices/preview", "notices/revoke":
			id := domain.ID(r.URL.Query().Get("id"))
			if !domain.ValidID(string(id), false) {
				return domain.Invalid("/id")
			}
			data.ConfirmNotice, e = tx.Notice(ctx, id)
			if data.ConfirmNotice == nil && e == nil {
				return domain.Fail("not_found")
			}
		case "admins":
			data.Admins, e = tx.Admins(ctx)
		case "audit":
			before, _ := strconv.ParseInt(r.URL.Query().Get("before"), 10, 64)
			data.Audits, e = tx.Audits(ctx, before, 50)
			if len(data.Audits) == 50 {
				data.Next = strconv.FormatInt(data.Audits[49].Seq, 10)
			}
		}
		return e
	})
	if e != nil {
		h.problem(w, r, e)
		return
	}
	if page == "metrics" && h.Metrics != nil {
		days := 7
		if text := r.URL.Query().Get("days"); text != "" {
			days, _ = strconv.Atoi(text)
		}
		data.Report, e = h.Metrics.Report(ctx, days)
		if e != nil {
			h.problem(w, r, e)
			return
		}
	}
	if message != "" && page == "notices" {
		data.DraftID = domain.ID(r.PathValue("id"))
		data.Draft = business.NoticeCommand{OperationID: domain.ID(r.PostForm.Get("operation_id")), Version: formInt(r, "version"), Title: r.PostForm.Get("title"), Body: r.PostForm.Get("body"), Action: r.PostForm.Get("action"), Platform: r.PostForm.Get("platform"), MinVersion: r.PostForm.Get("min_version"), MaxVersion: r.PostForm.Get("max_version")}
		data.PublishAt = r.PostForm.Get("publish_at")
		data.ExpireAt = r.PostForm.Get("expire_at")
		data.Targets = r.PostForm.Get("targets")
	}
	h.render(w, r, data)
}
func (h *Handler) formError(w http.ResponseWriter, r *http.Request, e error) {
	h.loadPage(w, r, userError(e))
}
func metricDimension(text string) string {
	var fields map[string]string
	if json.Unmarshal([]byte(text), &fields) != nil {
		return "—"
	}
	out := []string{}
	for _, k := range []string{"os", "app_version", "surface", "outcome", "duration_bucket", "memory_bucket"} {
		if value := fields[k]; value != "" {
			out = append(out, value)
		}
	}
	return strings.Join(out, " · ")
}
