package storage

import (
	"context"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"encoding/json"
)

func (d *DB) ViewBusiness(ctx context.Context, fn func(ports.BusinessTx) error) error {
	return d.View(ctx, func(tx ports.Tx) error { return fn(tx.(*transaction)) })
}
func (d *DB) UpdateBusiness(ctx context.Context, fn func(ports.BusinessTx) error) (ports.CommitState, error) {
	return d.Update(ctx, func(tx ports.Tx) error { return fn(tx.(*transaction)) })
}
func decodeJSON(s string, out any) error {
	if json.Unmarshal([]byte(s), out) != nil {
		return domain.Fail("storage_corrupt")
	}
	return nil
}
func encoded(v any) string { b, _ := json.Marshal(v); return string(b) }
func adminRow(r row) (*domain.Admin, error) {
	if r == nil {
		return nil, nil
	}
	a := &domain.Admin{ID: domain.ID(r.s("id")), Login: r.s("login"), PasswordHash: r.s("password_hash"), Status: r.s("status"), Version: r.n("version"), LastStep: r.n("last_step"), CreatedMS: r.n("created_ms")}
	secret, ok := r["secret_cipher"].([]byte)
	if !ok || len(secret) < 28 {
		return nil, domain.Fail("storage_corrupt")
	}
	a.SecretCipher = append([]byte(nil), secret...)
	if e := decodeJSON(r.s("roles"), &a.Roles); e != nil {
		return nil, e
	}
	if e := decodeJSON(r.s("recovery_hashes"), &a.RecoveryHashes); e != nil {
		return nil, e
	}
	return a, nil
}

