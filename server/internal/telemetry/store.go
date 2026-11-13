package telemetry

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"sync"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	_ "modernc.org/sqlite"
)

const PolicyVersion = 1

type Store struct {
	db    *sql.DB
	gate  chan struct{}
	clock ports.Clock
	once  sync.Once
}

const schema = `
CREATE TABLE metadata(version INTEGER PRIMARY KEY CHECK(version=1)) STRICT;
INSERT INTO metadata VALUES(1);
CREATE TABLE subjects(id TEXT PRIMARY KEY,owner TEXT NOT NULL,device TEXT NOT NULL,auth_epoch TEXT NOT NULL,epoch INTEGER NOT NULL,enabled INTEGER NOT NULL,policy INTEGER NOT NULL,updated_ms INTEGER NOT NULL,UNIQUE(owner,device)) STRICT;
CREATE TABLE consent_operations(owner TEXT NOT NULL,device TEXT NOT NULL,id TEXT NOT NULL,hash TEXT NOT NULL,result TEXT NOT NULL,at_ms INTEGER NOT NULL,PRIMARY KEY(owner,device,id)) STRICT;
CREATE TABLE batches(subject TEXT NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,id TEXT NOT NULL,hash TEXT NOT NULL,at_ms INTEGER NOT NULL,PRIMARY KEY(subject,id)) STRICT;
CREATE TABLE events(subject TEXT NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,id TEXT NOT NULL,name TEXT NOT NULL,received_ms INTEGER NOT NULL,properties TEXT NOT NULL CHECK(json_valid(properties)),PRIMARY KEY(subject,id)) STRICT;
CREATE INDEX events_time ON events(received_ms);
CREATE TABLE contributions(subject TEXT NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,day TEXT NOT NULL,name TEXT NOT NULL,dimension TEXT NOT NULL,count INTEGER NOT NULL,PRIMARY KEY(subject,day,name,dimension)) STRICT;
CREATE INDEX contributions_day ON contributions(day,name);
`

func Open(directory string, clock ports.Clock) (*Store, error) {
	if e := os.MkdirAll(directory, 0700); e != nil {
		return nil, e
	}
	path := filepath.Join(directory, "metrics.sqlite")
	u := url.URL{Scheme: "file", Path: filepath.ToSlash(path)}
	if !strings.HasPrefix(u.Path, "/") {
		u.Path = "/" + u.Path
	}
	q := url.Values{}
	for _, v := range []string{"foreign_keys(1)", "journal_mode(WAL)", "synchronous(FULL)", "busy_timeout(1000)", "wal_autocheckpoint(256)", "max_page_count(131072)"} {
		q.Add("_pragma", v)
	}
	q.Set("_txlock", "immediate")
	u.RawQuery = q.Encode()
	db, e := sql.Open("sqlite", u.String())
	if e != nil {
		return nil, e
	}
	db.SetMaxOpenConns(1)
	db.SetMaxIdleConns(1)
	fail := func(e error) (*Store, error) { db.Close(); return nil, domain.Wrap("telemetry_storage_failed", e) }
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	var count int
	if e = db.QueryRowContext(ctx, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='metadata'").Scan(&count); e != nil {
		return fail(e)
	}
	if count == 0 {
		tx, e := db.BeginTx(ctx, nil)
		if e != nil {
			return fail(e)
		}
		if _, e = tx.ExecContext(ctx, schema); e != nil {
			tx.Rollback()
			return fail(e)
		}
		if e = tx.Commit(); e != nil {
			return fail(e)
		}
	}
	var version int
	if e = db.QueryRowContext(ctx, "SELECT version FROM metadata").Scan(&version); e != nil || version != 1 {
		return fail(domain.Fail("protocol_upgrade_required"))
	}
	return &Store{db: db, gate: make(chan struct{}, 4), clock: clock}, nil
}
func (s *Store) Close() error { var e error; s.once.Do(func() { e = s.db.Close() }); return e }
func (s *Store) write(ctx context.Context, fn func(*sql.Tx) error) error {
	select {
	case s.gate <- struct{}{}:
		defer func() { <-s.gate }()
	default:
		return domain.Fail("telemetry_busy")
	}
	tx, e := s.db.BeginTx(ctx, nil)
	if e != nil {
		return domain.Wrap("telemetry_storage_failed", e)
	}
	defer tx.Rollback()
	if e = fn(tx); e != nil {
		return e
	}
	if e = tx.Commit(); e != nil {
		return domain.Wrap("telemetry_storage_failed", e)
	}
	return nil
}

