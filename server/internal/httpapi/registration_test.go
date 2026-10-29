package httpapi

import (
	"daymark.local/server/internal/application"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
	"net/http/httptest"
	"strconv"
	"strings"
	"testing"
)

func TestDisabledRegistrationRejectsBeforeDependencyAccess(t *testing.T) {
	codec, e := protocol.NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	h := New(nil, nil, codec)
	r := httptest.NewRequest("POST", "/v1/auth/register", strings.NewReader("invalid"))
	r.Header.Set("X-Daymark-DTO-Version", "1")
	w := httptest.NewRecorder()
	h.Router().ServeHTTP(w, r)
	if w.Code != 404 || w.Header().Get("Cache-Control") != "no-store" {
		t.Fatal("disabled registration accessed dependencies", w.Code)
	}
}
func TestRegistrationGlobalLimitPrecedesKDFAndIgnoresForwardedIdentity(t *testing.T) {
	codec, e := protocol.NewCodec()
	if e != nil {
		t.Fatal(e)
	}
	h := New(nil, &application.Sync{Clock: ports.SystemClock{}}, codec)
	h.RegistrationEnabled = true
	router := h.Router()
	for i := 0; i < 21; i++ {
		r := httptest.NewRequest("POST", "/v1/auth/register", strings.NewReader("{}"))
		r.RemoteAddr = "192.0.2." + strconv.Itoa(i+1) + ":1234"
		r.Header.Set("X-Daymark-DTO-Version", "1")
		r.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()
		router.ServeHTTP(w, r)
		want := 422
		if i == 20 {
			want = 429
		}
		if w.Code != want {
			t.Fatal("global registration admission", i, w.Code)
		}
	}
	h.attempts = map[string]window{}
	for i := 0; i < 6; i++ {
		r := httptest.NewRequest("POST", "/v1/auth/register", strings.NewReader("{}"))
		r.RemoteAddr = "192.0.2.1:1234"
		r.Header.Set("X-Forwarded-For", "198.51.100."+strconv.Itoa(i))
		r.Header.Set("X-Daymark-DTO-Version", "1")
		r.Header.Set("Content-Type", "application/json")
		w := httptest.NewRecorder()
		router.ServeHTTP(w, r)
		want := 422
		if i == 5 {
			want = 429
		}
		if w.Code != want {
			t.Fatal("forwarded header bypassed socket-IP limit", i, w.Code)
		}
	}
}
