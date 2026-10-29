package application

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"errors"
	"strings"
	"time"

	"daymark.local/server/internal/domain"
	"daymark.local/server/internal/ports"
	"daymark.local/server/internal/protocol"
)

type CursorClaims struct {
	Version   int             `json:"version"`
	KeyID     string          `json:"key_id"`
	ServerID  domain.ID       `json:"server_id"`
	OwnerID   domain.ID       `json:"owner_id"`
	DatasetID domain.ID       `json:"dataset_id"`
	DataEpoch domain.ID       `json:"data_epoch"`
	After     domain.Counter  `json:"after_seq"`
	Through   *domain.Counter `json:"through_seq"`
	ExpiresMS int64           `json:"expires_ms"`
}
type CursorCodec struct {
	ServerID  domain.ID
	ActiveKey string
	Keys      map[string][]byte
	Clock     ports.Clock
	TTL       time.Duration
}

func (c *CursorCodec) Issue(scope domain.Scope, after domain.Counter, through *domain.Counter) (string, error) {
	claims := CursorClaims{1, c.ActiveKey, c.ServerID, scope.Principal.UserID, scope.DatasetID, scope.DataEpoch, after, through, c.Clock.Now().Add(c.TTL).UnixMilli()}
	raw, e := json.Marshal(claims)
	if e != nil {
		return "", e
	}
	v, e := protocol.StrictValue(raw)
	if e != nil {
		return "", e
	}
	canonical, e := protocol.Canonical(v)
	if e != nil {
		return "", e
	}
	key := c.Keys[c.ActiveKey]
	if len(key) != 32 {
		return "", domain.Fail("internal_error")
	}
	mac := hmac.New(sha256.New, key)
	_, _ = mac.Write(canonical)
	return base64.RawURLEncoding.EncodeToString(raw) + "." + base64.RawURLEncoding.EncodeToString(mac.Sum(nil)), nil
}
func (c *CursorCodec) Verify(token string, scope domain.Scope) (CursorClaims, error) {
	var claims CursorClaims
	if len(token) > 4096 {
		return claims, domain.Invalid("/cursor")
	}
	parts := strings.Split(token, ".")
	if len(parts) != 2 {
		return claims, domain.Invalid("/cursor")
	}
	raw, e := base64.RawURLEncoding.Strict().DecodeString(parts[0])
	if e != nil {
		return claims, domain.Invalid("/cursor")
	}
	signature, e := base64.RawURLEncoding.Strict().DecodeString(parts[1])
	if e != nil || len(signature) != 32 {
		return claims, domain.Invalid("/cursor")
	}
	v, e := protocol.StrictValue(raw)
	if e != nil {
		return claims, domain.Invalid("/cursor")
	}
	decoder := json.NewDecoder(strings.NewReader(string(raw)))
	decoder.DisallowUnknownFields()
	if e = decoder.Decode(&claims); e != nil {
		return claims, domain.Invalid("/cursor")
	}
	key, ok := c.Keys[claims.KeyID]
	if !ok {
		return claims, domain.Fail("cursor_expired")
	}
	canonical, e := protocol.Canonical(v)
	if e != nil {
		return claims, e
	}
	mac := hmac.New(sha256.New, key)
	_, _ = mac.Write(canonical)
	if !hmac.Equal(signature, mac.Sum(nil)) {
		return claims, domain.Invalid("/cursor")
	}
	if claims.Version != 1 || claims.ServerID != c.ServerID || claims.OwnerID != scope.Principal.UserID || claims.DatasetID != scope.DatasetID {
		return claims, domain.Invalid("/cursor")
	}
	if claims.DataEpoch != scope.DataEpoch {
		return claims, domain.Fail("epoch_changed")
	}
	if c.Clock.Now().UnixMilli() >= claims.ExpiresMS {
		return claims, domain.Fail("cursor_expired")
	}
	if claims.After < 0 || claims.Through != nil && *claims.Through < claims.After {
		return claims, domain.Invalid("/cursor")
	}
	return claims, nil
}

