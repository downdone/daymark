package application

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"io"
	"slices"
	"sort"
	"sync"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
)

type CreateJobInput struct {
	ProtocolVersion     int            `json:"protocol_version"`
	DataEpoch           domain.ID      `json:"data_epoch"`
	OperationID         domain.ID      `json:"operation_id"`
	MutationID          domain.ID      `json:"mutation_id,omitempty"`
	InstallationID      domain.ID      `json:"installation_id,omitempty"`
	ClientSeq           domain.Counter `json:"client_seq,omitempty"`
	ExpectedBytes       domain.Counter `json:"expected_bytes,omitempty"`
	BaseHead            domain.Counter `json:"base_head_seq,omitempty"`
	DTOVersion          int            `json:"dto_version,omitempty"`
	SourceExportVersion int            `json:"source_export_version,omitempty"`
}
type SealInput struct {
	ProtocolVersion int            `json:"protocol_version"`
	DataEpoch       domain.ID      `json:"data_epoch"`
	Chunks          []domain.Chunk `json:"chunks"`
	TotalBytes      domain.Counter `json:"total_bytes"`
	PayloadHash     string         `json:"payload_sha256"`
}
type jobMetadata struct {
	Input         CreateJobInput  `json:"input"`
	InputHash     string          `json:"input_hash"`
	PayloadHash   string          `json:"payload_hash,omitempty"`
	CanonicalHash string          `json:"canonical_hash,omitempty"`
	BatchSeq      domain.Counter  `json:"batch_seq,omitempty"`
	Receipt       *domain.Receipt `json:"receipt,omitempty"`
}
type SealedResponse struct {
	Job domain.Job         `json:"job"`
	Ref *domain.PayloadRef `json:"payload_ref"`
}
type Transfer struct {
	Store    ports.Store
	Auth     *Auth
	Sync     *Sync
	Codec    *protocol.Codec
	Files    ports.Artifacts
	Clock    ports.Clock
	MaxDisk  int64
	gate     chan struct{}
	wake     chan struct{}
	workerID string
	wg       sync.WaitGroup
	cancel   context.CancelFunc
	OnError  func(error)
}

