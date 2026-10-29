package application

import (
	"context"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"strings"
	"sync"
	"testing"
)

func TestRegistrationIsAtomicConcurrentAndNeverReplacesIdentity(t *testing.T) {
	f := newFixture(t)
	const name = "registered@example.invalid"
	const password = "registration-password"
	var wg sync.WaitGroup
	errors := make(chan error, 8)
	for i := 0; i < 8; i++ {
		wg.Go(func() { errors <- f.auth.Register(t.Context(), " Registered@Example.Invalid ", []byte(password)) })
	}
	wg.Wait()
	close(errors)
	for e := range errors {
		if e != nil {
			t.Fatal(e)
		}
	}
	var original domain.User
	if e := f.db.View(t.Context(), func(tx ports.Tx) error {
		u, e := tx.UserByLogin(t.Context(), name)
		if e != nil {
			return e
		}
		if u == nil {
			t.Fatal("registration missing user")
		}
		original = *u
		ds, e := tx.Datasets(t.Context(), u.ID)
		if e != nil {
			return e
		}
		if len(ds) != 1 || ds[0].Head != 0 {
			t.Fatal("registration must create one empty dataset")
		}
		return nil
	}); e != nil {
		t.Fatal(e)
	}
	if e := f.auth.Register(t.Context(), name, []byte("different-password")); e != nil {
		t.Fatal(e)
	}
	if _, e := f.auth.Login(t.Context(), LoginInput{1, name, password, domain.StableID("registration-test"), "test", "windows"}); e != nil {
		t.Fatal("original password changed", e)
	}
	if _, e := f.auth.Login(t.Context(), LoginInput{1, name, "different-password", domain.StableID("registration-test"), "test", "windows"}); e == nil {
		t.Fatal("registration overwrote existing password")
	}
	_, e := f.db.Update(t.Context(), func(tx ports.Tx) error {
		original.Status = "DELETED"
		original.PasswordHash = ""
		return tx.PutUser(t.Context(), original)
	})
	if e != nil {
		t.Fatal(e)
	}
	if e = f.auth.Register(t.Context(), name, []byte(password)); e != nil {
		t.Fatal(e)
	}
	if e = f.db.View(t.Context(), func(tx ports.Tx) error {
		u, e := tx.UserByLogin(t.Context(), name)
		if e != nil {
			return e
		}
		if u.ID != original.ID || u.Status != "DELETED" || u.PasswordHash != "" {
			t.Fatal("registration revived a deleted identity")
		}
		ds, e := tx.Datasets(t.Context(), u.ID)
		if e == nil && len(ds) != 1 {
			t.Fatal("duplicate dataset")
		}
		return e
	}); e != nil {
		t.Fatal(e)
	}
}
func TestRegistrationRejectsInvalidInputBeforeCreatingRows(t *testing.T) {
	f := newFixture(t)
	for _, c := range []struct{ name, password string }{
		{"new-account", "short"}, {"new-account", strings.Repeat("🔑", 14)},
		{"new-account", strings.Repeat("字", 400)}, {"invalid account", "long-valid-password"},
		{"Kelvin", "long-valid-password"},
	} {
		if e := f.auth.Register(t.Context(), c.name, []byte(c.password)); domain.Code(e) != "validation_failed" {
			t.Fatal("invalid registration", e)
		}
	}
	if e := f.db.View(t.Context(), func(tx ports.Tx) error {
		u, e := tx.UserByLogin(t.Context(), "new-account")
		if u != nil {
			t.Fatal("invalid registration left a user")
		}
		return e
	}); e != nil {
		t.Fatal(e)
	}
	if e := f.auth.Register(t.Context(), "unicode-password", []byte(strings.Repeat("🔑", 15))); e != nil {
		t.Fatal("unicode password rejected", e)
	}
}

// The failure is injected after PutUser into a real SQLite transaction.
type registrationRejectDataset struct{ ports.Tx }

func (tx registrationRejectDataset) InsertDataset(context.Context, domain.Dataset, int64) error {
	return domain.Fail("quota_exceeded")
}

type registrationFailStore struct{ ports.Store }

func (s registrationFailStore) Update(ctx context.Context, fn func(ports.Tx) error) (ports.CommitState, error) {
	return s.Store.Update(ctx, func(tx ports.Tx) error { return fn(registrationRejectDataset{tx}) })
}
func TestRegistrationDatasetFailureRollsBackActualUserInsert(t *testing.T) {
	f := newFixture(t)
	a, e := NewAuth(t.Context(), registrationFailStore{f.db}, f.clock, f.hasher)
	if e != nil {
		t.Fatal(e)
	}
	if e = a.Register(t.Context(), "rollback-user", []byte("registration-password")); domain.Code(e) != "quota_exceeded" {
		t.Fatal("write failure not propagated", e)
	}
	if e = f.db.View(t.Context(), func(tx ports.Tx) error {
		u, e := tx.UserByLogin(t.Context(), "rollback-user")
		if u != nil {
			t.Fatal("failed registration left a user without dataset")
		}
		return e
	}); e != nil {
		t.Fatal(e)
	}
}
