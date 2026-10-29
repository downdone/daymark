package application

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"fmt"
	"regexp"
	"strings"
	"time"
	"unicode/utf8"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"golang.org/x/crypto/argon2"
)

type ArgonHasher struct{ gate chan struct{} }

func NewArgonHasher(parallel int) *ArgonHasher {
	if parallel < 1 {
		parallel = 1
	}
	return &ArgonHasher{make(chan struct{}, parallel)}
}
func (h *ArgonHasher) enter(ctx context.Context) error {
	select {
	case h.gate <- struct{}{}:
		return nil
	case <-ctx.Done():
		return domain.Wrap("cancelled", ctx.Err())
	}
}
func (h *ArgonHasher) Hash(ctx context.Context, password []byte) (string, error) {
	if e := h.enter(ctx); e != nil {
		return "", e
	}
	defer func() { <-h.gate }()
	salt := make([]byte, 16)
	if _, e := rand.Read(salt); e != nil {
		return "", domain.Wrap("token_source_failed", e)
	}
	key := argon2.IDKey(password, salt, 3, 65536, 1, 32)
	if e := ctx.Err(); e != nil {
		return "", e
	}
	enc := base64.RawStdEncoding
	return fmt.Sprintf("$argon2id$v=19$m=65536,t=3,p=1$%s$%s", enc.EncodeToString(salt), enc.EncodeToString(key)), nil
}
func (h *ArgonHasher) Verify(ctx context.Context, phc string, password []byte) (bool, error) {
	parts := strings.Split(phc, "$")
	if len(parts) != 6 || parts[1] != "argon2id" || parts[2] != "v=19" {
		return false, domain.Fail("invalid_credentials")
	}
	var memory, iterations, parallel uint32
	if _, e := fmt.Sscanf(parts[3], "m=%d,t=%d,p=%d", &memory, &iterations, &parallel); e != nil || fmt.Sprintf("m=%d,t=%d,p=%d", memory, iterations, parallel) != parts[3] || memory < 8192 || memory > 262144 || iterations < 1 || iterations > 10 || parallel < 1 || parallel > 4 {
		return false, domain.Fail("invalid_credentials")
	}
	salt, e := base64.RawStdEncoding.DecodeString(parts[4])
	if e != nil || len(salt) != 16 {
		return false, domain.Fail("invalid_credentials")
	}
	want, e := base64.RawStdEncoding.DecodeString(parts[5])
	if e != nil || len(want) != 32 {
		return false, domain.Fail("invalid_credentials")
	}
	if e = h.enter(ctx); e != nil {
		return false, e
	}
	defer func() { <-h.gate }()
	got := argon2.IDKey(password, salt, iterations, memory, uint8(parallel), 32)
	if e = ctx.Err(); e != nil {
		return false, e
	}
	return subtle.ConstantTimeCompare(want, got) == 1, nil
}

type Auth struct {
	store                 ports.Store
	clock                 ports.Clock
	hasher                ports.Hasher
	dummy                 string
	AccessTTL, RefreshTTL time.Duration
}

func NewAuth(ctx context.Context, store ports.Store, clock ports.Clock, hasher ports.Hasher) (*Auth, error) {
	dummy, e := hasher.Hash(ctx, []byte("not-a-real-account-password"))
	if e != nil {
		return nil, e
	}
	return &Auth{store: store, clock: clock, hasher: hasher, dummy: dummy, AccessTTL: 15 * time.Minute, RefreshTTL: 30 * 24 * time.Hour}, nil
}

var loginPattern = regexp.MustCompile(`^[a-z0-9._@+\-]{3,254}$`)

func normalizedLogin(v string) (string, error) {
	for _, c := range v {
		if c > 127 {
			return "", domain.Invalid("/login")
		}
	}
	v = strings.ToLower(strings.TrimSpace(v))
	if !loginPattern.MatchString(v) {
		return "", domain.Invalid("/login")
	}
	return v, nil
}

