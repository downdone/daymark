package main

import (
	"context"
	"crypto/tls"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/artifact"
	"daymark.local/server/internal/config"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/httpapi"
	"daymark.local/server/internal/maintenance"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"daymark.local/server/internal/storage"
)

func main() {
	if e := run(); e != nil {
		log.Printf("daymark-server failed: %s", domain.Code(e))
		os.Exit(1)
	}
}
func run() error {
	flags := flag.NewFlagSet("daymark-server", flag.ContinueOnError)
	configPath := flags.String("config", "", "JSON configuration path")
	data := flags.String("data-dir", "", "protected data directory (required without config)")
	listen := flags.String("listen", "", "listen address")
	origin := flags.String("origin", "", "public HTTPS origin")
	dev := flags.Bool("dev", false, "allow loopback HTTP development")
	addUser := flags.String("create-user", "", "create account; password is read from stdin, never argv")
	resetPassword := flags.String("reset-password", "", "reset invited account password; new password from stdin; revokes sessions")
	backupNow := flags.Bool("backup-now", false, "create and verify a protected backup, then exit")
	restoreArchive := flags.String("restore", "", "encrypted archive to restore (source must be stopped)")
	restoreTarget := flags.String("restore-to", "", "new, nonexistent restore directory; never overwrites old data")
	restoreBackupKey := flags.String("restore-key-file", "", "escrowed 32-byte backup key file")
	restoreLedgerDir := flags.String("restore-ledger-dir", "", "independent current deletion ledger directory")
	restoreLedgerKey := flags.String("restore-ledger-key-file", "", "escrowed 32-byte deletion ledger key file")
	if e := flags.Parse(os.Args[1:]); e != nil {
		return e
	}
	commands := 0
	for _, enabled := range []bool{*addUser != "", *resetPassword != "", *backupNow, *restoreArchive != ""} {
		if enabled {
			commands++
		}
	}
	if commands > 1 {
		return domain.Invalid("/maintenance_command")
	}
	c, e := config.Load(*configPath)
	if e != nil {
		return e
	}
	if *data != "" {
		c.DataDir = *data
	}
	if *listen != "" {
		c.Listen = *listen
	}
	if *origin != "" {
		c.PublicOrigin = *origin
	}
	if *dev {
		c.Development = true
	}
	if e = c.Validate(); e != nil {
		return e
	}
	if *restoreArchive != "" {
		if *restoreTarget == "" || *restoreBackupKey == "" || *restoreLedgerDir == "" || *restoreLedgerKey == "" {
			return domain.Invalid("/restore_options")
		}
		lease, e := storage.AcquireDirectory(c.DataDir)
		if e != nil {
			return e
		}
		defer lease.Close()
		key, e := os.ReadFile(*restoreBackupKey)
		if e != nil {
			return e
		}
		ledgerKey, e := os.ReadFile(*restoreLedgerKey)
		if e != nil {
			return e
		}
		if len(key) != 32 || len(ledgerKey) != 32 {
			return domain.Invalid("/restore_keys")
		}
		ledger, e := maintenance.OpenLedger(*restoreLedgerDir, ledgerKey, nil, false)
		if e != nil {
			return e
		}
		defer ledger.Close()
		id, e := maintenance.ArchiveID(*restoreArchive)
		if e != nil {
			return e
		}
		vault, e := maintenance.OpenVault(filepath.Dir(*restoreArchive), key)
		if e != nil {
			return e
		}
		defer vault.Close()
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
		defer cancel()
		report, e := maintenance.Restore(ctx, vault, id, *restoreTarget, ledger, ledgerKey, key)
		if e != nil {
			return e
		}
		_, e = fmt.Fprintf(os.Stdout, "Restore verified; ready for explicit switch. Deletions replayed: %d\n", report.ReplayedDeletions)
		return e
	}
	db, e := storage.Open(context.Background(), c.DataDir, storage.Options{Queue: c.WriterQueue, Readers: c.ReadPool, Busy: time.Duration(c.BusyMS) * time.Millisecond})
	if e != nil {
		return e
	}
	defer db.Close()
	clock := ports.SystemClock{}
	auth, e := application.NewAuth(context.Background(), db, clock, application.NewArgonHasher(c.KDFParallelism))
	if e != nil {
		return e
	}
	if *addUser != "" || *resetPassword != "" {
		password, e := io.ReadAll(io.LimitReader(os.Stdin, 1026))
		if e != nil {
			return e
		}
		if len(password) > 1025 {
			return domain.Invalid("/password")
		}
		password = []byte(strings.TrimSuffix(strings.TrimSuffix(string(password), "\n"), "\r"))
		if *resetPassword != "" {
			e := auth.ResetPasswordAdmin(context.Background(), *resetPassword, password)
			clear(password)
			if e != nil {
				return e
			}
			_, e = fmt.Fprintln(os.Stdout, "Password reset; existing sessions revoked.")
			return e
		}
		id, e := auth.CreateUser(context.Background(), *addUser, password)
		clear(password)
		if e != nil {
			return e
		}
		_, e = fmt.Fprintln(os.Stdout, "Created account", id)
		return e
	}
	key, e := config.KeyFile(c.DataDir, "cursor.key")
	if e != nil {
		return e
	}
	var state domain.State
	e = db.View(context.Background(), func(tx ports.Tx) error { var e error; state, e = tx.State(context.Background()); return e })
	if e != nil {
		return e
	}
	codec, e := protocol.NewCodec()
	if e != nil {
		return e
	}
	keyID := maintenance.KeyID(key)
	cursor := &application.CursorCodec{ServerID: state.ServerID, ActiveKey: keyID, Keys: map[string][]byte{keyID: key}, Clock: clock, TTL: 90 * 24 * time.Hour}
	syncer := application.NewSync(db, auth, clock, cursor)
	files, e := artifact.Open(filepath.Join(c.DataDir, "artifacts"))
	if e != nil {
		return e
	}
	defer files.Close()
	transfer := application.NewTransfer(db, auth, syncer, codec, files, clock, c.MaxArtifactBytes)
	transfer.OnError = func(e error) { log.Printf("transfer maintenance: %s", domain.Code(e)) }
	ledgerKey, e := config.SecretFile(c.DataDir, "ledger.key", state.LedgerID == nil)
	if e != nil {
		return e
	}
	backupKey, e := config.SecretFile(c.DataDir, "backup.key", state.LedgerID == nil)
	if e != nil {
		return e
	}
	ledger, e := maintenance.OpenLedger(filepath.Join(c.DataDir, "deletion-ledger"), ledgerKey, state.LedgerID, state.LedgerID == nil)
	if e != nil {
		return e
	}
	defer ledger.Close()
	var durableLedger ports.Ledger = ledger
	if c.BackupTarget != "" {
		id := ledger.ID()
		replica, e := maintenance.OpenLedger(filepath.Join(c.BackupTarget, "deletion-ledger"), ledgerKey, &id, true)
		if e != nil {
			return e
		}
		defer replica.Close()
		durableLedger = &maintenance.ReplicatedLedger{Primary: ledger, Replica: replica}
	}
	account, e := application.NewAccount(context.Background(), db, auth, clock, durableLedger, files)
	if e != nil {
		return e
	}
	account.OnError = func(e error) { log.Printf("account maintenance: %s", domain.Code(e)) }
	vault, e := maintenance.OpenVault(filepath.Join(c.DataDir, "backups"), backupKey)
	if e != nil {
		return e
	}
	defer vault.Close()
	backups := &maintenance.BackupService{Store: db, Clock: clock, Vault: vault, ScratchDir: filepath.Join(c.DataDir, "staging"), Target: c.BackupTarget, Keep: c.BackupKeep, OnError: func(e error) { log.Printf("backup maintenance: %s", domain.Code(e)) }}
	if *backupNow {
		operation, e := domain.NewID()
		if e != nil {
			return e
		}
		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
		defer cancel()
		result, e := backups.Backup(ctx, operation)
		if e != nil {
			return e
		}
		_, e = fmt.Fprintf(os.Stdout, "Backup verified: %s; protected target verified: %t\n", result.JobID, result.VerifiedMS != nil)
		return e
	}
	syncer.Exporter = transfer
	syncer.Resolver = transfer
	transfer.Start(context.Background())
	defer transfer.Stop()
	account.Start(context.Background())
	defer account.Stop()
	backups.Start(context.Background())
	defer backups.Stop()
	handler := httpapi.New(auth, syncer, codec)
	handler.RegistrationEnabled = c.RegistrationEnabled
	handler.AttachTransfer(transfer)
	handler.Account = account
	handler.Timeout = c.RequestTimeout()
	server := &http.Server{Addr: c.Listen, Handler: handler.Router(), ReadHeaderTimeout: 5 * time.Second, ReadTimeout: c.RequestTimeout(), WriteTimeout: 15 * time.Second, IdleTimeout: 60 * time.Second, MaxHeaderBytes: 16384, TLSConfig: &tls.Config{MinVersion: tls.VersionTLS12}}
	stopped := make(chan error, 1)
	go func() {
		if c.TLSCert != "" {
			stopped <- server.ListenAndServeTLS(c.TLSCert, c.TLSKey)
		} else {
			stopped <- server.ListenAndServe()
		}
	}()
	signals, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer cancel()
	select {
	case e = <-stopped:
		if errors.Is(e, http.ErrServerClosed) {
			return nil
		}
		return e
	case <-signals.Done():
		ctx, release := context.WithTimeout(context.Background(), 15*time.Second)
		defer release()
		if e = server.Shutdown(ctx); e != nil {
			return e
		}
		return nil
	}
}
