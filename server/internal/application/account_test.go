package application

import (
	"bytes"
	"context"
	"errors"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/maintenance"
	"daymark.local/server/internal/ports"
)

func TestDeletionCapabilitySurvivesRevokedBearerAndErasesFacts(t *testing.T) {
	f, x := transferFixture(t)
	ledger, e := maintenance.OpenLedger(t.TempDir(), bytes.Repeat([]byte{9}, 32), nil, true)
	if e != nil {
		t.Fatal(e)
	}
	defer ledger.Close()
	account, e := NewAccount(t.Context(), f.db, f.auth, f.clock, ledger, x.Files)
	if e != nil {
		t.Fatal(e)
	}
	if _, e = f.sync.Push(t.Context(), f.scope, f.mutation(t, createCommand(domain.StableID("deleted-task"), "private synthetic"))); e != nil {
		t.Fatal(e)
	}
	operation := domain.StableID("delete-user")
	first, e := account.Prepare(t.Context(), f.scope.Principal, []byte("synthetic-password"), operation)
	if e != nil {
		t.Fatal(e)
	}
	rotated, e := account.Prepare(t.Context(), f.scope.Principal, []byte("synthetic-password"), operation)
	if e != nil || first.Token == rotated.Token {
		t.Fatal("prepare must rotate unrecoverable secret", e)
	}
	if _, e = account.Confirm(t.Context(), first.Token, operation); domain.Code(e) != "auth_expired" {
		t.Fatal(e)
	}
	pending, e := account.Confirm(t.Context(), rotated.Token, operation)
	if e != nil || pending.State != "PENDING_LEDGER" {
		t.Fatal(pending, e)
	}
	if _, e = f.auth.Authenticate(t.Context(), f.pair.Access); e == nil {
		t.Fatal("bearer survived deletion")
	}
	same, e := account.Confirm(t.Context(), rotated.Token, operation)
	if e != nil || same.State != "PENDING_LEDGER" {
		t.Fatal(e)
	}
	if ran, e := account.RunOne(t.Context()); e != nil || !ran {
		t.Fatal(ran, e)
	}
	complete, e := account.Status(t.Context(), rotated.Token, operation)
	if e != nil || complete.State != "COMPLETED" {
		t.Fatal(complete, e)
	}
	events, e := ledger.Read(t.Context())
	if e != nil || len(events) != 1 {
		t.Fatal(e)
	}
	e = f.db.View(t.Context(), func(tx ports.Tx) error {
		u, e := tx.User(t.Context(), f.scope.Principal.UserID)
		if e != nil {
			return e
		}
		if u.Status != "DELETED" || u.PasswordHash != "" {
			t.Fatal("private account remains")
		}
		datasets, e := tx.Datasets(t.Context(), u.ID)
		if e == nil && len(datasets) != 0 {
			t.Fatal("dataset remains")
		}
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
}

type failingLedger struct {
	ports.Ledger
	fail bool
}

func (l *failingLedger) Append(ctx context.Context, e domain.LedgerEvent) (string, error) {
	if l.fail {
		return "", errors.New("synthetic ledger unavailable")
	}
	return l.Ledger.Append(ctx, e)
}
func TestDeletionLedgerFailureStaysDisabledAndRecovers(t *testing.T) {
	f, x := transferFixture(t)
	real, e := maintenance.OpenLedger(t.TempDir(), bytes.Repeat([]byte{1}, 32), nil, true)
	if e != nil {
		t.Fatal(e)
	}
	defer real.Close()
	ledger := &failingLedger{Ledger: real, fail: true}
	a, e := NewAccount(t.Context(), f.db, f.auth, f.clock, ledger, x.Files)
	if e != nil {
		t.Fatal(e)
	}
	op := domain.StableID("failure-delete")
	intent, e := a.Prepare(t.Context(), f.scope.Principal, []byte("synthetic-password"), op)
	if e != nil {
		t.Fatal(e)
	}
	if _, e = a.Confirm(t.Context(), intent.Token, op); e != nil {
		t.Fatal(e)
	}
	if _, e = a.RunOne(t.Context()); e == nil {
		t.Fatal("missing ledger failure")
	}
	s, e := a.Status(t.Context(), intent.Token, op)
	if e != nil || s.State != "PENDING_LEDGER" {
		t.Fatal(s, e)
	}
	ledger.fail = false
	f.clock.value = f.clock.value.Add(6 * time.Second)
	if ran, e := a.RunOne(t.Context()); e != nil || !ran {
		t.Fatal(ran, e)
	}
	s, e = a.Status(t.Context(), intent.Token, op)
	if e != nil || s.State != "COMPLETED" {
		t.Fatal(s, e)
	}
}
