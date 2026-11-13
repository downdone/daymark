package business

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"crypto/subtle"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"regexp"
	"slices"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"github.com/pquerna/otp"
	"github.com/pquerna/otp/totp"
)

type AdminService struct {
	Store     ports.BusinessStore
	Hasher    ports.Hasher
	Clock     ports.Clock
	key       []byte
	dummyHash string
}

func NewAdmin(store ports.BusinessStore, hasher ports.Hasher, clock ports.Clock, key []byte) (*AdminService, error) {
	if len(key) != 32 {
		return nil, domain.Fail("storage_corrupt")
	}
	secret, e := RandomToken()
	if e != nil {
		return nil, e
	}
	hash, e := hasher.Hash(context.Background(), []byte(secret))
	if e != nil {
		return nil, e
	}
	return &AdminService{Store: store, Hasher: hasher, Clock: clock, key: append([]byte(nil), key...), dummyHash: hash}, nil
}
func RandomToken() (string, error) {
	b := make([]byte, 24)
	if _, e := rand.Read(b); e != nil {
		return "", domain.Wrap("token_source_failed", e)
	}
	return base64.RawURLEncoding.EncodeToString(b), nil
}
func tokenHash(s string) [32]byte { return sha256.Sum256([]byte(s)) }
func (s *AdminService) CSRF(token string) string {
	h := hmac.New(sha256.New, s.key)
	h.Write([]byte("admin-csrf-v1:" + token))
	return base64.RawURLEncoding.EncodeToString(h.Sum(nil))
}
func (s *AdminService) ValidCSRF(token, value string) bool {
	return len(token) >= 32 && hmac.Equal([]byte(s.CSRF(token)), []byte(value))
}
func (s *AdminService) encrypt(id domain.ID, secret string) ([]byte, error) {
	b, e := aes.NewCipher(s.key)
	if e != nil {
		return nil, e
	}
	g, e := cipher.NewGCM(b)
	if e != nil {
		return nil, e
	}
	nonce := make([]byte, g.NonceSize())
	if _, e = rand.Read(nonce); e != nil {
		return nil, e
	}
	return g.Seal(nonce, nonce, []byte(secret), []byte(id)), nil
}
func (s *AdminService) decrypt(a domain.Admin) (string, error) {
	b, e := aes.NewCipher(s.key)
	if e != nil {
		return "", e
	}
	g, e := cipher.NewGCM(b)
	if e != nil || len(a.SecretCipher) < g.NonceSize() {
		return "", domain.Fail("storage_corrupt")
	}
	plain, e := g.Open(nil, a.SecretCipher[:g.NonceSize()], a.SecretCipher[g.NonceSize():], []byte(a.ID))
	if e != nil {
		return "", domain.Fail("storage_corrupt")
	}
	defer clear(plain)
	return string(plain), nil
}

var adminLogin = regexp.MustCompile(`^[a-z0-9][a-z0-9_.-]{2,63}$`)
var versionPattern = regexp.MustCompile(`^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$`)

func ValidRoles(roles []string) bool {
	if len(roles) == 0 || len(roles) > 5 {
		return false
	}
	seen := map[string]bool{}
	for _, r := range roles {
		if !slices.Contains([]string{"owner", "support", "publisher", "analyst", "auditor"}, r) || seen[r] {
			return false
		}
		seen[r] = true
	}
	return true
}
func Allowed(a domain.Admin, permission string) bool {
	if slices.Contains(a.Roles, "owner") {
		return true
	}
	role := map[string]string{"users": "support", "notices": "publisher", "metrics": "analyst", "audit": "auditor"}[permission]
	return role != "" && slices.Contains(a.Roles, role)
}

type Enrollment struct {
	ID            domain.ID `json:"id"`
	Secret        string    `json:"secret"`
	URL           string    `json:"otpauth_url"`
	RecoveryCodes []string  `json:"recovery_codes"`
}

// Provision is a local maintenance operation. Secrets are returned once to the
// maintenance terminal; they are never included in HTTP responses or logs.
func (s *AdminService) Provision(ctx context.Context, login string, password []byte, roles []string) (Enrollment, error) {
	return s.provision(ctx, login, password, roles, false)
}