type BatchReference struct {
	Seq domain.Counter `json:"seq"`
	Ref struct {
		JobID domain.ID      `json:"job_id"`
		Hash  string         `json:"sha256"`
		Bytes domain.Counter `json:"byte_count"`
	} `json:"batch_ref"`
}
type Delivery struct {
	Batch       any    `json:"batch"`
	AfterCursor string `json:"after_cursor"`
}
type PullResponse struct {
	ProtocolVersion int            `json:"protocol_version"`
	DataEpoch       domain.ID      `json:"data_epoch"`
	Through         domain.Counter `json:"through_seq"`
	Batches         []Delivery     `json:"batches"`
	Next            string         `json:"next_cursor"`
	HasMore         bool           `json:"has_more"`
}
type BatchExporter interface {
	ExportBatch(context.Context, domain.Scope, domain.Batch) (BatchReference, error)
}
type MutationResolver interface {
	ResolveMutation(context.Context, domain.Scope, domain.Mutation) (domain.Mutation, func(), error)
}
type Sync struct {
	Store                          ports.Store
	Auth                           *Auth
	Clock                          ports.Clock
	Cursor                         *CursorCodec
	Exporter                       BatchExporter
	Resolver                       MutationResolver
	MaxCommands, MaxInlineEntities int
	MaxInlineBytes                 int
}

