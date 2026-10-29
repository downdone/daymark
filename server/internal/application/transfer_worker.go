package application

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"slices"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

func (t *Transfer) Start(parent context.Context) {
	ctx, cancel := context.WithCancel(parent)
	t.cancel = cancel
	for range 2 {
		t.wg.Add(1)
		go func() {
			defer t.wg.Done()
			ticker := time.NewTicker(time.Second)
			defer ticker.Stop()
			for {
				for {
					ran, e := t.RunOne(ctx)
					if e != nil && ctx.Err() == nil && t.OnError != nil {
						t.OnError(e)
					}
					if e != nil || !ran {
						break
					}
				}
				select {
				case <-ctx.Done():
					return
				case <-t.wake:
				case <-ticker.C:
				}
			}
		}()
	}
	t.wg.Add(1)
	go func() {
		defer t.wg.Done()
		timer := time.NewTimer(0)
		defer timer.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-timer.C:
				run, cancel := context.WithTimeout(ctx, 20*time.Second)
				e := t.Reconcile(run)
				cancel()
				if e != nil && ctx.Err() == nil && t.OnError != nil {
					t.OnError(e)
				}
				timer.Reset(time.Minute)
			}
		}
	}()
}
func (t *Transfer) Stop() {
	if t.cancel != nil {
		t.cancel()
	}
	t.wg.Wait()
}
func (t *Transfer) claim(ctx context.Context, id *domain.ID) (*domain.Job, error) {
	var out *domain.Job
	_, e := t.Store.Update(ctx, func(tx ports.Tx) error {
		var jobs []domain.Job
		var e error
		now := t.Clock.Now().UnixMilli()
		if id != nil {
			j, e := tx.Job(ctx, *id)
			if e != nil {
				return e
			}
			if j != nil {
				jobs = []domain.Job{*j}
			}
		} else {
			jobs, e = tx.RunnableJobs(ctx, now, 8, "SNAPSHOT", "EXPORT", "BATCH")
			if e != nil {
				return e
			}
		}
		for _, j := range jobs {
			if j.State != "BUILDING" || !slices.Contains([]string{"SNAPSHOT", "EXPORT", "BATCH"}, j.Kind) || milliseconds(j.ExpiresAt) <= now || j.LeaseUntil != nil && *j.LeaseUntil > now {
				continue
			}
			expected, attempt := j.State, j.Attempt
			j.Attempt++
			j.LeaseOwner = domain.Ptr(t.workerID)
			j.LeaseUntil = domain.Ptr(now + 60000)
			j.UpdatedMS = now
			if e = tx.UpdateJob(ctx, j, expected, attempt); e != nil {
				return e
			}
			out = &j
			return nil
		}
		return nil
	})
	return out, e
}