// Recover is only reachable through the exclusive-directory maintenance CLI.
// It rotates every credential and invalidates existing sessions by version.
func (s *AdminService) Recover(ctx context.Context, login string, password []byte) (Enrollment, error) {
	return s.provision(ctx, login, password, []string{"owner"}, true)
}
func (s *AdminService) provision(ctx context.Context, login string, password []byte, roles []string, recover bool) (Enrollment, error) {
	var out Enrollment
	login = strings.ToLower(strings.TrimSpace(login))
	if !adminLogin.MatchString(login) || !utf8.Valid(password) || utf8.RuneCount(password) < 8 || len(password) > 1024 || !ValidRoles(roles) {
		return out, domain.Invalid("/admin")
	}
	id, e := domain.NewID()
	if e != nil {
		return out, e
	}
	key, e := totp.Generate(totp.GenerateOpts{Issuer: "Daymark Admin", AccountName: login, SecretSize: 20})
	if e != nil {
		return out, domain.Wrap("token_source_failed", e)
	}
	hash, e := s.Hasher.Hash(ctx, password)
	if e != nil {
		return out, e
	}
	encrypted, e := s.encrypt(id, key.Secret())
	if e != nil {
		return out, e
	}
	codes := []string{}
	hashes := []string{}
	for i := 0; i < 8; i++ {
		code, e := RandomToken()
		if e != nil {
			return out, e
		}
		codes = append(codes, code)
		h := tokenHash(code)
		hashes = append(hashes, hex.EncodeToString(h[:]))
	}
	_, e = s.Store.UpdateBusiness(ctx, func(tx ports.BusinessTx) error {
		admins, e := tx.Admins(ctx)
		if e != nil {
			return e
		}
		if !recover && len(admins) >= 100 {
			return domain.Fail("quota_exceeded")
		}
		old, e := tx.AdminByLogin(ctx, login)
		if e != nil {
			return e
		}
		if recover {
			if old == nil {
				return domain.Fail("not_found")
			}
			id = old.ID
			old.SecretCipher, e = s.encrypt(id, key.Secret())
			if e != nil {
				return e
			}
			old.PasswordHash, old.RecoveryHashes = hash, hashes
			old.Status, old.LastStep = "ACTIVE", -1
			old.Version++
			if e = tx.PutAdmin(ctx, *old); e != nil {
				return e
			}
			return tx.AppendAudit(ctx, domain.Audit{Actor: id, Action: "admin.recover", Target: id, Reason: "local_maintenance", AtMS: s.Clock.Now().UnixMilli()})
		}
		if old != nil {
			return domain.Fail("admin_conflict")
		}
		if len(admins) == 0 && !slices.Contains(roles, "owner") {
			return domain.Fail("admin_last_owner")
		}
		if e = tx.PutAdmin(ctx, domain.Admin{ID: id, Login: login, PasswordHash: hash, SecretCipher: encrypted, RecoveryHashes: hashes, Roles: roles, Status: "ACTIVE", Version: 1, LastStep: -1, CreatedMS: s.Clock.Now().UnixMilli()}); e != nil {
			return e
		}
		return tx.AppendAudit(ctx, domain.Audit{Actor: id, Action: "admin.provision", Target: id, Reason: "local_maintenance", AtMS: s.Clock.Now().UnixMilli()})
	})
	if e != nil {
		return out, e
	}
	return Enrollment{id, key.Secret(), key.URL(), codes}, nil
}
func (s *AdminService) Login(ctx context.Context, login string, password []byte, code, requestID string) (string, error) {
	login = strings.ToLower(strings.TrimSpace(login))
	if !adminLogin.MatchString(login) || len(password) > 1024 || len(code) > 64 {
		return "", domain.Fail("invalid_credentials")
	}
	var a *domain.Admin
	e := s.Store.ViewBusiness(ctx, func(tx ports.BusinessTx) error { var e error; a, e = tx.AdminByLogin(ctx, login); return e })
	if e != nil {
		return "", e
	}
	if a == nil || a.Status != "ACTIVE" {
		// Match the password KDF cost without revealing whether an identity exists.
		if _, e := s.Hasher.Verify(ctx, s.dummyHash, password); e != nil {
			return "", e
		}
		return "", domain.Fail("invalid_credentials")
	}
	ok, e := s.Hasher.Verify(ctx, a.PasswordHash, password)
	if e != nil || !ok {
		return "", domain.Fail("invalid_credentials")
	}
	secret, e := s.decrypt(*a)
	if e != nil {
		return "", e
	}
	now := s.Clock.Now()
	step := int64(-1)
	recovery := ""
	if len(code) == 6 {
		for _, offset := range []int64{0, -1, 1} {
			candidate := now.Unix()/30 + offset
			valid, e := totp.ValidateCustom(code, secret, time.Unix(candidate*30, 0), totp.ValidateOpts{Period: 30, Skew: 0, Digits: otp.DigitsSix, Algorithm: otp.AlgorithmSHA1})
			if e == nil && valid {
				step = candidate
				break
			}
		}
	} else {
		h := tokenHash(strings.TrimSpace(code))
		recovery = hex.EncodeToString(h[:])
		if !slices.Contains(a.RecoveryHashes, recovery) {
			recovery = ""
		}
	}
	secret = ""
	if step < 0 && recovery == "" {
		return "", domain.Fail("admin_mfa_invalid")
	}
	token, e := RandomToken()
	if e != nil {
		return "", e
	}
	hash := tokenHash(token)
	_, e = s.Store.UpdateBusiness(ctx, func(tx ports.BusinessTx) error {
		current, e := tx.Admin(ctx, a.ID)
		if e != nil {
			return e
		}
		if current == nil || current.Status != "ACTIVE" || current.Version != a.Version || current.PasswordHash != a.PasswordHash {
			return domain.Fail("invalid_credentials")
		}
		if recovery != "" {
			i := slices.Index(current.RecoveryHashes, recovery)
			if i < 0 {
				return domain.Fail("admin_mfa_invalid")
			}
			current.RecoveryHashes = slices.Delete(current.RecoveryHashes, i, i+1)
		} else {
			if step <= current.LastStep {
				return domain.Fail("admin_mfa_replayed")
			}
			current.LastStep = step
		}
		state, e := tx.State(ctx)
		if e != nil {
			return e
		}
		if e = tx.PutAdmin(ctx, *current); e != nil {
			return e
		}
		if e = tx.PutAdminSession(ctx, domain.AdminSession{Hash: hash, AdminID: a.ID, Version: a.Version, AuthEpoch: state.AuthEpoch, CreatedMS: now.UnixMilli(), ExpiresMS: now.Add(8 * time.Hour).UnixMilli()}); e != nil {
			return e
		}
		return tx.AppendAudit(ctx, domain.Audit{Actor: a.ID, Action: "admin.login", Target: a.ID, RequestID: requestID, AtMS: now.UnixMilli()})
	})
	if e != nil {
		return "", e
	}
	return token, nil
}
func (s *AdminService) authorize(ctx context.Context, tx ports.BusinessTx, token, permission string, fresh bool) (domain.Admin, error) {
	if len(token) != 32 {
		return domain.Admin{}, domain.Fail("admin_session_expired")
	}
	session, e := tx.AdminSession(ctx, tokenHash(token))
	if e != nil {
		return domain.Admin{}, e
	}
	now := s.Clock.Now().UnixMilli()
	if session == nil || session.ExpiresMS <= now {
		return domain.Admin{}, domain.Fail("admin_session_expired")
	}
	state, e := tx.State(ctx)
	if e != nil {
		return domain.Admin{}, e
	}
	a, e := tx.Admin(ctx, session.AdminID)
	if e != nil {
		return domain.Admin{}, e
	}
	if a == nil || a.Status != "ACTIVE" || session.Version != a.Version || session.AuthEpoch != state.AuthEpoch {
		return domain.Admin{}, domain.Fail("admin_session_expired")
	}
	if permission != "" && !Allowed(*a, permission) {
		return domain.Admin{}, domain.Fail("admin_forbidden")
	}
	if fresh && now-session.CreatedMS > 15*60*1000 {
		return domain.Admin{}, domain.Fail("admin_reauthentication_required")
	}
	return *a, nil
}
func (s *AdminService) Me(ctx context.Context, token, permission string) (domain.Admin, error) {
	var a domain.Admin
	e := s.Store.ViewBusiness(ctx, func(tx ports.BusinessTx) error {
		var e error
		a, e = s.authorize(ctx, tx, token, permission, false)
		return e
	})
	return a, e
}
func (s *AdminService) Logout(ctx context.Context, token string) error {
	_, e := s.Store.UpdateBusiness(ctx, func(tx ports.BusinessTx) error { return tx.RemoveAdminSession(ctx, tokenHash(token)) })
	return e
}
func (s *AdminService) Read(ctx context.Context, token, permission string, fn func(ports.BusinessTx) error) error {
	return s.Store.ViewBusiness(ctx, func(tx ports.BusinessTx) error {
		if _, e := s.authorize(ctx, tx, token, permission, false); e != nil {
			return e
		}
		return fn(tx)
	})
}
func (s *AdminService) command(ctx context.Context, token, permission, action string, operation domain.ID, payload any, target domain.ID, reason, requestID string, fn func(ports.BusinessTx, domain.Admin) (domain.CommandReceipt, error)) (domain.CommandReceipt, error) {
	var out domain.CommandReceipt
	if !domain.ValidID(string(operation), false) || !domain.ValidID(requestID, false) {
		return out, domain.Invalid("/operation_id")
	}
	body, e := json.Marshal(payload)
	if e != nil {
		return out, e
	}
	sum := sha256.Sum256(append([]byte(action+":"), body...))
	hash := hex.EncodeToString(sum[:])
	_, e = s.Store.UpdateBusiness(ctx, func(tx ports.BusinessTx) error {
		a, e := s.authorize(ctx, tx, token, permission, true)
		if e != nil {
			return e
		}
		prior, e := tx.BusinessOperation(ctx, a.ID, operation)
		if e != nil {
			return e
		}
		if prior != nil {
			if subtle.ConstantTimeCompare([]byte(prior.Hash), []byte(hash)) != 1 {
				return domain.Fail("admin_conflict")
			}
			if json.Unmarshal([]byte(prior.Result), &out) != nil {
				return domain.Fail("storage_corrupt")
			}
			return nil
		}
		out, e = fn(tx, a)
		if e != nil {
			return e
		}
		if target == "" {
			target = out.ID
		}
		now := s.Clock.Now().UnixMilli()
		if e = tx.AppendAudit(ctx, domain.Audit{Actor: a.ID, Action: action, Target: target, Reason: reason, RequestID: requestID, AtMS: now}); e != nil {
			return e
		}
		receipt, _ := json.Marshal(out)
		return tx.PutBusinessOperation(ctx, domain.BusinessOperation{Actor: a.ID, ID: operation, Hash: hash, Result: string(receipt), AtMS: now})
	})
	return out, e
}

