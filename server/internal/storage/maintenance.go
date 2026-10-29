package storage

import (
	"context"
	"database/sql"
	"os"
	"path/filepath"

	"daymark.local/server/internal/domain"
)

func (t *transaction) BindLedger(ctx context.Context, id domain.ID) error {
	r, e := t.tx.ExecContext(ctx, "UPDATE service_state SET ledger_id=? WHERE singleton=1 AND (ledger_id IS NULL OR ledger_id=?)", id, id)
	if e != nil {
		return e
	}
	n, e := r.RowsAffected()
	if e != nil {
		return e
	}
	if n != 1 {
		return domain.Fail("storage_corrupt")
	}
	return nil
}
func (t *transaction) AllDatasets(ctx context.Context) ([]domain.Dataset, error) {
	rows, e := t.rows(ctx, "SELECT * FROM datasets ORDER BY id")
	if e != nil {
		return nil, e
	}
	out := []domain.Dataset{}
	for _, r := range rows {
		out = append(out, dataset(r))
	}
	return out, nil
}
func (t *transaction) RestoreEpochs(ctx context.Context, auth domain.ID, epochs map[domain.ID]domain.ID, at int64) error {
	if e := t.exec(ctx, "UPDATE service_state SET auth_epoch=? WHERE singleton=1", auth); e != nil {
		return e
	}
	if e := t.exec(ctx, "UPDATE sessions SET revoked_ms=coalesce(revoked_ms,?)", at); e != nil {
		return e
	}
	if e := t.exec(ctx, "UPDATE account_deletions SET state='EXPIRED',updated_ms=? WHERE state='INTENT'", at); e != nil {
		return e
	}
	for id, epoch := range epochs {
		if e := t.exec(ctx, "UPDATE datasets SET data_epoch=?,retained_after_seq=head_seq WHERE id=?", epoch, id); e != nil {
			return e
		}
		if e := t.exec(ctx, "DELETE FROM change_batches WHERE dataset_id=?", id); e != nil {
			return e
		}
	}
	for _, table := range []string{"retention_pins", "job_read_leases", "device_progress"} {
		if e := t.exec(ctx, "DELETE FROM "+table); e != nil {
			return e
		}
	}
	return t.exec(ctx, "UPDATE jobs SET state=CASE WHEN kind='DELETE_ACCOUNT' AND state<>'COMMITTED' THEN 'BUILDING' ELSE 'EXPIRED' END,lease_owner=NULL,lease_until_ms=NULL,attempt=attempt+1,updated_ms=?", at)
}
func (t *transaction) BackupRecord(ctx context.Context, id domain.ID) (*domain.BackupRecord, error) {
	r, e := t.one(ctx, "SELECT * FROM backup_records WHERE job_id=?", id)
	if e != nil || r == nil {
		return nil, e
	}
	return &domain.BackupRecord{JobID: id, SnapshotMS: r.n("snapshot_ms"), Hash: r.hash("sha256"), ArtifactKey: r.s("local_artifact_key"), TargetID: r.text("remote_target_id"), VerifiedMS: r.intPtr("remote_verified_ms"), SchemaVersion: int(r.n("schema_version"))}, nil
}
func (t *transaction) PutBackupRecord(ctx context.Context, r domain.BackupRecord) error {
	return t.upsert(ctx, "backup_records", map[string]any{"job_id": r.JobID, "snapshot_ms": r.SnapshotMS, "sha256": r.Hash[:], "local_artifact_key": r.ArtifactKey, "remote_target_id": r.TargetID, "remote_verified_ms": r.VerifiedMS, "schema_version": r.SchemaVersion}, []string{"job_id"})
}
func (d *DB) InspectBackup(ctx context.Context, path string) (domain.BackupInfo, error) {
	u := databaseURL(path)
	u.RawQuery = "mode=ro&_pragma=foreign_keys(1)&_pragma=query_only(1)"
	db, e := sql.Open("sqlite", u.String())
	if e != nil {
		return domain.BackupInfo{}, e
	}
	defer db.Close()
	if e = d.check(ctx, db); e != nil {
		return domain.BackupInfo{}, e
	}
	tx, e := db.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if e != nil {
		return domain.BackupInfo{}, e
	}
	defer tx.Rollback()
	repo := &transaction{tx: tx}
	state, e := repo.State(ctx)
	if e != nil {
		return domain.BackupInfo{}, e
	}
	datasets, e := repo.AllDatasets(ctx)
	if e != nil {
		return domain.BackupInfo{}, e
	}
	var count int64
	if e = tx.QueryRowContext(ctx, "SELECT count(*) FROM tasks").Scan(&count); e != nil {
		return domain.BackupInfo{}, e
	}
	return domain.BackupInfo{State: state, Datasets: datasets, TaskCount: count}, nil
}

