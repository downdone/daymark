package httpapi

import (
	"context"
	"encoding/json"
	"encoding/pem"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/artifact"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"daymark.local/server/internal/storage"
)

// The Qt executable is an explicit integration prerequisite, never a mock
// success. Ordinary Go runs report SKIP if it has not been built on this host.
func TestQtDesktopEndToEnd(t *testing.T) {
	executable := os.Getenv("DAYMARK_QT_SYNC_TEST_EXE")
	if executable == "" {
		t.Skip("set DAYMARK_QT_SYNC_TEST_EXE to the built AccountSyncTest executable")
	}
	ctx, cancel := context.WithTimeout(t.Context(), 180*time.Second)
	defer cancel()
	root := t.TempDir()
	db, e := storage.Open(ctx, filepath.Join(root, "db"), storage.DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	defer db.Close()
	clock := ports.SystemClock{}
	auth, e := application.NewAuth(ctx, db, clock, application.NewArgonHasher(1))
	if e != nil {
		t.Fatal(e)
	}
	password := "synthetic-isolated-password"
	for _, login := range []string{"second@example.invalid"} {
		if _, e = auth.CreateUser(ctx, login, []byte(password)); e != nil {
			t.Fatal(e)
		}
	}
	codec, e := protocol.NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	var state domain.State
	if e = db.View(ctx, func(tx ports.Tx) error { var e error; state, e = tx.State(ctx); return e }); e != nil {
		t.Fatal(e)
	}
	cursor := &application.CursorCodec{ServerID: state.ServerID, ActiveKey: "integration", Keys: map[string][]byte{"integration": []byte(strings.Repeat("x", 32))}, Clock: clock, TTL: time.Hour}
	syncer := application.NewSync(db, auth, clock, cursor)
	files, e := artifact.Open(filepath.Join(root, "artifacts"))
	if e != nil {
		t.Fatal(e)
	}
	defer files.Close()
	transfer := application.NewTransfer(db, auth, syncer, codec, files, clock, 1024*1024*1024)
	syncer.Exporter = transfer
	syncer.Resolver = transfer
	transfer.Start(ctx)
	defer transfer.Stop()
	handler := New(auth, syncer, codec)
	handler.RegistrationEnabled = true
	handler.AttachTransfer(transfer)
	router := handler.Router()
	var staged atomic.Int32
	var rebased atomic.Bool
	var droppedPush, droppedLogout atomic.Bool
	var expiredStage atomic.Bool
	server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method == "POST" && strings.HasSuffix(r.URL.Path, "/publish") && !rebased.Load() {
			principal, authError := auth.Authenticate(r.Context(), strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer "))
			if authError == nil {
				_, injectError := db.Update(r.Context(), func(tx ports.Tx) error {
					datasets, e := tx.Datasets(r.Context(), principal.UserID)
					if e != nil || len(datasets) != 1 {
						return e
					}
					d := datasets[0]
					if d.Head < 4 || !rebased.CompareAndSwap(false, true) {
						return nil
					}
					scope := domain.Scope{Principal: principal, DatasetID: d.ID, DataEpoch: d.DataEpoch}
					g, e := tx.LoadGraph(r.Context(), scope)
					if e != nil {
						return e
					}
					id := domain.StableID("concurrent-import-writer")
					mid := domain.StableID("concurrent-import-mutation")
					at := domain.At(clock.Now())
					command := &domain.CreateTask{Event: domain.Event{Type: "task.create", EventAt: at}, TaskID: id, Title: "Synthetic concurrent task"}
					plan, e := domain.PlanCommands(g, []domain.Command{command}, d.ID)
					if e != nil {
						return e
					}
					if e = tx.Apply(r.Context(), scope, plan); e != nil {
						return e
					}
					changes := []domain.Entity{}
					for _, delta := range plan.Changes {
						changes = append(changes, delta.After)
					}
					batch := domain.Batch{Seq: d.Head + 1, MutationID: mid, InstallationID: principal.InstallationID, CommittedAt: at, Changes: changes, Structures: plan.Structures, Aliases: plan.Aliases, Edges: plan.Edges}
					if e = tx.AppendBatch(r.Context(), scope, batch); e != nil {
						return e
					}
					receipt := domain.EmptyReceipt(mid, d.DataEpoch, d.Head+1, at)
					receipt.Result = "committed"
					receipt.BatchSeq = domain.Ptr(d.Head + 1)
					response, _ := json.Marshal(receipt)
					return tx.AppendReceipt(r.Context(), scope, domain.StoredReceipt{Receipt: receipt, Response: response})
				})
				if injectError != nil {
					t.Errorf("concurrent import writer: %v", injectError)
				}
			}
		}
		recorder := httptest.NewRecorder()
		router.ServeHTTP(recorder, r)
		if recorder.Code < 300 && ((r.Method == "POST" && strings.HasSuffix(r.URL.Path, "/sync/push") && droppedPush.CompareAndSwap(false, true)) || (r.URL.Path == "/v1/auth/logout" && droppedLogout.CompareAndSwap(false, true))) {
			connection, _, e := w.(http.Hijacker).Hijack()
			if e != nil {
				t.Errorf("fault proxy hijack: %v", e)
				return
			}
			_ = connection.Close()
			return
		}
		if recorder.Code < 300 && r.Method == "POST" && strings.HasSuffix(r.URL.Path, "/staged-mutations") {
			staged.Add(1)
			if expiredStage.CompareAndSwap(false, true) {
				var result domain.Job
				if e := json.Unmarshal(recorder.Body.Bytes(), &result); e != nil {
					t.Error(e)
					return
				}
				_, e := db.Update(r.Context(), func(tx ports.Tx) error {
					job, e := tx.Job(r.Context(), result.ID)
					if e != nil {
						return e
					}
					oldState, attempt := job.State, job.Attempt
					job.State, job.Reserved = "EXPIRED", 0
					job.ExpiresAt = domain.At(clock.Now().Add(-time.Second))
					result = *job
					return tx.UpdateJob(r.Context(), *job, oldState, attempt)
				})
				if e != nil {
					t.Errorf("expire real staged job: %v", e)
					return
				}
				recorder.Body.Reset()
				_ = json.NewEncoder(recorder.Body).Encode(result)
			}
		}
		if recorder.Code >= 400 {
			var p struct {
				Code string `json:"code"`
			}
			_ = json.Unmarshal(recorder.Body.Bytes(), &p)
			t.Logf("HTTP %s %s status=%d code=%s", r.Method, r.URL.Path, recorder.Code, p.Code)
		}
		for k, values := range recorder.Header() {
			for _, v := range values {
				w.Header().Add(k, v)
			}
		}
		w.WriteHeader(recorder.Code)
		_, _ = w.Write(recorder.Body.Bytes())
	}))
	defer server.Close()
	certificate := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: server.Certificate().Raw})
	ca := filepath.Join(root, "ca.pem")
	if e = os.WriteFile(ca, certificate, 0600); e != nil {
		t.Fatal(e)
	}
	arguments := []string{}
	if strings.Contains(filepath.Base(executable), "test_task_workspace") {
		arguments = append(arguments, "accountSyncRealServer")
	}
	command := exec.CommandContext(ctx, executable, arguments...)
	command.WaitDelay = 5 * time.Second
	graphPath := filepath.Join(root, "client-graph.json")
	command.Env = append(os.Environ(), "QT_QPA_PLATFORM=offscreen", "DAYMARK_SYNC_E2E_URL="+server.URL, "DAYMARK_SYNC_E2E_PASSWORD="+password, "DAYMARK_SYNC_E2E_CA="+ca, "DAYMARK_SYNC_E2E_GRAPH_PATH="+graphPath)
	output, e := command.CombinedOutput()
	if e != nil {
		t.Fatalf("Qt integration failed: %v\n%s", e, output)
	}
	if !strings.Contains(string(output), "PASS native credentials;") {
		t.Fatalf("missing integration completion evidence: %s", output)
	}
	if staged.Load() == 0 || !rebased.Load() || !droppedPush.Load() || !droppedLogout.Load() || !expiredStage.Load() {
		t.Fatal("large mutation/rebase path not exercised")
	}
	clientJSON, e := os.ReadFile(graphPath)
	if e != nil {
		t.Fatal(e)
	}
	var client struct {
		Entities []domain.Entity `json:"entities"`
	}
	if e = json.Unmarshal(clientJSON, &client); e != nil {
		t.Fatal(e)
	}
	if e = db.View(ctx, func(tx ports.Tx) error {
		user, e := tx.UserByLogin(ctx, "synthetic@example.invalid")
		if e != nil {
			return e
		}
		datasets, e := tx.Datasets(ctx, user.ID)
		if e != nil {
			return e
		}
		d := datasets[0]
		g, e := tx.LoadGraph(ctx, domain.Scope{Principal: domain.Principal{UserID: user.ID}, DatasetID: d.ID, DataEpoch: d.DataEpoch})
		if e != nil {
			return e
		}
		if len(g.Entities) != len(client.Entities) {
			t.Fatalf("graph entity counts differ %d/%d", len(g.Entities), len(client.Entities))
		}
		for _, entity := range client.Entities {
			remote, exists := g.Entities[entity.Key()]
			if !exists || !domain.Equal(remote.Value, entity.Value) {
				clientValue, _ := json.Marshal(entity.Value)
				serverValue, _ := json.Marshal(remote.Value)
				t.Fatalf("canonical business facts differ for %s\nclient=%s\nserver=%s", entity.Key().String(), clientValue, serverValue)
			}
		}
		return nil
	}); e != nil {
		t.Fatal(e)
	}
	t.Log(strings.TrimSpace(string(output)))
}