type UserCommand struct {
	OperationID domain.ID `json:"operation_id"`
	Version     int64     `json:"version"`
	Status      string    `json:"status"`
	Reason      string    `json:"reason"`
}

func (s *AdminService) UserCommand(ctx context.Context, token string, id domain.ID, in UserCommand, revoke bool, requestID string) (domain.CommandReceipt, error) {
	if !domain.ValidID(string(id), false) || in.Version < 1 || !slices.Contains([]string{"security", "user_request", "policy", "maintenance"}, in.Reason) || (!revoke && in.Status != "ACTIVE" && in.Status != "DISABLED") {
		return domain.CommandReceipt{}, domain.Invalid("/user")
	}
	action := "user.status"
	if revoke {
		action = "user.revoke_sessions"
	}
	return s.command(ctx, token, "users", action, in.OperationID, struct {
		ID    domain.ID
		Input UserCommand
	}{id, in}, id, in.Reason, requestID, func(tx ports.BusinessTx, a domain.Admin) (domain.CommandReceipt, error) {
		u, e := tx.User(ctx, id)
		if e != nil {
			return domain.CommandReceipt{}, e
		}
		if u == nil {
			return domain.CommandReceipt{}, domain.Fail("not_found")
		}
		if u.Status == "DELETING" || u.Status == "DELETED" {
			return domain.CommandReceipt{}, domain.Fail("admin_conflict")
		}
		if u.CredentialVersion != in.Version {
			return domain.CommandReceipt{}, domain.Fail("admin_conflict")
		}
		u.CredentialVersion++
		u.UpdatedMS = s.Clock.Now().UnixMilli()
		if !revoke {
			u.Status = in.Status
		}
		if e = tx.RevokeSessions(ctx, id, u.UpdatedMS); e != nil {
			return domain.CommandReceipt{}, e
		}
		e = tx.PutUser(ctx, *u)
		return domain.CommandReceipt{ID: id, Version: u.CredentialVersion}, e
	})
}

