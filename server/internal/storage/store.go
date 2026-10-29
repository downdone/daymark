package storage

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"embed"
	"errors"
	"fmt"
	"net/url"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"modernc.org/sqlite"
)

//go:embed migrations/*.sql
var migrations embed.FS

type DB struct {
	writer, reader *sql.DB
	lock           *os.File
	gate           chan struct{}
	queue          chan struct{}
	closed         atomic.Bool
	poison         atomic.Bool
	once           sync.Once
	path           string
	busy           time.Duration
}
type Options struct {
	Queue, Readers    int
	Busy              time.Duration
	MigrationOverride map[string][]byte
}

func DefaultOptions() Options { return Options{Queue: 32, Readers: 4, Busy: 2 * time.Second} }
func Open(ctx context.Context, dir string, o Options) (out *DB, err error) {
	if o.Queue < 1 || o.Readers < 1 || o.Busy <= 0 {
		return nil, domain.Invalid("/storage_options")
	}
	abs, e := filepath.Abs(dir)
	if e != nil {
		return nil, e
	}
	if e = os.MkdirAll(abs, 0700); e != nil {
		return nil, domain.Wrap("storage_io", e)
	}
	f, e := os.OpenFile(filepath.Join(abs, "instance.lock"), os.O_CREATE|os.O_RDWR, 0600)
	if e != nil {
		return nil, domain.Wrap("storage_io", e)
	}
	if e = lockFile(f); e != nil {
		f.Close()
		return nil, domain.Wrap("already_running", e)
	}
	d := &DB{lock: f, gate: make(chan struct{}, 1), queue: make(chan struct{}, o.Queue), path: filepath.Join(abs, "daymark.sqlite"), busy: o.Busy}
	d.gate <- struct{}{}
	defer func() {
		if err != nil {
			d.Close()
		}
	}()
	u := databaseURL(d.path)
	q := url.Values{}
	q.Set("_txlock", "immediate")
	for _, v := range []string{"foreign_keys(1)", "journal_mode(WAL)", "synchronous(FULL)", fmt.Sprintf("busy_timeout(%d)", o.Busy.Milliseconds())} {
		q.Add("_pragma", v)
	}
	u.RawQuery = q.Encode()
	d.writer, e = sql.Open("sqlite", u.String())
	if e != nil {
		return nil, d.classify(e)
	}
	d.writer.SetMaxOpenConns(1)
	d.writer.SetMaxIdleConns(1)
	if e = d.writer.PingContext(ctx); e != nil {
		return nil, d.classify(e)
	}
	var version string
	if e = d.writer.QueryRowContext(ctx, "SELECT sqlite_version()").Scan(&version); e != nil {
		return nil, e
	}
	if !safeEngine(version) {
		return nil, domain.Fail("unsupported_sqlite_engine")
	}
	if e = d.migrate(ctx, o.MigrationOverride); e != nil {
		return nil, e
	}
	q.Del("_pragma")
	q.Set("_txlock", "deferred")
	for _, v := range []string{"foreign_keys(1)", "synchronous(FULL)", "query_only(1)", fmt.Sprintf("busy_timeout(%d)", o.Busy.Milliseconds())} {
		q.Add("_pragma", v)
	}
	u.RawQuery = q.Encode()
	d.reader, e = sql.Open("sqlite", u.String())
	if e != nil {
		return nil, d.classify(e)
	}
	d.reader.SetMaxOpenConns(o.Readers)
	d.reader.SetMaxIdleConns(o.Readers)
	if e = d.reader.PingContext(ctx); e != nil {
		return nil, d.classify(e)
	}
	if e = d.check(ctx, d.reader); e != nil {
		return nil, e
	}
	return d, nil
}
func safeEngine(v string) bool {
	var a, b, c int
	if _, e := fmt.Sscanf(v, "%d.%d.%d", &a, &b, &c); e != nil {
		return false
	}
	return a > 3 || a == 3 && (b > 51 || b == 51 && c >= 3 || b == 50 && c >= 7 || b == 44 && c >= 6)
}
func (d *DB) migrate(ctx context.Context, override map[string][]byte) error {
	files, e := migrations.ReadDir("migrations")
	if e != nil {
		return e
	}
	sort.Slice(files, func(i, j int) bool { return files[i].Name() < files[j].Name() })
	var exists int
	if e = d.writer.QueryRowContext(ctx, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='schema_migrations'").Scan(&exists); e != nil {
		return e
	}
	version := 0
	if exists == 1 {
		rows, e := d.writer.QueryContext(ctx, "SELECT version,checksum FROM schema_migrations ORDER BY version")
		if e != nil {
			return e
		}
		for rows.Next() {
			var v int
			var old string
			if e = rows.Scan(&v, &old); e != nil {
				rows.Close()
				return e
			}
			if v != version+1 || v > len(files) {
				rows.Close()
				return domain.Fail("migration_failed")
			}
			b, e := migrations.ReadFile("migrations/" + files[v-1].Name())
			if x, ok := override[files[v-1].Name()]; ok {
				b = x
			}
			h := sha256.Sum256(b)
			if e != nil || old != fmt.Sprintf("%x", h) {
				rows.Close()
				return domain.Fail("migration_failed")
			}
			version = v
		}
		e = rows.Err()
		rows.Close()
		if e != nil {
			return e
		}
	} else {
		var count int
		if e = d.writer.QueryRowContext(ctx, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'").Scan(&count); e != nil {
			return e
		}
		if count != 0 {
			return domain.Fail("migration_failed")
		}
	}
	if version == len(files) {
		return nil
	}
	if version > 0 {
		backup := filepath.Join(filepath.Dir(d.path), fmt.Sprintf("pre-migration-%d.sqlite", time.Now().UTC().UnixNano()))
		if e = d.snapshot(ctx, backup); e != nil {
			return e
		}
	}
	tx, e := d.writer.BeginTx(ctx, nil)
	if e != nil {
		return d.classify(e)
	}
	defer tx.Rollback()
	for i := version; i < len(files); i++ {
		b, e := migrations.ReadFile("migrations/" + files[i].Name())
		if x, ok := override[files[i].Name()]; ok {
			b = x
		}
		if e != nil {
			return e
		}
		if _, e = tx.ExecContext(ctx, string(b)); e != nil {
			return domain.Wrap("migration_failed", e)
		}
		h := sha256.Sum256(b)
		if _, e = tx.ExecContext(ctx, "INSERT INTO schema_migrations(version,checksum,applied_ms) VALUES(?,?,?)", i+1, fmt.Sprintf("%x", h), time.Now().UTC().UnixMilli()); e != nil {
			return e
		}
	}
	var states int
	if e = tx.QueryRowContext(ctx, "SELECT count(*) FROM service_state").Scan(&states); e != nil {
		return e
	}
	if states == 0 {
		s, e := domain.NewID()
		if e != nil {
			return e
		}
		a, e := domain.NewID()
		if e != nil {
			return e
		}
		if _, e = tx.ExecContext(ctx, "INSERT INTO service_state(singleton,server_instance_id,auth_epoch,schema_version) VALUES(1,?,?,?)", s, a, len(files)); e != nil {
			return e
		}
	}
	if _, e = tx.ExecContext(ctx, "UPDATE service_state SET schema_version=? WHERE singleton=1", len(files)); e != nil {
		return e
	}
	if e = tx.Commit(); e != nil {
		return domain.Wrap("migration_failed", e)
	}
	return nil
}
func (d *DB) classify(e error) error {
	if e == nil {
		return nil
	}
	if errors.Is(e, context.Canceled) {
		return domain.Wrap("cancelled", e)
	}
	if errors.Is(e, context.DeadlineExceeded) {
		return domain.Wrap("timeout", e)
	}
	var se *sqlite.Error
	if errors.As(e, &se) {
		switch se.Code() & 255 {
		case 5, 6:
			return domain.Wrap("busy", e)
		case 13:
			d.poison.Store(true)
			return domain.Wrap("storage_full", e)
		case 10:
			d.poison.Store(true)
			return domain.Wrap("storage_io", e)
		case 11, 26:
			d.poison.Store(true)
			return domain.Wrap("storage_corrupt", e)
		}
	}
	var f *domain.Fault
	if errors.As(e, &f) {
		if f.Code == "storage_corrupt" || f.Code == "storage_io" || f.Code == "storage_full" {
			d.poison.Store(true)
		}
		return e
	}
	return domain.Wrap("internal_error", e)
}
func (d *DB) Ready() error {
	if d.closed.Load() || d.poison.Load() {
		return domain.Fail("temporarily_unavailable")
	}
	return nil
}
func (d *DB) View(ctx context.Context, fn func(ports.Tx) error) error {
	if e := d.Ready(); e != nil {
		return e
	}
	tx, e := d.reader.BeginTx(ctx, &sql.TxOptions{ReadOnly: true})
	if e != nil {
		return d.classify(e)
	}
	defer tx.Rollback()
	if e = fn(&transaction{tx: tx}); e != nil {
		return d.classify(e)
	}
	return d.classify(tx.Commit())
}
func (d *DB) enter(ctx context.Context) error {
	if e := d.Ready(); e != nil {
		return e
	}
	select {
	case d.queue <- struct{}{}:
	default:
		return domain.Fail("busy")
	}
	select {
	case <-ctx.Done():
		<-d.queue
		return d.classify(ctx.Err())
	case <-d.gate:
		if e := ctx.Err(); e != nil {
			d.leave()
			return d.classify(e)
		}
		return nil
	}
}
func (d *DB) leave() { d.gate <- struct{}{}; <-d.queue }
func (d *DB) Update(ctx context.Context, fn func(ports.Tx) error) (ports.CommitState, error) {
	if e := d.enter(ctx); e != nil {
		return ports.NotCommitted, e
	}
	defer d.leave()
	tx, e := d.writer.BeginTx(ctx, nil)
	if e != nil {
		return ports.NotCommitted, d.classify(e)
	}
	defer tx.Rollback()
	if e = fn(&transaction{tx: tx}); e != nil {
		return ports.NotCommitted, d.classify(e)
	}
	if e = tx.Commit(); e != nil {
		return ports.CommitUnknown, d.classify(e)
	}
	return ports.Committed, nil
}
func (d *DB) check(ctx context.Context, db *sql.DB) error {
	var state string
	if e := db.QueryRowContext(ctx, "PRAGMA integrity_check").Scan(&state); e != nil || state != "ok" {
		return domain.Fail("storage_corrupt")
	}
	rows, e := db.QueryContext(ctx, "PRAGMA foreign_key_check")
	if e != nil {
		return d.classify(e)
	}
	defer rows.Close()
	if rows.Next() {
		return domain.Fail("storage_corrupt")
	}
	return rows.Err()
}
func (d *DB) snapshot(ctx context.Context, target string) error {
	if _, e := os.Stat(target); e == nil {
		return domain.Invalid("/backup_target")
	}
	if _, e := d.writer.ExecContext(ctx, "VACUUM INTO ?", target); e != nil {
		return d.classify(e)
	}
	u := databaseURL(target)
	db, e := sql.Open("sqlite", u.String()+"?mode=ro")
	if e != nil {
		return e
	}
	defer db.Close()
	return d.check(ctx, db)
}
func (d *DB) Snapshot(ctx context.Context, target string) error {
	if e := d.enter(ctx); e != nil {
		return e
	}
	defer d.leave()
	return d.snapshot(ctx, target)
}
func (d *DB) Close() error {
	var result error
	d.once.Do(func() {
		d.closed.Store(true)
		if d.reader != nil {
			result = errors.Join(result, d.reader.Close())
		}
		if d.writer != nil {
			result = errors.Join(result, d.writer.Close())
		}
		if d.lock != nil {
			result = errors.Join(result, unlockFile(d.lock), d.lock.Close())
		}
	})
	return result
}