func NewSync(store ports.Store, auth *Auth, clock ports.Clock, cursor *CursorCodec) *Sync {
	return &Sync{Store: store, Auth: auth, Clock: clock, Cursor: cursor, MaxCommands: 64, MaxInlineEntities: 512, MaxInlineBytes: 1048576}
}
func (s *Sync) GetReceipt(ctx context.Context, scope domain.Scope, id domain.ID) (*domain.Receipt, error) {
	var out *domain.Receipt
	e := s.Store.View(ctx, func(tx ports.Tx) error {
		if e := s.Auth.Check(ctx, tx, scope.Principal); e != nil {
			return e
		}
		r, e := tx.Receipt(ctx, scope, id)
		if r != nil {
			out = &r.Receipt
		}
		return e
	})
	return out, e
}
func (s *Sync) Push(ctx context.Context, scope domain.Scope, m domain.Mutation) (domain.Receipt, error) {
	var out domain.Receipt
	if m.DatasetID != scope.DatasetID || m.DataEpoch != scope.DataEpoch {
		return out, domain.Fail("epoch_changed")
	}
	if m.InstallationID != scope.Principal.InstallationID {
		return out, domain.Invalid("/installation_id")
	}
	// Recover committed staged work before touching an expired payload reference.
	var prior *domain.StoredReceipt
	e := s.Store.View(ctx, func(tx ports.Tx) error {
		if e := s.Auth.Check(ctx, tx, scope.Principal); e != nil {
			return e
		}
		var e error
		prior, e = tx.Receipt(ctx, scope, m.MutationID)
		return e
	})
	if e != nil {
		return out, e
	}
	if prior != nil {
		if prior.Hash != m.Hash || prior.OuterHash != m.OuterHash {
			return out, domain.Fail("mutation_payload_mismatch")
		}
		return prior.Receipt, nil
	}
	if m.Ref != nil {
		if s.Resolver == nil {
			return out, domain.Fail("payload_too_large")
		}
		var release func()
		m, release, e = s.Resolver.ResolveMutation(ctx, scope, m)
		if e != nil {
			return out, e
		}
		defer release()
	}
	if m.Ref == nil && len(m.Commands) > s.MaxCommands {
		return out, domain.Fail("payload_too_large")
	}
	state, e := s.Store.Update(ctx, func(tx ports.Tx) error {
		if e := s.Auth.Check(ctx, tx, scope.Principal); e != nil {
			return e
		}
		dataset, e := tx.Dataset(ctx, scope)
		if e != nil {
			return e
		}
		old, e := tx.Receipt(ctx, scope, m.MutationID)
		if e != nil {
			return e
		}
		if old != nil {
			if old.Hash != m.Hash || old.OuterHash != m.OuterHash {
				return domain.Fail("mutation_payload_mismatch")
			}
			out = old.Receipt
			return nil
		}
		if m.Ref != nil {
			job, e := tx.Job(ctx, m.Ref.JobID)
			if e != nil {
				return e
			}
			if job == nil || job.OwnerID == nil || *job.OwnerID != scope.Principal.UserID || job.DatasetID == nil || *job.DatasetID != scope.DatasetID || job.DataEpoch == nil || *job.DataEpoch != scope.DataEpoch || job.State != "SEALED" || milliseconds(job.ExpiresAt) <= s.Clock.Now().UnixMilli() {
				return domain.Fail("payload_ref_expired")
			}
		}
		graph, e := tx.LoadGraph(ctx, scope)
		if e != nil {
			return e
		}
		if e = domain.CheckPreconditions(graph, m.Preconditions, domain.Preconditions{}); e != nil {
			return e
		}
		plan, e := domain.PlanCommands(graph, m.Commands, scope.DatasetID)
		if e != nil {
			return e
		}
		if e = domain.CheckPreconditions(graph, m.Preconditions, plan.Reads); e != nil {
			return e
		}
		if m.Ref == nil && len(plan.Changes) > s.MaxInlineEntities {
			return domain.Fail("payload_too_large")
		}
		if len(plan.Changes) > 100000 {
			return domain.Fail("quota_exceeded")
		}
		at := domain.At(s.Clock.Now())
		out = domain.EmptyReceipt(m.MutationID, scope.DataEpoch, dataset.Head, at)
		if len(plan.Changes) > 0 || len(plan.Structures) > 0 || len(plan.Aliases) > 0 || len(plan.Edges) > 0 {
			if dataset.Head == domain.Counter(1<<63-1) {
				return domain.Fail("quota_exceeded")
			}
			seq := dataset.Head + 1
			batch := domain.Batch{Seq: seq, MutationID: m.MutationID, InstallationID: m.InstallationID, CommittedAt: at, Changes: []domain.Entity{}, Structures: plan.Structures, Aliases: plan.Aliases, Edges: plan.Edges}
			for _, d := range plan.Changes {
				batch.Changes = append(batch.Changes, d.After)
				out.Entities = append(out.Entities, domain.Version{Type: d.After.Type, ID: d.After.ID, Revision: d.After.Revision})
			}
			encoded, e := json.Marshal(batch)
			if e != nil {
				return e
			}
			if len(encoded) > s.MaxInlineBytes && s.Exporter == nil {
				return domain.Fail("payload_too_large")
			}
			if e = tx.Apply(ctx, scope, plan); e != nil {
				return e
			}
			if e = tx.AppendBatch(ctx, scope, batch); e != nil {
				return e
			}
			out.Result = "committed"
			out.BatchSeq = &seq
			out.ObservedHead = seq
			out.Structures = plan.Structures
		}
		if m.Ref != nil {
			job, e := tx.Job(ctx, m.Ref.JobID)
			if e != nil {
				return e
			}
			expected := job.State
			job.State = "COMMITTED"
			job.UpdatedMS = s.Clock.Now().UnixMilli()
			if e = tx.UpdateJob(ctx, *job, expected, job.Attempt); e != nil {
				return e
			}
		}
		response, e := json.Marshal(out)
		if e != nil {
			return e
		}
		var ref *domain.ID
		if m.Ref != nil {
			ref = &m.Ref.JobID
		}
		return tx.AppendReceipt(ctx, scope, domain.StoredReceipt{Receipt: out, Hash: m.Hash, OuterHash: m.OuterHash, PayloadRef: ref, Response: response})
	})
	if e == nil {
		return out, nil
	}
	if state == ports.CommitUnknown {
		// New budget and connection: the request ctx may already be cancelled.
		probe, cancel := context.WithTimeout(context.WithoutCancel(ctx), 2*time.Second)
		defer cancel()
		var saved *domain.StoredReceipt
		lookup := s.Store.View(probe, func(tx ports.Tx) error { var e error; saved, e = tx.Receipt(probe, scope, m.MutationID); return e })
		if lookup == nil && saved != nil && saved.Hash == m.Hash && saved.OuterHash == m.OuterHash {
			return saved.Receipt, nil
		}
		return domain.Receipt{}, domain.Wrap("commit_result_unknown", e)
	}
	return domain.Receipt{}, e
}
func milliseconds(at domain.Instant) int64 { n, _ := at.Millis(); return n }
func (s *Sync) Pull(ctx context.Context, scope domain.Scope, token string, limit int) (PullResponse, error) {
	claims, e := s.Cursor.Verify(token, scope)
	if e != nil {
		return PullResponse{}, e
	}
	if limit <= 0 || limit > 128 {
		limit = 128
	}
	out := PullResponse{ProtocolVersion: 1, DataEpoch: scope.DataEpoch, Batches: []Delivery{}}
	var batches []domain.Batch
	e = s.Store.View(ctx, func(tx ports.Tx) error {
		if e := s.Auth.Check(ctx, tx, scope.Principal); e != nil {
			return e
		}
		d, e := tx.Dataset(ctx, scope)
		if e != nil {
			return e
		}
		if claims.After < d.RetainedAfter {
			return domain.Fail("cursor_expired")
		}
		through := d.Head
		if claims.Through != nil {
			through = *claims.Through
		}
		if through > d.Head || claims.After > through {
			return domain.Invalid("/cursor")
		}
		out.Through = through
		batches, e = tx.Batches(ctx, scope, claims.After, through, limit)
		return e
	})
	if e != nil {
		return out, e
	}
	after := claims.After
	bytes := 0
	for _, batch := range batches {
		if batch.Seq != after+1 {
			return out, domain.Fail("storage_corrupt")
		}
		b, e := json.Marshal(batch)
		if e != nil {
			return out, e
		}
		var value any = batch
		if len(b) > s.MaxInlineBytes {
			if s.Exporter == nil {
				return out, domain.Fail("payload_too_large")
			}
			value, e = s.Exporter.ExportBatch(ctx, scope, batch)
			if e != nil {
				return out, e
			}
			b, e = json.Marshal(value)
			if e != nil {
				return out, e
			}
		}
		if bytes+len(b) > s.MaxInlineBytes && len(out.Batches) > 0 {
			break
		}
		bytes += len(b)
		after = batch.Seq
		var bound *domain.Counter
		if after < out.Through {
			bound = &out.Through
		}
		checkpoint, e := s.Cursor.Issue(scope, after, bound)
		if e != nil {
			return out, e
		}
		out.Batches = append(out.Batches, Delivery{Batch: value, AfterCursor: checkpoint})
	}
	if after < out.Through && len(out.Batches) == 0 {
		return out, domain.Fail("storage_corrupt")
	}
	out.HasMore = after < out.Through
	if len(out.Batches) > 0 {
		out.Next = out.Batches[len(out.Batches)-1].AfterCursor
	} else {
		out.Next, e = s.Cursor.Issue(scope, after, nil)
	}
	return out, e
}
func (s *Sync) ReportProgress(ctx context.Context, scope domain.Scope, applied domain.Counter) error {
	_, e := s.Store.Update(ctx, func(tx ports.Tx) error {
		if e := s.Auth.Check(ctx, tx, scope.Principal); e != nil {
			return e
		}
		return tx.Progress(ctx, scope, applied, s.Clock.Now().UnixMilli())
	})
	return e
}
func IsContextError(e error) bool {
	return errors.Is(e, context.Canceled) || errors.Is(e, context.DeadlineExceeded)
}
