package telemetry

import (
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
	"time"

	"daymark.local/server/internal/domain"
)

type testClock struct{ at time.Time }

func (c *testClock) Now() time.Time { return c.at }
func testID(t *testing.T) domain.ID {
	t.Helper()
	v, e := domain.NewID()
	if e != nil {
		t.Fatal(e)
	}
	return v
}
func openTest(t *testing.T) (*Store, *testClock, domain.Principal) {
	t.Helper()
	c := &testClock{time.Date(2026, 11, 12, 12, 0, 0, 0, time.UTC)}
	s, e := Open(t.TempDir(), c)
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { s.Close() })
	return s, c, domain.Principal{UserID: testID(t), InstallationID: testID(t), AuthEpoch: testID(t)}
}
func sample(t *testing.T, c *testClock, epoch int64) Batch {
	return Batch{ID: testID(t), ConsentEpoch: epoch, SchemaVersion: 1, Events: []Event{{ID: testID(t), Name: "app.startup", Version: 1, OccurredAt: c.Now().Format(time.RFC3339), Properties: map[string]string{"app_version": "3.0.0", "os": "windows", "duration_bucket": "500_1000ms"}}}}
}
func enable(t *testing.T, s *Store, p domain.Principal) Consent {
	t.Helper()
	out, e := s.ChangeConsent(context.Background(), p, ConsentChange{OperationID: testID(t), Enabled: true, PolicyVersion: 1})
	if e != nil {
		t.Fatal(e)
	}
	return out
}
func TestConsentWithdrawalFenceReplayAndOwnerIsolation(t *testing.T) {
	s, c, p := openTest(t)
	ctx := context.Background()
	initial, e := s.Consent(ctx, p)
	if e != nil || initial.Enabled || initial.SubjectID != "" {
		t.Fatal(initial, e)
	}
	if _, e = s.Ingest(ctx, p, sample(t, c, 1)); domain.Code(e) != "telemetry_consent_required" {
		t.Fatal(e)
	}
	operation := ConsentChange{OperationID: testID(t), Enabled: true, PolicyVersion: 1}
	on, e := s.ChangeConsent(ctx, p, operation)
	if e != nil {
		t.Fatal(e)
	}
	batch := sample(t, c, on.Epoch)
	if _, e = s.Ingest(ctx, p, batch); e != nil {
		t.Fatal(e)
	}
	if _, e = s.Ingest(ctx, p, batch); e != nil {
		t.Fatal(e)
	}
	var count int
	s.db.QueryRow("SELECT count(*) FROM events").Scan(&count)
	if count != 1 {
		t.Fatal(count)
	}
	other := p
	other.UserID = testID(t)
	if _, e = s.Ingest(ctx, other, batch); domain.Code(e) != "telemetry_consent_required" {
		t.Fatal("cross owner", e)
	}
	off, e := s.ChangeConsent(ctx, p, ConsentChange{OperationID: testID(t), ExpectedEpoch: on.Epoch, Enabled: false, PolicyVersion: 1})
	if e != nil || off.Enabled {
		t.Fatal(off, e)
	}
	if _, e = s.Ingest(ctx, p, batch); domain.Code(e) != "telemetry_consent_required" {
		t.Fatal(e)
	}
	if _, e = s.ChangeConsent(ctx, p, operation); e != nil {
		t.Fatal(e)
	}
	current, _ := s.Consent(ctx, p)
	if current.Enabled || current.Epoch != off.Epoch {
		t.Fatal("replayed enable revived consent")
	}
	s.db.QueryRow("SELECT count(*) FROM contributions").Scan(&count)
	if count != 0 {
		t.Fatal("withdrawal retained aggregate contribution")
	}
}
func TestAllowlistPrivacyAndSchemaCatalog(t *testing.T) {
	s, c, p := openTest(t)
	consent := enable(t, s, p)
	batch := sample(t, c, consent.Epoch)
	batch.Events[0].Properties["task_title"] = "SYNTHETIC_PRIVATE_CANARY"
	if _, e := s.Ingest(context.Background(), p, batch); domain.Code(e) != "telemetry_event_invalid" {
		t.Fatal(e)
	}
	var n int
	s.db.QueryRow("SELECT count(*) FROM events").Scan(&n)
	if n != 0 {
		t.Fatal("invalid batch committed")
	}
	bytes, e := os.ReadFile(filepath.Join("..", "..", "..", "docs", "api", "business-v1.openapi.json"))
	if e != nil {
		t.Fatal(e)
	}
	var spec struct {
		Catalog map[string]map[string]any `json:"x-event-catalog"`
	}
	if json.Unmarshal(bytes, &spec) != nil {
		t.Fatal("catalog")
	}
	for name, fields := range spec.Catalog {
		event := sample(t, c, consent.Epoch).Events[0]
		event.Name = name
		delete(event.Properties, "duration_bucket")
		for field, values := range fields {
			if list, ok := values.([]any); ok {
				event.Properties[field] = list[0].(string)
			}
		}
		if e = ValidateEvent(event, c.Now()); e != nil {
			t.Fatalf("catalog drift %s: %v", name, e)
		}
	}
}
func TestAggregationSmallCohortsRetentionAndAccountCleanup(t *testing.T) {
	s, c, p := openTest(t)
	ctx := context.Background()
	consents := []Consent{}
	principals := []domain.Principal{}
	for i := 0; i < 10; i++ {
		v := p
		v.InstallationID = testID(t)
		on := enable(t, s, v)
		if _, e := s.Ingest(ctx, v, sample(t, c, on.Epoch)); e != nil {
			t.Fatal(e)
		}
		consents = append(consents, on)
		principals = append(principals, v)
		if i == 8 {
			r, e := s.Report(ctx, 7)
			if e != nil || !r.Suppressed || r.SampleSize != nil {
				t.Fatal(r, e)
			}
		}
	}
	r, e := s.Report(ctx, 7)
	if e != nil || r.Suppressed || len(r.Rows) != 1 || r.Rows[0].Count != 10 {
		t.Fatal(r, e)
	}
	if _, e = s.ChangeConsent(ctx, principals[0], ConsentChange{OperationID: testID(t), ExpectedEpoch: consents[0].Epoch, Enabled: false, PolicyVersion: 1}); e != nil {
		t.Fatal(e)
	}
	r, _ = s.Report(ctx, 7)
	if !r.Suppressed {
		t.Fatal("withdrawal not reflected")
	}
	if e = s.DropAccount(ctx, p.UserID); e != nil {
		t.Fatal(e)
	}
	var count int
	s.db.QueryRow("SELECT count(*) FROM subjects").Scan(&count)
	if count != 0 {
		t.Fatal("deleted account owner binding retained")
	}
	c.at = c.at.AddDate(0, 0, 100)
	if e = s.Maintain(ctx); e != nil {
		t.Fatal(e)
	}
}
func TestNewAuthEpochRequiresExplicitConsent(t *testing.T) {
	s, _, p := openTest(t)
	enable(t, s, p)
	p.AuthEpoch = testID(t)
	out, e := s.Consent(context.Background(), p)
	if e != nil || out.Enabled || !out.NeedsConfirmation {
		t.Fatal(out, e)
	}
}
