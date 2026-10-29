package storage

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

// Separate OS processes exercise the real WAL and native instance lock. The
// barrier is reached after actual writes, not a simulated repository response.
func TestProcessKillKeepsBusinessBatchAndReceiptAtomic(t *testing.T) {
	for _, phase := range []string{"after-business", "before-receipt", "before-commit", "after-commit"} {
		t.Run(phase, func(t *testing.T) {
			root := t.TempDir()
			db, e := Open(t.Context(), root, DefaultOptions())
			if e != nil {
				t.Fatal(e)
			}
			user, _ := domain.NewID()
			dataset, _ := domain.NewID()
			epoch, _ := domain.NewID()
			scope := domain.Scope{Principal: domain.Principal{UserID: user}, DatasetID: dataset, DataEpoch: epoch}
			_, e = db.Update(t.Context(), func(tx ports.Tx) error {
				if e := tx.PutUser(t.Context(), domain.User{ID: user, Login: "synthetic", PasswordHash: "test-only", CredentialVersion: 1, Status: "ACTIVE"}); e != nil {
					return e
				}
				return tx.InsertDataset(t.Context(), domain.Dataset{ID: dataset, OwnerID: user, DataEpoch: epoch}, 0)
			})
			if e != nil {
				t.Fatal(e)
			}
			if e = db.Close(); e != nil {
				t.Fatal(e)
			}
			raw, _ := json.Marshal(scope)
			command := exec.Command(os.Args[0], "-test.run=^TestCrashHelper$")
			command.Env = append(os.Environ(), "DAYMARK_CRASH_ROOT="+root, "DAYMARK_CRASH_SCOPE="+string(raw), "DAYMARK_CRASH_PHASE="+phase)
			output, e := command.StdoutPipe()
			if e != nil {
				t.Fatal(e)
			}
			input, e := command.StdinPipe()
			if e != nil {
				t.Fatal(e)
			}
			defer input.Close()
			if e = command.Start(); e != nil {
				t.Fatal(e)
			}
			defer func() { _ = command.Process.Kill(); _ = command.Wait() }()
			reached := make(chan bool, 1)
			go func() {
				scan := bufio.NewScanner(output)
				for scan.Scan() {
					if scan.Text() == "BARRIER" {
						reached <- true
						return
					}
				}
				reached <- false
			}()
			select {
			case ok := <-reached:
				if !ok {
					t.Fatal("child failed before real write barrier")
				}
			case <-time.After(15 * time.Second):
				t.Fatal("write barrier timeout")
			}
			if e = command.Process.Kill(); e != nil {
				t.Fatal(e)
			}
			_ = command.Wait()
			reopened, e := Open(t.Context(), root, DefaultOptions())
			if e != nil {
				t.Fatal(e)
			}
			defer reopened.Close()
			e = reopened.View(t.Context(), func(tx ports.Tx) error {
				g, e := tx.LoadGraph(t.Context(), scope)
				if e != nil {
					return e
				}
				d, e := tx.Dataset(t.Context(), scope)
				if e != nil {
					return e
				}
				receipt, e := tx.Receipt(t.Context(), scope, domain.StableID("crash-mutation"))
				if e != nil {
					return e
				}
				committed := phase == "after-commit"
				if committed {
					if len(g.Entities) != 1 || d.Head != 1 || receipt == nil {
						t.Fatal("committed aggregate incomplete")
					}
				} else if len(g.Entities) != 0 || d.Head != 0 || receipt != nil {
					t.Fatal("uncommitted aggregate survived kill")
				}
				return nil
			})
			if e != nil {
				t.Fatal(e)
			}
		})
	}
}
func TestCrashHelper(t *testing.T) {
	root := os.Getenv("DAYMARK_CRASH_ROOT")
	if root == "" {
		return
	}
	ctx := context.Background()
	var scope domain.Scope
	if e := json.Unmarshal([]byte(os.Getenv("DAYMARK_CRASH_SCOPE")), &scope); e != nil {
		t.Fatal(e)
	}
	db, e := Open(ctx, root, DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	defer db.Close()
	phase := os.Getenv("DAYMARK_CRASH_PHASE")
	barrier := func() { fmt.Fprintln(os.Stdout, "BARRIER"); var b [1]byte; _, _ = os.Stdin.Read(b[:]) }
	id := domain.StableID("crash-task")
	mid := domain.StableID("crash-mutation")
	batch := domain.Batch{Seq: 1, MutationID: mid, InstallationID: mid, CommittedAt: "2026-10-28T08:00:00.000Z", Changes: []domain.Entity{sampleTask(id)}, Structures: []domain.StructureVersion{}, Aliases: []domain.Alias{}, Edges: []domain.Edge{}}
	receipt := domain.EmptyReceipt(mid, scope.DataEpoch, 1, batch.CommittedAt)
	receipt.Result = "committed"
	receipt.BatchSeq = domain.Ptr(domain.Counter(1))
	response, _ := json.Marshal(receipt)
	_, e = db.Update(ctx, func(tx ports.Tx) error {
		if e := tx.Apply(ctx, scope, domain.Plan{Changes: []domain.Delta{{After: sampleTask(id)}}}); e != nil {
			return e
		}
		if phase == "after-business" {
			barrier()
		}
		if e := tx.AppendBatch(ctx, scope, batch); e != nil {
			return e
		}
		if phase == "before-receipt" {
			barrier()
		}
		if e := tx.AppendReceipt(ctx, scope, domain.StoredReceipt{Receipt: receipt, Response: response}); e != nil {
			return e
		}
		if phase == "before-commit" {
			barrier()
		}
		return nil
	})
	if e != nil {
		t.Fatal(e)
	}
	if phase == "after-commit" {
		barrier()
	}
}