type Consent struct {
	SubjectID         domain.ID `json:"subject_id"`
	Epoch             int64     `json:"epoch"`
	Enabled           bool      `json:"enabled"`
	PolicyVersion     int       `json:"policy_version"`
	NeedsConfirmation bool      `json:"needs_confirmation"`
}
type ConsentChange struct {
	OperationID   domain.ID `json:"operation_id"`
	ExpectedEpoch int64     `json:"expected_epoch"`
	Enabled       bool      `json:"enabled"`
	PolicyVersion int       `json:"policy_version"`
}

func readConsent(ctx context.Context, q interface {
	QueryRowContext(context.Context, string, ...any) *sql.Row
}, p domain.Principal) (Consent, error) {
	var c Consent
	var auth string
	var enabled int
	e := q.QueryRowContext(ctx, "SELECT id,epoch,enabled,policy,auth_epoch FROM subjects WHERE owner=? AND device=?", p.UserID, p.InstallationID).Scan(&c.SubjectID, &c.Epoch, &enabled, &c.PolicyVersion, &auth)
	if errors.Is(e, sql.ErrNoRows) {
		return Consent{PolicyVersion: PolicyVersion}, nil
	}
	if e != nil {
		return c, domain.Wrap("telemetry_storage_failed", e)
	}
	c.NeedsConfirmation = auth != string(p.AuthEpoch)
	c.Enabled = enabled == 1 && !c.NeedsConfirmation
	c.NeedsConfirmation = c.NeedsConfirmation || c.PolicyVersion != PolicyVersion
	if c.NeedsConfirmation {
		c.Enabled = false
	}
	return c, nil
}
func (s *Store) Consent(ctx context.Context, p domain.Principal) (Consent, error) {
	return readConsent(ctx, s.db, p)
}
func (s *Store) ChangeConsent(ctx context.Context, p domain.Principal, in ConsentChange) (Consent, error) {
	var out Consent
	if !domain.ValidID(string(in.OperationID), false) || in.ExpectedEpoch < 0 || in.ExpectedEpoch >= 9007199254740991 || in.Enabled && in.PolicyVersion != PolicyVersion {
		return out, domain.Fail("telemetry_consent_invalid")
	}
	b, _ := json.Marshal(in)
	hash := sha256.Sum256(b)
	digest := hex.EncodeToString(hash[:])
	now := s.clock.Now().UnixMilli()
	e := s.write(ctx, func(tx *sql.Tx) error {
		var priorHash, result string
		e := tx.QueryRowContext(ctx, "SELECT hash,result FROM consent_operations WHERE owner=? AND device=? AND id=?", p.UserID, p.InstallationID, in.OperationID).Scan(&priorHash, &result)
		if e == nil {
			if digest != priorHash {
				return domain.Fail("telemetry_consent_conflict")
			}
			if json.Unmarshal([]byte(result), &out) != nil {
				return domain.Fail("telemetry_storage_failed")
			}
			return nil
		}
		if !errors.Is(e, sql.ErrNoRows) {
			return e
		}
		current, e := readConsent(ctx, tx, p)
		if e != nil {
			return e
		}
		if current.Epoch != in.ExpectedEpoch {
			return domain.Fail("telemetry_consent_conflict")
		}
		id := current.SubjectID
		if id == "" {
			var count int
			if e = tx.QueryRowContext(ctx, "SELECT count(*) FROM subjects WHERE owner=?", p.UserID).Scan(&count); e != nil {
				return e
			}
			if count >= 50 {
				return domain.Fail("quota_exceeded")
			}
			id, e = domain.NewID()
			if e != nil {
				return e
			}
		}
		out = Consent{SubjectID: id, Epoch: current.Epoch + 1, Enabled: in.Enabled, PolicyVersion: PolicyVersion}
		enabled := 0
		if out.Enabled {
			enabled = 1
		}
		if _, e = tx.ExecContext(ctx, `INSERT INTO subjects VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(owner,device) DO UPDATE SET auth_epoch=excluded.auth_epoch,epoch=excluded.epoch,enabled=excluded.enabled,policy=excluded.policy,updated_ms=excluded.updated_ms`, id, p.UserID, p.InstallationID, p.AuthEpoch, out.Epoch, enabled, PolicyVersion, now); e != nil {
			return e
		}
		if !in.Enabled {
			for _, table := range []string{"events", "contributions", "batches"} {
				if _, e = tx.ExecContext(ctx, "DELETE FROM "+table+" WHERE subject=?", id); e != nil {
					return e
				}
			}
		}
		receipt, _ := json.Marshal(out)
		_, e = tx.ExecContext(ctx, "INSERT INTO consent_operations VALUES(?,?,?,?,?,?)", p.UserID, p.InstallationID, in.OperationID, digest, string(receipt), now)
		return e
	})
	return out, e
}

