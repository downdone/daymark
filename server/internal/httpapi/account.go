package httpapi

import (
	"net/http"
	"strings"

	"daymark.local/server/internal/domain"
)

func (h *Handler) registerAccount(mux *http.ServeMux) {
	mux.HandleFunc("POST /v1/account/deletion-intents", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		var in struct {
			Operation domain.ID `json:"operation_id"`
			Password  string    `json:"current_password"`
		}
		if e := h.decode(w, r, "DeletionPrepare", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		out, e := h.Account.Prepare(r.Context(), p, []byte(in.Password), in.Operation)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 201, "DeletionIntent", out)
	}))
	mux.HandleFunc("DELETE /v1/account", func(w http.ResponseWriter, r *http.Request) {
		capability, e := deletionToken(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		var in struct {
			Operation domain.ID `json:"operation_id"`
		}
		if e = h.decode(w, r, "DeletionConfirm", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		out, e := h.Account.Confirm(r.Context(), capability, in.Operation)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 202, "DeletionStatus", out)
	})
	mux.HandleFunc("GET /v1/account/deletions/{operation_id}", func(w http.ResponseWriter, r *http.Request) {
		capability, e := deletionToken(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		operation := domain.ID(r.PathValue("operation_id"))
		if !domain.ValidID(string(operation), false) {
			h.problem(w, r, domain.Invalid("/operation_id"))
			return
		}
		out, e := h.Account.Status(r.Context(), capability, operation)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "DeletionStatus", out)
	})
}
func deletionToken(r *http.Request) (string, error) {
	headers := r.Header.Values("Authorization")
	if len(headers) != 1 {
		return "", domain.Fail("auth_expired")
	}
	parts := strings.Fields(headers[0])
	if len(parts) != 2 || parts[0] != "Deletion" {
		return "", domain.Fail("auth_expired")
	}
	return parts[1], nil
}