type AdminCommand struct {
	OperationID domain.ID `json:"operation_id"`
	Version     int64     `json:"version"`
	Status      string    `json:"status"`
	Roles       []string  `json:"roles"`
}

func (s *AdminService) AdminCommand(ctx context.Context, token string, id domain.ID, in AdminCommand, requestID string) (domain.CommandReceipt, error) {
	if !domain.ValidID(string(id), false) || in.Version < 1 || !ValidRoles(in.Roles) || !slices.Contains([]string{"ACTIVE", "DISABLED"}, in.Status) {
		return domain.CommandReceipt{}, domain.Invalid("/admin")
	}
	return s.command(ctx, token, "owner", "admin.permissions", in.OperationID, struct {
		ID    domain.ID
		Input AdminCommand
	}{id, in}, id, "operator_request", requestID, func(tx ports.BusinessTx, actor domain.Admin) (domain.CommandReceipt, error) {
		a, e := tx.Admin(ctx, id)
		if e != nil {
			return domain.CommandReceipt{}, e
		}
		if a == nil {
			return domain.CommandReceipt{}, domain.Fail("not_found")
		}
		if a.Version != in.Version {
			return domain.CommandReceipt{}, domain.Fail("admin_conflict")
		}
		if actor.ID == id && !slices.Equal(a.Roles, in.Roles) {
			return domain.CommandReceipt{}, domain.Fail("admin_forbidden")
		}
		admins, e := tx.Admins(ctx)
		if e != nil {
			return domain.CommandReceipt{}, e
		}
		owners := 0
		for _, v := range admins {
			if v.ID != id && v.Status == "ACTIVE" && slices.Contains(v.Roles, "owner") {
				owners++
			}
		}
		if owners == 0 && (in.Status != "ACTIVE" || !slices.Contains(in.Roles, "owner")) {
			return domain.CommandReceipt{}, domain.Fail("admin_last_owner")
		}
		a.Status = in.Status
		a.Roles = in.Roles
		a.Version++
		e = tx.PutAdmin(ctx, *a)
		return domain.CommandReceipt{ID: id, Version: a.Version}, e
	})
}

