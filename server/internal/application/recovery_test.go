package application

import (
	"bytes"
	"os"
	"path/filepath"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/maintenance"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/storage"
)

func TestActualEncryptedBackupRestoreEpochAndDeletionLedger(t *testing.T) {
	f, x := transferFixture(t)
	ledgerKey := bytes.Repeat([]byte{21}, 32)
	backupKey := bytes.Repeat([]byte{22}, 32)
	ledger, e := maintenance.OpenLedger(t.TempDir(), ledgerKey, nil, true)
	if e != nil {
		t.Fatal(e)
	}
	defer ledger.Close()
	account, e := NewAccount(t.Context(), f.db, f.auth, f.clock, ledger, x.Files)
	if e != nil {
		t.Fatal(e)
	}
	id := domain.StableID("disaster-task")
	if _, e = f.sync.Push(t.Context(), f.scope, f.mutation(t, createCommand(id, "protected synthetic data"))); e != nil {
		t.Fatal(e)
	}
	vault, e := maintenance.OpenVault(t.TempDir(), backupKey)
	if e != nil {
		t.Fatal(e)
	}
	defer vault.Close()
	backups := maintenance.BackupService{Store: f.db, Clock: f.clock, Vault: vault, ScratchDir: t.TempDir(), Target: t.TempDir()}
	record, e := backups.Backup(t.Context(), domain.StableID("recovery-backup"))
	if e != nil || record.VerifiedMS == nil {
		t.Fatal(record, e)
	}
	same, e := backups.Backup(t.Context(), domain.StableID("recovery-backup"))
	if e != nil || same.JobID != record.JobID || same.Hash != record.Hash {
		t.Fatal("backup replay", e)
	}
	// The user is deleted AFTER the backed-up database snapshot.
	op := domain.StableID("post-backup-deletion")
	intent, e := account.Prepare(t.Context(), f.scope.Principal, []byte("synthetic-password"), op)
	if e != nil {
		t.Fatal(e)
	}
	if _, e = account.Confirm(t.Context(), intent.Token, op); e != nil {
		t.Fatal(e)
	}
	if ran, e := account.RunOne(t.Context()); e != nil || !ran {
		t.Fatal(ran, e)
	}
	target := filepath.Join(t.TempDir(), "restored")
	report, e := maintenance.Restore(t.Context(), vault, record.JobID, target, ledger, ledgerKey, backupKey)
	if e != nil || !report.ReadyForExplicitSwitch || report.ReplayedDeletions != 1 {
		t.Fatal(report, e)
	}
	restored, e := storage.Open(t.Context(), target, storage.DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	defer restored.Close()
	e = restored.View(t.Context(), func(tx ports.Tx) error {
		u, e := tx.User(t.Context(), f.scope.Principal.UserID)
		if e != nil {
			return e
		}
		if u == nil || u.Status != "DELETED" || u.PasswordHash != "" {
			t.Fatal("restored deleted account revived")
		}
		state, e := tx.State(t.Context())
		if e == nil && state.AuthEpoch == f.scope.Principal.AuthEpoch {
			t.Fatal("auth epoch not rotated")
		}
		datasets, e := tx.Datasets(t.Context(), f.scope.Principal.UserID)
		if len(datasets) != 0 {
			t.Fatal("restored private data")
		}
		return e
	})
	if e != nil {
		t.Fatal(e)
	}
	// Restoring to an existing directory must leave its original bytes untouched.
	marker := filepath.Join(target, "existing-marker")
	if e = os.WriteFile(marker, []byte("preserve"), 0600); e != nil {
		t.Fatal(e)
	}
	if _, e = maintenance.Restore(t.Context(), vault, record.JobID, target, ledger, ledgerKey, backupKey); e == nil {
		t.Fatal("overwrote existing target")
	}
	data, e := os.ReadFile(marker)
	if e != nil || string(data) != "preserve" {
		t.Fatal("existing target changed")
	}
}
func TestBackupResumeAndRotationRequireVerifiedRecoveryPoints(t *testing.T) {
	f, x := transferFixture(t)
	ledger, e := maintenance.OpenLedger(t.TempDir(), bytes.Repeat([]byte{31}, 32), nil, true)
	if e != nil {
		t.Fatal(e)
	}
	defer ledger.Close()
	if _, e = NewAccount(t.Context(), f.db, f.auth, f.clock, ledger, x.Files); e != nil {
		t.Fatal(e)
	}
	vault, e := maintenance.OpenVault(t.TempDir(), bytes.Repeat([]byte{32}, 32))
	if e != nil {
		t.Fatal(e)
	}
	defer vault.Close()
	backups := maintenance.BackupService{Store: f.db, Clock: f.clock, Vault: vault, ScratchDir: t.TempDir(), Keep: 2}
	operation := domain.StableID("unverified-backup")
	old, e := backups.Backup(t.Context(), operation)
	if e != nil || old.VerifiedMS != nil {
		t.Fatal(old, e)
	}
	if e = backups.Rotate(t.Context()); e != nil {
		t.Fatal(e)
	}
	oldPath, _ := vault.Path(old.JobID)
	if _, e = os.Stat(oldPath); e != nil {
		t.Fatal("unverified only recovery point removed")
	}
	backups.Target = t.TempDir()
	if e = backups.Resume(t.Context()); e != nil {
		t.Fatal(e)
	}
	same, e := backups.Backup(t.Context(), operation)
	if e != nil || same.JobID != old.JobID || same.VerifiedMS == nil {
		t.Fatal("READY did not resume to verified COMMITTED", same, e)
	}
	records := []domain.BackupRecord{same}
	for i := 0; i < 3; i++ {
		f.clock.value = f.clock.value.Add(time.Hour)
		record, e := backups.Backup(t.Context(), domain.StableID("retention-"+string(rune('a'+i))))
		if e != nil {
			t.Fatal(e)
		}
		records = append(records, record)
	}
	latest := records[len(records)-1]
	latestPath, _ := vault.Path(latest.JobID)
	targetPath := filepath.Join(backups.Target, filepath.Base(latestPath))
	if e = os.WriteFile(targetPath, []byte("synthetic corruption"), 0600); e != nil {
		t.Fatal(e)
	}
	if e = backups.Rotate(t.Context()); e == nil {
		t.Fatal("rotation ignored corrupt retained target")
	}
	if _, e = os.Stat(oldPath); e != nil {
		t.Fatal("old backup pruned before replacement verification")
	}
	if e = os.Remove(targetPath); e != nil {
		t.Fatal(e)
	}
	if e = backups.Rotate(t.Context()); e != nil {
		t.Fatal(e)
	}
	if _, e = os.Stat(oldPath); !os.IsNotExist(e) {
		t.Fatal("old verified archive was not retired", e)
	}
	for _, r := range records[len(records)-2:] {
		path, _ := vault.Path(r.JobID)
		if _, e = os.Stat(path); e != nil {
			t.Fatal("retained recovery point missing", e)
		}
	}
}
