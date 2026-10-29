package httpapi

import (
	"bytes"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"daymark.local/server/internal/storage"
)

func TestTLSLoginAtomicPushPullAndProtocolErrors(t *testing.T) {
	db, e := storage.Open(t.Context(), t.TempDir(), storage.DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	defer db.Close()
	clock := ports.SystemClock{}
	auth, e := application.NewAuth(t.Context(), db, clock, application.NewArgonHasher(1))
	if e != nil {
		t.Fatal(e)
	}
	codec, e := protocol.NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	var state domain.State
	_ = db.View(t.Context(), func(tx ports.Tx) error { state, e = tx.State(t.Context()); return e })
	cursor := &application.CursorCodec{ServerID: state.ServerID, ActiveKey: "test", Keys: map[string][]byte{"test": []byte(strings.Repeat("x", 32))}, Clock: clock, TTL: time.Hour}
	syncer := application.NewSync(db, auth, clock, cursor)
	handler := New(auth, syncer, codec)
	handler.RegistrationEnabled = true
	server := httptest.NewTLSServer(handler.Router())
	defer server.Close()
	client := server.Client()
	request := func(method, path, token string, body []byte) (int, []byte) {
		t.Helper()
		req, e := http.NewRequestWithContext(t.Context(), method, server.URL+path, bytes.NewReader(body))
		if e != nil {
			t.Fatal(e)
		}
		req.Header.Set("X-Daymark-DTO-Version", "1")
		req.Header.Set("Content-Type", "application/json")
		if token != "" {
			req.Header.Set("Authorization", "Bearer "+token)
		}
		res, e := client.Do(req)
		if e != nil {
			t.Fatal(e)
		}
		defer res.Body.Close()
		data, e := io.ReadAll(res.Body)
		if e != nil {
			t.Fatal(e)
		}
		if res.Header.Get("Cache-Control") != "no-store" {
			t.Fatal("personal response cached")
		}
		if res.StatusCode >= 400 {
			v, e := protocol.StrictValue(data)
			if e != nil {
				t.Fatal(e)
			}
			if e = codec.Validate("Problem", v); e != nil {
				t.Fatal("invalid problem", e)
			}
		}
		return res.StatusCode, data
	}
	status, raw := request("GET", "/v1/capabilities", "", nil)
	if status != 200 {
		t.Fatal(status)
	}
	var cap map[string]any
	_ = json.Unmarshal(raw, &cap)
	if cap["protocol_version"] != float64(1) {
		t.Fatal("capability")
	}
	registration := []byte(`{"protocol_version":1,"login":" Synthetic@Example.Invalid ","password":"synthetic-password"}`)
	if status, _ = request("POST", "/v1/auth/register", "", registration); status != 204 {
		t.Fatal("registration", status)
	}
	if status, _ = request("POST", "/v1/auth/register", "", []byte(`{"protocol_version":1,"login":"synthetic@example.invalid","password":"different-password"}`)); status != 204 {
		t.Fatal("duplicate must not enumerate login", status)
	}
	if status, _ = request("POST", "/v1/auth/register", "", []byte(`{"protocol_version":1,"login":"invalid-new","password":"short"}`)); status != 422 {
		t.Fatal("short password accepted", status)
	}
	if status, _ = request("POST", "/v1/auth/register", "", []byte(`{"protocol_version":1,"login":"invalid-new","password":"long-valid-password","extra":true}`)); status != 422 {
		t.Fatal("unknown registration field accepted", status)
	}
	if status, _ = request("POST", "/v1/auth/register", "", []byte(`{"protocol_version":1,"login":"invalid-new","login":"other-new","password":"long-valid-password"}`)); status != 400 {
		t.Fatal("duplicate registration key accepted", status)
	}
	if status, _ = request("POST", "/v1/auth/register", "", registration); status != 429 {
		t.Fatal("registration limit bypassed", status)
	}
	installation := domain.StableID("tls-installation")
	login, _ := json.Marshal(application.LoginInput{ProtocolVersion: 1, Login: "synthetic@example.invalid", Password: "synthetic-password", InstallationID: installation, DeviceName: "TLS test", Platform: "windows"})
	status, raw = request("POST", "/v1/auth/login", "", login)
	if status != 200 {
		t.Fatal("login", status)
	}
	var pair domain.TokenPair
	if e = json.Unmarshal(raw, &pair); e != nil {
		t.Fatal(e)
	}
	status, raw = request("GET", "/v1/me", pair.Access, nil)
	if status != 200 {
		t.Fatal(status)
	}
	var me struct {
		Datasets []domain.Dataset `json:"datasets"`
	}
	if e = json.Unmarshal(raw, &me); e != nil {
		t.Fatal(e)
	}
	principal, e := auth.Authenticate(t.Context(), pair.Access)
	if e != nil {
		t.Fatal(e)
	}
	scope := domain.Scope{Principal: principal, DatasetID: me.Datasets[0].ID, DataEpoch: me.Datasets[0].DataEpoch}
	id := domain.StableID("tls-task")
	mid := domain.StableID("tls-mutation")
	command := &domain.CreateTask{Event: domain.Event{Type: "task.create", EventAt: "2026-10-28T08:00:00.000Z"}, TaskID: id, Title: "TLS synthetic"}
	plan, e := domain.PlanCommands(domain.EmptyGraph(), []domain.Command{command}, scope.DatasetID)
	if e != nil {
		t.Fatal(e)
	}
	payload, _ := json.Marshal(map[string]any{"protocol_version": 1, "data_epoch": scope.DataEpoch, "mutation_id": mid, "installation_id": installation, "client_seq": "1", "preconditions": plan.Reads, "commands": []domain.Command{command}})
	base := "/v1/datasets/" + string(scope.DatasetID)
	status, raw = request("POST", base+"/sync/push", pair.Access, payload)
	if status != 200 {
		t.Fatalf("push %d: %s", status, raw)
	}
	var receipt domain.Receipt
	if e = json.Unmarshal(raw, &receipt); e != nil || receipt.BatchSeq == nil || *receipt.BatchSeq != 1 {
		t.Fatal(e)
	}
	status, replayed := request("POST", base+"/sync/push", pair.Access, payload)
	if status != 200 || !bytes.Equal(raw, replayed) {
		t.Fatal("retry changed receipt")
	}
	start, e := cursor.Issue(scope, 0, nil)
	if e != nil {
		t.Fatal(e)
	}
	status, raw = request("GET", base+"/sync/pull?cursor="+start, pair.Access, nil)
	if status != 200 {
		t.Fatalf("pull %d: %s", status, raw)
	}
	var page application.PullResponse
	if e = json.Unmarshal(raw, &page); e != nil || len(page.Batches) != 1 || page.HasMore {
		t.Fatal(e)
	}
	status, _ = request("POST", base+"/sync/push", pair.Access, []byte(`{"protocol_version":1,"protocol_version":1}`))
	if status != 400 {
		t.Fatal("duplicate keys", status)
	}
	status, _ = request("POST", base+"/sync/push", "", bytes.Repeat([]byte("x"), 1048577))
	if status != 401 {
		t.Fatal("unauthorized body parsed before auth", status)
	}
	status, _ = request("POST", base+"/sync/push", pair.Access, bytes.Repeat([]byte("x"), 1048577))
	if status != 413 {
		t.Fatal("body limit", status)
	}
	status, _ = request("GET", base+"/sync/pull?cursor="+start+"&cursor="+start, pair.Access, nil)
	if status != 400 {
		t.Fatal("duplicate query parameter", status)
	}
	status, _ = request("DELETE", "/v1/capabilities", "", nil)
	if status != 405 {
		t.Fatal("method negotiation", status)
	}
	// Default TLS trust does not accept httptest's private certificate.
	if _, e = http.Get(server.URL + "/health/live"); e == nil {
		t.Fatal("untrusted TLS certificate accepted")
	}
}
