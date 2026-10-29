package application

import (
	"context"
	"crypto/subtle"
	"encoding/json"
	"slices"
	"sync"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
)

type Account struct {
	Store    ports.Store
	Auth     *Auth
	Clock    ports.Clock
	Ledger   ports.Ledger
	Files    ports.Artifacts
	workerID string
	cancel   context.CancelFunc
	wg       sync.WaitGroup
	OnError  func(error)
}
type DeletionIntent struct {
	ProtocolVersion int            `json:"protocol_version"`
	OperationID     domain.ID      `json:"operation_id"`
	Token           string         `json:"deletion_token"`
	ConfirmUntil    domain.Instant `json:"confirm_until_utc"`
	ExpiresAt       domain.Instant `json:"expires_at_utc"`
}
type DeletionStatus struct {
	ProtocolVersion int            `json:"protocol_version"`
	OperationID     domain.ID      `json:"operation_id"`
	State           string         `json:"state"`
	UpdatedAt       domain.Instant `json:"updated_at_utc"`
}
type deletionMetadata struct {
	OperationID domain.ID   `json:"operation_id"`
	UserID      domain.ID   `json:"user_id"`
	Artifacts   []domain.ID `json:"artifacts"`
	NotBeforeMS int64       `json:"not_before_ms"`
}

