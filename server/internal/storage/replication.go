package storage

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/json"

	"daymark.local/server/internal/domain"
)

func (t *transaction) Receipt(ctx context.Context, s domain.Scope, id domain.ID) (*domain.StoredReceipt, error) {
	if _, e := t.Dataset(ctx, s); e != nil {
		return nil, e
	}
	r, e := t.one(ctx, "SELECT * FROM mutation_receipts WHERE dataset_id=? AND mutation_id=?", s.DatasetID, id)
	if e != nil || r == nil {
		return nil, e
	}
	if r.s("data_epoch") != string(s.DataEpoch) {
		return nil, domain.Fail("epoch_changed")
	}
	var receipt domain.Receipt
	if e = json.Unmarshal([]byte(r.s("response_json")), &receipt); e != nil {
		return nil, domain.Wrap("storage_corrupt", e)
	}
	return &domain.StoredReceipt{Receipt: receipt, Hash: r.hash("request_hash"), OuterHash: r.hash("outer_hash"), PayloadRef: r.id("payload_ref"), Response: []byte(r.s("response_json"))}, nil
}
func (t *transaction) AppendReceipt(ctx context.Context, s domain.Scope, r domain.StoredReceipt) error {
	var seq any
	if r.Receipt.BatchSeq != nil {
		seq = *r.Receipt.BatchSeq
	}
	return t.exec(ctx, "INSERT INTO mutation_receipts(dataset_id,mutation_id,data_epoch,request_hash,outer_hash,payload_ref,result,batch_seq,response_json,created_ms) VALUES(?,?,?,?,?,?,?,?,?,?)", s.DatasetID, r.Receipt.MutationID, s.DataEpoch, r.Hash[:], r.OuterHash[:], nullableString(r.PayloadRef), r.Receipt.Result, seq, string(r.Response), milliseconds(r.Receipt.CommittedAt))
}
func (t *transaction) AppendBatch(ctx context.Context, s domain.Scope, b domain.Batch) error {
	data, e := json.Marshal(b)
	if e != nil {
		return e
	}
	h := sha256.Sum256(data)
	r, e := t.tx.ExecContext(ctx, "UPDATE datasets SET head_seq=? WHERE id=? AND owner_id=? AND data_epoch=? AND head_seq=?", b.Seq, s.DatasetID, s.Principal.UserID, s.DataEpoch, b.Seq-1)
	if e != nil {
		return e
	}
	n, e := r.RowsAffected()
	if e != nil {
		return e
	}
	if n != 1 {
		return domain.Fail("revision_conflict")
	}
	return t.exec(ctx, "INSERT INTO change_batches VALUES(?,?,?,?,?,?,?,?)", s.DatasetID, b.Seq, s.DataEpoch, b.MutationID, b.InstallationID, milliseconds(b.CommittedAt), string(data), h[:])
}
func (t *transaction) Batches(ctx context.Context, s domain.Scope, after, through domain.Counter, limit int) ([]domain.Batch, error) {
	rs, e := t.rows(ctx, "SELECT seq,payload_json,payload_hash FROM change_batches WHERE dataset_id=? AND data_epoch=? AND seq>? AND seq<=? ORDER BY seq LIMIT ?", s.DatasetID, s.DataEpoch, after, through, limit)
	if e != nil {
		return nil, e
	}
	out := []domain.Batch{}
	for _, r := range rs {
		b := []byte(r.s("payload_json"))
		if sha256.Sum256(b) != r.hash("payload_hash") {
			return nil, domain.Fail("storage_corrupt")
		}
		var v domain.Batch
		if e = json.Unmarshal(b, &v); e != nil {
			return nil, domain.Wrap("storage_corrupt", e)
		}
		if int64(v.Seq) != r.n("seq") {
			return nil, domain.Fail("storage_corrupt")
		}
		out = append(out, v)
	}
	return out, nil
}
func (t *transaction) Progress(ctx context.Context, s domain.Scope, seq domain.Counter, at int64) error {
	d, e := t.Dataset(ctx, s)
	if e != nil {
		return e
	}
	if seq < 0 || seq > d.Head {
		return domain.Invalid("/applied_seq")
	}
	return t.exec(ctx, "INSERT INTO device_progress VALUES(?,?,?,?,?) ON CONFLICT(dataset_id,installation_id) DO UPDATE SET applied_seq=CASE WHEN device_progress.data_epoch=excluded.data_epoch THEN max(device_progress.applied_seq,excluded.applied_seq) ELSE excluded.applied_seq END,data_epoch=excluded.data_epoch,reported_ms=excluded.reported_ms", s.DatasetID, s.Principal.InstallationID, s.DataEpoch, seq, at)
}
func (t *transaction) Compact(ctx context.Context, s domain.Scope, candidate domain.Counter, now int64) (domain.Counter, error) {
	d, e := t.Dataset(ctx, s)
	if e != nil {
		return 0, e
	}
	if candidate > d.Head {
		candidate = d.Head
	}
	var pin sql.NullInt64
	if e = t.tx.QueryRowContext(ctx, "SELECT min(after_seq) FROM retention_pins WHERE dataset_id=? AND expires_ms>?", s.DatasetID, now).Scan(&pin); e != nil {
		return 0, e
	}
	if pin.Valid && domain.Counter(pin.Int64) < candidate {
		candidate = domain.Counter(pin.Int64)
	}
	if candidate <= d.RetainedAfter {
		return d.RetainedAfter, nil
	}
	if e = t.exec(ctx, "DELETE FROM change_batches WHERE dataset_id=? AND seq<=?", s.DatasetID, candidate); e != nil {
		return 0, e
	}
	return candidate, t.exec(ctx, "UPDATE datasets SET retained_after_seq=? WHERE id=?", candidate, s.DatasetID)
}