func (t *transaction) MaintainBusiness(ctx context.Context, now int64) error {
	const day = int64(24 * 60 * 60 * 1000)
	for _, statement := range []struct {
		query  string
		cutoff int64
	}{
		{"DELETE FROM admin_sessions WHERE token_hash IN (SELECT token_hash FROM admin_sessions WHERE expires_ms<=? ORDER BY expires_ms LIMIT 1000)", now},
		{"DELETE FROM admin_audit WHERE seq IN (SELECT seq FROM admin_audit WHERE at_ms<? ORDER BY at_ms LIMIT 1000)", now - 180*day},
		{"DELETE FROM business_operations WHERE (actor,operation_id) IN (SELECT actor,operation_id FROM business_operations WHERE at_ms<? ORDER BY at_ms LIMIT 1000)", now - 90*day},
		{"DELETE FROM business_notices WHERE id IN (SELECT id FROM business_notices WHERE state<>'draft' AND CASE WHEN state='revoked' THEN updated_ms ELSE expire_ms END<? ORDER BY expire_ms LIMIT 1000)", now - 90*day},
	} {
		if e := t.exec(ctx, statement.query, statement.cutoff); e != nil {
			return e
		}
	}
	return nil
}
func (t *transaction) Admin(ctx context.Context, id domain.ID) (*domain.Admin, error) {
	r, e := t.one(ctx, "SELECT * FROM admin_identities WHERE id=?", id)
	if e != nil {
		return nil, e
	}
	return adminRow(r)
}
func (t *transaction) AdminByLogin(ctx context.Context, login string) (*domain.Admin, error) {
	r, e := t.one(ctx, "SELECT * FROM admin_identities WHERE login=?", login)
	if e != nil {
		return nil, e
	}
	return adminRow(r)
}
func (t *transaction) Admins(ctx context.Context) ([]domain.Admin, error) {
	rs, e := t.rows(ctx, "SELECT * FROM admin_identities ORDER BY id LIMIT 101")
	if e != nil {
		return nil, e
	}
	out := []domain.Admin{}
	for _, r := range rs {
		a, e := adminRow(r)
		if e != nil {
			return nil, e
		}
		out = append(out, *a)
	}
	return out, nil
}
func (t *transaction) PutAdmin(ctx context.Context, a domain.Admin) error {
	return t.upsert(ctx, "admin_identities", map[string]any{"id": a.ID, "login": a.Login, "password_hash": a.PasswordHash, "secret_cipher": a.SecretCipher, "recovery_hashes": encoded(a.RecoveryHashes), "roles": encoded(a.Roles), "status": a.Status, "version": a.Version, "last_step": a.LastStep, "created_ms": a.CreatedMS}, []string{"id"})
}
func (t *transaction) AdminSession(ctx context.Context, h [32]byte) (*domain.AdminSession, error) {
	r, e := t.one(ctx, "SELECT * FROM admin_sessions WHERE token_hash=?", h[:])
	if e != nil || r == nil {
		return nil, e
	}
	return &domain.AdminSession{Hash: h, AdminID: domain.ID(r.s("admin_id")), Version: r.n("version"), AuthEpoch: domain.ID(r.s("auth_epoch")), CreatedMS: r.n("created_ms"), ExpiresMS: r.n("expires_ms")}, nil
}
func (t *transaction) PutAdminSession(ctx context.Context, s domain.AdminSession) error {
	return t.exec(ctx, "INSERT INTO admin_sessions VALUES(?,?,?,?,?,?)", s.Hash[:], s.AdminID, s.Version, s.AuthEpoch, s.CreatedMS, s.ExpiresMS)
}
func (t *transaction) RemoveAdminSession(ctx context.Context, h [32]byte) error {
	return t.exec(ctx, "DELETE FROM admin_sessions WHERE token_hash=?", h[:])
}
func (t *transaction) Users(ctx context.Context, q domain.UserQuery, now int64) ([]domain.UserSummary, error) {
	rs, e := t.rows(ctx, `SELECT u.id,u.login_normalized,u.status,u.credential_version,u.created_ms,(SELECT count(*) FROM sessions s WHERE s.user_id=u.id AND s.revoked_ms IS NULL AND s.refresh_expires_ms>?) AS sessions FROM users u WHERE u.id>? AND (?='' OR u.status=?) AND (?='' OR instr(u.login_normalized,?)>0) ORDER BY u.id LIMIT ?`, now, q.After, q.Status, q.Status, q.Search, q.Search, q.Limit)
	if e != nil {
		return nil, e
	}
	out := []domain.UserSummary{}
	for _, r := range rs {
		out = append(out, domain.UserSummary{ID: domain.ID(r.s("id")), Login: r.s("login_normalized"), Status: r.s("status"), Version: r.n("credential_version"), CreatedMS: r.n("created_ms"), Sessions: int(r.n("sessions"))})
	}
	return out, nil
}
func (t *transaction) Overview(ctx context.Context, now int64) (domain.BusinessOverview, error) {
	r, e := t.one(ctx, `SELECT (SELECT count(*) FROM users WHERE status<>'DELETED') AS users,(SELECT count(*) FROM users WHERE status='ACTIVE') AS active,(SELECT count(*) FROM sessions WHERE revoked_ms IS NULL AND refresh_expires_ms>?) AS sessions,(SELECT count(*) FROM business_notices WHERE state IN ('published','scheduled') AND publish_ms<=? AND expire_ms>?) AS notices`, now, now, now)
	if e != nil {
		return domain.BusinessOverview{}, e
	}
	return domain.BusinessOverview{Users: r.n("users"), ActiveUsers: r.n("active"), Sessions: r.n("sessions"), Notices: r.n("notices")}, nil
}
func noticeRow(r row) (*domain.Notice, error) {
	if r == nil {
		return nil, nil
	}
	n := &domain.Notice{ID: domain.ID(r.s("id")), Title: r.s("title"), Body: r.s("body"), State: r.s("state"), Action: r.s("action"), Platform: r.s("platform"), MinVersion: r.s("min_version"), MaxVersion: r.s("max_version"), Version: r.n("version"), PublishMS: r.n("publish_ms"), ExpireMS: r.n("expire_ms"), UpdatedMS: r.n("updated_ms"), CreatedBy: domain.ID(r.s("created_by"))}
	if e := decodeJSON(r.s("targets"), &n.Targets); e != nil {
		return nil, e
	}
	return n, nil
}
func (t *transaction) Notice(ctx context.Context, id domain.ID) (*domain.Notice, error) {
	r, e := t.one(ctx, "SELECT * FROM business_notices WHERE id=?", id)
	if e != nil {
		return nil, e
	}
	return noticeRow(r)
}
func (t *transaction) Notices(ctx context.Context, after domain.ID, limit int) ([]domain.Notice, error) {
	rs, e := t.rows(ctx, "SELECT * FROM business_notices WHERE id>? ORDER BY id LIMIT ?", after, limit)
	if e != nil {
		return nil, e
	}
	out := []domain.Notice{}
	for _, r := range rs {
		n, e := noticeRow(r)
		if e != nil {
			return nil, e
		}
		out = append(out, *n)
	}
	return out, nil
}
func (t *transaction) PutNotice(ctx context.Context, n domain.Notice) error {
	return t.upsert(ctx, "business_notices", map[string]any{"id": n.ID, "title": n.Title, "body": n.Body, "state": n.State, "action": n.Action, "platform": n.Platform, "min_version": n.MinVersion, "max_version": n.MaxVersion, "targets": encoded(n.Targets), "version": n.Version, "publish_ms": n.PublishMS, "expire_ms": n.ExpireMS, "updated_ms": n.UpdatedMS, "created_by": n.CreatedBy}, []string{"id"})
}
func (t *transaction) AppendAudit(ctx context.Context, a domain.Audit) error {
	return t.exec(ctx, "INSERT INTO admin_audit(actor,action,target,reason,request_id,at_ms) VALUES(?,?,?,?,?,?)", a.Actor, a.Action, a.Target, a.Reason, a.RequestID, a.AtMS)
}
func (t *transaction) Audits(ctx context.Context, before int64, limit int) ([]domain.Audit, error) {
	rs, e := t.rows(ctx, "SELECT * FROM admin_audit WHERE (?=0 OR seq<?) ORDER BY seq DESC LIMIT ?", before, before, limit)
	if e != nil {
		return nil, e
	}
	out := []domain.Audit{}
	for _, r := range rs {
		out = append(out, domain.Audit{Seq: r.n("seq"), Actor: domain.ID(r.s("actor")), Action: r.s("action"), Target: domain.ID(r.s("target")), Reason: r.s("reason"), RequestID: r.s("request_id"), AtMS: r.n("at_ms")})
	}
	return out, nil
}
func (t *transaction) BusinessOperation(ctx context.Context, actor, id domain.ID) (*domain.BusinessOperation, error) {
	r, e := t.one(ctx, "SELECT * FROM business_operations WHERE actor=? AND operation_id=?", actor, id)
	if e != nil || r == nil {
		return nil, e
	}
	return &domain.BusinessOperation{Actor: actor, ID: id, Hash: r.s("request_hash"), Result: r.s("result"), AtMS: r.n("at_ms")}, nil
}
func (t *transaction) PutBusinessOperation(ctx context.Context, o domain.BusinessOperation) error {
	return t.exec(ctx, "INSERT INTO business_operations VALUES(?,?,?,?,?)", o.Actor, o.ID, o.Hash, o.Result, o.AtMS)
}
func (t *transaction) CleanupQueue(ctx context.Context, limit int) ([]domain.Cleanup, error) {
	rs, e := t.rows(ctx, "SELECT * FROM business_cleanup ORDER BY at_ms,user_id LIMIT ?", limit)
	if e != nil {
		return nil, e
	}
	out := []domain.Cleanup{}
	for _, r := range rs {
		out = append(out, domain.Cleanup{UserID: domain.ID(r.s("user_id")), AtMS: r.n("at_ms")})
	}
	return out, nil
}
func (t *transaction) AckCleanup(ctx context.Context, c domain.Cleanup) error {
	return t.exec(ctx, "DELETE FROM business_cleanup WHERE user_id=? AND at_ms=?", c.UserID, c.AtMS)
}
