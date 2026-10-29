package httpapi

import (
	"encoding/json"
	"net/http/httptest"
	"os"
	"regexp"
	"strings"
	"testing"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/protocol"
)

func TestEveryProtectedContractOperationRejectsBeforeDependencyAccess(t *testing.T) {
	raw, err := os.ReadFile("../../../docs/api/sync-v1.openapi.json")
	if err != nil {
		t.Fatal(err)
	}
	type operation struct {
		ID       string                `json:"operationId"`
		Security []map[string][]string `json:"security"`
	}
	var api struct {
		Paths    map[string]map[string]operation `json:"paths"`
		Security []map[string][]string           `json:"security"`
	}
	if err = json.Unmarshal(raw, &api); err != nil {
		t.Fatal(err)
	}
	codec, err := protocol.NewCodec()
	if err != nil {
		t.Fatal(err)
	}
	// Missing dependencies make an accidental pre-auth SQL/business access fail.
	h := New(nil, nil, codec)
	h.Transfer = &application.Transfer{}
	h.Account = &application.Account{}
	router := h.Router()
	parameter := regexp.MustCompile(`\{[^}]+\}`)
	protected, total := 0, 0
	for path, methods := range api.Paths {
		for method, op := range methods {
			if op.ID == "" {
				continue
			}
			total++
			security := op.Security
			if security == nil {
				security = api.Security
			}
			if len(security) == 0 {
				continue
			}
			protected++
			t.Run(op.ID, func(t *testing.T) {
				url := parameter.ReplaceAllString(path, "20000000-0000-4000-8000-000000000001")
				r := httptest.NewRequest(strings.ToUpper(method), url, strings.NewReader("malformed body must not be parsed"))
				r.Header.Set("X-Daymark-DTO-Version", "1")
				w := httptest.NewRecorder()
				router.ServeHTTP(w, r)
				if w.Code != 401 || w.Header().Get("Cache-Control") != "no-store" {
					t.Fatalf("authorization boundary: status=%d", w.Code)
				}
				value, e := protocol.StrictValue(w.Body.Bytes())
				if e != nil {
					t.Fatal(e)
				}
				if e = codec.Validate("Problem", value); e != nil {
					t.Fatal(e)
				}
			})
		}
	}
	if total != 35 || protected < 25 {
		t.Fatalf("unexpected contract coverage: %d operations/%d protected", total, protected)
	}
	t.Logf("all %d protected operations among %d contract operations checked", protected, total)
}
