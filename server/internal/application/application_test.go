package application

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"strings"
	"sync"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"daymark.local/server/internal/storage"
)

type testClock struct{ value time.Time }

func TestInvitedAccountAdminRecoveryRevokesSessionsAndCannotReviveDeletion(t *testing.T) {
	f := newFixture(t)
	if _, e := normalizedLogin("Kelvin@example.invalid"); e == nil {
		t.Fatal("non ASCII login folded into another identity")
	}
	if e := f.auth.ResetPasswordAdmin(t.Context(), " SyNtHeTiC@Example.Invalid ", []byte("replacement-password")); e != nil {
		t.Fatal(e)
	}
	if _, e := f.auth.Authenticate(t.Context(), f.pair.Access); e == nil {
		t.Fatal("old access survived reset")
	}
	if _, e := f.auth.Refresh(t.Context(), f.pair.Refresh); e == nil {
		t.Fatal("old refresh survived reset")
	}
	_, e := f.db.Update(t.Context(), func(tx ports.Tx) error {
		u, e := tx.User(t.Context(), f.scope.Principal.UserID)
		if e != nil {
			return e
		}
		u.Status = "DELETED"
		u.PasswordHash = ""
		return tx.PutUser(t.Context(), *u)
	})
	if e != nil {
		t.Fatal(e)
	}
	if e = f.auth.ResetPasswordAdmin(t.Context(), "synthetic@example.invalid", []byte("another-password")); domain.Code(e) != "account_disabled" {
		t.Fatal("admin revived deleted identity", e)
	}
}

func (c *testClock) Now() time.Time { return c.value }

type fakeHasher struct{ beforeVerify func() }

func (h *fakeHasher) Hash(ctx context.Context, b []byte) (string, error) {
	sum := sha256.Sum256(b)
	return "test-only:" + hex.EncodeToString(sum[:]), nil
}
func (h *fakeHasher) Verify(ctx context.Context, hash string, b []byte) (bool, error) {
	if h.beforeVerify != nil {
		h.beforeVerify()
	}
	want, _ := h.Hash(ctx, b)
	return hash == want, nil
}

