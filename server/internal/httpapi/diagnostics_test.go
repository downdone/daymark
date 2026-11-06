package httpapi

import (
	"context"
	"encoding/json"
	"net/http/httptest"
	"testing"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/protocol"
)

func TestDiagnosticRegistryAndProblemCompatibility(t *testing.T) {
	h := &Handler{status: protocol.ErrorStatuses()}
	seen := map[string]bool{}
	for key, status := range h.status {
		code := serverDiagnostic(key)
		if code == "S999" || seen[code] {
			t.Fatalf("unregistered or duplicate diagnostic: %s", key)
		}
		seen[code] = true
		r := httptest.NewRequest("GET", "/synthetic", nil)
		r = r.WithContext(context.WithValue(r.Context(), requestKey{}, "synthetic-request"))
		w := httptest.NewRecorder()
		h.problem(w, r, domain.Fail(key))
		if w.Code != status || w.Header().Get("X-Daymark-Error-Code") != code {
			t.Fatalf("wrong diagnostic for %s", key)
		}
		var body map[string]any
		if err := json.Unmarshal(w.Body.Bytes(), &body); err != nil || body["code"] != key {
			t.Fatalf("business key changed: %s", key)
		}
		codec, err := protocol.NewCodec()
		if err != nil || codec.Validate("Problem", body) != nil {
			t.Fatalf("v1 Problem compatibility broken: %s", key)
		}
	}
}
