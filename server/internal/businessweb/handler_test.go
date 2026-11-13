package businessweb_test

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"net/http"
	"net/http/cookiejar"
	"net/http/httptest"
	"net/url"
	"regexp"
	"strings"
	"testing"
	"time"

	"daymark.local/server/internal/application"
	"daymark.local/server/internal/business"
	"daymark.local/server/internal/businessweb"
	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/httpapi"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"daymark.local/server/internal/storage"
	"daymark.local/server/internal/telemetry"
	"github.com/pquerna/otp/totp"
)

type fixture struct {
	ctx     context.Context
	db      *storage.DB
	admin   *business.AdminService
	metrics *telemetry.Store
	auth    *application.Auth
	web     *businessweb.Handler
	server  *httptest.Server
	client  *http.Client
	owner   business.Enrollment
	clock   ports.Clock
}

func newID(t *testing.T) domain.ID {
	t.Helper()
	v, e := domain.NewID()
	if e != nil {
		t.Fatal(e)
	}
	return v
}
func fixtureFor(t *testing.T, enabled bool) *fixture {
	t.Helper()
	ctx := t.Context()
	db, e := storage.Open(ctx, t.TempDir(), storage.DefaultOptions())
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { db.Close() })
	clock := ports.SystemClock{}
	hasher := application.NewArgonHasher(1)
	auth, e := application.NewAuth(ctx, db, clock, hasher)
	if e != nil {
		t.Fatal(e)
	}
	admin, e := business.NewAdmin(db, hasher, clock, make([]byte, 32))
	if e != nil {
		t.Fatal(e)
	}
	owner, e := admin.Provision(ctx, "synthetic-owner", []byte("synthetic-password"), []string{"owner"})
	if e != nil {
		t.Fatal(e)
	}
	metrics, e := telemetry.Open(t.TempDir(), clock)
	if e != nil {
		t.Fatal(e)
	}
	t.Cleanup(func() { metrics.Close() })
	codec, e := protocol.NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	var state domain.State
	db.View(ctx, func(tx ports.Tx) error { var e error; state, e = tx.State(ctx); return e })
	cursor := &application.CursorCodec{ServerID: state.ServerID, ActiveKey: "test", Keys: map[string][]byte{"test": bytes.Repeat([]byte("x"), 32)}, Clock: clock, TTL: time.Hour}
	syncer := application.NewSync(db, auth, clock, cursor)
	legacy := httpapi.New(auth, syncer, codec)
	legacy.RegistrationEnabled = true
	server := httptest.NewUnstartedServer(nil)
	origin := "https://" + server.Listener.Addr().String()
	var metricAPI *telemetry.Store
	if enabled {
		metricAPI = metrics
	}
	web, e := businessweb.New(admin, metricAPI, auth, clock, origin, enabled)
	if e != nil {
		t.Fatal(e)
	}
	server.Config.Handler = web.Wrap(legacy.Router())
	server.StartTLS()
	t.Cleanup(server.Close)
	client := server.Client()
	jar, _ := cookiejar.New(nil)
	client.Jar = jar
	client.CheckRedirect = func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse }
	return &fixture{ctx, db, admin, metrics, auth, web, server, client, owner, clock}
}
func (f *fixture) request(t *testing.T, method, path string, body []byte, kind, access, csrf, origin string) (*http.Response, []byte) {
	t.Helper()
	request, e := http.NewRequestWithContext(f.ctx, method, f.server.URL+path, bytes.NewReader(body))
	if e != nil {
		t.Fatal(e)
	}
	if kind != "" {
		request.Header.Set("Content-Type", kind)
	}
	request.Header.Set("X-Daymark-DTO-Version", "1")
	if access != "" {
		request.Header.Set("Authorization", "Bearer "+access)
	}
	if csrf != "" {
		request.Header.Set("X-CSRF-Token", csrf)
	}
	if origin != "" {
		request.Header.Set("Origin", origin)
	}
	response, e := f.client.Do(request)
	if e != nil {
		t.Fatal(e)
	}
	defer response.Body.Close()
	data, e := io.ReadAll(response.Body)
	if e != nil {
		t.Fatal(e)
	}
	return response, data
}
func (f *fixture) login(t *testing.T, enrollment business.Enrollment, login string) string {
	t.Helper()
	response, page := f.request(t, "GET", "/admin/session/login", nil, "", "", "", "")
	if response.StatusCode != 200 {
		t.Fatal(response.StatusCode, string(page))
	}
	if response.Header.Get("Referrer-Policy") != "same-origin" {
		t.Fatal("browser form Origin would be suppressed")
	}
	if !strings.Contains(response.Header.Get("Set-Cookie"), "HttpOnly") || !strings.Contains(response.Header.Get("Set-Cookie"), "Secure") {
		t.Fatal("unsafe cookie")
	}
	match := regexp.MustCompile(`name="csrf" value="([^"]+)"`).FindSubmatch(page)
	if len(match) != 2 {
		t.Fatal("csrf form missing")
	}
	code, _ := totp.GenerateCode(enrollment.Secret, f.clock.Now())
	form := url.Values{"csrf": {string(match[1])}, "login": {login}, "password": {"synthetic-password"}, "otp": {code}}
	response, page = f.request(t, "POST", "/admin/session/login", []byte(form.Encode()), "application/x-www-form-urlencoded", "", "", f.web.Origin)
	if response.StatusCode != 303 {
		t.Fatal(response.StatusCode, string(page))
	}
	parsed, _ := url.Parse(f.server.URL + "/admin")
	for _, cookie := range f.client.Jar.Cookies(parsed) {
		if cookie.Name == "daymark_admin" {
			return f.admin.CSRF(cookie.Value)
		}
	}
	t.Fatal("session cookie missing")
	return ""
}
func marshal(t *testing.T, value any) []byte {
	t.Helper()
	b, e := json.Marshal(value)
	if e != nil {
		t.Fatal(e)
	}
	return b
}
func TestHTTPSAdminLoginPermissionsCASAndUserRevocation(t *testing.T) {
	f := fixtureFor(t, true)
	user, e := f.auth.CreateUser(f.ctx, "synthetic-user", []byte("synthetic-password"))
	if e != nil {
		t.Fatal(e)
	}
	pair, e := f.auth.Login(f.ctx, application.LoginInput{ProtocolVersion: 1, Login: "synthetic-user", Password: "synthetic-password", InstallationID: newID(t), DeviceName: "synthetic", Platform: "windows"})
	if e != nil {
		t.Fatal(e)
	}
	response, _ := f.request(t, "GET", "/admin/api/v1/users", nil, "", pair.Access, "", "")
	if response.StatusCode != 401 {
		t.Fatal("desktop token gained admin access", response.StatusCode)
	}
	csrf := f.login(t, f.owner, "synthetic-owner")
	for _, path := range []string{"/admin", "/admin/users", "/admin/notices", "/admin/admins", "/admin/audit", "/admin/metrics"} {
		response, page := f.request(t, "GET", path, nil, "", "", "", "")
		if response.StatusCode != 200 || !strings.Contains(string(page), "</html>") {
			t.Fatal(path, response.StatusCode, string(page))
		}
		if !strings.Contains(response.Header.Get("Content-Security-Policy"), "frame-ancestors 'none'") {
			t.Fatal("CSP missing")
		}
	}
	input := business.UserCommand{OperationID: newID(t), Version: 1, Status: "DISABLED", Reason: "security"}
	body := marshal(t, input)
	response, _ = f.request(t, "POST", "/admin/api/v1/users/"+string(user)+"/status", body, "application/json", "", csrf, "https://other.invalid")
	if response.StatusCode != 403 {
		t.Fatal("cross origin", response.StatusCode)
	}
	response, page := f.request(t, "POST", "/admin/api/v1/users/"+string(user)+"/status", body, "application/json", "", csrf, f.web.Origin)
	if response.StatusCode != 200 {
		t.Fatal(response.StatusCode, string(page))
	}
	response, _ = f.request(t, "GET", "/v1/me", nil, "", pair.Access, "", "")
	if response.StatusCode != 403 || response.Header.Get("X-Daymark-Error-Code") != "S111" {
		t.Fatal("disabled user session must remain revoked", response.StatusCode)
	}
	response, _ = f.request(t, "POST", "/admin/api/v1/users/"+string(user)+"/status", body, "application/json", "", csrf, f.web.Origin)
	if response.StatusCode != 200 {
		t.Fatal("idempotent retry failed")
	}
	input.OperationID = newID(t)
	response, _ = f.request(t, "POST", "/admin/api/v1/users/"+string(user)+"/status", marshal(t, input), "application/json", "", csrf, f.web.Origin)
	if response.StatusCode != 409 {
		t.Fatal("CAS not enforced", response.StatusCode)
	}
}
func TestNoticePreviewEscapesHTMLAndPublisherCannotManageUsers(t *testing.T) {
	f := fixtureFor(t, true)
	csrf := f.login(t, f.owner, "synthetic-owner")
	in := business.NoticeCommand{OperationID: newID(t), Title: "Synthetic <script>", Body: "<script>PRIVATE_CANARY</script>", Platform: "all", Action: "none", ExpireMS: time.Now().Add(24 * time.Hour).UnixMilli()}
	response, data := f.request(t, "POST", "/admin/api/v1/notices", marshal(t, in), "application/json", "", csrf, f.web.Origin)
	if response.StatusCode != 201 {
		t.Fatal(response.StatusCode, string(data))
	}
	var receipt domain.CommandReceipt
	json.Unmarshal(data, &receipt)
	response, page := f.request(t, "GET", "/admin/notices/preview?id="+string(receipt.ID), nil, "", "", "", "")
	if response.StatusCode != 200 || strings.Contains(string(page), "<script>PRIVATE_CANARY") || !strings.Contains(string(page), "&lt;script&gt;PRIVATE_CANARY") {
		t.Fatal("HTML escaped preview", response.StatusCode, string(page))
	}
	publisher, e := f.admin.Provision(f.ctx, "synthetic-publisher", []byte("synthetic-password"), []string{"publisher"})
	if e != nil {
		t.Fatal(e)
	}
	f.login(t, publisher, "synthetic-publisher")
	response, _ = f.request(t, "GET", "/admin/api/v1/users", nil, "", "", "", "")
	if response.StatusCode != 403 {
		t.Fatal("publisher gained users privilege")
	}
	response, _ = f.request(t, "GET", "/admin/api/v1/notices", nil, "", "", "", "")
	if response.StatusCode != 200 {
		t.Fatal(response.StatusCode)
	}
}
func TestTelemetryHTTPSConsentStrictPayloadWithdrawalAndCoreAvailability(t *testing.T) {
	f := fixtureFor(t, true)
	_, e := f.auth.CreateUser(f.ctx, "synthetic-user", []byte("synthetic-password"))
	if e != nil {
		t.Fatal(e)
	}
	pair, e := f.auth.Login(f.ctx, application.LoginInput{ProtocolVersion: 1, Login: "synthetic-user", Password: "synthetic-password", InstallationID: newID(t), DeviceName: "synthetic", Platform: "windows"})
	if e != nil {
		t.Fatal(e)
	}
	on := telemetry.ConsentChange{OperationID: newID(t), Enabled: true, PolicyVersion: 1}
	response, data := f.request(t, "PUT", "/telemetry/v1/consent", marshal(t, on), "application/json", pair.Access, "", "")
	if response.StatusCode != 200 {
		t.Fatal(response.StatusCode, string(data))
	}
	var consent telemetry.Consent
	json.Unmarshal(data, &consent)
	batch := telemetry.Batch{ID: newID(t), ConsentEpoch: consent.Epoch, SchemaVersion: 1, Events: []telemetry.Event{{ID: newID(t), Name: "surface.used", Version: 1, OccurredAt: time.Now().UTC().Format(time.RFC3339), Properties: map[string]string{"app_version": "3.0.0", "os": "windows", "surface": "mini"}}}}
	response, data = f.request(t, "POST", "/telemetry/v1/batches", marshal(t, batch), "application/json", pair.Access, "", "")
	if response.StatusCode != 202 {
		t.Fatal(response.StatusCode, string(data))
	}
	batch.Events[0].Properties["task_title"] = "PRIVATE_CANARY"
	response, data = f.request(t, "POST", "/telemetry/v1/batches", marshal(t, batch), "application/json", pair.Access, "", "")
	if response.StatusCode != 422 || strings.Contains(string(data), "PRIVATE_CANARY") || response.Header.Get("X-Daymark-Error-Code") != "S605" {
		t.Fatal("privacy/diagnostic failure", response.StatusCode, string(data))
	}
	off := telemetry.ConsentChange{OperationID: newID(t), ExpectedEpoch: consent.Epoch, Enabled: false, PolicyVersion: 1}
	response, data = f.request(t, "PUT", "/telemetry/v1/consent", marshal(t, off), "application/json", pair.Access, "", "")
	if response.StatusCode != 200 {
		t.Fatal(response.StatusCode, string(data))
	}
	delete(batch.Events[0].Properties, "task_title")
	response, _ = f.request(t, "POST", "/telemetry/v1/batches", marshal(t, batch), "application/json", pair.Access, "", "")
	if response.StatusCode != 403 {
		t.Fatal("withdrawn subject accepted")
	}
	response, _ = f.request(t, "GET", "/v1/me", nil, "", pair.Access, "", "")
	if response.StatusCode != 200 {
		t.Fatal("statistics affected business login")
	}
	response, _ = f.request(t, "PUT", "/telemetry/v1/consent", []byte(`{"operation_id":"x","operation_id":"y"}`), "application/json", pair.Access, "", "")
	if response.StatusCode != 400 {
		t.Fatal("duplicate JSON keys accepted")
	}
}
func TestDisabledModulesHaveNoPublicSurface(t *testing.T) {
	f := fixtureFor(t, false)
	for _, path := range []string{"/admin", "/admin/session/login", "/telemetry/v1/consent"} {
		response, _ := f.request(t, "GET", path, nil, "", "", "", "")
		if response.StatusCode != 404 {
			t.Fatal(path, response.StatusCode)
		}
	}
	response, _ := f.request(t, "GET", "/v1/capabilities", nil, "", "", "", "")
	if response.StatusCode != 200 {
		t.Fatal("core not available")
	}
}

