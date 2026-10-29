package storage

import (
	"context"
	"daymark.local/server/internal/domain"
)

func (t *transaction) State(ctx context.Context) (domain.State, error) {
	r, e := t.one(ctx, "SELECT * FROM service_state WHERE singleton=1")
	if e != nil {
		return domain.State{}, e
	}
	if r == nil {
		return domain.State{}, domain.Fail("storage_corrupt")
	}
	return domain.State{ServerID: domain.ID(r.s("server_instance_id")), AuthEpoch: domain.ID(r.s("auth_epoch")), SchemaVersion: int(r.n("schema_version")), LedgerID: r.id("ledger_id")}, nil
}
func user(r row) *domain.User {
	if r == nil {
		return nil
	}
	return &domain.User{ID: domain.ID(r.s("id")), Login: r.s("login_normalized"), PasswordHash: r.s("password_hash"), CredentialVersion: r.n("credential_version"), Status: r.s("status"), CreatedMS: r.n("created_ms"), UpdatedMS: r.n("updated_ms")}
}
func (t *transaction) User(ctx context.Context, id domain.ID) (*domain.User, error) {
	r, e := t.one(ctx, "SELECT * FROM users WHERE id=?", id)
	return user(r), e
}
func (t *transaction) UserByLogin(ctx context.Context, login string) (*domain.User, error) {
	r, e := t.one(ctx, "SELECT * FROM users WHERE login_normalized=?", login)
	return user(r), e
}
func (t *transaction) PutUser(ctx context.Context, u domain.User) error {
	return t.upsert(ctx, "users", map[string]any{"id": u.ID, "login_normalized": u.Login, "password_hash": u.PasswordHash, "credential_version": u.CredentialVersion, "status": u.Status, "created_ms": u.CreatedMS, "updated_ms": u.UpdatedMS}, []string{"id"})
}
func session(r row) *domain.Session {
	if r == nil {
		return nil
	}
	return &domain.Session{ID: domain.ID(r.s("id")), UserID: domain.ID(r.s("user_id")), InstallationID: domain.ID(r.s("installation_id")), AuthEpoch: domain.ID(r.s("auth_epoch")), DeviceName: r.s("device_label"), Platform: r.s("platform"), CredentialVersion: r.n("credential_version"), Generation: r.n("generation"), AccessHash: r.hash("access_hash"), RefreshHash: r.hash("refresh_hash"), CreatedMS: r.n("created_ms"), AccessExpiryMS: r.n("access_expires_ms"), RefreshExpiryMS: r.n("refresh_expires_ms"), RevokedMS: r.intPtr("revoked_ms")}
}
func (t *transaction) Session(ctx context.Context, id domain.ID) (*domain.Session, error) {
	r, e := t.one(ctx, "SELECT * FROM sessions WHERE id=?", id)
	return session(r), e
}
func (t *transaction) SessionByHash(ctx context.Context, h [32]byte, refresh bool) (*domain.Session, error) {
	field := "access_hash"
	if refresh {
		field = "refresh_hash"
	}
	r, e := t.one(ctx, "SELECT * FROM sessions WHERE "+field+"=?", h[:])
	return session(r), e
}
func (t *transaction) PutSession(ctx context.Context, s domain.Session) error {
	return t.upsert(ctx, "sessions", map[string]any{"id": s.ID, "user_id": s.UserID, "installation_id": s.InstallationID, "device_label": s.DeviceName, "platform": s.Platform, "auth_epoch": s.AuthEpoch, "credential_version": s.CredentialVersion, "generation": s.Generation, "access_hash": s.AccessHash[:], "refresh_hash": s.RefreshHash[:], "created_ms": s.CreatedMS, "access_expires_ms": s.AccessExpiryMS, "refresh_expires_ms": s.RefreshExpiryMS, "revoked_ms": s.RevokedMS}, []string{"id"})
}
func (t *transaction) RetiredRefresh(ctx context.Context, h [32]byte) (*domain.ID, error) {
	r, e := t.one(ctx, "SELECT session_id FROM refresh_history WHERE token_hash=?", h[:])
	if r == nil {
		return nil, e
	}
	return r.id("session_id"), e
}
func (t *transaction) RetireRefresh(ctx context.Context, h [32]byte, id domain.ID, at, expiry int64) error {
	return t.exec(ctx, "INSERT INTO refresh_history VALUES(?,?,?,?)", h[:], id, at, expiry)
}
func (t *transaction) Sessions(ctx context.Context, id domain.ID) ([]domain.Session, error) {
	rs, e := t.rows(ctx, "SELECT * FROM sessions WHERE user_id=? ORDER BY created_ms,id", id)
	if e != nil {
		return nil, e
	}
	out := []domain.Session{}
	for _, r := range rs {
		out = append(out, *session(r))
	}
	return out, nil
}
func (t *transaction) RevokeSessions(ctx context.Context, id domain.ID, at int64) error {
	return t.exec(ctx, "UPDATE sessions SET revoked_ms=coalesce(revoked_ms,?) WHERE user_id=?", at, id)
}
func dataset(r row) domain.Dataset {
	return domain.Dataset{ID: domain.ID(r.s("id")), OwnerID: domain.ID(r.s("owner_id")), DataEpoch: domain.ID(r.s("data_epoch")), Head: domain.Counter(r.n("head_seq")), RetainedAfter: domain.Counter(r.n("retained_after_seq"))}
}
func (t *transaction) Dataset(ctx context.Context, s domain.Scope) (domain.Dataset, error) {
	r, e := t.one(ctx, "SELECT * FROM datasets WHERE id=? AND owner_id=?", s.DatasetID, s.Principal.UserID)
	if e != nil {
		return domain.Dataset{}, e
	}
	if r == nil {
		return domain.Dataset{}, domain.Fail("not_found")
	}
	d := dataset(r)
	if d.DataEpoch != s.DataEpoch {
		return domain.Dataset{}, domain.Fail("epoch_changed")
	}
	return d, nil
}
func (t *transaction) Datasets(ctx context.Context, id domain.ID) ([]domain.Dataset, error) {
	rs, e := t.rows(ctx, "SELECT * FROM datasets WHERE owner_id=? ORDER BY id", id)
	if e != nil {
		return nil, e
	}
	out := []domain.Dataset{}
	for _, r := range rs {
		out = append(out, dataset(r))
	}
	return out, nil
}
func (t *transaction) InsertDataset(ctx context.Context, d domain.Dataset, at int64) error {
	return t.exec(ctx, "INSERT INTO datasets(id,owner_id,data_epoch,head_seq,retained_after_seq,created_ms) VALUES(?,?,?,?,?,?)", d.ID, d.OwnerID, d.DataEpoch, d.Head, d.RetainedAfter, at)
}