// ResetPasswordAdmin is exposed only by the stopped-service maintenance CLI.
// It cannot reactivate a disabled/deleted identity and revokes every session.
func (a *Auth) ResetPasswordAdmin(ctx context.Context, login string, password []byte) error {
	name, e := normalizedLogin(login)
	if e != nil {
		return e
	}
	if e = passwordPolicy(password); e != nil {
		return e
	}
	hash, e := a.hasher.Hash(ctx, password)
	if e != nil {
		return e
	}
	_, e = a.store.Update(ctx, func(tx ports.Tx) error {
		user, e := tx.UserByLogin(ctx, name)
		if e != nil {
			return e
		}
		if user == nil {
			return domain.Fail("not_found")
		}
		if user.Status != "ACTIVE" {
			return domain.Fail("account_disabled")
		}
		user.PasswordHash = hash
		user.CredentialVersion++
		user.UpdatedMS = a.clock.Now().UnixMilli()
		if e = tx.PutUser(ctx, *user); e != nil {
			return e
		}
		return tx.RevokeSessions(ctx, user.ID, a.clock.Now().UnixMilli())
	})
	return e
}
func passwordPolicy(v []byte) error {
	if len(v) < 12 || len(v) > 1024 || !utf8.Valid(v) {
		return domain.Invalid("/password")
	}
	return nil
}
func token() (string, [32]byte, error) {
	var raw [32]byte
	if _, e := rand.Read(raw[:]); e != nil {
		return "", [32]byte{}, domain.Wrap("token_source_failed", e)
	}
	s := base64.RawURLEncoding.EncodeToString(raw[:])
	return s, sha256.Sum256([]byte(s)), nil
}
func tokenHash(s string) ([32]byte, error) {
	b, e := base64.RawURLEncoding.Strict().DecodeString(s)
	if e != nil || len(b) != 32 || base64.RawURLEncoding.EncodeToString(b) != s {
		return [32]byte{}, domain.Fail("auth_expired")
	}
	return sha256.Sum256([]byte(s)), nil
}
func (a *Auth) CreateUser(ctx context.Context, login string, password []byte) (domain.ID, error) {
	name, e := normalizedLogin(login)
	if e != nil {
		return "", e
	}
	if e = passwordPolicy(password); e != nil {
		return "", e
	}
	hash, e := a.hasher.Hash(ctx, password)
	if e != nil {
		return "", e
	}
	id, e := domain.NewID()
	if e != nil {
		return "", e
	}
	dataset, e := domain.NewID()
	if e != nil {
		return "", e
	}
	epoch, e := domain.NewID()
	if e != nil {
		return "", e
	}
	now := a.clock.Now().UnixMilli()
	_, e = a.store.Update(ctx, func(tx ports.Tx) error {
		old, e := tx.UserByLogin(ctx, name)
		if e != nil {
			return e
		}
		if old != nil {
			return domain.Fail("revision_conflict")
		}
		if e = tx.PutUser(ctx, domain.User{ID: id, Login: name, PasswordHash: hash, CredentialVersion: 1, Status: "ACTIVE", CreatedMS: now, UpdatedMS: now}); e != nil {
			return e
		}
		return tx.InsertDataset(ctx, domain.Dataset{ID: dataset, OwnerID: id, DataEpoch: epoch}, now)
	})
	return id, e
}

// Register never alters an existing identity, including disabled/deleted users.
// A generic success avoids exposing whether a normalized login already exists.
func (a *Auth) Register(ctx context.Context, login string, password []byte) error {
	if utf8.RuneCount(password) < 15 {
		return domain.Invalid("/password")
	}
	_, e := a.CreateUser(ctx, login, password)
	if domain.Code(e) == "revision_conflict" {
		return nil
	}
	return e
}

type LoginInput struct {
	ProtocolVersion int       `json:"protocol_version"`
	Login           string    `json:"login"`
	Password        string    `json:"password"`
	InstallationID  domain.ID `json:"installation_id"`
	DeviceName      string    `json:"device_name"`
	Platform        string    `json:"platform"`
}