type fixture struct {
	db     *storage.DB
	auth   *Auth
	sync   *Sync
	codec  *protocol.Codec
	clock  *testClock
	hasher *fakeHasher
	scope  domain.Scope
	pair   domain.TokenPair
}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	f := &fixture{clock: &testClock{time.Date(2026, 10, 28, 8, 0, 0, 0, time.UTC)}, hasher: &fakeHasher{}}
	var e error
	f.db, e = storage.Open(t.Context(), t.TempDir(), storage.DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { f.db.Close() })
	f.auth, e = NewAuth(t.Context(), f.db, f.clock, f.hasher)
	if e != nil {
		t.Fatal(e)
	}
	uid, e := f.auth.CreateUser(t.Context(), "synthetic@example.invalid", []byte("synthetic-password"))
	if e != nil {
		t.Fatal(e)
	}
	installation := domain.StableID("test-installation")
	f.pair, e = f.auth.Login(t.Context(), LoginInput{1, "synthetic@example.invalid", "synthetic-password", installation, "test", "windows"})
	if e != nil {
		t.Fatal(e)
	}
	principal, e := f.auth.Authenticate(t.Context(), f.pair.Access)
	if e != nil {
		t.Fatal(e)
	}
	var state domain.State
	e = f.db.View(t.Context(), func(tx ports.Tx) error {
		ds, e := tx.Datasets(t.Context(), uid)
		if e != nil {
			return e
		}
		f.scope = domain.Scope{Principal: principal, DatasetID: ds[0].ID, DataEpoch: ds[0].DataEpoch}
		state, e = tx.State(t.Context())
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
	f.codec, e = protocol.NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	cursor := &CursorCodec{ServerID: state.ServerID, ActiveKey: "test", Keys: map[string][]byte{"test": []byte(strings.Repeat("x", 32))}, Clock: f.clock, TTL: 90 * 24 * time.Hour}
	f.sync = NewSync(f.db, f.auth, f.clock, cursor)
	return f
}
func (f *fixture) mutation(t *testing.T, commands ...domain.Command) domain.Mutation {
	t.Helper()
	var graph domain.Graph
	e := f.db.View(t.Context(), func(tx ports.Tx) error { var e error; graph, e = tx.LoadGraph(t.Context(), f.scope); return e })
	if e != nil {
		t.Fatal(e)
	}
	plan, e := domain.PlanCommands(graph, commands, f.scope.DatasetID)
	if e != nil {
		t.Fatal(e)
	}
	id, _ := domain.NewID()
	raw, e := json.Marshal(map[string]any{"protocol_version": 1, "data_epoch": f.scope.DataEpoch, "mutation_id": id, "installation_id": f.scope.Principal.InstallationID, "client_seq": domain.Counter(1), "preconditions": plan.Reads, "commands": commands})
	if e != nil {
		t.Fatal(e)
	}
	out, e := f.codec.DecodeMutation(raw, f.scope.DatasetID)
	if e != nil {
		t.Fatal(e)
	}
	return out
}
func createCommand(id domain.ID, title string) *domain.CreateTask {
	return &domain.CreateTask{Event: domain.Event{Type: "task.create", EventAt: "2026-10-28T08:00:00.000Z"}, TaskID: id, Title: title}
}
func patchCommand(id domain.ID, title string) *domain.PatchTask {
	return &domain.PatchTask{Event: domain.Event{Type: "task.patch", EventAt: "2026-10-28T08:00:00.000Z"}, TaskID: id, Patch: domain.Patch{Title: domain.Field[string]{Present: true, Value: title}}}
}
func TestRefreshReplayRevocationCommits(t *testing.T) {
	f := newFixture(t)
	next, e := f.auth.Refresh(t.Context(), f.pair.Refresh)
	if e != nil {
		t.Fatal(e)
	}
	if next.Refresh == f.pair.Refresh || next.Generation != 2 || next.RefreshExpiry != f.pair.RefreshExpiry {
		t.Fatal("rotation")
	}
	_, e = f.auth.Refresh(t.Context(), f.pair.Refresh)
	if domain.Code(e) != "refresh_replayed" {
		t.Fatal(e)
	}
	_, e = f.auth.Authenticate(t.Context(), next.Access)
	if domain.Code(e) != "device_revoked" {
		t.Fatal("revocation rolled back", e)
	}
	e = f.db.View(t.Context(), func(tx ports.Tx) error {
		s, e := tx.Session(t.Context(), next.SessionID)
		if e == nil && s.RevokedMS == nil {
			t.Fatal("not persisted")
		}
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
}
func TestPasswordChangeAndKDFRace(t *testing.T) {
	f := newFixture(t)
	if e := f.auth.ChangePassword(t.Context(), f.scope.Principal, []byte("synthetic-password"), []byte("synthetic-new-password")); e != nil {
		t.Fatal(e)
	}
	if _, e := f.auth.Authenticate(t.Context(), f.pair.Access); e == nil {
		t.Fatal("old session active")
	}
	input := LoginInput{1, " SYNTHETIC@EXAMPLE.INVALID ", "synthetic-password", domain.StableID("other"), "other", "macos"}
	if _, e := f.auth.Login(t.Context(), input); domain.Code(e) != "invalid_credentials" {
		t.Fatal(e)
	}
	input.Password = "synthetic-new-password"
	if _, e := f.auth.Login(t.Context(), input); e != nil {
		t.Fatal(e)
	}
	f.hasher.beforeVerify = func() {
		f.hasher.beforeVerify = nil
		_, e := f.db.Update(t.Context(), func(tx ports.Tx) error {
			u, e := tx.User(t.Context(), f.scope.Principal.UserID)
			if e != nil {
				return e
			}
			u.CredentialVersion++
			return tx.PutUser(t.Context(), *u)
		})
		if e != nil {
			t.Fatal(e)
		}
	}
	if _, e := f.auth.Login(t.Context(), input); domain.Code(e) != "invalid_credentials" {
		t.Fatal("KDF race accepted", e)
	}
}
func TestConcurrentMutationIdempotencyAndChangedPayload(t *testing.T) {
	f := newFixture(t)
	id := domain.StableID("task-one")
	m := f.mutation(t, createCommand(id, "one"))
	results := make(chan domain.Receipt, 2)
	errs := make(chan error, 2)
	var wg sync.WaitGroup
	for range 2 {
		wg.Add(1)
		go func() { defer wg.Done(); r, e := f.sync.Push(t.Context(), f.scope, m); results <- r; errs <- e }()
	}
	wg.Wait()
	close(results)
	close(errs)
	for e := range errs {
		if e != nil {
			t.Fatal(e)
		}
	}
	var first *domain.Receipt
	for r := range results {
		if first == nil {
			first = &r
		} else if !domain.Equal(*first, r) {
			t.Fatal("different replay receipt")
		}
	}
	altered := m
	altered.Hash = sha256.Sum256([]byte("different payload"))
	if _, e := f.sync.Push(t.Context(), f.scope, altered); domain.Code(e) != "mutation_payload_mismatch" {
		t.Fatal(e)
	}
	if e := f.db.View(t.Context(), func(tx ports.Tx) error {
		d, e := tx.Dataset(t.Context(), f.scope)
		if e == nil && d.Head != 1 {
			t.Fatal("double batch", d.Head)
		}
		return e
	}); e != nil {
		t.Fatal(e)
	}
}
func TestConflictNoOpAndReceiptBeforeStaleCAS(t *testing.T) {
	f := newFixture(t)
	id := domain.StableID("conflict-task")
	create := f.mutation(t, createCommand(id, "one"))
	r, e := f.sync.Push(t.Context(), f.scope, create)
	if e != nil {
		t.Fatal(e)
	}
	old := f.mutation(t, patchCommand(id, "stale"))
	fresh := f.mutation(t, patchCommand(id, "new"))
	if _, e = f.sync.Push(t.Context(), f.scope, fresh); e != nil {
		t.Fatal(e)
	}
	if _, e = f.sync.Push(t.Context(), f.scope, old); domain.Code(e) != "revision_conflict" {
		t.Fatal(e)
	}
	replay, e := f.sync.Push(t.Context(), f.scope, create)
	if e != nil || !domain.Equal(r, replay) {
		t.Fatal("stale CAS checked before receipt", e)
	}
	noop := f.mutation(t, patchCommand(id, "new"))
	r, e = f.sync.Push(t.Context(), f.scope, noop)
	if e != nil || r.Result != "no_op" || r.BatchSeq != nil || r.ObservedHead != 2 {
		t.Fatal(r, e)
	}
}
func TestCursorBoundedPagesAndPerBatchCheckpoints(t *testing.T) {
	f := newFixture(t)
	for _, name := range []string{"one", "two"} {
		if _, e := f.sync.Push(t.Context(), f.scope, f.mutation(t, createCommand(domain.StableID(name), name))); e != nil {
			t.Fatal(e)
		}
	}
	cursor, e := f.sync.Cursor.Issue(f.scope, 0, nil)
	if e != nil {
		t.Fatal(e)
	}
	page, e := f.sync.Pull(t.Context(), f.scope, cursor, 1)
	if e != nil || !page.HasMore || page.Through != 2 {
		t.Fatal(page, e)
	}
	claim, e := f.sync.Cursor.Verify(page.Batches[0].AfterCursor, f.scope)
	if e != nil || claim.After != 1 || claim.Through == nil || *claim.Through != 2 {
		t.Fatal(claim, e)
	}
	if _, e = f.sync.Push(t.Context(), f.scope, f.mutation(t, createCommand(domain.StableID("three"), "three"))); e != nil {
		t.Fatal(e)
	}
	page, e = f.sync.Pull(t.Context(), f.scope, page.Next, 1)
	if e != nil || page.HasMore || page.Through != 2 {
		t.Fatal(page, e)
	}
	claim, e = f.sync.Cursor.Verify(page.Next, f.scope)
	if e != nil || claim.Through != nil || claim.After != 2 {
		t.Fatal(claim, e)
	}
	page, e = f.sync.Pull(t.Context(), f.scope, page.Next, 128)
	if e != nil || len(page.Batches) != 1 || page.Through != 3 {
		t.Fatal(page, e)
	}
	bad := f.scope
	bad.DatasetID = domain.StableID("other-dataset")
	if _, e = f.sync.Cursor.Verify(cursor, bad); e == nil {
		t.Fatal("cursor scope")
	}
	parts := strings.Split(cursor, ".")
	parts[1] = "A" + parts[1][1:]
	if _, e = f.sync.Cursor.Verify(strings.Join(parts, "."), f.scope); e == nil {
		t.Fatal("tampered cursor")
	}
}

type unknownCommitStore struct {
	ports.Store
	once sync.Once
}

func (s *unknownCommitStore) Update(ctx context.Context, fn func(ports.Tx) error) (ports.CommitState, error) {
	state, e := s.Store.Update(ctx, fn)
	if e != nil {
		return state, e
	}
	uncertain := false
	s.once.Do(func() { uncertain = true })
	if uncertain {
		return ports.CommitUnknown, errors.New("synthetic post-commit disconnect")
	}
	return state, nil
}
func TestUncertainCommitRecoversFromActualReceipt(t *testing.T) {
	f := newFixture(t)
	f.sync.Store = &unknownCommitStore{Store: f.db}
	r, e := f.sync.Push(t.Context(), f.scope, f.mutation(t, createCommand(domain.StableID("unknown"), "unknown")))
	if e != nil || r.Result != "committed" {
		t.Fatal(r, e)
	}
}
func TestArgonHasherRealAndBounds(t *testing.T) {
	h := NewArgonHasher(1)
	hash, e := h.Hash(t.Context(), []byte("synthetic real password"))
	if e != nil {
		t.Fatal(e)
	}
	ok, e := h.Verify(t.Context(), hash, []byte("synthetic real password"))
	if e != nil || !ok {
		t.Fatal(e)
	}
	ok, e = h.Verify(t.Context(), hash, []byte("wrong"))
	if e != nil || ok {
		t.Fatal("wrong password")
	}
	if _, e = h.Verify(t.Context(), strings.Replace(hash, "m=65536", "m=4294967295", 1), []byte("x")); e == nil {
		t.Fatal("unbounded PHC memory")
	}
}