type transaction struct{ tx *sql.Tx }

func databaseURL(path string) url.URL {
	p := filepath.ToSlash(path)
	if !strings.HasPrefix(p, "/") {
		p = "/" + p
	}
	return url.URL{Scheme: "file", Path: p}
}

type row map[string]any

func (r row) s(k string) string {
	v := r[k]
	if v == nil {
		return ""
	}
	switch v := v.(type) {
	case string:
		return v
	case []byte:
		return string(v)
	default:
		return fmt.Sprint(v)
	}
}
func (r row) n(k string) int64 { v, _ := r[k].(int64); return v }
func (r row) b(k string) bool  { return r.n(k) != 0 }
func (r row) id(k string) *domain.ID {
	if r[k] == nil {
		return nil
	}
	return domain.Ptr(domain.ID(r.s(k)))
}
func (r row) instant(k string) domain.Instant { return domain.FromMillis(r.n(k)) }
func (r row) instantPtr(k string) *domain.Instant {
	if r[k] == nil {
		return nil
	}
	return domain.Ptr(r.instant(k))
}
func (r row) intPtr(k string) *int64 {
	if r[k] == nil {
		return nil
	}
	return domain.Ptr(r.n(k))
}
func (r row) text(k string) *string {
	if r[k] == nil {
		return nil
	}
	return domain.Ptr(r.s(k))
}
func (r row) date(k string) *domain.Date {
	if r[k] == nil {
		return nil
	}
	return domain.Ptr(domain.Date(r.s(k)))
}
func (r row) hash(k string) [32]byte {
	var h [32]byte
	if b, ok := r[k].([]byte); ok {
		copy(h[:], b)
	}
	return h
}
func (t *transaction) rows(ctx context.Context, query string, args ...any) ([]row, error) {
	rows, e := t.tx.QueryContext(ctx, query, args...)
	if e != nil {
		return nil, e
	}
	defer rows.Close()
	columns, e := rows.Columns()
	if e != nil {
		return nil, e
	}
	out := []row{}
	for rows.Next() {
		values := make([]any, len(columns))
		ptr := make([]any, len(columns))
		for i := range values {
			ptr[i] = &values[i]
		}
		if e = rows.Scan(ptr...); e != nil {
			return nil, e
		}
		r := row{}
		for i, k := range columns {
			if b, ok := values[i].([]byte); ok {
				values[i] = append([]byte(nil), b...)
			}
			r[k] = values[i]
		}
		out = append(out, r)
	}
	return out, rows.Err()
}
func (t *transaction) one(ctx context.Context, q string, args ...any) (row, error) {
	rows, e := t.rows(ctx, q, args...)
	if e != nil {
		return nil, e
	}
	if len(rows) == 0 {
		return nil, nil
	}
	if len(rows) != 1 {
		return nil, domain.Fail("storage_corrupt")
	}
	return rows[0], nil
}
func (t *transaction) exec(ctx context.Context, q string, args ...any) error {
	_, e := t.tx.ExecContext(ctx, q, args...)
	return e
}
func (t *transaction) upsert(ctx context.Context, table string, fields map[string]any, keys []string) error {
	names := make([]string, 0, len(fields))
	for n := range fields {
		names = append(names, n)
	}
	sort.Strings(names)
	args := []any{}
	qs, sets := []string{}, []string{}
	for _, n := range names {
		args = append(args, fields[n])
		qs = append(qs, "?")
		key := false
		for _, k := range keys {
			key = key || k == n
		}
		if !key {
			sets = append(sets, n+"=excluded."+n)
		}
	}
	return t.exec(ctx, "INSERT INTO "+table+"("+strings.Join(names, ",")+") VALUES("+strings.Join(qs, ",")+") ON CONFLICT("+strings.Join(keys, ",")+") DO UPDATE SET "+strings.Join(sets, ","), args...)
}
func nullableString[T ~string](v *T) any {
	if v == nil {
		return nil
	}
	return string(*v)
}
func milliseconds(v domain.Instant) int64 { n, _ := v.Millis(); return n }
func nullableTime(v *domain.Instant) any {
	if v == nil {
		return nil
	}
	return milliseconds(*v)
}
