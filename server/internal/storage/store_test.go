package storage

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

func openTest(t *testing.T) (*DB, domain.Scope) {
	t.Helper()
	db, e := Open(t.Context(), t.TempDir(), DefaultOptions())
	if e != nil {
		t.Fatalf("%v; cause: %v", e, errors.Unwrap(e))
	}
	t.Cleanup(func() {
		if e := db.Close(); e != nil {
			t.Error(e)
		}
	})
	u, _ := domain.NewID()
	d, _ := domain.NewID()
	epoch, _ := domain.NewID()
	scope := domain.Scope{Principal: domain.Principal{UserID: u}, DatasetID: d, DataEpoch: epoch}
	_, e = db.Update(t.Context(), func(tx ports.Tx) error {
		if e := tx.PutUser(t.Context(), domain.User{ID: u, Login: "synthetic", PasswordHash: "synthetic-only", CredentialVersion: 1, Status: "ACTIVE"}); e != nil {
			return e
		}
		return tx.InsertDataset(t.Context(), domain.Dataset{ID: d, OwnerID: u, DataEpoch: epoch}, 0)
	})
	if e != nil {
		t.Fatal(e)
	}
	return db, scope
}
func sampleTask(id domain.ID) domain.Entity {
	return domain.Entity{Type: domain.TaskKind, ID: id, Revision: 1, Operation: "replace", Value: &domain.Task{ID: id, Title: "synthetic", PlanningState: "INBOX", Status: "OPEN", CreatedAt: "2026-10-28T08:00:00.000Z", UpdatedAt: "2026-10-28T08:00:00.000Z", TagIDs: []domain.ID{}, Links: []domain.Link{}}}
}
func TestRealEngineAndConnectionPragmas(t *testing.T) {
	db, _ := openTest(t)
	var version string
	if e := db.reader.QueryRowContext(t.Context(), "SELECT sqlite_version()").Scan(&version); e != nil {
		t.Fatal(e)
	}
	t.Log("SQLite", version)
	if !safeEngine(version) {
		t.Fatal("unsafe WAL engine")
	}
	for name, pool := range map[string]*sql.DB{"writer": db.writer, "reader": db.reader} {
		for key, want := range map[string]int{"foreign_keys": 1, "synchronous": 2} {
			var got int
			if e := pool.QueryRowContext(t.Context(), "PRAGMA "+key).Scan(&got); e != nil || got != want {
				t.Fatalf("%s %s=%d %v", name, key, got, e)
			}
		}
	}
	if _, e := db.reader.ExecContext(t.Context(), "CREATE TABLE forbidden(x)"); e == nil {
		t.Fatal("read pool permits writes")
	}
}
func TestLastReceiptFailureRollsBackEverything(t *testing.T) {
	db, s := openTest(t)
	id, _ := domain.NewID()
	mid, _ := domain.NewID()
	batch := domain.Batch{Seq: 1, MutationID: mid, InstallationID: mid, CommittedAt: "2026-10-28T08:00:00.000Z", Changes: []domain.Entity{sampleTask(id)}, Structures: []domain.StructureVersion{}, Aliases: []domain.Alias{}, Edges: []domain.Edge{}}
	receipt := domain.EmptyReceipt(mid, s.DataEpoch, 1, batch.CommittedAt)
	receipt.Result = "committed"
	receipt.BatchSeq = domain.Ptr(domain.Counter(1))
	b, _ := json.Marshal(receipt)
	stored := domain.StoredReceipt{Receipt: receipt, Response: b}
	state, e := db.Update(t.Context(), func(tx ports.Tx) error {
		if e := tx.Apply(t.Context(), s, domain.Plan{Changes: []domain.Delta{{After: sampleTask(id)}}}); e != nil {
			return e
		}
		if e := tx.AppendBatch(t.Context(), s, batch); e != nil {
			return e
		}
		if e := tx.AppendReceipt(t.Context(), s, stored); e != nil {
			return e
		}
		return tx.AppendReceipt(t.Context(), s, stored)
	})
	if e == nil || state != ports.NotCommitted {
		t.Fatal(state, e)
	}
	e = db.View(t.Context(), func(tx ports.Tx) error {
		g, e := tx.LoadGraph(t.Context(), s)
		if e != nil {
			return e
		}
		if len(g.Entities) != 0 {
			t.Fatal("partial task")
		}
		d, e := tx.Dataset(t.Context(), s)
		if e != nil {
			return e
		}
		if d.Head != 0 {
			t.Fatal("partial head")
		}
		r, e := tx.Receipt(t.Context(), s, mid)
		if r != nil {
			t.Fatal("partial receipt")
		}
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
}
func TestAggregateRoundtripCASAndScope(t *testing.T) {
	db, s := openTest(t)
	id, _ := domain.NewID()
	entity := sampleTask(id)
	entity.Value.(*domain.Task).Schedule = &domain.Schedule{Kind: "ALL_DAY", StartOn: domain.Ptr(domain.Date("2030-01-01")), EndOn: domain.Ptr(domain.Date("2030-01-02"))}
	entity.Value.(*domain.Task).StartOn = domain.Ptr(domain.Date("2030-01-01"))
	entity.Value.(*domain.Task).DeadlineOn = domain.Ptr(domain.Date("2030-01-01"))
	_, e := db.Update(t.Context(), func(tx ports.Tx) error {
		return tx.Apply(t.Context(), s, domain.Plan{Changes: []domain.Delta{{After: entity}}})
	})
	if e != nil {
		t.Fatal(e)
	}
	if e = db.View(t.Context(), func(tx ports.Tx) error {
		g, e := tx.LoadGraph(t.Context(), s)
		if e != nil {
			return e
		}
		if !domain.Equal(g.Entities[entity.Key()], entity) {
			t.Fatal("aggregate roundtrip differs")
		}
		bad := s
		bad.Principal.UserID = "other"
		if _, e = tx.LoadGraph(t.Context(), bad); domain.Code(e) != "not_found" {
			t.Fatal(e)
		}
		return nil
	}); e != nil {
		t.Fatal(e)
	}
	wrong := entity
	wrong.Revision = 2
	before := entity
	before.Revision = 7
	_, e = db.Update(t.Context(), func(tx ports.Tx) error {
		return tx.Apply(t.Context(), s, domain.Plan{Changes: []domain.Delta{{Before: &before, After: wrong}}})
	})
	if domain.Code(e) != "revision_conflict" {
		t.Fatal(e)
	}
}
func TestRollbackOnCallbackAndCancellation(t *testing.T) {
	db, _ := openTest(t)
	sentinel := errors.New("synthetic failure")
	_, e := db.Update(t.Context(), func(tx ports.Tx) error { return sentinel })
	if !errors.Is(e, sentinel) {
		t.Fatal(e)
	}
	ctx, cancel := context.WithCancel(t.Context())
	cancel()
	called := false
	_, e = db.Update(ctx, func(tx ports.Tx) error { called = true; return nil })
	if called || domain.Code(e) != "cancelled" {
		t.Fatal(called, e)
	}
}
func TestInstanceLockChecksumAndConsistentBackup(t *testing.T) {
	dir := t.TempDir()
	db, e := Open(t.Context(), dir, DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	other, e := Open(t.Context(), dir, DefaultOptions())
	if other != nil || domain.Code(e) != "already_running" {
		t.Fatal(other, e)
	}
	backup := filepath.Join(t.TempDir(), "snapshot.sqlite")
	if e = db.Snapshot(t.Context(), backup); e != nil {
		t.Fatal(e)
	}
	if _, e = os.Stat(backup); e != nil {
		t.Fatal(e)
	}
	if e = db.Close(); e != nil {
		t.Fatal(e)
	}
	o := DefaultOptions()
	o.MigrationOverride = map[string][]byte{"0001_initial.sql": []byte("changed historical migration")}
	if _, e = Open(t.Context(), dir, o); domain.Code(e) != "migration_failed" {
		t.Fatal(e)
	}
	db, e = Open(t.Context(), dir, DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	db.Close()
}
func TestWriterBusyIsBounded(t *testing.T) {
	dir := t.TempDir()
	o := DefaultOptions()
	o.Busy = 30 * time.Millisecond
	db, e := Open(t.Context(), dir, o)
	if e != nil {
		t.Fatal(e)
	}
	defer db.Close()
	external, e := sql.Open("sqlite", db.path+"?_txlock=immediate")
	if e != nil {
		t.Fatal(e)
	}
	defer external.Close()
	external.SetMaxOpenConns(1)
	tx, e := external.BeginTx(t.Context(), nil)
	if e != nil {
		t.Fatal(e)
	}
	defer tx.Rollback()
	started := time.Now()
	_, e = db.Update(t.Context(), func(tx ports.Tx) error { t.Fatal("writer ran while external lock held"); return nil })
	if domain.Code(e) != "busy" {
		t.Fatal(e)
	}
	if time.Since(started) > time.Second {
		t.Fatal("unbounded busy wait")
	}
}
