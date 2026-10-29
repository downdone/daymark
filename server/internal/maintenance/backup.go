package maintenance

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

type BackupService struct {
	Store              ports.Store
	Clock              ports.Clock
	Vault              *Vault
	ScratchDir, Target string
	mu                 sync.Mutex
	cancel             context.CancelFunc
	wg                 sync.WaitGroup
	OnError            func(error)
	Keep               int
}

func (b *BackupService) Backup(ctx context.Context, operation domain.ID) (domain.BackupRecord, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	var job domain.Job
	var record *domain.BackupRecord
	var state domain.State
	key := "system:backup:" + string(operation)
	_, e := b.Store.Update(ctx, func(tx ports.Tx) error {
		var e error
		state, e = tx.State(ctx)
		if e != nil {
			return e
		}
		if state.LedgerID == nil {
			return domain.Fail("storage_corrupt")
		}
		old, e := tx.JobByOperation(ctx, key)
		if e != nil {
			return e
		}
		if old != nil {
			job = *old
			record, e = tx.BackupRecord(ctx, old.ID)
			return e
		}
		id, e := domain.NewID()
		if e != nil {
			return e
		}
		now := b.Clock.Now()
		job = domain.Job{ProtocolVersion: 1, ID: id, Kind: "BACKUP", State: "BUILDING", OperationKey: key, Metadata: json.RawMessage(`{}`), ExpiresAt: domain.At(now.AddDate(10, 0, 0)), CreatedMS: now.UnixMilli(), UpdatedMS: now.UnixMilli()}
		return tx.InsertJob(ctx, job)
	})
	if e != nil {
		return domain.BackupRecord{}, e
	}
	if record != nil && record.VerifiedMS != nil {
		return *record, nil
	}
	if record == nil {
		if e = os.MkdirAll(b.ScratchDir, 0700); e != nil {
			return domain.BackupRecord{}, e
		}
		temp, e := os.CreateTemp(b.ScratchDir, "backup-*.sqlite")
		if e != nil {
			return domain.BackupRecord{}, e
		}
		path := temp.Name()
		temp.Close()
		os.Remove(path)
		defer os.Remove(path)
		started := b.Clock.Now().UnixMilli()
		if e = b.Store.Snapshot(ctx, path); e != nil {
			return domain.BackupRecord{}, e
		}
		info, e := b.Store.InspectBackup(ctx, path)
		if e != nil {
			return domain.BackupRecord{}, e
		}
		if info.State.ServerID != state.ServerID || info.State.LedgerID == nil || *info.State.LedgerID != *state.LedgerID {
			return domain.BackupRecord{}, domain.Fail("storage_corrupt")
		}
		archive, e := b.Vault.Seal(ctx, job.ID, path, started)
		if e != nil {
			return domain.BackupRecord{}, e
		}
		// Inspect the authenticated archive, including a previously published file
		// recovered after a crash between filesystem seal and DB registration.
		verify, e := os.CreateTemp(b.ScratchDir, "verify-*.sqlite")
		if e != nil {
			return domain.BackupRecord{}, e
		}
		verifiedPath := verify.Name()
		verify.Close()
		os.Remove(verifiedPath)
		defer os.Remove(verifiedPath)
		if _, e = b.Vault.Extract(ctx, job.ID, verifiedPath); e != nil {
			return domain.BackupRecord{}, e
		}
		actual, e := b.Store.InspectBackup(ctx, verifiedPath)
		if e != nil {
			return domain.BackupRecord{}, e
		}
		if actual.State.ServerID != state.ServerID || actual.State.LedgerID == nil || *actual.State.LedgerID != *state.LedgerID {
			return domain.BackupRecord{}, domain.Fail("storage_corrupt")
		}
		record = &domain.BackupRecord{JobID: job.ID, SnapshotMS: archive.Header.SnapshotMS, Hash: archive.Hash, ArtifactKey: archive.Key, SchemaVersion: actual.State.SchemaVersion}
		_, e = b.Store.Update(ctx, func(tx ports.Tx) error {
			fresh, e := tx.Job(ctx, job.ID)
			if e != nil {
				return e
			}
			if fresh == nil || fresh.Kind != "BACKUP" {
				return domain.Fail("not_found")
			}
			if e = tx.PutBackupRecord(ctx, *record); e != nil {
				return e
			}
			expected := fresh.State
			fresh.State = "READY"
			fresh.UpdatedMS = b.Clock.Now().UnixMilli()
			return tx.UpdateJob(ctx, *fresh, expected, fresh.Attempt)
		})
		if e != nil {
			return domain.BackupRecord{}, e
		}
	}
	if b.Target == "" {
		return *record, nil
	}
	if _, e = b.Vault.CopyVerified(ctx, job.ID, b.Target); e != nil {
		return *record, e
	}
	now := b.Clock.Now().UnixMilli()
	target := "configured-protected-target"
	record.TargetID = &target
	record.VerifiedMS = &now
	_, e = b.Store.Update(ctx, func(tx ports.Tx) error {
		fresh, e := tx.Job(ctx, job.ID)
		if e != nil {
			return e
		}
		if fresh == nil {
			return domain.Fail("not_found")
		}
		if e = tx.PutBackupRecord(ctx, *record); e != nil {
			return e
		}
		expected := fresh.State
		fresh.State = "COMMITTED"
		fresh.UpdatedMS = now
		return tx.UpdateJob(ctx, *fresh, expected, fresh.Attempt)
	})
	return *record, e
}
func (b *BackupService) Start(parent context.Context) {
	ctx, cancel := context.WithCancel(parent)
	b.cancel = cancel
	b.wg.Add(1)
	go func() {
		defer b.wg.Done()
		timer := time.NewTimer(0)
		defer timer.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-timer.C:
				run, recoverCancel := context.WithTimeout(ctx, 2*time.Minute)
				recoverError := b.Resume(run)
				recoverCancel()
				if recoverError != nil && ctx.Err() == nil && b.OnError != nil {
					b.OnError(recoverError)
				}
				op, e := domain.NewID()
				if e == nil {
					run, release := context.WithTimeout(ctx, 2*time.Minute)
					_, e = b.Backup(run, op)
					release()
				}
				if e != nil && b.OnError != nil {
					b.OnError(e)
				}
				if e == nil {
					run, release := context.WithTimeout(ctx, 2*time.Minute)
					rotateError := b.Rotate(run)
					release()
					if rotateError != nil && b.OnError != nil {
						b.OnError(rotateError)
					}
				}
				timer.Reset(30 * time.Minute)
			}
		}
	}()
}
func (b *BackupService) Resume(ctx context.Context) error {
	var jobs []domain.Job
	if e := b.Store.View(ctx, func(tx ports.Tx) error { var e error; jobs, e = tx.AllJobs(ctx, nil); return e }); e != nil {
		return e
	}
	for _, j := range jobs {
		if j.Kind != "BACKUP" || j.State != "BUILDING" && (j.State != "READY" || b.Target == "") {
			continue
		}
		operation := domain.ID(strings.TrimPrefix(j.OperationKey, "system:backup:"))
		if !domain.ValidID(string(operation), false) {
			return domain.Fail("storage_corrupt")
		}
		if _, e := b.Backup(ctx, operation); e != nil {
			return e
		}
	}
	return nil
}
func (b *BackupService) Rotate(ctx context.Context) error {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.Target == "" {
		return nil
	}
	keep := b.Keep
	if keep < 2 {
		keep = 48
	}
	type candidate struct {
		job    domain.Job
		record domain.BackupRecord
	}
	list := []candidate{}
	cancelled := []domain.Job{}
	if e := b.Store.View(ctx, func(tx ports.Tx) error {
		jobs, e := tx.AllJobs(ctx, nil)
		if e != nil {
			return e
		}
		for _, j := range jobs {
			if j.Kind != "BACKUP" {
				continue
			}
			if j.State == "CANCELLED" {
				cancelled = append(cancelled, j)
				continue
			}
			if j.State != "COMMITTED" {
				continue
			}
			r, e := tx.BackupRecord(ctx, j.ID)
			if e != nil {
				return e
			}
			if r == nil || r.VerifiedMS == nil {
				continue
			}
			list = append(list, candidate{j, *r})
		}
		return nil
	}); e != nil {
		return e
	}
	sort.Slice(list, func(i, j int) bool {
		if list[i].record.SnapshotMS != list[j].record.SnapshotMS {
			return list[i].record.SnapshotMS > list[j].record.SnapshotMS
		}
		return list[i].job.ID > list[j].job.ID
	})
	if len(list) < keep {
		return nil
	}
	// Verify retained recovery points in both locations before retiring anything.
	for _, c := range list[:keep] {
		if _, e := b.Vault.Verify(ctx, c.job.ID); e != nil {
			return e
		}
		if _, e := b.Vault.CopyVerified(ctx, c.job.ID, b.Target); e != nil {
			return e
		}
	}
	for _, c := range list[keep:] {
		_, e := b.Store.Update(ctx, func(tx ports.Tx) error {
			j, e := tx.Job(ctx, c.job.ID)
			if e != nil {
				return e
			}
			if j == nil || j.State != "COMMITTED" {
				return domain.Fail("job_state_conflict")
			}
			j.State = "CANCELLED"
			j.UpdatedMS = b.Clock.Now().UnixMilli()
			return tx.UpdateJob(ctx, *j, "COMMITTED", j.Attempt)
		})
		if e != nil {
			return e
		}
		cancelled = append(cancelled, c.job)
	}
	for _, j := range cancelled {
		if e := b.Vault.Remove(j.ID); e != nil {
			return e
		}
		if e := RemoveTargetArchive(b.Target, j.ID); e != nil {
			return e
		}
	}
	return nil
}
func (b *BackupService) Stop() {
	if b.cancel != nil {
		b.cancel()
	}
	b.wg.Wait()
}
func ArchiveID(path string) (domain.ID, error) {
	name := filepath.Base(path)
	if filepath.Ext(name) != ".dmb" {
		return "", domain.Invalid("/backup_archive")
	}
	id := domain.ID(name[:len(name)-4])
	if !domain.ValidID(string(id), false) {
		return "", domain.Invalid("/backup_archive")
	}
	return id, nil
}