// RunOne is also a deterministic integration seam: no sleeps or background
// timers are necessary to verify claim, fencing and a complete build.
func (t *Transfer) RunOne(ctx context.Context) (bool, error) {
	j, e := t.claim(ctx, nil)
	if e != nil || j == nil {
		return false, e
	}
	return true, t.process(ctx, *j)
}
func (t *Transfer) process(parent context.Context, j domain.Job) error {
	if e := t.enter(parent); e != nil {
		return e
	}
	defer func() { <-t.gate }()
	ctx, cancel := context.WithTimeout(parent, 30*time.Second)
	defer cancel()
	finished := make(chan struct{})
	defer close(finished)
	go func() {
		ticker := time.NewTicker(20 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-finished:
				return
			case <-ctx.Done():
				return
			case <-ticker.C:
				_, e := t.Store.Update(ctx, func(tx ports.Tx) error {
					fresh, e := tx.Job(ctx, j.ID)
					if e != nil {
						return e
					}
					if fresh == nil || fresh.Attempt != j.Attempt || fresh.State != j.State || domain.Value(fresh.LeaseOwner) != t.workerID {
						return domain.Fail("lost_lease")
					}
					fresh.LeaseUntil = domain.Ptr(t.Clock.Now().UnixMilli() + 60000)
					return tx.UpdateJob(ctx, *fresh, j.State, j.Attempt)
				})
				if e != nil {
					cancel()
					return
				}
			}
		}
	}()
	e := t.build(ctx, j)
	if e == nil {
		return nil
	}
	probe, release := context.WithTimeout(context.WithoutCancel(parent), 2*time.Second)
	defer release()
	_, mark := t.Store.Update(probe, func(tx ports.Tx) error {
		fresh, e2 := tx.Job(probe, j.ID)
		if e2 != nil {
			return e2
		}
		if fresh == nil || fresh.Attempt != j.Attempt || fresh.State != j.State {
			return domain.Fail("lost_lease")
		}
		transient := errors.Is(e, context.Canceled) || errors.Is(e, context.DeadlineExceeded) || domain.Code(e) == "busy" || domain.Code(e) == "lost_lease"
		if !transient {
			fresh.State = "FAILED"
		}
		fresh.SafeError = domain.Ptr(domain.Code(e))
		fresh.LeaseUntil = nil
		fresh.LeaseOwner = nil
		fresh.UpdatedMS = t.Clock.Now().UnixMilli()
		if e2 = tx.UpdateJob(probe, *fresh, j.State, j.Attempt); e2 != nil {
			return e2
		}
		if transient {
			return nil
		}
		return tx.ReleasePin(probe, j.ID)
	})
	if mark != nil && domain.Code(mark) != "lost_lease" {
		return mark
	}
	return e
}
func (t *Transfer) workerScope(ctx context.Context, tx ports.Tx, j domain.Job) (domain.Scope, error) {
	if j.OwnerID == nil {
		return domain.Scope{}, domain.Fail("not_found")
	}
	user, e := tx.User(ctx, *j.OwnerID)
	if e != nil {
		return domain.Scope{}, e
	}
	if user == nil || user.Status != "ACTIVE" {
		return domain.Scope{}, domain.Fail("account_disabled")
	}
	scope := domain.Scope{Principal: domain.Principal{UserID: *j.OwnerID}}
	if j.DatasetID != nil {
		scope.DatasetID = *j.DatasetID
		scope.DataEpoch = domain.Value(j.DataEpoch)
		if _, e = tx.Dataset(ctx, scope); e != nil {
			return scope, e
		}
	}
	return scope, nil
}
func (t *Transfer) build(ctx context.Context, j domain.Job) error {
	var data []byte
	var manifest domain.Manifest
	var scope domain.Scope
	e := t.Store.View(ctx, func(tx ports.Tx) error {
		var e error
		scope, e = t.workerScope(ctx, tx, j)
		if e != nil {
			return e
		}
		manifest = domain.Manifest{ProtocolVersion: 1, JobID: j.ID, DatasetID: j.DatasetID, DataEpoch: j.DataEpoch, Kind: j.Kind, DTOVersion: 1, ExpiresAt: j.ExpiresAt, Chunks: []domain.Chunk{}}
		switch j.Kind {
		case "SNAPSHOT":
			d, e := tx.Dataset(ctx, scope)
			if e != nil {
				return e
			}
			g, e := tx.LoadGraph(ctx, scope)
			if e != nil {
				return e
			}
			snap := snapshot(scope, d, g)
			manifest.Cutoff = &snap.Cutoff
			manifest.EntityCount = domain.Counter(len(snap.Entities))
			resume, e := t.Sync.Cursor.Issue(scope, d.Head, nil)
			if e != nil {
				return e
			}
			manifest.ResumeCursor = &resume
			data, e = json.Marshal(snap)
			return e
		case "EXPORT":
			datasets, e := tx.Datasets(ctx, *j.OwnerID)
			if e != nil {
				return e
			}
			snapshots := []domain.Snapshot{}
			for _, d := range datasets {
				s := domain.Scope{Principal: scope.Principal, DatasetID: d.ID, DataEpoch: d.DataEpoch}
				g, e := tx.LoadGraph(ctx, s)
				if e != nil {
					return e
				}
				snap := snapshot(s, d, g)
				snapshots = append(snapshots, snap)
				manifest.EntityCount += domain.Counter(len(snap.Entities))
			}
			data, e = json.Marshal(map[string]any{"export_format_version": 1, "datasets": snapshots})
			return e
		case "BATCH":
			m, e := metadata(j)
			if e != nil {
				return e
			}
			if m.BatchSeq < 1 {
				return domain.Fail("storage_corrupt")
			}
			batches, e := tx.Batches(ctx, scope, m.BatchSeq-1, m.BatchSeq, 1)
			if e != nil {
				return e
			}
			if len(batches) != 1 {
				return domain.Fail("cursor_expired")
			}
			manifest.Cutoff = &m.BatchSeq
			manifest.EntityCount = domain.Counter(len(batches[0].Changes))
			data, e = json.Marshal(batches[0])
			return e
		}
		return domain.Fail("job_state_conflict")
	})
	if e != nil {
		return e
	}
	if int64(len(data)) > int64(j.Reserved) {
		return domain.Fail("quota_exceeded")
	}
	if e = t.Files.CleanAttempts(j.ID, j.Attempt); e != nil {
		return e
	}
	chunks, hash, e := t.Files.PublishAttempt(ctx, j.ID, j.Attempt, data)
	if e != nil {
		return e
	}
	manifest.Chunks = chunks
	manifest.TotalBytes = domain.Counter(len(data))
	manifest.PayloadHash = hash
	manifest.ManifestHash, e = manifestHash(manifest)
	if e != nil {
		return e
	}
	_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
		current, e := tx.Job(ctx, j.ID)
		if e != nil {
			return e
		}
		if current == nil || current.State != j.State || current.Attempt != j.Attempt || domain.Value(current.LeaseOwner) != t.workerID {
			return domain.Fail("lost_lease")
		}
		if milliseconds(current.ExpiresAt) <= t.Clock.Now().UnixMilli() {
			return domain.Fail("snapshot_expired")
		}
		if _, e = t.workerScope(ctx, tx, *current); e != nil {
			return e
		}
		if j.Kind == "SNAPSHOT" {
			d, e := tx.Dataset(ctx, scope)
			if e != nil {
				return e
			}
			if manifest.Cutoff == nil || d.RetainedAfter > *manifest.Cutoff {
				return domain.Fail("cursor_expired")
			}
		}
		existing, e := tx.Chunks(ctx, j.ID)
		if e != nil {
			return e
		}
		for _, chunk := range chunks {
			found := false
			for _, old := range existing {
				if old.Index == chunk.Index {
					found = true
					if old.Hash != chunk.Hash || old.Bytes != chunk.Bytes {
						return domain.Fail("chunk_hash_mismatch")
					}
				}
			}
			if !found {
				if e = tx.PutChunk(ctx, j.ID, chunk); e != nil {
					return e
				}
			}
		}
		if e = tx.PutManifest(ctx, j.ID, manifest); e != nil {
			return e
		}
		current.State = "READY"
		current.Reserved = manifest.TotalBytes
		current.LeaseOwner = nil
		current.LeaseUntil = nil
		current.UpdatedMS = t.Clock.Now().UnixMilli()
		if e = tx.UpdateJob(ctx, *current, j.State, j.Attempt); e != nil {
			return e
		}
		if j.Kind == "SNAPSHOT" {
			return tx.Pin(ctx, scope.DatasetID, j.ID, *manifest.Cutoff, milliseconds(current.ExpiresAt))
		}
		return tx.ReleasePin(ctx, j.ID)
	})
	return e
}
func (t *Transfer) ExportBatch(ctx context.Context, scope domain.Scope, batch domain.Batch) (BatchReference, error) {
	var out BatchReference
	data, e := json.Marshal(batch)
	if e != nil {
		return out, e
	}
	if len(data) > 268435456 {
		return out, domain.Fail("payload_too_large")
	}
	operation := domain.StableID(fmt.Sprintf("batch:%s:%s:%d", scope.DataEpoch, batch.Seq.String(), t.Clock.Now().Unix()/86400))
	key, e := operationKey(scope.Principal.UserID, &scope.DatasetID, "BATCH", operation)
	if e != nil {
		return out, e
	}
	var j domain.Job
	_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
		if e := t.Auth.Check(ctx, tx, scope.Principal); e != nil {
			return e
		}
		d, e := tx.Dataset(ctx, scope)
		if e != nil {
			return e
		}
		old, e := tx.JobByOperation(ctx, key)
		if e != nil {
			return e
		}
		if old != nil {
			j = *old
			return nil
		}
		if d.RetainedAfter >= batch.Seq {
			return domain.Fail("cursor_expired")
		}
		all, e := tx.AllJobs(ctx, nil)
		if e != nil {
			return e
		}
		var bytes int64
		for _, job := range all {
			bytes += int64(job.Reserved)
		}
		if bytes+int64(len(data)) > t.MaxDisk {
			return domain.Fail("quota_exceeded")
		}
		id, e := domain.NewID()
		if e != nil {
			return e
		}
		now := t.Clock.Now()
		j = domain.Job{ProtocolVersion: 1, ID: id, OwnerID: domain.Ptr(scope.Principal.UserID), DatasetID: &scope.DatasetID, DataEpoch: &scope.DataEpoch, Kind: "BATCH", State: "BUILDING", OperationKey: key, ExpiresAt: domain.At(now.Add(24 * time.Hour)), Reserved: domain.Counter(len(data)), CreatedMS: now.UnixMilli(), UpdatedMS: now.UnixMilli()}
		if e = setMetadata(&j, jobMetadata{BatchSeq: batch.Seq}); e != nil {
			return e
		}
		if e = tx.InsertJob(ctx, j); e != nil {
			return e
		}
		return tx.Pin(ctx, scope.DatasetID, id, batch.Seq-1, milliseconds(j.ExpiresAt))
	})
	if e != nil {
		return out, e
	}
	if j.State != "READY" {
		claimed, e := t.claim(ctx, &j.ID)
		if e != nil {
			return out, e
		}
		if claimed == nil {
			return out, domain.Fail("busy")
		}
		if e = t.process(ctx, *claimed); e != nil {
			return out, e
		}
	}
	manifest, e := t.GetManifest(ctx, scope.Principal, &scope, j.ID)
	if e != nil {
		return out, e
	}
	sum := sha256.Sum256(data)
	if manifest.PayloadHash != hex.EncodeToString(sum[:]) {
		return out, domain.Fail("storage_corrupt")
	}
	out.Seq = batch.Seq
	out.Ref.JobID = j.ID
	out.Ref.Hash = manifest.PayloadHash
	out.Ref.Bytes = manifest.TotalBytes
	return out, nil
}
func (t *Transfer) Reconcile(ctx context.Context) error {
	nowInstant := t.Clock.Now()
	if _, e := t.Store.Update(ctx, func(tx ports.Tx) error {
		return tx.Maintain(ctx, nowInstant.UnixMilli(), nowInstant.AddDate(0, 0, -30).UnixMilli())
	}); e != nil {
		return e
	}
	var jobs []domain.Job
	if e := t.Store.View(ctx, func(tx ports.Tx) error { var e error; jobs, e = tx.AllJobs(ctx, nil); return e }); e != nil {
		return e
	}
	known := map[domain.ID]bool{}
	now := t.Clock.Now().UnixMilli()
	for _, job := range jobs {
		known[job.ID] = true
		if job.LeaseUntil != nil && *job.LeaseUntil > now {
			continue
		}
		expired := milliseconds(job.ExpiresAt) <= now
		terminal := slices.Contains([]string{"FAILED", "CANCELLED", "EXPIRED"}, job.State)
		if !expired && !terminal {
			continue
		}
		if terminal && now-job.UpdatedMS < 30000 {
			continue
		}
		if job.State == "PUBLISHING" {
			continue
		}
		canDelete := false
		_, e := t.Store.Update(ctx, func(tx ports.Tx) error {
			fresh, e := tx.Job(ctx, job.ID)
			if e != nil || fresh == nil {
				return e
			}
			active, e := tx.HasReadLease(ctx, job.ID, now)
			if e != nil {
				return e
			}
			if active {
				return nil
			}
			if fresh.LeaseUntil != nil && *fresh.LeaseUntil > now {
				return nil
			}
			if !slices.Contains([]string{"FAILED", "CANCELLED", "EXPIRED", "COMMITTED"}, fresh.State) {
				expected := fresh.State
				fresh.State = "EXPIRED"
				fresh.UpdatedMS = now
				if e = tx.UpdateJob(ctx, *fresh, expected, fresh.Attempt); e != nil {
					return e
				}
			}
			if e = tx.ReleasePin(ctx, job.ID); e != nil {
				return e
			}
			canDelete = true
			return nil
		})
		if e != nil {
			return e
		}
		if !canDelete {
			continue
		}
		if e = t.Files.RemoveJob(job.ID); e != nil {
			return e
		}
		_, e = t.Store.Update(ctx, func(tx ports.Tx) error {
			fresh, e := tx.Job(ctx, job.ID)
			if e != nil || fresh == nil {
				return e
			}
			if !slices.Contains([]string{"FAILED", "CANCELLED", "EXPIRED", "COMMITTED"}, fresh.State) {
				return domain.Fail("job_state_conflict")
			}
			fresh.Reserved = 0
			return tx.UpdateJob(ctx, *fresh, fresh.State, fresh.Attempt)
		})
		if e != nil {
			return e
		}
	}
	orphans, e := t.Files.Orphans(known, t.Clock.Now().Add(-time.Hour))
	if e != nil {
		return e
	}
	for _, id := range orphans {
		if e = t.Files.RemoveJob(id); e != nil {
			return e
		}
	}
	return nil
}