type Event struct {
	ID         domain.ID         `json:"id"`
	Name       string            `json:"name"`
	Version    int               `json:"version"`
	OccurredAt string            `json:"occurred_at"`
	Properties map[string]string `json:"properties"`
}
type Batch struct {
	ID            domain.ID `json:"batch_id"`
	ConsentEpoch  int64     `json:"consent_epoch"`
	SchemaVersion int       `json:"schema_version"`
	Events        []Event   `json:"events"`
}

var appVersion = regexp.MustCompile(`^(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,3})$`)
var safeDiagnostic = regexp.MustCompile(`^S[0-9]{3}-C[0-9]{3}$`)

func ValidateEvent(e Event, now time.Time) error {
	if !domain.ValidID(string(e.ID), false) || e.Version != 1 || len(e.Properties) < 2 || !appVersion.MatchString(e.Properties["app_version"]) || !slices.Contains([]string{"windows", "macos", "linux"}, e.Properties["os"]) {
		return domain.Fail("telemetry_event_invalid")
	}
	at, err := time.Parse(time.RFC3339, e.OccurredAt)
	if err != nil || at.Before(now.Add(-72*time.Hour)) || at.After(now.Add(10*time.Minute)) {
		return domain.Fail("telemetry_event_invalid")
	}
	fields := map[string][]string{"app_version": nil, "os": nil}
	switch e.Name {
	case "app.startup":
		fields["duration_bucket"] = []string{"lt_250ms", "250_500ms", "500_1000ms", "1000_2000ms", "ge_2000ms"}
	case "surface.used":
		fields["surface"] = []string{"main", "widget", "mini", "ball"}
	case "update.outcome":
		fields["outcome"] = []string{"ready", "updated", "cancelled", "failed"}
		fields["diagnostic_code"] = nil
	case "performance.sample":
		fields["memory_bucket"] = []string{"lt_64MiB", "64_128MiB", "128_256MiB", "256_512MiB", "ge_512MiB"}
	default:
		return domain.Fail("telemetry_event_invalid")
	}
	for key, value := range e.Properties {
		allowed, ok := fields[key]
		if !ok || len(value) > 40 || allowed != nil && !slices.Contains(allowed, value) || key == "diagnostic_code" && !safeDiagnostic.MatchString(value) {
			return domain.Fail("telemetry_event_invalid")
		}
	}
	for key := range fields {
		if key != "diagnostic_code" && e.Properties[key] == "" {
			return domain.Fail("telemetry_event_invalid")
		}
	}
	return nil
}

type Receipt struct {
	Accepted int       `json:"accepted"`
	BatchID  domain.ID `json:"batch_id"`
}

func (s *Store) Ingest(ctx context.Context, p domain.Principal, in Batch) (Receipt, error) {
	out := Receipt{Accepted: len(in.Events), BatchID: in.ID}
	if !domain.ValidID(string(in.ID), false) || in.SchemaVersion != 1 || len(in.Events) < 1 || len(in.Events) > 32 {
		return out, domain.Fail("telemetry_event_invalid")
	}
	now := s.clock.Now()
	seen := map[domain.ID]bool{}
	for _, event := range in.Events {
		if e := ValidateEvent(event, now); e != nil {
			return out, e
		}
		if seen[event.ID] {
			return out, domain.Fail("telemetry_event_invalid")
		}
		seen[event.ID] = true
	}
	data, _ := json.Marshal(in)
	if len(data) > 32768 {
		return out, domain.Fail("payload_too_large")
	}
	h := sha256.Sum256(data)
	digest := hex.EncodeToString(h[:])
	day := now.UTC().Format("2006-01-02")
	e := s.write(ctx, func(tx *sql.Tx) error {
		consent, e := readConsent(ctx, tx, p)
		if e != nil {
			return e
		}
		if !consent.Enabled || consent.Epoch != in.ConsentEpoch {
			return domain.Fail("telemetry_consent_required")
		}
		var old string
		e = tx.QueryRowContext(ctx, "SELECT hash FROM batches WHERE subject=? AND id=?", consent.SubjectID, in.ID).Scan(&old)
		if e == nil {
			if old != digest {
				return domain.Fail("telemetry_batch_conflict")
			}
			return nil
		}
		if !errors.Is(e, sql.ErrNoRows) {
			return e
		}
		var count int
		if e = tx.QueryRowContext(ctx, "SELECT coalesce(sum(count),0) FROM contributions WHERE subject=? AND day=?", consent.SubjectID, day).Scan(&count); e != nil {
			return e
		}
		if count+len(in.Events) > 2000 {
			return domain.Fail("rate_limited")
		}
		if e = tx.QueryRowContext(ctx, "SELECT count(*) FROM events").Scan(&count); e != nil {
			return e
		}
		if count > 500000 {
			return domain.Fail("telemetry_busy")
		}
		for _, event := range in.Events {
			properties, _ := json.Marshal(event.Properties)
			var prior string
			e = tx.QueryRowContext(ctx, "SELECT properties FROM events WHERE subject=? AND id=?", consent.SubjectID, event.ID).Scan(&prior)
			if e == nil {
				if prior != string(properties) {
					return domain.Fail("telemetry_batch_conflict")
				}
				continue
			}
			if !errors.Is(e, sql.ErrNoRows) {
				return e
			}
			if _, e = tx.ExecContext(ctx, "INSERT INTO events VALUES(?,?,?,?,?)", consent.SubjectID, event.ID, event.Name, now.UnixMilli(), string(properties)); e != nil {
				return e
			}
			if _, e = tx.ExecContext(ctx, `INSERT INTO contributions VALUES(?,?,?,?,1) ON CONFLICT(subject,day,name,dimension) DO UPDATE SET count=count+1`, consent.SubjectID, day, event.Name, string(properties)); e != nil {
				return e
			}
		}
		_, e = tx.ExecContext(ctx, "INSERT INTO batches VALUES(?,?,?,?)", consent.SubjectID, in.ID, digest, now.UnixMilli())
		return e
	})
	return out, e
}