func NewAccount(ctx context.Context, store ports.Store, auth *Auth, clock ports.Clock, ledger ports.Ledger, files ports.Artifacts) (*Account, error) {
	if _, e := ledger.Read(ctx); e != nil {
		return nil, e
	}
	_, e := store.Update(ctx, func(tx ports.Tx) error { return tx.BindLedger(ctx, ledger.ID()) })
	if e != nil {
		return nil, e
	}
	worker, e := domain.NewID()
	if e != nil {
		return nil, e
	}
	return &Account{Store: store, Auth: auth, Clock: clock, Ledger: ledger, Files: files, workerID: string(worker)}, nil
}
func (a *Account) Prepare(ctx context.Context, p domain.Principal, password []byte, operation domain.ID) (DeletionIntent, error) {
	var out DeletionIntent
	if !domain.ValidID(string(operation), false) {
		return out, domain.Invalid("/operation_id")
	}
	user, e := a.Auth.Reauthenticate(ctx, p, password)
	if e != nil {
		return out, e
	}
	capability, hash, e := token()
	if e != nil {
		return out, e
	}
	now := a.Clock.Now()
	_, e = a.Store.Update(ctx, func(tx ports.Tx) error {
		if e := a.Auth.Check(ctx, tx, p); e != nil {
			return e
		}
		fresh, e := tx.User(ctx, p.UserID)
		if e != nil {
			return e
		}
		if fresh.CredentialVersion != user.CredentialVersion {
			return domain.Fail("reauthentication_required")
		}
		old, e := tx.Deletion(ctx, operation)
		if e != nil {
			return e
		}
		if old != nil {
			if old.UserID != p.UserID {
				return domain.Fail("not_found")
			}
			if old.State != "INTENT" {
				return domain.Fail("job_state_conflict")
			}
		}
		state, e := tx.State(ctx)
		if e != nil {
			return e
		}
		d := domain.Deletion{OperationID: operation, UserID: p.UserID, AuthEpoch: state.AuthEpoch, CredentialVersion: fresh.CredentialVersion, State: "INTENT", TokenHash: hash, ConfirmUntilMS: now.Add(5 * time.Minute).UnixMilli(), ExpiresMS: now.Add(30 * 24 * time.Hour).UnixMilli(), RequestedMS: now.UnixMilli(), UpdatedMS: now.UnixMilli()}
		if e = tx.PutDeletion(ctx, d); e != nil {
			return e
		}
		out = DeletionIntent{1, operation, capability, domain.FromMillis(d.ConfirmUntilMS), domain.FromMillis(d.ExpiresMS)}
		return nil
	})
	return out, e
}
func (a *Account) capability(ctx context.Context, tx ports.Tx, raw string, operation domain.ID) (domain.Deletion, error) {
	h, e := tokenHash(raw)
	if e != nil {
		return domain.Deletion{}, e
	}
	d, e := tx.Deletion(ctx, operation)
	if e != nil {
		return domain.Deletion{}, e
	}
	if d == nil || subtle.ConstantTimeCompare(h[:], d.TokenHash[:]) != 1 {
		return domain.Deletion{}, domain.Fail("auth_expired")
	}
	state, e := tx.State(ctx)
	if e != nil {
		return domain.Deletion{}, e
	}
	if state.AuthEpoch != d.AuthEpoch || a.Clock.Now().UnixMilli() >= d.ExpiresMS {
		return domain.Deletion{}, domain.Fail("auth_expired")
	}
	return *d, nil
}
func status(d domain.Deletion) DeletionStatus {
	return DeletionStatus{1, d.OperationID, d.State, domain.FromMillis(d.UpdatedMS)}
}
func (a *Account) Status(ctx context.Context, capability string, operation domain.ID) (DeletionStatus, error) {
	var out DeletionStatus
	e := a.Store.View(ctx, func(tx ports.Tx) error {
		d, e := a.capability(ctx, tx, capability, operation)
		if e != nil {
			return e
		}
		if d.State == "INTENT" && a.Clock.Now().UnixMilli() >= d.ConfirmUntilMS {
			d.State = "EXPIRED"
		}
		out = status(d)
		return nil
	})
	return out, e
}
func (a *Account) Confirm(ctx context.Context, capability string, operation domain.ID) (DeletionStatus, error) {
	var out DeletionStatus
	jobID, e := domain.NewID()
	if e != nil {
		return out, e
	}
	_, e = a.Store.Update(ctx, func(tx ports.Tx) error {
		d, e := a.capability(ctx, tx, capability, operation)
		if e != nil {
			return e
		}
		if slices.Contains([]string{"PENDING_LEDGER", "LEDGER_CONFIRMED", "COMPLETED"}, d.State) {
			out = status(d)
			return nil
		}
		if d.State != "INTENT" {
			return domain.Fail("job_state_conflict")
		}
		now := a.Clock.Now().UnixMilli()
		if now >= d.ConfirmUntilMS {
			return domain.Fail("deletion_intent_expired")
		}
		user, e := tx.User(ctx, d.UserID)
		if e != nil {
			return e
		}
		if user == nil || user.Status != "ACTIVE" || user.CredentialVersion != d.CredentialVersion {
			return domain.Fail("reauthentication_required")
		}
		jobs, e := tx.AllJobs(ctx, &d.UserID)
		if e != nil {
			return e
		}
		meta := deletionMetadata{OperationID: operation, UserID: d.UserID, Artifacts: []domain.ID{}, NotBeforeMS: now}
		for _, j := range jobs {
			meta.Artifacts = append(meta.Artifacts, j.ID)
			if j.State == "UPLOADING" {
				meta.NotBeforeMS = max(meta.NotBeforeMS, now+30000)
			}
			if j.LeaseUntil != nil {
				meta.NotBeforeMS = max(meta.NotBeforeMS, *j.LeaseUntil)
			}
			if j.State != "COMMITTED" {
				expected := j.State
				j.State = "CANCELLED"
				j.UpdatedMS = now
				if e = tx.UpdateJob(ctx, j, expected, j.Attempt); e != nil {
					return e
				}
			}
			if e = tx.ReleasePin(ctx, j.ID); e != nil {
				return e
			}
		}
		metadata, e := json.Marshal(meta)
		if e != nil {
			return e
		}
		key, e := operationKey(d.UserID, nil, "DELETE_ACCOUNT", operation)
		if e != nil {
			return e
		}
		job := domain.Job{ProtocolVersion: 1, ID: jobID, OwnerID: &d.UserID, Kind: "DELETE_ACCOUNT", State: "BUILDING", OperationKey: key, Metadata: metadata, ExpiresAt: domain.At(a.Clock.Now().AddDate(100, 0, 0)), CreatedMS: now, UpdatedMS: now}
		if e = tx.InsertJob(ctx, job); e != nil {
			return e
		}
		user.Status = "DELETING"
		user.UpdatedMS = now
		if e = tx.PutUser(ctx, *user); e != nil {
			return e
		}
		if e = tx.RevokeSessions(ctx, d.UserID, now); e != nil {
			return e
		}
		d.State = "PENDING_LEDGER"
		d.JobID = &jobID
		d.UpdatedMS = now
		if e = tx.PutDeletion(ctx, d); e != nil {
			return e
		}
		if e = tx.CancelIntents(ctx, d.UserID, now); e != nil {
			return e
		}
		out = status(d)
		return nil
	})
	return out, e
}
func (a *Account) Start(parent context.Context) {
	ctx, cancel := context.WithCancel(parent)
	a.cancel = cancel
	a.wg.Add(1)
	go func() {
		defer a.wg.Done()
		ticker := time.NewTicker(time.Second)
		defer ticker.Stop()
		for {
			ran, e := a.RunOne(ctx)
			if e != nil && a.OnError != nil {
				a.OnError(e)
			}
			if ran && e == nil {
				continue
			}
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
			}
		}
	}()
}
func (a *Account) Stop() {
	if a.cancel != nil {
		a.cancel()
	}
	a.wg.Wait()
}
func (a *Account) RunOne(parent context.Context) (bool, error) {
	ctx, cancel := context.WithTimeout(parent, 30*time.Second)
	defer cancel()
	var job *domain.Job
	var meta deletionMetadata
	_, e := a.Store.Update(ctx, func(tx ports.Tx) error {
		jobs, e := tx.RunnableJobs(ctx, a.Clock.Now().UnixMilli(), 128, "DELETE_ACCOUNT")
		if e != nil {
			return e
		}
		for _, j := range jobs {
			if j.Kind != "DELETE_ACCOUNT" {
				continue
			}
			if e = json.Unmarshal(j.Metadata, &meta); e != nil {
				return domain.Wrap("storage_corrupt", e)
			}
			if a.Clock.Now().UnixMilli() < meta.NotBeforeMS {
				continue
			}
			expected, attempt := j.State, j.Attempt
			j.State = "PUBLISHING"
			j.Attempt++
			j.LeaseOwner = domain.Ptr(a.workerID)
			j.LeaseUntil = domain.Ptr(a.Clock.Now().UnixMilli() + 60000)
			if e = tx.UpdateJob(ctx, j, expected, attempt); e != nil {
				return e
			}
			job = &j
			return nil
		}
		return nil
	})
	if e != nil || job == nil {
		return false, e
	}
	e = a.erase(ctx, *job, meta)
	if e != nil {
		probe, release := context.WithTimeout(context.WithoutCancel(parent), 2*time.Second)
		defer release()
		_, _ = a.Store.Update(probe, func(tx ports.Tx) error {
			fresh, e2 := tx.Job(probe, job.ID)
			if e2 != nil || fresh == nil {
				return e2
			}
			if fresh.Attempt != job.Attempt || fresh.State != "PUBLISHING" {
				return domain.Fail("lost_lease")
			}
			fresh.SafeError = domain.Ptr(domain.Code(e))
			fresh.LeaseUntil = domain.Ptr(a.Clock.Now().Add(5 * time.Second).UnixMilli())
			return tx.UpdateJob(probe, *fresh, "PUBLISHING", job.Attempt)
		})
		return true, e
	}
	return true, nil
}
func (a *Account) erase(ctx context.Context, job domain.Job, meta deletionMetadata) error {
	var d *domain.Deletion
	e := a.Store.View(ctx, func(tx ports.Tx) error { var e error; d, e = tx.Deletion(ctx, meta.OperationID); return e })
	if e != nil {
		return e
	}
	if d == nil || d.UserID != meta.UserID || d.JobID == nil || *d.JobID != job.ID {
		return domain.Fail("storage_corrupt")
	}
	receipt, e := a.Ledger.Append(ctx, domain.LedgerEvent{OperationID: d.OperationID, UserID: d.UserID, RequestedMS: d.RequestedMS})
	if e != nil {
		return e
	}
	_, e = a.Store.Update(ctx, func(tx ports.Tx) error {
		fresh, e := tx.Job(ctx, job.ID)
		if e != nil {
			return e
		}
		if fresh == nil || fresh.Attempt != job.Attempt || fresh.State != "PUBLISHING" {
			return domain.Fail("lost_lease")
		}
		current, e := tx.Deletion(ctx, d.OperationID)
		if e != nil {
			return e
		}
		if current.State == "PENDING_LEDGER" {
			current.State = "LEDGER_CONFIRMED"
			current.LedgerReceipt = &receipt
			current.UpdatedMS = a.Clock.Now().UnixMilli()
			return tx.PutDeletion(ctx, *current)
		}
		return nil
	})
	if e != nil {
		return e
	}
	for _, id := range meta.Artifacts {
		active := false
		e = a.Store.View(ctx, func(tx ports.Tx) error {
			var e error
			active, e = tx.HasReadLease(ctx, id, a.Clock.Now().UnixMilli())
			return e
		})
		if e != nil {
			return e
		}
		if active {
			return domain.Fail("busy")
		}
		if e = a.Files.RemoveJob(id); e != nil {
			return e
		}
	}
	_, e = a.Store.Update(ctx, func(tx ports.Tx) error {
		fresh, e := tx.Job(ctx, job.ID)
		if e != nil {
			return e
		}
		if fresh == nil || fresh.Attempt != job.Attempt || fresh.State != "PUBLISHING" {
			return domain.Fail("lost_lease")
		}
		current, e := tx.Deletion(ctx, d.OperationID)
		if e != nil {
			return e
		}
		if current.State != "LEDGER_CONFIRMED" {
			return domain.Fail("job_state_conflict")
		}
		if e = tx.PurgeUserJobs(ctx, meta.UserID, job.ID); e != nil {
			return e
		}
		if e = tx.EraseAccount(ctx, meta.UserID, a.Clock.Now().UnixMilli()); e != nil {
			return e
		}
		current.State = "COMPLETED"
		current.UpdatedMS = a.Clock.Now().UnixMilli()
		if e = tx.PutDeletion(ctx, *current); e != nil {
			return e
		}
		fresh.State = "COMMITTED"
		fresh.LeaseOwner = nil
		fresh.LeaseUntil = nil
		fresh.SafeError = nil
		fresh.UpdatedMS = a.Clock.Now().UnixMilli()
		meta.Artifacts = []domain.ID{}
		fresh.Metadata, e = json.Marshal(meta)
		if e != nil {
			return e
		}
		return tx.UpdateJob(ctx, *fresh, "PUBLISHING", job.Attempt)
	})
	return e
}