func (a *Auth) Login(ctx context.Context, in LoginInput) (domain.TokenPair, error) {
	name, e := normalizedLogin(in.Login)
	if e != nil {
		return domain.TokenPair{}, domain.Fail("invalid_credentials")
	}
	if len(in.Password) > 1024 {
		return domain.TokenPair{}, domain.Fail("invalid_credentials")
	}
	var user *domain.User
	if e = a.store.View(ctx, func(tx ports.Tx) error { var err error; user, err = tx.UserByLogin(ctx, name); return err }); e != nil {
		return domain.TokenPair{}, e
	}
	hash := a.dummy
	if user != nil {
		hash = user.PasswordHash
	}
	ok, e := a.hasher.Verify(ctx, hash, []byte(in.Password))
	if e != nil || !ok || user == nil || user.Status != "ACTIVE" {
		if e != nil && domain.Code(e) != "invalid_credentials" {
			return domain.TokenPair{}, e
		}
		return domain.TokenPair{}, domain.Fail("invalid_credentials")
	}
	if !domain.ValidID(string(in.InstallationID), false) || in.DeviceName == "" || len([]rune(in.DeviceName)) > 128 || (in.Platform != "windows" && in.Platform != "macos") {
		return domain.TokenPair{}, domain.Invalid("/device")
	}
	var pair domain.TokenPair
	for attempt := 0; attempt < 3; attempt++ {
		access, ah, e := token()
		if e != nil {
			return pair, e
		}
		refresh, rh, e := token()
		if e != nil {
			return pair, e
		}
		if ah == rh {
			continue
		}
		sid, e := domain.NewID()
		if e != nil {
			return pair, e
		}
		now := a.clock.Now()
		session := domain.Session{ID: sid, UserID: user.ID, InstallationID: in.InstallationID, DeviceName: in.DeviceName, Platform: in.Platform, CredentialVersion: user.CredentialVersion, Generation: 1, AccessHash: ah, RefreshHash: rh, CreatedMS: now.UnixMilli(), AccessExpiryMS: now.Add(a.AccessTTL).UnixMilli(), RefreshExpiryMS: now.Add(a.RefreshTTL).UnixMilli()}
		collision := false
		_, e = a.store.Update(ctx, func(tx ports.Tx) error {
			fresh, e := tx.User(ctx, user.ID)
			if e != nil {
				return e
			}
			if fresh == nil || fresh.Status != "ACTIVE" || fresh.CredentialVersion != user.CredentialVersion {
				return domain.Fail("invalid_credentials")
			}
			state, e := tx.State(ctx)
			if e != nil {
				return e
			}
			session.AuthEpoch = state.AuthEpoch
			for _, h := range [][32]byte{ah, rh} {
				for _, isRefresh := range []bool{false, true} {
					old, e := tx.SessionByHash(ctx, h, isRefresh)
					if e != nil {
						return e
					}
					if old != nil {
						collision = true
						return domain.Fail("token_source_failed")
					}
				}
				old, e := tx.RetiredRefresh(ctx, h)
				if e != nil {
					return e
				}
				if old != nil {
					collision = true
					return domain.Fail("token_source_failed")
				}
			}
			return tx.PutSession(ctx, session)
		})
		if e != nil {
			if collision {
				continue
			}
			return pair, e
		}
		pair = domain.TokenPair{ProtocolVersion: 1, Access: access, Refresh: refresh, AccessExpiry: domain.FromMillis(session.AccessExpiryMS), RefreshExpiry: domain.FromMillis(session.RefreshExpiryMS), SessionID: sid, Generation: 1}
		return pair, nil
	}
	return pair, domain.Fail("token_source_failed")
}
func checkSession(ctx context.Context, tx ports.Tx, s *domain.Session, now int64) error {
	if s == nil {
		return domain.Fail("auth_expired")
	}
	u, e := tx.User(ctx, s.UserID)
	if e != nil {
		return e
	}
	state, e := tx.State(ctx)
	if e != nil {
		return e
	}
	if s.RevokedMS != nil {
		return domain.Fail("device_revoked")
	}
	if u == nil || u.Status != "ACTIVE" {
		return domain.Fail("account_disabled")
	}
	if s.AuthEpoch != state.AuthEpoch || s.CredentialVersion != u.CredentialVersion || now >= s.AccessExpiryMS {
		return domain.Fail("auth_expired")
	}
	return nil
}
func (a *Auth) Authenticate(ctx context.Context, access string) (domain.Principal, error) {
	h, e := tokenHash(access)
	if e != nil {
		return domain.Principal{}, e
	}
	var p domain.Principal
	e = a.store.View(ctx, func(tx ports.Tx) error {
		s, e := tx.SessionByHash(ctx, h, false)
		if e != nil {
			return e
		}
		if e = checkSession(ctx, tx, s, a.clock.Now().UnixMilli()); e != nil {
			return e
		}
		p = domain.Principal{UserID: s.UserID, SessionID: s.ID, InstallationID: s.InstallationID, AuthEpoch: s.AuthEpoch, CredentialVersion: s.CredentialVersion, ExpiresAt: s.AccessExpiryMS}
		return nil
	})
	return p, e
}
func (a *Auth) Check(ctx context.Context, tx ports.Tx, p domain.Principal) error {
	s, e := tx.Session(ctx, p.SessionID)
	if e != nil {
		return e
	}
	if e = checkSession(ctx, tx, s, a.clock.Now().UnixMilli()); e != nil {
		return e
	}
	if s.UserID != p.UserID || s.InstallationID != p.InstallationID || s.AuthEpoch != p.AuthEpoch || s.CredentialVersion != p.CredentialVersion || a.clock.Now().UnixMilli() >= p.ExpiresAt {
		return domain.Fail("auth_expired")
	}
	return nil
}
func (a *Auth) Refresh(ctx context.Context, raw string) (domain.TokenPair, error) {
	h, e := tokenHash(raw)
	if e != nil {
		return domain.TokenPair{}, e
	}
	access, ah, e := token()
	if e != nil {
		return domain.TokenPair{}, e
	}
	refresh, rh, e := token()
	if e != nil {
		return domain.TokenPair{}, e
	}
	var out domain.TokenPair
	var rejection error
	_, e = a.store.Update(ctx, func(tx ports.Tx) error {
		s, e := tx.SessionByHash(ctx, h, true)
		if e != nil {
			return e
		}
		now := a.clock.Now()
		if s == nil {
			old, e := tx.RetiredRefresh(ctx, h)
			if e != nil {
				return e
			}
			if old == nil {
				return domain.Fail("auth_expired")
			}
			s, e = tx.Session(ctx, *old)
			if e != nil {
				return e
			}
			if s == nil || s.RevokedMS != nil {
				return domain.Fail("device_revoked")
			}
			s.RevokedMS = domain.Ptr(now.UnixMilli())
			if e = tx.PutSession(ctx, *s); e != nil {
				return e
			}
			rejection = domain.Fail("refresh_replayed")
			return nil
		}
		u, e := tx.User(ctx, s.UserID)
		if e != nil {
			return e
		}
		state, e := tx.State(ctx)
		if e != nil {
			return e
		}
		if s.RevokedMS != nil || u == nil || u.Status != "ACTIVE" || s.AuthEpoch != state.AuthEpoch || s.CredentialVersion != u.CredentialVersion || now.UnixMilli() >= s.RefreshExpiryMS {
			return domain.Fail("auth_expired")
		}
		if ah == rh {
			return domain.Fail("token_source_failed")
		}
		for _, hash := range [][32]byte{ah, rh} {
			for _, kind := range []bool{false, true} {
				v, e := tx.SessionByHash(ctx, hash, kind)
				if e != nil {
					return e
				}
				if v != nil {
					return domain.Fail("token_source_failed")
				}
			}
			v, e := tx.RetiredRefresh(ctx, hash)
			if e != nil {
				return e
			}
			if v != nil {
				return domain.Fail("token_source_failed")
			}
		}
		if e = tx.RetireRefresh(ctx, h, s.ID, now.UnixMilli(), s.RefreshExpiryMS); e != nil {
			return e
		}
		s.AccessHash = ah
		s.RefreshHash = rh
		s.Generation++
		s.AccessExpiryMS = min(now.Add(a.AccessTTL).UnixMilli(), s.RefreshExpiryMS)
		if e = tx.PutSession(ctx, *s); e != nil {
			return e
		}
		out = domain.TokenPair{ProtocolVersion: 1, Access: access, Refresh: refresh, AccessExpiry: domain.FromMillis(s.AccessExpiryMS), RefreshExpiry: domain.FromMillis(s.RefreshExpiryMS), SessionID: s.ID, Generation: domain.Counter(s.Generation)}
		return nil
	})
	if e != nil {
		return domain.TokenPair{}, e
	}
	if rejection != nil {
		return domain.TokenPair{}, rejection
	}
	return out, nil
}
func (a *Auth) Revoke(ctx context.Context, p domain.Principal, id domain.ID) error {
	_, e := a.store.Update(ctx, func(tx ports.Tx) error {
		if e := a.Check(ctx, tx, p); e != nil {
			return e
		}
		s, e := tx.Session(ctx, id)
		if e != nil {
			return e
		}
		if s == nil || s.UserID != p.UserID {
			return domain.Fail("not_found")
		}
		if s.RevokedMS != nil {
			return nil
		}
		s.RevokedMS = domain.Ptr(a.clock.Now().UnixMilli())
		return tx.PutSession(ctx, *s)
	})
	return e
}
func (a *Auth) ChangePassword(ctx context.Context, p domain.Principal, oldPassword, newPassword []byte) error {
	u, e := a.Reauthenticate(ctx, p, oldPassword)
	if e != nil {
		return e
	}
	if e = passwordPolicy(newPassword); e != nil {
		return e
	}
	hash, e := a.hasher.Hash(ctx, newPassword)
	if e != nil {
		return e
	}
	_, e = a.store.Update(ctx, func(tx ports.Tx) error {
		if e := a.Check(ctx, tx, p); e != nil {
			return e
		}
		current, e := tx.User(ctx, p.UserID)
		if e != nil {
			return e
		}
		if current.CredentialVersion != u.CredentialVersion {
			return domain.Fail("reauthentication_required")
		}
		current.PasswordHash = hash
		current.CredentialVersion++
		current.UpdatedMS = a.clock.Now().UnixMilli()
		if e = tx.PutUser(ctx, *current); e != nil {
			return e
		}
		if e = tx.RevokeSessions(ctx, p.UserID, current.UpdatedMS); e != nil {
			return e
		}
		return tx.CancelIntents(ctx, p.UserID, current.UpdatedMS)
	})
	return e
}
func (a *Auth) Reauthenticate(ctx context.Context, p domain.Principal, password []byte) (domain.User, error) {
	var u *domain.User
	e := a.store.View(ctx, func(tx ports.Tx) error {
		if e := a.Check(ctx, tx, p); e != nil {
			return e
		}
		var e error
		u, e = tx.User(ctx, p.UserID)
		return e
	})
	if e != nil {
		return domain.User{}, e
	}
	ok, e := a.hasher.Verify(ctx, u.PasswordHash, password)
	if e != nil {
		return domain.User{}, e
	}
	if !ok {
		return domain.User{}, domain.Fail("reauthentication_required")
	}
	return *u, nil
}
func (a *Auth) Me(ctx context.Context, p domain.Principal) (map[string]any, error) {
	var out map[string]any
	e := a.store.View(ctx, func(tx ports.Tx) error {
		if e := a.Check(ctx, tx, p); e != nil {
			return e
		}
		u, e := tx.User(ctx, p.UserID)
		if e != nil {
			return e
		}
		ds, e := tx.Datasets(ctx, p.UserID)
		if e != nil {
			return e
		}
		out = map[string]any{"protocol_version": 1, "user_id": u.ID, "login": u.Login, "datasets": ds}
		return nil
	})
	return out, e
}
func (a *Auth) Devices(ctx context.Context, p domain.Principal) ([]domain.SessionView, error) {
	out := []domain.SessionView{}
	e := a.store.View(ctx, func(tx ports.Tx) error {
		if e := a.Check(ctx, tx, p); e != nil {
			return e
		}
		sessions, e := tx.Sessions(ctx, p.UserID)
		if e != nil {
			return e
		}
		for _, s := range sessions {
			if s.RevokedMS == nil && s.RefreshExpiryMS > a.clock.Now().UnixMilli() {
				out = append(out, domain.SessionView{ID: s.ID, InstallationID: s.InstallationID, DeviceName: s.DeviceName, Platform: s.Platform, CreatedAt: domain.FromMillis(s.CreatedMS), ExpiresAt: domain.FromMillis(s.RefreshExpiryMS), Current: s.ID == p.SessionID})
			}
		}
		return nil
	})
	return out, e
}
