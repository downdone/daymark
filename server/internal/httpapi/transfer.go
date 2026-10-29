package httpapi

import (
	"io"
	"net/http"
	"strconv"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/domain"
)

func (h *Handler) AttachTransfer(t *application.Transfer) {
	h.Transfer = t
}
func (h *Handler) registerTransfer(mux *http.ServeMux) {
	base := "/v1/datasets/{dataset_id}"
	for _, c := range []struct {
		path, kind, schema string
		status             int
	}{{"snapshots", "SNAPSHOT", "OperationRequest", 202}, {"staged-mutations", "STAGED", "StagedCreate", 201}, {"imports", "IMPORT", "ImportCreate", 201}} {
		mux.HandleFunc("POST "+base+"/"+c.path, h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
			var in application.CreateJobInput
			if e := h.decode(w, r, c.schema, &in); e != nil {
				h.problem(w, r, e)
				return
			}
			s, e := h.scope(r, p, in.DataEpoch)
			if e != nil {
				h.problem(w, r, e)
				return
			}
			j, e := h.Transfer.Create(r.Context(), p, &s, c.kind, in)
			if e != nil {
				h.problem(w, r, e)
				return
			}
			h.write(w, r, c.status, "Job", j)
		}))
	}
	mux.HandleFunc("GET "+base+"/jobs/{job_id}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		s, e := h.currentScope(r, p)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		j, e := h.Transfer.Get(r.Context(), p, &s, id)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Job", j)
	}))
	mux.HandleFunc("DELETE "+base+"/jobs/{job_id}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		s, e := h.scope(r, p, domain.ID(r.URL.Query().Get("data_epoch")))
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		j, e := h.Transfer.Cancel(r.Context(), s, id)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Job", j)
	}))
	mux.HandleFunc("POST "+base+"/jobs/{job_id}/renew", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		var in struct {
			Epoch   domain.ID `json:"data_epoch"`
			Seconds int       `json:"extend_seconds"`
		}
		if e := h.decode(w, r, "RenewRequest", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		s, e := h.scope(r, p, in.Epoch)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		j, e := h.Transfer.Renew(r.Context(), s, id, in.Seconds)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Job", j)
	}))
	mux.HandleFunc("POST "+base+"/jobs/{job_id}/seal", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		var in application.SealInput
		if e := h.decode(w, r, "SealRequest", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		s, e := h.scope(r, p, in.DataEpoch)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		j, e := h.Transfer.Seal(r.Context(), s, id, in)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Sealed", j)
	}))
	mux.HandleFunc("POST "+base+"/imports/{job_id}/publish", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		var in application.CreateJobInput
		if e := h.decode(w, r, "OperationRequest", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		s, e := h.scope(r, p, in.DataEpoch)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		receipt, e := h.Transfer.PublishImport(r.Context(), s, id, in.OperationID)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Receipt", receipt)
	}))
	for _, path := range []string{base + "/jobs/{job_id}/manifest", base + "/snapshots/{job_id}"} {
		mux.HandleFunc("GET "+path, h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
			s, e := h.scope(r, p, domain.ID(r.URL.Query().Get("data_epoch")))
			if e != nil {
				h.problem(w, r, e)
				return
			}
			id, e := jobID(r)
			if e != nil {
				h.problem(w, r, e)
				return
			}
			m, e := h.Transfer.GetManifest(r.Context(), p, &s, id)
			if e != nil {
				h.problem(w, r, e)
				return
			}
			h.write(w, r, 200, "Manifest", m)
		}))
	}
	mux.HandleFunc("POST "+base+"/snapshots/{job_id}/release", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		var in struct {
			Epoch   domain.ID      `json:"data_epoch"`
			Applied domain.Counter `json:"applied_seq"`
		}
		if e := h.decode(w, r, "ReleaseRequest", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		s, e := h.scope(r, p, in.Epoch)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		if e = h.Transfer.ReleaseSnapshot(r.Context(), s, id, in.Applied); e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 204, "", nil)
	}))
	mux.HandleFunc("PUT "+base+"/jobs/{job_id}/chunks/{index}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		s, e := h.scope(r, p, domain.ID(r.URL.Query().Get("data_epoch")))
		if e != nil {
			h.problem(w, r, e)
			return
		}
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		index, e := chunkIndex(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		if r.Header.Get("Content-Type") != "application/octet-stream" {
			h.problem(w, r, domain.Fail("unsupported_media_type"))
			return
		}
		if r.ContentLength < 1 || r.ContentLength > 262144 {
			h.problem(w, r, domain.Fail("payload_too_large"))
			return
		}
		r.Body = http.MaxBytesReader(w, r.Body, 262144)
		chunk, e := h.Transfer.PutChunk(r.Context(), s, id, index, r.Body, r.ContentLength, r.Header.Get("X-Chunk-SHA256"))
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Chunk", chunk)
	}))
	mux.HandleFunc("GET "+base+"/jobs/{job_id}/chunks/{index}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		s, e := h.scope(r, p, domain.ID(r.URL.Query().Get("data_epoch")))
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.streamChunk(w, r, p, &s)
	}))
	mux.HandleFunc("POST /v1/account/exports", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		var in application.CreateJobInput
		if e := h.decode(w, r, "ExportRequest", &in); e != nil {
			h.problem(w, r, e)
			return
		}
		j, e := h.Transfer.Create(r.Context(), p, nil, "EXPORT", in)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 202, "Job", j)
	}))
	mux.HandleFunc("GET /v1/account/exports/{job_id}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		j, e := h.Transfer.Get(r.Context(), p, nil, id)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Job", j)
	}))
	mux.HandleFunc("GET /v1/account/exports/{job_id}/manifest", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) {
		id, e := jobID(r)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		m, e := h.Transfer.GetManifest(r.Context(), p, nil, id)
		if e != nil {
			h.problem(w, r, e)
			return
		}
		h.write(w, r, 200, "Manifest", m)
	}))
	mux.HandleFunc("GET /v1/account/exports/{job_id}/chunks/{index}", h.authorized(func(w http.ResponseWriter, r *http.Request, p domain.Principal) { h.streamChunk(w, r, p, nil) }))
}
func jobID(r *http.Request) (domain.ID, error) {
	id := domain.ID(r.PathValue("job_id"))
	if !domain.ValidID(string(id), false) {
		return "", domain.Invalid("/job_id")
	}
	return id, nil
}
func chunkIndex(r *http.Request) (int, error) {
	n, e := strconv.Atoi(r.PathValue("index"))
	if e != nil || n < 0 || n > 65535 {
		return 0, domain.Invalid("/index")
	}
	return n, nil
}
func (h *Handler) currentScope(r *http.Request, p domain.Principal) (domain.Scope, error) {
	dataset := domain.ID(r.PathValue("dataset_id"))
	me, e := h.Auth.Me(r.Context(), p)
	if e != nil {
		return domain.Scope{}, e
	}
	for _, d := range me["datasets"].([]domain.Dataset) {
		if d.ID == dataset {
			return domain.Scope{Principal: p, DatasetID: d.ID, DataEpoch: d.DataEpoch}, nil
		}
	}
	return domain.Scope{}, domain.Fail("not_found")
}
func (h *Handler) streamChunk(w http.ResponseWriter, r *http.Request, p domain.Principal, scope *domain.Scope) {
	id, e := jobID(r)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	index, e := chunkIndex(r)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	chunk, reader, e := h.Transfer.OpenChunk(r.Context(), p, scope, id, index)
	if e != nil {
		h.problem(w, r, e)
		return
	}
	defer reader.Close()
	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Length", strconv.FormatInt(int64(chunk.Bytes), 10))
	w.Header().Set("X-Chunk-SHA256", chunk.Hash)
	w.WriteHeader(200)
	_, _ = io.Copy(w, reader)
}
