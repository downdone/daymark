package business_test

import (
	"context"
	"crypto/sha256"
	"fmt"
	"testing"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/business"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/storage"
	"github.com/pquerna/otp/totp"
)

type clock struct{ at time.Time }

func (c *clock) Now() time.Time { return c.at }

type hasher struct{}

func (hasher) Hash(_ context.Context, b []byte) (string, error) {
	return fmt.Sprintf("%x", sha256.Sum256(b)), nil
}
func (h hasher) Verify(ctx context.Context, s string, b []byte) (bool, error) {
	hash, _ := h.Hash(ctx, b)
	return s == hash, nil
}

type fixture struct {
	ctx   context.Context
	db    *storage.DB
	admin *business.AdminService
	clock *clock
	owner business.Enrollment
	token string
}

func setup(t *testing.T) *fixture {
	t.Helper()
	ctx := context.Background()
	db, e := storage.Open(ctx, t.TempDir(), storage.DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { db.Close() })
	c := &clock{time.Date(2026, 11, 12, 12, 0, 0, 0, time.UTC)}
	s, e := business.NewAdmin(db, hasher{}, c, make([]byte, 32))
	if e != nil {
		t.Fatal(e)
	}
	owner, e := s.Provision(ctx, "test-owner", []byte("synthetic-password"), []string{"owner"})
	if e != nil {
		t.Fatal(e)
	}
	code, _ := totp.GenerateCode(owner.Secret, c.Now())
	request, _ := domain.NewID()
	token, e := s.Login(ctx, "test-owner", []byte("synthetic-password"), code, string(request))
	if e != nil {
		t.Fatal(e)
	}
	return &fixture{ctx, db, s, c, owner, token}
}
func id(t *testing.T) domain.ID {
	t.Helper()
	v, e := domain.NewID()
	if e != nil {
		t.Fatal(e)
	}
	return v
}
func requireCode(t *testing.T, e error, code string) {
	t.Helper()
	if domain.Code(e) != code {
		t.Fatalf("want %s, got %v", code, e)
	}
}
func TestMFARecoveryReplayAndEpoch(t *testing.T) {
	f := setup(t)
	code, _ := totp.GenerateCode(f.owner.Secret, f.clock.Now())
	_, e := f.admin.Login(f.ctx, "test-owner", []byte("synthetic-password"), code, string(id(t)))
	requireCode(t, e, "admin_mfa_replayed")
	_, e = f.admin.Login(f.ctx, "test-owner", []byte("wrong-password"), f.owner.RecoveryCodes[0], string(id(t)))
	requireCode(t, e, "invalid_credentials")
	token, e := f.admin.Login(f.ctx, "test-owner", []byte("synthetic-password"), f.owner.RecoveryCodes[0], string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	_, e = f.admin.Login(f.ctx, "test-owner", []byte("synthetic-password"), f.owner.RecoveryCodes[0], string(id(t)))
	requireCode(t, e, "admin_mfa_invalid")
	if _, e = f.admin.Me(f.ctx, token, "owner"); e != nil {
		t.Fatal(e)
	}
	f.db.Update(f.ctx, func(tx ports.Tx) error {
		auth := id(t)
		return tx.RestoreEpochs(f.ctx, auth, map[domain.ID]domain.ID{}, f.clock.Now().UnixMilli())
	})
	_, e = f.admin.Me(f.ctx, token, "owner")
	requireCode(t, e, "admin_session_expired")
}

func TestMaintenanceRecoveryRotatesAllCredentialsAndKeepsRoles(t *testing.T) {
	f := setup(t)
	_, e := f.admin.Recover(f.ctx, "not-existing", []byte("synthetic-new-password"))
	requireCode(t, e, "not_found")
	recovered, e := f.admin.Recover(f.ctx, "test-owner", []byte("synthetic-new-password"))
	if e != nil || recovered.ID != f.owner.ID || recovered.Secret == f.owner.Secret {
		t.Fatal(recovered.ID, e)
	}
	_, e = f.admin.Me(f.ctx, f.token, "owner")
	requireCode(t, e, "admin_session_expired")
	_, e = f.admin.Login(f.ctx, "test-owner", []byte("synthetic-password"), f.owner.RecoveryCodes[1], string(id(t)))
	requireCode(t, e, "invalid_credentials")
	_, e = f.admin.Login(f.ctx, "test-owner", []byte("synthetic-new-password"), f.owner.RecoveryCodes[1], string(id(t)))
	requireCode(t, e, "admin_mfa_invalid")
	token, e := f.admin.Login(f.ctx, "test-owner", []byte("synthetic-new-password"), recovered.RecoveryCodes[0], string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	if _, e = f.admin.Me(f.ctx, token, "owner"); e != nil {
		t.Fatal(e)
	}
	if _, e = f.admin.Provision(f.ctx, "recover-support", []byte("synthetic-password"), []string{"support"}); e != nil {
		t.Fatal(e)
	}
	support, e := f.admin.Recover(f.ctx, "recover-support", []byte("synthetic-new-password"))
	if e != nil {
		t.Fatal(e)
	}
	token, e = f.admin.Login(f.ctx, "recover-support", []byte("synthetic-new-password"), support.RecoveryCodes[0], string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	_, e = f.admin.Me(f.ctx, token, "owner")
	requireCode(t, e, "admin_forbidden")
	if _, e = f.admin.Me(f.ctx, token, "users"); e != nil {
		t.Fatal(e)
	}
}

func TestRevokedDraftRetentionStartsAtRevocation(t *testing.T) {
	f := setup(t)
	in := business.NoticeCommand{OperationID: id(t), Title: "synthetic draft", Action: "none", Platform: "all"}
	created, e := f.admin.NoticeCommand(f.ctx, f.token, "", in, "create", string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	_, e = f.admin.NoticeCommand(f.ctx, f.token, created.ID, business.NoticeCommand{OperationID: id(t), Version: created.Version}, "revoke", string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	for pass := 0; pass < 2; pass++ {
		_, e = f.db.UpdateBusiness(f.ctx, func(tx ports.BusinessTx) error { return tx.MaintainBusiness(f.ctx, f.clock.Now().UnixMilli()) })
		if e != nil {
			t.Fatal(e)
		}
		e = f.db.ViewBusiness(f.ctx, func(tx ports.BusinessTx) error {
			n, e := tx.Notice(f.ctx, created.ID)
			if (n == nil) != (pass == 1) {
				t.Fatal("wrong revoked draft retention", pass)
			}
			return e
		})
		if e != nil {
			t.Fatal(e)
		}
		f.clock.at = f.clock.at.AddDate(0, 0, 91)
	}
}

func TestBusinessRetentionIsBoundedAndPreservesCurrentFacts(t *testing.T) {
	f := setup(t)
	old := f.clock.Now().AddDate(0, 0, -181).UnixMilli()
	_, e := f.db.UpdateBusiness(f.ctx, func(tx ports.BusinessTx) error {
		for i := 0; i < 1005; i++ {
			if e := tx.AppendAudit(f.ctx, domain.Audit{Actor: f.owner.ID, Action: "synthetic.expired", AtMS: old}); e != nil {
				return e
			}
		}
		return nil
	})
	if e != nil {
		t.Fatal(e)
	}
	for pass, want := range []int{5, 0} {
		_, e = f.db.UpdateBusiness(f.ctx, func(tx ports.BusinessTx) error { return tx.MaintainBusiness(f.ctx, f.clock.Now().UnixMilli()) })
		if e != nil {
			t.Fatal(e)
		}
		remaining := 0
		e = f.db.ViewBusiness(f.ctx, func(tx ports.BusinessTx) error {
			rows, e := tx.Audits(f.ctx, 0, 1100)
			for _, r := range rows {
				if r.Action == "synthetic.expired" {
					remaining++
				}
			}
			return e
		})
		if e != nil || remaining != want {
			t.Fatal(pass, remaining, want, e)
		}
	}
	if _, e = f.admin.Me(f.ctx, f.token, "owner"); e != nil {
		t.Fatal("active session removed", e)
	}
}
func TestUserCASIdempotencyAndRevocation(t *testing.T) {
	f := setup(t)
	auth, e := application.NewAuth(f.ctx, f.db, f.clock, hasher{})
	if e != nil {
		t.Fatal(e)
	}
	user, e := auth.CreateUser(f.ctx, "synthetic-user", []byte("synthetic-password"))
	if e != nil {
		t.Fatal(e)
	}
	input := business.UserCommand{OperationID: id(t), Version: 1, Status: "DISABLED", Reason: "security"}
	receipt, e := f.admin.UserCommand(f.ctx, f.token, user, input, false, string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	if receipt.Version != 2 {
		t.Fatal(receipt)
	}
	again, e := f.admin.UserCommand(f.ctx, f.token, user, input, false, string(id(t)))
	if e != nil || again != receipt {
		t.Fatal("idempotent receipt", e)
	}
	input.OperationID = id(t)
	_, e = f.admin.UserCommand(f.ctx, f.token, user, input, false, string(id(t)))
	requireCode(t, e, "admin_conflict")
	input.Version = 2
	input.Status = "ACTIVE"
	if _, e = f.admin.UserCommand(f.ctx, f.token, user, input, false, string(id(t))); e != nil {
		t.Fatal(e)
	}
	if e = f.db.View(f.ctx, func(tx ports.Tx) error {
		u, e := tx.User(f.ctx, user)
		if e != nil {
			return e
		}
		if u.Status != "ACTIVE" || u.CredentialVersion != 3 {
			t.Fatal(u.Status, u.CredentialVersion)
		}
		return nil
	}); e != nil {
		t.Fatal(e)
	}
}
func TestLeastPrivilegeFreshMFAAndLastOwner(t *testing.T) {
	f := setup(t)
	_, e := f.admin.AdminCommand(f.ctx, f.token, f.owner.ID, business.AdminCommand{OperationID: id(t), Version: 1, Status: "DISABLED", Roles: []string{"owner"}}, string(id(t)))
	requireCode(t, e, "admin_last_owner")
	support, e := f.admin.Provision(f.ctx, "test-support", []byte("synthetic-password"), []string{"support"})
	if e != nil {
		t.Fatal(e)
	}
	code, _ := totp.GenerateCode(support.Secret, f.clock.Now())
	token, e := f.admin.Login(f.ctx, "test-support", []byte("synthetic-password"), code, string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	_, e = f.admin.AdminCommand(f.ctx, token, support.ID, business.AdminCommand{OperationID: id(t), Version: 1, Status: "ACTIVE", Roles: []string{"owner"}}, string(id(t)))
	requireCode(t, e, "admin_forbidden")
	f.clock.at = f.clock.at.Add(16 * time.Minute)
	_, e = f.admin.AdminCommand(f.ctx, f.token, support.ID, business.AdminCommand{OperationID: id(t), Version: 1, Status: "DISABLED", Roles: []string{"support"}}, string(id(t)))
	requireCode(t, e, "admin_reauthentication_required")
}
func TestNoticeLifecycleAndProtectedDraft(t *testing.T) {
	f := setup(t)
	in := business.NoticeCommand{OperationID: id(t), Title: "Synthetic maintenance", Body: "Plain text <script> is never executable", Action: "none", Platform: "all", ExpireMS: f.clock.Now().Add(24 * time.Hour).UnixMilli()}
	created, e := f.admin.NoticeCommand(f.ctx, f.token, "", in, "create", string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	again, e := f.admin.NoticeCommand(f.ctx, f.token, "", in, "create", string(id(t)))
	if e != nil || again != created {
		t.Fatal(e)
	}
	publish := business.NoticeCommand{OperationID: id(t), Version: created.Version}
	published, e := f.admin.NoticeCommand(f.ctx, f.token, created.ID, publish, "publish", string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	in.OperationID = id(t)
	in.Version = published.Version
	_, e = f.admin.NoticeCommand(f.ctx, f.token, created.ID, in, "edit", string(id(t)))
	requireCode(t, e, "admin_conflict")
	revoked, e := f.admin.NoticeCommand(f.ctx, f.token, created.ID, business.NoticeCommand{OperationID: id(t), Version: published.Version}, "revoke", string(id(t)))
	if e != nil {
		t.Fatal(e)
	}
	_, e = f.admin.NoticeCommand(f.ctx, f.token, created.ID, business.NoticeCommand{OperationID: id(t), Version: revoked.Version}, "publish", string(id(t)))
	requireCode(t, e, "admin_conflict")
}

type rollbackStore struct{ ports.BusinessStore }

func (s rollbackStore) UpdateBusiness(ctx context.Context, fn func(ports.BusinessTx) error) (ports.CommitState, error) {
	return s.BusinessStore.UpdateBusiness(ctx, func(tx ports.BusinessTx) error {
		if e := fn(tx); e != nil {
			return e
		}
		return domain.Fail("admin_conflict")
	})
}
func TestAuditAndBusinessEffectsRollbackTogether(t *testing.T) {
	f := setup(t)
	f.admin.Store = rollbackStore{f.db}
	in := business.NoticeCommand{OperationID: id(t), Title: "rollback", Action: "none", Platform: "all"}
	_, e := f.admin.NoticeCommand(f.ctx, f.token, "", in, "create", string(id(t)))
	requireCode(t, e, "admin_conflict")
	e = f.db.ViewBusiness(f.ctx, func(tx ports.BusinessTx) error {
		rows, e := tx.Notices(f.ctx, "", 50)
		if e != nil {
			return e
		}
		if len(rows) != 0 {
			t.Fatal("notice survived rollback")
		}
		a, e := tx.Audits(f.ctx, 0, 50)
		if e != nil {
			return e
		}
		for _, item := range a {
			if item.Action == "notice.create" {
				t.Fatal("audit survived rollback")
			}
		}
		return nil
	})
	if e != nil {
		t.Fatal(e)
	}
}
