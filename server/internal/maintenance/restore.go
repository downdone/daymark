package maintenance

import (
	"context"
	"io"
	"os"
	"path/filepath"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/storage"
)

type RestoreReport struct {
	Target                 string
	SnapshotMS             int64
	NewAuthEpoch           domain.ID
	NewEpochs              map[domain.ID]domain.ID
	ReplayedDeletions      int
	ReadyForExplicitSwitch bool
}

// Restore only creates a NEW directory. Caller must hold the stopped source's
// DirectoryLease; cutover is deliberately a separate operator action.
func Restore(ctx context.Context, vault *Vault, archiveID domain.ID, target string, ledger ports.Ledger, ledgerKey, backupKey []byte) (RestoreReport, error) {
	var report RestoreReport
	if len(ledgerKey) != 32 || len(backupKey) != 32 {
		return report, domain.Invalid("/restore_keys")
	}
	events, e := ledger.Read(ctx)
	if e != nil {
		return report, e
	}
	if _, e = os.Lstat(target); !os.IsNotExist(e) {
		return report, domain.Invalid("/new_restore_directory")
	}
	parent := filepath.Dir(target)
	scratch, e := os.MkdirTemp(parent, ".daymark-restore-")
	if e != nil {
		return report, e
	}
	defer func() { _ = os.Remove(filepath.Join(scratch, "verified.sqlite")); _ = os.Remove(scratch) }()
	verified := filepath.Join(scratch, "verified.sqlite")
	archive, e := vault.Extract(ctx, archiveID, verified)
	if e != nil {
		return report, e
	}
	info, e := storage.InspectFile(ctx, verified)
	if e != nil {
		return report, e
	}
	if info.State.LedgerID == nil || *info.State.LedgerID != ledger.ID() {
		return report, domain.Fail("storage_corrupt")
	}
	if e = os.Mkdir(target, 0700); e != nil {
		return report, e
	}
	if e = copyExclusive(verified, filepath.Join(target, "daymark.sqlite")); e != nil {
		return report, e
	}
	clone, e := OpenLedger(filepath.Join(target, "deletion-ledger"), ledgerKey, info.State.LedgerID, true)
	if e != nil {
		return report, e
	}
	for _, event := range events {
		if _, e = clone.Append(ctx, event); e != nil {
			clone.Close()
			return report, e
		}
	}
	clone.Close()
	if e = writeExclusive(filepath.Join(target, "ledger.key"), ledgerKey); e != nil {
		return report, e
	}
	if e = writeExclusive(filepath.Join(target, "backup.key"), backupKey); e != nil {
		return report, e
	}
	db, e := storage.Open(ctx, target, storage.DefaultOptions())
	if e != nil {
		return report, e
	}
	defer db.Close()
	auth, e := domain.NewID()
	if e != nil {
		return report, e
	}
	epochs := map[domain.ID]domain.ID{}
	var datasets []domain.Dataset
	if e = db.View(ctx, func(tx ports.Tx) error { var e error; datasets, e = tx.AllDatasets(ctx); return e }); e != nil {
		return report, e
	}
	for _, d := range datasets {
		id, e := domain.NewID()
		if e != nil {
			return report, e
		}
		epochs[d.ID] = id
	}
	_, e = db.Update(ctx, func(tx ports.Tx) error {
		for _, event := range events {
			if e := tx.ReplayDeletion(ctx, event); e != nil {
				return e
			}
		}
		return tx.RestoreEpochs(ctx, auth, epochs, archive.Header.SnapshotMS)
	})
	if e != nil {
		return report, e
	}
	if _, e = db.InspectBackup(ctx, filepath.Join(target, "daymark.sqlite")); e != nil {
		return report, e
	}
	if e = db.View(ctx, func(tx ports.Tx) error {
		living, e := tx.AllDatasets(ctx)
		if e != nil {
			return e
		}
		keep := map[domain.ID]bool{}
		for _, d := range living {
			keep[d.ID] = true
		}
		for id := range epochs {
			if !keep[id] {
				delete(epochs, id)
			}
		}
		return nil
	}); e != nil {
		return report, e
	}
	report = RestoreReport{Target: target, SnapshotMS: archive.Header.SnapshotMS, NewAuthEpoch: auth, NewEpochs: epochs, ReplayedDeletions: len(events), ReadyForExplicitSwitch: true}
	return report, nil
}
func writeExclusive(path string, data []byte) error {
	f, e := os.OpenFile(path, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return e
	}
	defer f.Close()
	if _, e = f.Write(data); e != nil {
		return e
	}
	return f.Sync()
}
func copyExclusive(source, target string) error {
	in, e := os.Open(source)
	if e != nil {
		return e
	}
	defer in.Close()
	out, e := os.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if e != nil {
		return e
	}
	defer out.Close()
	if _, e = io.Copy(out, in); e != nil {
		return e
	}
	return out.Sync()
}