func NewTransfer(store ports.Store, auth *Auth, syncer *Sync, codec *protocol.Codec, files ports.Artifacts, clock ports.Clock, maxDisk int64) *Transfer {
	id, _ := domain.NewID()
	return &Transfer{Store: store, Auth: auth, Sync: syncer, Codec: codec, Files: files, Clock: clock, MaxDisk: maxDisk, gate: make(chan struct{}, 2), wake: make(chan struct{}, 1), workerID: string(id)}
}
func (t *Transfer) enter(ctx context.Context) error {
	select {
	case t.gate <- struct{}{}:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}
func (t *Transfer) notify() {
	select {
	case t.wake <- struct{}{}:
	default:
	}
}
func operationKey(owner domain.ID, dataset *domain.ID, kind string, operation domain.ID) (string, error) {
	value := map[string]any{"owner": string(owner), "dataset": nil, "kind": kind, "operation": string(operation)}
	if dataset != nil {
		value["dataset"] = string(*dataset)
	}
	hash, e := protocol.Digest(value)
	return hex.EncodeToString(hash[:]), e
}
func metadata(j domain.Job) (jobMetadata, error) {
	var m jobMetadata
	e := json.Unmarshal(j.Metadata, &m)
	if e != nil {
		return m, domain.Wrap("storage_corrupt", e)
	}
	return m, nil
}
func setMetadata(j *domain.Job, m jobMetadata) error {
	b, e := json.Marshal(m)
	if e != nil {
		return e
	}
	j.Metadata = b
	return nil
}
func (t *Transfer) checkJob(ctx context.Context, tx ports.Tx, p domain.Principal, scope *domain.Scope, id domain.ID, ready bool) (domain.Job, error) {
	if e := t.Auth.Check(ctx, tx, p); e != nil {
		return domain.Job{}, e
	}
	j, e := tx.Job(ctx, id)
	if e != nil {
		return domain.Job{}, e
	}
	if j == nil || j.OwnerID == nil || *j.OwnerID != p.UserID {
		return domain.Job{}, domain.Fail("not_found")
	}
	if scope != nil {
		if j.DatasetID == nil || *j.DatasetID != scope.DatasetID {
			return domain.Job{}, domain.Fail("not_found")
		}
		if _, e = tx.Dataset(ctx, *scope); e != nil {
			return domain.Job{}, e
		}
		if j.DataEpoch == nil || *j.DataEpoch != scope.DataEpoch {
			return domain.Job{}, domain.Fail("epoch_changed")
		}
	} else if j.Kind != "EXPORT" {
		return domain.Job{}, domain.Fail("not_found")
	}
	if j.State == "EXPIRED" || j.State != "COMMITTED" && milliseconds(j.ExpiresAt) <= t.Clock.Now().UnixMilli() {
		return domain.Job{}, domain.Fail("snapshot_expired")
	}
	if ready && j.State != "READY" {
		return domain.Job{}, domain.Fail("job_state_conflict")
	}
	return *j, nil
}
func (t *Transfer) Create(ctx context.Context, p domain.Principal, scope *domain.Scope, kind string, in CreateJobInput) (domain.Job, error) {
	var out domain.Job
	if !domain.ValidID(string(in.OperationID), false) {
		return out, domain.Invalid("/operation_id")
	}
	if kind != "SNAPSHOT" && kind != "STAGED" && kind != "IMPORT" && kind != "EXPORT" {
		return out, domain.Invalid("/kind")
	}
	var dataset, epoch *domain.ID
	if scope != nil {
		dataset = &scope.DatasetID
		epoch = &scope.DataEpoch
	}
	if kind != "EXPORT" && scope == nil {
		return out, domain.Invalid("/scope")
	}
	if scope != nil && in.DataEpoch != scope.DataEpoch {
		return out, domain.Fail("epoch_changed")
	}
	if kind == "IMPORT" && (in.DTOVersion != 1 || in.SourceExportVersion < 1 || in.SourceExportVersion > 10) {
		return out, domain.Invalid("/import_version")
	}
	key, e := operationKey(p.UserID, dataset, kind, in.OperationID)
	if e != nil {
		return out, e
	}
	v, e := protocol.Value(in)
	if e != nil {
		return out, e
	}
	digest, e := protocol.Digest(v)
	if e != nil {
		return out, e
	}
	inputHash := hex.EncodeToString(digest[:])
	id, e := domain.NewID()
	if e != nil {
		return out, e
	}
	now := t.Clock.Now()
	reserved := int64(268435456)
	state := "BUILDING"
	ttl := 24 * time.Hour
	if kind == "STAGED" || kind == "IMPORT" {
		reserved = int64(in.ExpectedBytes)
		if reserved < 1 || reserved > 67108864 || in.InstallationID != p.InstallationID || !domain.ValidID(string(in.MutationID), false) {
			return out, domain.Invalid("/job_input")
		}
		state = "UPLOADING"
		ttl = time.Hour
	}
	_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
		if e := t.Auth.Check(ctx, tx, p); e != nil {
			return e
		}
		if scope != nil {
			if _, e = tx.Dataset(ctx, *scope); e != nil {
				return e
			}
		}
		old, e := tx.JobByOperation(ctx, key)
		if e != nil {
			return e
		}
		if old != nil {
			m, e := metadata(*old)
			if e != nil {
				return e
			}
			if m.InputHash != inputHash {
				return domain.Fail("job_state_conflict")
			}
			out = *old
			return nil
		}
		jobs, e := tx.AllJobs(ctx, nil)
		if e != nil {
			return e
		}
		var used int64
		active := 0
		for _, j := range jobs {
			if !slices.Contains([]string{"FAILED", "CANCELLED", "EXPIRED"}, j.State) {
				used += int64(j.Reserved)
			}
			if j.OwnerID != nil && *j.OwnerID == p.UserID && j.Kind == kind && slices.Contains([]string{"UPLOADING", "SEALED", "BUILDING", "PUBLISHING"}, j.State) {
				active++
			}
		}
		maxActive := 4
		if kind == "SNAPSHOT" || kind == "IMPORT" || kind == "EXPORT" {
			maxActive = 1
		}
		if active >= maxActive || used+reserved > t.MaxDisk {
			return domain.Fail("quota_exceeded")
		}
		out = domain.Job{ProtocolVersion: 1, ID: id, OwnerID: domain.Ptr(p.UserID), DatasetID: dataset, DataEpoch: epoch, Kind: kind, State: state, OperationKey: key, ExpiresAt: domain.At(now.Add(ttl)), Reserved: domain.Counter(reserved), CreatedMS: now.UnixMilli(), UpdatedMS: now.UnixMilli()}
		if e = setMetadata(&out, jobMetadata{Input: in, InputHash: inputHash}); e != nil {
			return e
		}
		if e = tx.InsertJob(ctx, out); e != nil {
			return e
		}
		if kind == "SNAPSHOT" {
			d, e := tx.Dataset(ctx, *scope)
			if e != nil {
				return e
			}
			return tx.Pin(ctx, d.ID, out.ID, d.Head, milliseconds(out.ExpiresAt))
		}
		return nil
	})
	if e == nil {
		t.notify()
	}
	return out, e
}
func (t *Transfer) Get(ctx context.Context, p domain.Principal, scope *domain.Scope, id domain.ID) (domain.Job, error) {
	var out domain.Job
	e := t.Store.View(ctx, func(tx ports.Tx) error { var e error; out, e = t.checkJob(ctx, tx, p, scope, id, false); return e })
	return out, e
}
func (t *Transfer) PutChunk(ctx context.Context, scope domain.Scope, id domain.ID, index int, reader io.Reader, size int64, hash string) (domain.Chunk, error) {
	j, e := t.Get(ctx, scope.Principal, &scope, id)
	if e != nil {
		return domain.Chunk{}, e
	}
	if j.State != "UPLOADING" || j.Kind != "STAGED" && j.Kind != "IMPORT" {
		return domain.Chunk{}, domain.Fail("job_state_conflict")
	}
	chunk, e := t.Files.Put(ctx, id, index, reader, size, hash)
	if e != nil {
		return chunk, e
	}
	_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
		j, e := t.checkJob(ctx, tx, scope.Principal, &scope, id, false)
		if e != nil {
			return e
		}
		if j.State != "UPLOADING" {
			return domain.Fail("job_state_conflict")
		}
		chunks, e := tx.Chunks(ctx, id)
		if e != nil {
			return e
		}
		used := int64(0)
		for _, c := range chunks {
			if c.Index == index {
				if c.Hash == hash && int64(c.Bytes) == size {
					return nil
				}
				return domain.Fail("chunk_hash_mismatch")
			}
			used += int64(c.Bytes)
		}
		if used+size > int64(j.Reserved) {
			return domain.Fail("quota_exceeded")
		}
		return tx.PutChunk(ctx, id, chunk)
	})
	return chunk, e
}
func (t *Transfer) Seal(ctx context.Context, scope domain.Scope, id domain.ID, in SealInput) (SealedResponse, error) {
	var out SealedResponse
	if e := t.enter(ctx); e != nil {
		return out, e
	}
	defer func() { <-t.gate }()
	var j domain.Job
	var chunks []domain.Chunk
	e := t.Store.View(ctx, func(tx ports.Tx) error {
		var e error
		j, e = t.checkJob(ctx, tx, scope.Principal, &scope, id, false)
		if e != nil {
			return e
		}
		chunks, e = tx.Chunks(ctx, id)
		return e
	})
	if e != nil {
		return out, e
	}
	m, e := metadata(j)
	if e != nil {
		return out, e
	}
	if j.Kind != "STAGED" && j.Kind != "IMPORT" || j.State != "UPLOADING" && j.State != "SEALED" {
		return out, domain.Fail("job_state_conflict")
	}
	if j.State == "SEALED" {
		var total domain.Counter
		for _, c := range chunks {
			total += c.Bytes
		}
		if in.TotalBytes != total {
			return out, domain.Fail("job_state_conflict")
		}
		if m.PayloadHash != in.PayloadHash || !sameChunks(chunks, in.Chunks) {
			return out, domain.Fail("job_state_conflict")
		}
		out.Job = j
		if j.Kind == "STAGED" {
			out.Ref = &domain.PayloadRef{JobID: id, CanonicalHash: m.CanonicalHash}
		}
		return out, nil
	}
	if !sameChunks(chunks, in.Chunks) || len(chunks) == 0 || int64(in.TotalBytes) > int64(j.Reserved) {
		return out, domain.Fail("chunk_hash_mismatch")
	}
	data, e := t.Files.Read(ctx, id, chunks, int64(j.Reserved))
	if e != nil {
		return out, e
	}
	sum := sha256.Sum256(data)
	if hex.EncodeToString(sum[:]) != in.PayloadHash || domain.Counter(len(data)) != in.TotalBytes {
		return out, domain.Fail("chunk_hash_mismatch")
	}
	if j.Kind == "STAGED" {
		if e = t.Codec.Decode("InlinePayload", data, &struct{}{}); e != nil {
			return out, e
		}
		expanded, e := t.expand(m.Input, data, scope.DatasetID)
		if e != nil {
			return out, e
		}
		m.CanonicalHash = hex.EncodeToString(expanded.Hash[:])
	} else {
		var payload domain.ImportPayload
		if e = t.Codec.Decode("ImportPayload", data, &payload); e != nil {
			return out, e
		}
		if payload.BaseHead != m.Input.BaseHead {
			return out, domain.Invalid("/base_head_seq")
		}
		hash, e := importHash(scope, m.Input, payload)
		if e != nil {
			return out, e
		}
		m.CanonicalHash = hex.EncodeToString(hash[:])
	}
	m.PayloadHash = in.PayloadHash
	_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
		current, e := t.checkJob(ctx, tx, scope.Principal, &scope, id, false)
		if e != nil {
			return e
		}
		if current.State != "UPLOADING" && current.State != "SEALED" {
			return domain.Fail("job_state_conflict")
		}
		actual, e := tx.Chunks(ctx, id)
		if e != nil {
			return e
		}
		if !sameChunks(actual, chunks) {
			return domain.Fail("job_state_conflict")
		}
		if current.State == "SEALED" {
			old, e := metadata(current)
			if e != nil {
				return e
			}
			if old.PayloadHash != m.PayloadHash {
				return domain.Fail("job_state_conflict")
			}
			out.Job = current
			return nil
		}
		expected := current.State
		current.State = "SEALED"
		current.UpdatedMS = t.Clock.Now().UnixMilli()
		if e = setMetadata(&current, m); e != nil {
			return e
		}
		if e = tx.UpdateJob(ctx, current, expected, current.Attempt); e != nil {
			return e
		}
		out.Job = current
		return nil
	})
	if e == nil && j.Kind == "STAGED" {
		out.Ref = &domain.PayloadRef{JobID: id, CanonicalHash: m.CanonicalHash}
	}
	return out, e
}
func sameChunks(a, b []domain.Chunk) bool {
	if len(a) != len(b) {
		return false
	}
	for i, x := range a {
		y := b[i]
		if x.Index != y.Index || x.Hash != y.Hash || x.Bytes != y.Bytes {
			return false
		}
	}
	return true
}
func (t *Transfer) expand(in CreateJobInput, data []byte, dataset domain.ID) (domain.Mutation, error) {
	v, e := protocol.StrictValue(data)
	if e != nil {
		return domain.Mutation{}, e
	}
	m := v.(map[string]any)
	m["protocol_version"] = 1
	m["data_epoch"] = string(in.DataEpoch)
	m["mutation_id"] = string(in.MutationID)
	m["installation_id"] = string(in.InstallationID)
	m["client_seq"] = in.ClientSeq.String()
	raw, e := json.Marshal(m)
	if e != nil {
		return domain.Mutation{}, e
	}
	return t.Codec.DecodeMutation(raw, dataset)
}
func importHash(scope domain.Scope, in CreateJobInput, payload domain.ImportPayload) ([32]byte, error) {
	v, e := protocol.Value(payload)
	if e != nil {
		return [32]byte{}, e
	}
	m := map[string]any{"protocol_version": 1, "kind": "import", "dataset_id": string(scope.DatasetID), "data_epoch": string(scope.DataEpoch), "mutation_id": string(in.MutationID), "installation_id": string(in.InstallationID), "client_seq": in.ClientSeq.String(), "source_export_version": in.SourceExportVersion, "payload": v}
	if e = protocol.NormalizeSets(m); e != nil {
		return [32]byte{}, e
	}
	return protocol.Digest(m)
}
func (t *Transfer) ResolveMutation(ctx context.Context, scope domain.Scope, m domain.Mutation) (domain.Mutation, func(), error) {
	noop := func() {}
	j, e := t.Get(ctx, scope.Principal, &scope, m.Ref.JobID)
	if e != nil {
		return m, noop, e
	}
	if j.State != "SEALED" || j.Kind != "STAGED" {
		return m, noop, domain.Fail("payload_ref_expired")
	}
	meta, e := metadata(j)
	if e != nil {
		return m, noop, e
	}
	lease, release, e := t.acquireLease(ctx, scope.Principal, &scope, j.ID, false)
	_ = lease
	if e != nil {
		return m, noop, e
	}
	var chunks []domain.Chunk
	e = t.Store.View(ctx, func(tx ports.Tx) error { var e error; chunks, e = tx.Chunks(ctx, j.ID); return e })
	if e != nil {
		release()
		return m, noop, e
	}
	data, e := t.Files.Read(ctx, j.ID, chunks, int64(j.Reserved))
	if e != nil {
		release()
		return m, noop, e
	}
	expanded, e := t.expand(meta.Input, data, scope.DatasetID)
	if e != nil {
		release()
		return m, noop, e
	}
	if expanded.Hash != m.Hash || expanded.OuterHash != m.OuterHash || hex.EncodeToString(expanded.Hash[:]) != meta.CanonicalHash {
		release()
		return m, noop, domain.Fail("mutation_payload_mismatch")
	}
	expanded.Ref = m.Ref
	return expanded, release, nil
}
func (t *Transfer) PublishImport(ctx context.Context, scope domain.Scope, id domain.ID, operation domain.ID) (domain.Receipt, error) {
	var out domain.Receipt
	if e := t.enter(ctx); e != nil {
		return out, e
	}
	defer func() { <-t.gate }()
	j, e := t.Get(ctx, scope.Principal, &scope, id)
	if e != nil {
		return out, e
	}
	meta, e := metadata(j)
	if e != nil {
		return out, e
	}
	if meta.Input.OperationID != operation || j.Kind != "IMPORT" {
		return out, domain.Fail("job_state_conflict")
	}
	if j.State == "COMMITTED" && meta.Receipt != nil {
		return *meta.Receipt, nil
	}
	if j.State != "SEALED" {
		return out, domain.Fail("job_state_conflict")
	}
	_, release, e := t.acquireLease(ctx, scope.Principal, &scope, id, false)
	if e != nil {
		return out, e
	}
	defer release()
	var chunks []domain.Chunk
	e = t.Store.View(ctx, func(tx ports.Tx) error { var e error; chunks, e = tx.Chunks(ctx, id); return e })
	if e != nil {
		return out, e
	}
	data, e := t.Files.Read(ctx, id, chunks, int64(j.Reserved))
	if e != nil {
		return out, e
	}
	var payload domain.ImportPayload
	if e = t.Codec.Decode("ImportPayload", data, &payload); e != nil {
		return out, e
	}
	hash, e := importHash(scope, meta.Input, payload)
	if e != nil {
		return out, e
	}
	state, e := t.Store.Update(ctx, func(tx ports.Tx) error {
		current, e := t.checkJob(ctx, tx, scope.Principal, &scope, id, false)
		if e != nil {
			return e
		}
		prior, e := tx.Receipt(ctx, scope, meta.Input.MutationID)
		if e != nil {
			return e
		}
		if prior != nil {
			if prior.Hash != hash {
				return domain.Fail("mutation_payload_mismatch")
			}
			out = prior.Receipt
			return nil
		}
		if current.State != "SEALED" {
			return domain.Fail("job_state_conflict")
		}
		dataset, e := tx.Dataset(ctx, scope)
		if e != nil {
			return e
		}
		if dataset.Head != payload.BaseHead {
			return domain.Fail("import_rebase_required")
		}
		g, e := tx.LoadGraph(ctx, scope)
		if e != nil {
			return e
		}
		plan, e := domain.PlanImport(g, payload, scope.DatasetID)
		if e != nil {
			return e
		}
		at := domain.At(t.Clock.Now())
		out = domain.EmptyReceipt(meta.Input.MutationID, scope.DataEpoch, dataset.Head, at)
		if len(plan.Changes) > 0 || len(plan.Structures) > 0 || len(plan.Aliases) > 0 || len(plan.Edges) > 0 {
			seq := dataset.Head + 1
			if seq <= dataset.Head {
				return domain.Fail("quota_exceeded")
			}
			batch := domain.Batch{Seq: seq, MutationID: meta.Input.MutationID, InstallationID: meta.Input.InstallationID, CommittedAt: at, Changes: []domain.Entity{}, Structures: plan.Structures, Aliases: plan.Aliases, Edges: plan.Edges}
			for _, d := range plan.Changes {
				batch.Changes = append(batch.Changes, d.After)
				out.Entities = append(out.Entities, domain.Version{Type: d.After.Type, ID: d.After.ID, Revision: d.After.Revision})
			}
			if e = tx.Apply(ctx, scope, plan); e != nil {
				return e
			}
			if e = tx.AppendBatch(ctx, scope, batch); e != nil {
				return e
			}
			out.Result = "committed"
			out.BatchSeq = &seq
			out.ObservedHead = seq
			out.Structures = plan.Structures
		}
		response, e := json.Marshal(out)
		if e != nil {
			return e
		}
		if e = tx.AppendReceipt(ctx, scope, domain.StoredReceipt{Receipt: out, Hash: hash, Response: response}); e != nil {
			return e
		}
		expected := current.State
		current.State = "COMMITTED"
		current.UpdatedMS = t.Clock.Now().UnixMilli()
		meta.Receipt = &out
		if e = setMetadata(&current, meta); e != nil {
			return e
		}
		return tx.UpdateJob(ctx, current, expected, current.Attempt)
	})
	if state == ports.CommitUnknown && e != nil {
		probe, cancel := context.WithTimeout(context.WithoutCancel(ctx), 2*time.Second)
		defer cancel()
		saved, err := t.Sync.GetReceipt(probe, scope, meta.Input.MutationID)
		if err == nil && saved != nil {
			return *saved, nil
		}
		return domain.Receipt{}, domain.Wrap("commit_result_unknown", e)
	}
	return out, e
}
func (t *Transfer) Renew(ctx context.Context, scope domain.Scope, id domain.ID, seconds int) (domain.Job, error) {
	var out domain.Job
	if seconds < 1 || seconds > 3600 {
		return out, domain.Invalid("/extend_seconds")
	}
	_, e := t.Store.Update(ctx, func(tx ports.Tx) error {
		var e error
		out, e = t.checkJob(ctx, tx, scope.Principal, &scope, id, false)
		if e != nil {
			return e
		}
		if slices.Contains([]string{"COMMITTED", "PUBLISHING", "CANCELLED", "FAILED"}, out.State) {
			return domain.Fail("job_state_conflict")
		}
		maxExpiry := time.UnixMilli(out.CreatedMS).Add(48 * time.Hour)
		next := min(milliseconds(out.ExpiresAt)+int64(seconds)*1000, maxExpiry.UnixMilli())
		expected := out.State
		out.ExpiresAt = domain.FromMillis(next)
		out.UpdatedMS = t.Clock.Now().UnixMilli()
		if e = tx.UpdateJob(ctx, out, expected, out.Attempt); e != nil {
			return e
		}
		if out.Kind == "SNAPSHOT" {
			m, e := tx.Manifest(ctx, out.ID)
			if e != nil {
				return e
			}
			if m != nil && m.Cutoff != nil {
				return tx.Pin(ctx, scope.DatasetID, out.ID, *m.Cutoff, next)
			}
			return tx.Pin(ctx, scope.DatasetID, out.ID, 0, next)
		}
		return nil
	})
	return out, e
}
func (t *Transfer) Cancel(ctx context.Context, scope domain.Scope, id domain.ID) (domain.Job, error) {
	var out domain.Job
	_, e := t.Store.Update(ctx, func(tx ports.Tx) error {
		var e error
		out, e = t.checkJob(ctx, tx, scope.Principal, &scope, id, false)
		if e != nil {
			return e
		}
		if out.State == "PUBLISHING" || out.State == "COMMITTED" {
			return domain.Fail("job_state_conflict")
		}
		expected := out.State
		out.State = "CANCELLED"
		out.UpdatedMS = t.Clock.Now().UnixMilli()
		if e = tx.UpdateJob(ctx, out, expected, out.Attempt); e != nil {
			return e
		}
		return tx.ReleasePin(ctx, id)
	})
	return out, e
}
func (t *Transfer) ReleaseSnapshot(ctx context.Context, scope domain.Scope, id domain.ID, applied domain.Counter) error {
	_, e := t.Store.Update(ctx, func(tx ports.Tx) error {
		j, e := t.checkJob(ctx, tx, scope.Principal, &scope, id, true)
		if e != nil {
			return e
		}
		if j.Kind != "SNAPSHOT" {
			return domain.Fail("job_state_conflict")
		}
		m, e := tx.Manifest(ctx, id)
		if e != nil {
			return e
		}
		d, e := tx.Dataset(ctx, scope)
		if e != nil {
			return e
		}
		if m == nil || m.Cutoff == nil || applied < *m.Cutoff || applied > d.Head {
			return domain.Invalid("/applied_seq")
		}
		return tx.ReleasePin(ctx, id)
	})
	return e
}
func (t *Transfer) acquireLease(ctx context.Context, p domain.Principal, scope *domain.Scope, id domain.ID, ready bool) (domain.Job, func(), error) {
	var j domain.Job
	lease, e := domain.NewID()
	if e != nil {
		return j, func() {}, e
	}
	_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
		var e error
		j, e = t.checkJob(ctx, tx, p, scope, id, ready)
		if e != nil {
			return e
		}
		if !ready && j.State != "SEALED" {
			return domain.Fail("job_state_conflict")
		}
		n, e := tx.ReadLeaseCount(ctx, id, t.Clock.Now().UnixMilli())
		if e != nil {
			return e
		}
		if n >= 4 {
			return domain.Fail("busy")
		}
		now := t.Clock.Now().UnixMilli()
		return tx.ReadLease(ctx, id, lease, string(p.SessionID), now, min(now+20000, milliseconds(j.ExpiresAt)))
	})
	var once sync.Once
	release := func() {
		once.Do(func() {
			probe, cancel := context.WithTimeout(context.Background(), 2*time.Second)
			defer cancel()
			_, _ = t.Store.Update(probe, func(tx ports.Tx) error { return tx.ReleaseReadLease(probe, lease) })
		})
	}
	if e != nil {
		return j, func() {}, e
	}
	return j, release, nil
}
func (t *Transfer) GetManifest(ctx context.Context, p domain.Principal, scope *domain.Scope, id domain.ID) (domain.Manifest, error) {
	var out domain.Manifest
	e := t.Store.View(ctx, func(tx ports.Tx) error {
		j, e := t.checkJob(ctx, tx, p, scope, id, true)
		if e != nil {
			return e
		}
		m, e := tx.Manifest(ctx, id)
		if e != nil {
			return e
		}
		if m == nil {
			return domain.Fail("storage_corrupt")
		}
		if j.Kind == "SNAPSHOT" && scope != nil && m.Cutoff != nil {
			d, e := tx.Dataset(ctx, *scope)
			if e != nil {
				return e
			}
			if d.RetainedAfter > *m.Cutoff {
				return domain.Fail("cursor_expired")
			}
		}
		out = *m
		out.ExpiresAt = j.ExpiresAt
		return nil
	})
	return out, e
}

