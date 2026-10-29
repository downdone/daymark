package storage

import (
	"context"
	"encoding/hex"
	"encoding/json"
	"strings"

	"daymark.local/server/internal/domain"
)

func job(r row) *domain.Job {
	if r == nil {
		return nil
	}
	return &domain.Job{ProtocolVersion: 1, ID: domain.ID(r.s("id")), OwnerID: r.id("owner_id"), DatasetID: r.id("dataset_id"), DataEpoch: r.id("data_epoch"), Kind: r.s("kind"), State: r.s("state"), OperationKey: r.s("operation_key"), Metadata: []byte(r.s("metadata_json")), Attempt: r.n("attempt"), LeaseOwner: r.text("lease_owner"), LeaseUntil: r.intPtr("lease_until_ms"), ExpiresAt: r.instant("expires_ms"), Reserved: domain.Counter(r.n("bytes_reserved")), SafeError: r.text("safe_error_code"), CreatedMS: r.n("created_ms"), UpdatedMS: r.n("updated_ms")}
}
func (t *transaction) Job(ctx context.Context, id domain.ID) (*domain.Job, error) {
	r, e := t.one(ctx, "SELECT * FROM jobs WHERE id=?", id)
	return job(r), e
}
func (t *transaction) JobByOperation(ctx context.Context, key string) (*domain.Job, error) {
	r, e := t.one(ctx, "SELECT * FROM jobs WHERE operation_key=?", key)
	return job(r), e
}
func jobFields(j domain.Job) map[string]any {
	return map[string]any{"id": j.ID, "owner_id": nullableString(j.OwnerID), "dataset_id": nullableString(j.DatasetID), "kind": j.Kind, "state": j.State, "data_epoch": nullableString(j.DataEpoch), "operation_key": j.OperationKey, "metadata_json": string(j.Metadata), "attempt": j.Attempt, "lease_owner": j.LeaseOwner, "lease_until_ms": j.LeaseUntil, "expires_ms": milliseconds(j.ExpiresAt), "bytes_reserved": j.Reserved, "safe_error_code": j.SafeError, "created_ms": j.CreatedMS, "updated_ms": j.UpdatedMS}
}
func (t *transaction) InsertJob(ctx context.Context, j domain.Job) error {
	return t.upsertEntityInsert(ctx, "jobs", jobFields(j))
}
func (t *transaction) UpdateJob(ctx context.Context, j domain.Job, expected string, attempt int64) error {
	f := jobFields(j)
	names := []string{"state", "metadata_json", "attempt", "lease_owner", "lease_until_ms", "expires_ms", "bytes_reserved", "safe_error_code", "updated_ms"}
	args := []any{}
	sets := []string{}
	for _, n := range names {
		sets = append(sets, n+"=?")
		args = append(args, f[n])
	}
	args = append(args, j.ID, expected, attempt)
	r, e := t.tx.ExecContext(ctx, "UPDATE jobs SET "+strings.Join(sets, ",")+" WHERE id=? AND state=? AND attempt=?", args...)
	if e != nil {
		return e
	}
	n, e := r.RowsAffected()
	if e != nil {
		return e
	}
	if n != 1 {
		return domain.Fail("lost_lease")
	}
	return nil
}
func (t *transaction) RunnableJobs(ctx context.Context, now int64, limit int, kinds ...string) ([]domain.Job, error) {
	query := "SELECT * FROM jobs WHERE state IN ('BUILDING','PUBLISHING') AND (lease_until_ms IS NULL OR lease_until_ms<=?)"
	args := []any{now}
	if len(kinds) > 0 {
		holders := make([]string, len(kinds))
		for i, k := range kinds {
			holders[i] = "?"
			args = append(args, k)
		}
		query += " AND kind IN (" + strings.Join(holders, ",") + ")"
	}
	query += " ORDER BY created_ms,id LIMIT ?"
	args = append(args, limit)
	rs, e := t.rows(ctx, query, args...)
	if e != nil {
		return nil, e
	}
	out := []domain.Job{}
	for _, r := range rs {
		out = append(out, *job(r))
	}
	return out, nil
}
func (t *transaction) Chunks(ctx context.Context, id domain.ID) ([]domain.Chunk, error) {
	rs, e := t.rows(ctx, "SELECT * FROM job_chunks WHERE job_id=? ORDER BY chunk_index", id)
	if e != nil {
		return nil, e
	}
	out := []domain.Chunk{}
	for _, r := range rs {
		h := r.hash("sha256")
		out = append(out, domain.Chunk{Index: int(r.n("chunk_index")), Bytes: domain.Counter(r.n("byte_count")), Hash: hex.EncodeToString(h[:]), ArtifactKey: r.s("artifact_key")})
	}
	return out, nil
}
func (t *transaction) PutChunk(ctx context.Context, id domain.ID, c domain.Chunk) error {
	h, e := hex.DecodeString(c.Hash)
	if e != nil || len(h) != 32 {
		return domain.Invalid("/sha256")
	}
	return t.exec(ctx, "INSERT INTO job_chunks VALUES(?,?,?,?,?)", id, c.Index, c.Bytes, h, c.ArtifactKey)
}
func (t *transaction) PutManifest(ctx context.Context, id domain.ID, m domain.Manifest) error {
	b, e := json.Marshal(m)
	if e != nil {
		return e
	}
	h, e := hex.DecodeString(m.ManifestHash)
	if e != nil {
		return e
	}
	cutoff := domain.Value(m.Cutoff)
	return t.upsert(ctx, "snapshot_manifests", map[string]any{"job_id": id, "cutoff_seq": cutoff, "dto_version": m.DTOVersion, "entity_count": m.EntityCount, "total_bytes": m.TotalBytes, "manifest_json": string(b), "manifest_hash": h}, []string{"job_id"})
}
func (t *transaction) Manifest(ctx context.Context, id domain.ID) (*domain.Manifest, error) {
	r, e := t.one(ctx, "SELECT manifest_json FROM snapshot_manifests WHERE job_id=?", id)
	if e != nil || r == nil {
		return nil, e
	}
	var m domain.Manifest
	if e = json.Unmarshal([]byte(r.s("manifest_json")), &m); e != nil {
		return nil, domain.Wrap("storage_corrupt", e)
	}
	return &m, nil
}
func (t *transaction) Pin(ctx context.Context, dataset, job domain.ID, seq domain.Counter, expiry int64) error {
	return t.upsert(ctx, "retention_pins", map[string]any{"dataset_id": dataset, "job_id": job, "after_seq": seq, "expires_ms": expiry}, []string{"dataset_id", "job_id"})
}
func (t *transaction) ReleasePin(ctx context.Context, job domain.ID) error {
	return t.exec(ctx, "DELETE FROM retention_pins WHERE job_id=?", job)
}
func (t *transaction) ReadLease(ctx context.Context, job, id domain.ID, reader string, now, expiry int64) error {
	return t.exec(ctx, "INSERT INTO job_read_leases VALUES(?,?,?,?,?)", id, job, reader, now, expiry)
}
func (t *transaction) ReleaseReadLease(ctx context.Context, id domain.ID) error {
	return t.exec(ctx, "DELETE FROM job_read_leases WHERE id=?", id)
}
func (t *transaction) HasReadLease(ctx context.Context, id domain.ID, now int64) (bool, error) {
	r, e := t.one(ctx, "SELECT count(*) AS n FROM job_read_leases WHERE job_id=? AND expires_ms>?", id, now)
	return r.n("n") > 0, e
}
func (t *transaction) ReadLeaseCount(ctx context.Context, id domain.ID, now int64) (int, error) {
	r, e := t.one(ctx, "SELECT count(*) AS n FROM job_read_leases WHERE job_id=? AND expires_ms>?", id, now)
	return int(r.n("n")), e
}
func (t *transaction) AllJobs(ctx context.Context, owner *domain.ID) ([]domain.Job, error) {
	query := "SELECT * FROM jobs"
	args := []any{}
	if owner != nil {
		query += " WHERE owner_id=?"
		args = append(args, *owner)
	}
	query += " ORDER BY created_ms,id LIMIT 100001"
	rs, e := t.rows(ctx, query, args...)
	if e != nil {
		return nil, e
	}
	if len(rs) > 100000 {
		return nil, domain.Fail("quota_exceeded")
	}
	out := []domain.Job{}
	for _, r := range rs {
		out = append(out, *job(r))
	}
	return out, nil
}
func (t *transaction) Deletion(ctx context.Context, id domain.ID) (*domain.Deletion, error) {
	r, e := t.one(ctx, "SELECT * FROM account_deletions WHERE operation_id=?", id)
	if e != nil || r == nil {
		return nil, e
	}
	return &domain.Deletion{OperationID: id, UserID: domain.ID(r.s("user_id")), State: r.s("state"), TokenHash: r.hash("token_hash"), AuthEpoch: domain.ID(r.s("auth_epoch")), CredentialVersion: r.n("credential_version"), ConfirmUntilMS: r.n("confirm_until_ms"), ExpiresMS: r.n("expires_ms"), JobID: r.id("job_id"), LedgerReceipt: r.text("ledger_receipt"), RequestedMS: r.n("requested_ms"), UpdatedMS: r.n("updated_ms")}, nil
}
func (t *transaction) PutDeletion(ctx context.Context, d domain.Deletion) error {
	return t.upsert(ctx, "account_deletions", map[string]any{"operation_id": d.OperationID, "user_id": d.UserID, "state": d.State, "token_hash": d.TokenHash[:], "auth_epoch": d.AuthEpoch, "credential_version": d.CredentialVersion, "confirm_until_ms": d.ConfirmUntilMS, "expires_ms": d.ExpiresMS, "job_id": nullableString(d.JobID), "ledger_receipt": d.LedgerReceipt, "requested_ms": d.RequestedMS, "updated_ms": d.UpdatedMS}, []string{"operation_id"})
}
func (t *transaction) CancelIntents(ctx context.Context, id domain.ID, now int64) error {
	return t.exec(ctx, "UPDATE account_deletions SET state='CANCELLED',updated_ms=? WHERE user_id=? AND state='INTENT'", now, id)
}
func (t *transaction) EraseAccount(ctx context.Context, id domain.ID, now int64) error {
	// Foreign keys remain enabled. All mutual task/rule references are deferred.
	ds, e := t.Datasets(ctx, id)
	if e != nil {
		return e
	}
	for _, d := range ds {
		for _, table := range []string{"task_links", "task_tags", "task_schedules", "reminder_configs", "recurrence_successors", "task_aliases", "order_entries", "tasks", "recurrence_rules", "categories", "tags", "ordering_scopes", "structure_versions", "change_batches", "mutation_receipts", "device_progress", "retention_pins"} {
			if e = t.exec(ctx, "DELETE FROM "+table+" WHERE dataset_id=?", d.ID); e != nil {
				return e
			}
		}
	}
	if e = t.exec(ctx, "DELETE FROM refresh_history WHERE session_id IN (SELECT id FROM sessions WHERE user_id=?)", id); e != nil {
		return e
	}
	if e = t.exec(ctx, "DELETE FROM sessions WHERE user_id=?", id); e != nil {
		return e
	}
	if e = t.exec(ctx, "DELETE FROM datasets WHERE owner_id=?", id); e != nil {
		return e
	}
	// Keep only non-revivable user identity; the independent ledger is authoritative.
	return t.exec(ctx, "UPDATE users SET login_normalized=?,password_hash='',status='DELETED',credential_version=credential_version+1,updated_ms=? WHERE id=?", "deleted:"+string(id), now, id)
}
func (t *transaction) PurgeUserJobs(ctx context.Context, owner, except domain.ID) error {
	for _, table := range []string{"job_read_leases", "job_chunks", "snapshot_manifests", "retention_pins", "backup_records"} {
		if e := t.exec(ctx, "DELETE FROM "+table+" WHERE job_id IN (SELECT id FROM jobs WHERE owner_id=? AND id<>?)", owner, except); e != nil {
			return e
		}
	}
	return t.exec(ctx, "DELETE FROM jobs WHERE owner_id=? AND id<>?", owner, except)
}