func TestTelemetryStorageUnavailableDoesNotBreakCore(t *testing.T) {
	f := fixtureFor(t, false)
	f.web.TelemetryEnabled = true
	for _, path := range []string{"/telemetry/v1/consent", "/telemetry/v1/batches"} {
		method := "GET"
		if strings.HasSuffix(path, "batches") {
			method = "POST"
		}
		response, body := f.request(t, method, path, nil, "", "", "", "")
		if response.StatusCode != 503 || !bytes.Contains(body, []byte("telemetry_storage_failed")) {
			t.Fatal(path, response.StatusCode, string(body))
		}
	}
	response, _ := f.request(t, "GET", "/v1/capabilities", nil, "", "", "", "")
	if response.StatusCode != 200 {
		t.Fatal("optional store blocked core")
	}
}

func TestUnavailableAdminFailsClosedAndKeepsTaskAPIAvailable(t *testing.T) {
	f := fixtureFor(t, true)
	f.web.Admin = nil
	for _, path := range []string{"/admin", "/admin/session/login", "/admin/api/v1/users"} {
		response, _ := f.request(t, "GET", path, nil, "", "", "", "")
		if response.StatusCode != 503 {
			t.Fatal("missing admin must fail closed", path, response.StatusCode)
		}
	}
	response, _ := f.request(t, "GET", "/v1/capabilities", nil, "", "", "", "")
	if response.StatusCode != 200 {
		t.Fatal("admin failure blocked core")
	}
}