type NoticeCommand struct {
	OperationID domain.ID   `json:"operation_id"`
	Version     int64       `json:"version"`
	Title       string      `json:"title"`
	Body        string      `json:"body"`
	Action      string      `json:"action"`
	Platform    string      `json:"platform"`
	MinVersion  string      `json:"min_version"`
	MaxVersion  string      `json:"max_version"`
	Targets     []domain.ID `json:"targets"`
	PublishMS   int64       `json:"publish_ms"`
	ExpireMS    int64       `json:"expire_ms"`
}

func (s *AdminService) NoticeCommand(ctx context.Context, token string, id domain.ID, in NoticeCommand, action, requestID string) (domain.CommandReceipt, error) {
	if !slices.Contains([]string{"create", "edit", "publish", "revoke"}, action) || id != "" && !domain.ValidID(string(id), false) {
		return domain.CommandReceipt{}, domain.Invalid("/notice")
	}
	if action == "create" || action == "edit" {
		if !utf8.ValidString(in.Title) || strings.IndexFunc(in.Title, unicode.IsControl) >= 0 || len(in.MinVersion) > 32 || len(in.MaxVersion) > 32 || versionRangeInvalid(in.MinVersion, in.MaxVersion) {
			return domain.CommandReceipt{}, domain.Invalid("/notice")
		}
		if utf8.RuneCountInString(strings.TrimSpace(in.Title)) < 1 || utf8.RuneCountInString(in.Title) > 80 || utf8.RuneCountInString(in.Body) > 4000 || !utf8.ValidString(in.Body) || len(in.Targets) > 1000 || !slices.Contains([]string{"none", "open_account", "check_update", "release_notes"}, in.Action) || !slices.Contains([]string{"all", "windows", "macos"}, in.Platform) || in.MinVersion != "" && !versionPattern.MatchString(in.MinVersion) || in.MaxVersion != "" && !versionPattern.MatchString(in.MaxVersion) {
			return domain.CommandReceipt{}, domain.Invalid("/notice")
		}
		seen := map[domain.ID]bool{}
		for _, v := range in.Targets {
			if !domain.ValidID(string(v), false) || seen[v] {
				return domain.CommandReceipt{}, domain.Invalid("/targets")
			}
			seen[v] = true
		}
	}
	return s.command(ctx, token, "notices", "notice."+action, in.OperationID, struct {
		ID    domain.ID
		Input NoticeCommand
	}{id, in}, id, "operator_request", requestID, func(tx ports.BusinessTx, a domain.Admin) (domain.CommandReceipt, error) {
		now := s.Clock.Now().UnixMilli()
		var n *domain.Notice
		var e error
		if action == "create" {
			page, e := tx.Notices(ctx, "", 1001)
			if e != nil {
				return domain.CommandReceipt{}, e
			}
			if len(page) >= 1000 {
				return domain.CommandReceipt{}, domain.Fail("quota_exceeded")
			}
			newID, e := domain.NewID()
			if e != nil {
				return domain.CommandReceipt{}, e
			}
			n = &domain.Notice{ID: newID, State: "draft", CreatedBy: a.ID}
		} else {
			n, e = tx.Notice(ctx, id)
			if e != nil {
				return domain.CommandReceipt{}, e
			}
			if n == nil {
				return domain.CommandReceipt{}, domain.Fail("not_found")
			}
			if n.Version != in.Version || n.State == "revoked" {
				return domain.CommandReceipt{}, domain.Fail("admin_conflict")
			}
		}
		switch action {
		case "create", "edit":
			for _, target := range in.Targets {
				u, e := tx.User(ctx, target)
				if e != nil {
					return domain.CommandReceipt{}, e
				}
				if u == nil || u.Status == "DELETED" {
					return domain.CommandReceipt{}, domain.Fail("not_found")
				}
			}
			if action == "edit" && n.State != "draft" {
				return domain.CommandReceipt{}, domain.Fail("admin_conflict")
			}
			n.Title = strings.TrimSpace(in.Title)
			n.Body = in.Body
			n.Action = in.Action
			n.Platform = in.Platform
			n.MinVersion = in.MinVersion
			n.MaxVersion = in.MaxVersion
			n.Targets = in.Targets
			n.PublishMS = in.PublishMS
			n.ExpireMS = in.ExpireMS
		case "publish":
			if n.State != "draft" {
				return domain.CommandReceipt{}, domain.Fail("admin_conflict")
			}
			if n.PublishMS == 0 {
				n.PublishMS = now
			}
			if n.ExpireMS <= n.PublishMS || n.ExpireMS <= now || n.ExpireMS-now > 90*24*60*60*1000 {
				return domain.CommandReceipt{}, domain.Invalid("/expire_ms")
			}
			n.State = "published"
			if n.PublishMS > now {
				n.State = "scheduled"
			}
		case "revoke":
			n.State = "revoked"
		}
		n.Version++
		n.UpdatedMS = now
		e = tx.PutNotice(ctx, *n)
		return domain.CommandReceipt{ID: n.ID, Version: n.Version}, e
	})
}

// Compare bounded decimal components without integer overflow.
func versionRangeInvalid(minimum, maximum string) bool {
	if minimum == "" || maximum == "" {
		return false
	}
	if !versionPattern.MatchString(minimum) || !versionPattern.MatchString(maximum) || len(minimum) > 32 || len(maximum) > 32 {
		return true
	}
	a, b := strings.Split(minimum, "."), strings.Split(maximum, ".")
	for i := range a {
		if len(a[i]) != len(b[i]) {
			return len(a[i]) > len(b[i])
		}
		if a[i] != b[i] {
			return a[i] > b[i]
		}
	}
	return false
}