type Metric struct {
	Day       string `json:"day"`
	Name      string `json:"name"`
	Dimension string `json:"dimension"`
	Count     int64  `json:"count"`
	Devices   int64  `json:"devices"`
}
type Report struct {
	PolicyVersion int      `json:"policy_version"`
	Days          int      `json:"days"`
	SampleSize    *int64   `json:"sample_size"`
	Suppressed    bool     `json:"suppressed"`
	Truncated     bool     `json:"truncated"`
	Rows          []Metric `json:"rows"`
}

func (s *Store) Report(ctx context.Context, days int) (Report, error) {
	out := Report{PolicyVersion: PolicyVersion, Days: days, Rows: []Metric{}}
	if days < 1 || days > 90 {
		return out, domain.Invalid("/days")
	}
	after := s.clock.Now().UTC().AddDate(0, 0, -days+1).Format("2006-01-02")
	var sample int64
	if e := s.db.QueryRowContext(ctx, "SELECT count(DISTINCT subject) FROM contributions WHERE day>=?", after).Scan(&sample); e != nil {
		return out, domain.Wrap("telemetry_storage_failed", e)
	}
	if sample < 10 {
		out.Suppressed = true
		return out, nil
	}
	out.SampleSize = &sample
	rows, e := s.db.QueryContext(ctx, "SELECT day,name,dimension,sum(count),count(DISTINCT subject) FROM contributions WHERE day>=? GROUP BY day,name,dimension HAVING count(DISTINCT subject)>=10 ORDER BY day DESC,name,dimension LIMIT 501", after)
	if e != nil {
		return out, e
	}
	defer rows.Close()
	for rows.Next() {
		var m Metric
		if e = rows.Scan(&m.Day, &m.Name, &m.Dimension, &m.Count, &m.Devices); e != nil {
			return out, e
		}
		out.Rows = append(out.Rows, m)
	}
	if len(out.Rows) > 500 {
		out.Truncated = true
		out.Rows = out.Rows[:500]
	}
	return out, rows.Err()
}
func (s *Store) DropAccount(ctx context.Context, user domain.ID) error {
	return s.write(ctx, func(tx *sql.Tx) error {
		if _, e := tx.ExecContext(ctx, "DELETE FROM subjects WHERE owner=?", user); e != nil {
			return e
		}
		_, e := tx.ExecContext(ctx, "DELETE FROM consent_operations WHERE owner=?", user)
		return e
	})
}
func (s *Store) Maintain(ctx context.Context) error {
	return s.write(ctx, func(tx *sql.Tx) error {
		now := s.clock.Now()
		for _, query := range []struct {
			sql string
			arg any
		}{{"DELETE FROM events WHERE rowid IN (SELECT rowid FROM events WHERE received_ms<? LIMIT 1000)", now.AddDate(0, 0, -7).UnixMilli()}, {"DELETE FROM batches WHERE rowid IN (SELECT rowid FROM batches WHERE at_ms<? LIMIT 1000)", now.AddDate(0, 0, -7).UnixMilli()}, {"DELETE FROM consent_operations WHERE rowid IN (SELECT rowid FROM consent_operations WHERE at_ms<? LIMIT 1000)", now.AddDate(0, 0, -90).UnixMilli()}, {"DELETE FROM contributions WHERE rowid IN (SELECT rowid FROM contributions WHERE day<? LIMIT 1000)", now.AddDate(0, 0, -90).Format("2006-01-02")}} {
			if _, e := tx.ExecContext(ctx, query.sql, query.arg); e != nil {
				return e
			}
		}
		return nil
	})
}