type DirectoryLease struct{ file *os.File }

// Expiration and log compaction share the writer/pin serialization boundary.
// Receipts and operation identities are deliberately retained for replay.
func (t *transaction) Maintain(ctx context.Context, now, cutoff int64) error {
	for _, q := range []string{"DELETE FROM refresh_history WHERE expires_ms<=?", "DELETE FROM job_read_leases WHERE expires_ms<=?", "DELETE FROM retention_pins WHERE expires_ms<=?"} {
		if e := t.exec(ctx, q, now); e != nil {
			return e
		}
	}
	if e := t.exec(ctx, "DELETE FROM sessions WHERE refresh_expires_ms<=? AND NOT EXISTS(SELECT 1 FROM refresh_history h WHERE h.session_id=sessions.id)", now); e != nil {
		return e
	}
	if e := t.exec(ctx, "DELETE FROM device_progress WHERE reported_ms<?", cutoff); e != nil {
		return e
	}
	datasets, e := t.AllDatasets(ctx)
	if e != nil {
		return e
	}
	for _, d := range datasets {
		var candidate int64
		var recent sql.NullInt64
		if e = t.tx.QueryRowContext(ctx, "SELECT coalesce(max(CASE WHEN committed_ms<? THEN seq END),0),min(CASE WHEN committed_ms>=? THEN seq END) FROM change_batches WHERE dataset_id=? AND data_epoch=?", cutoff, cutoff, d.ID, d.DataEpoch).Scan(&candidate, &recent); e != nil {
			return e
		}
		if recent.Valid && candidate >= recent.Int64 {
			candidate = recent.Int64 - 1
		}
		scope := domain.Scope{Principal: domain.Principal{UserID: d.OwnerID}, DatasetID: d.ID, DataEpoch: d.DataEpoch}
		if _, e = t.Compact(ctx, scope, domain.Counter(candidate), now); e != nil {
			return e
		}
	}
	return nil
}

func InspectFile(ctx context.Context, path string) (domain.BackupInfo, error) {
	return new(DB).InspectBackup(ctx, path)
}
func (t *transaction) ReplayDeletion(ctx context.Context, event domain.LedgerEvent) error {
	u, e := t.User(ctx, event.UserID)
	if e != nil {
		return e
	}
	if u == nil {
		if e = t.PutUser(ctx, domain.User{ID: event.UserID, Login: "deleted:" + string(event.UserID), PasswordHash: "", CredentialVersion: 1, Status: "DELETED", CreatedMS: event.RequestedMS, UpdatedMS: event.RequestedMS}); e != nil {
			return e
		}
	}
	if e = t.exec(ctx, "DELETE FROM account_deletions WHERE user_id=?", event.UserID); e != nil {
		return e
	}
	if e = t.PurgeUserJobs(ctx, event.UserID, ""); e != nil {
		return e
	}
	return t.EraseAccount(ctx, event.UserID, event.RequestedMS)
}

func AcquireDirectory(dir string) (*DirectoryLease, error) {
	abs, e := filepath.Abs(dir)
	if e != nil {
		return nil, e
	}
	if e = os.MkdirAll(abs, 0700); e != nil {
		return nil, e
	}
	f, e := os.OpenFile(filepath.Join(abs, "instance.lock"), os.O_CREATE|os.O_RDWR, 0600)
	if e != nil {
		return nil, e
	}
	if e = lockFile(f); e != nil {
		f.Close()
		return nil, domain.Wrap("already_running", e)
	}
	return &DirectoryLease{file: f}, nil
}
func (l *DirectoryLease) Close() error {
	if l.file == nil {
		return nil
	}
	e := unlockFile(l.file)
	closeErr := l.file.Close()
	l.file = nil
	if e != nil {
		return e
	}
	return closeErr
}