type leasedReader struct {
	io.ReadCloser
	release func()
}

func (r *leasedReader) Close() error { e := r.ReadCloser.Close(); r.release(); return e }
func (t *Transfer) OpenChunk(ctx context.Context, p domain.Principal, scope *domain.Scope, id domain.ID, index int) (domain.Chunk, io.ReadCloser, error) {
	_, release, e := t.acquireLease(ctx, p, scope, id, true)
	if e != nil {
		return domain.Chunk{}, nil, e
	}
	var chunks []domain.Chunk
	e = t.Store.View(ctx, func(tx ports.Tx) error { var e error; chunks, e = tx.Chunks(ctx, id); return e })
	if e != nil {
		release()
		return domain.Chunk{}, nil, e
	}
	if index < 0 || index >= len(chunks) || chunks[index].Index != index {
		release()
		return domain.Chunk{}, nil, domain.Fail("not_found")
	}
	f, e := t.Files.OpenStored(id, chunks[index])
	if e != nil {
		release()
		return domain.Chunk{}, nil, e
	}
	return chunks[index], &leasedReader{f, release}, nil
}

// Explicit sorting keeps complete snapshots and manifests stable across restarts.
func snapshot(scope domain.Scope, dataset domain.Dataset, g domain.Graph) domain.Snapshot {
	entities := make([]domain.Entity, 0, len(g.Entities))
	for _, e := range g.Entities {
		entities = append(entities, e)
	}
	sort.Slice(entities, func(i, j int) bool { return entities[i].Key().String() < entities[j].Key().String() })
	structures := []domain.StructureVersion{}
	for k, v := range g.Structures {
		structures = append(structures, domain.StructureVersion{Kind: k.Kind, ID: k.ID, Revision: v})
	}
	sort.Slice(structures, func(i, j int) bool {
		a, b := structures[i], structures[j]
		if a.Kind != b.Kind {
			return a.Kind < b.Kind
		}
		return a.ID < b.ID
	})
	aliases := append([]domain.Alias{}, g.Aliases...)
	sort.Slice(aliases, func(i, j int) bool { return aliases[i].AliasID < aliases[j].AliasID })
	edges := append([]domain.Edge{}, g.Edges...)
	sort.Slice(edges, func(i, j int) bool {
		if edges[i].RuleID != edges[j].RuleID {
			return edges[i].RuleID < edges[j].RuleID
		}
		return edges[i].PredecessorID < edges[j].PredecessorID
	})
	return domain.Snapshot{DTOVersion: 1, DatasetID: scope.DatasetID, DataEpoch: scope.DataEpoch, Cutoff: dataset.Head, Entities: entities, Structures: structures, Aliases: aliases, Edges: edges}
}
func manifestHash(m domain.Manifest) (string, error) {
	v, e := protocol.Value(m)
	if e != nil {
		return "", e
	}
	value := v.(map[string]any)
	for _, key := range []string{"manifest_sha256", "expires_at_utc", "resume_cursor", "metadata"} {
		delete(value, key)
	}
	hash, e := protocol.Digest(value)
	return hex.EncodeToString(hash[:]), e
}
