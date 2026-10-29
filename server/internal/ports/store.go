package ports

import (
	"context"
	"daymark.local/server/internal/domain"
	"io"
	"time"
)

// Tx is valid only during its callback, on one SQL transaction/connection.
// Write callback errors roll back every effect; no adapter replays callbacks.
type Tx interface {
	State(context.Context) (domain.State, error)
	Maintain(context.Context, int64, int64) error
	BindLedger(context.Context, domain.ID) error
	AllDatasets(context.Context) ([]domain.Dataset, error)
	RestoreEpochs(context.Context, domain.ID, map[domain.ID]domain.ID, int64) error
	BackupRecord(context.Context, domain.ID) (*domain.BackupRecord, error)
	PutBackupRecord(context.Context, domain.BackupRecord) error
	User(context.Context, domain.ID) (*domain.User, error)
	UserByLogin(context.Context, string) (*domain.User, error)
	PutUser(context.Context, domain.User) error
	Session(context.Context, domain.ID) (*domain.Session, error)
	SessionByHash(context.Context, [32]byte, bool) (*domain.Session, error)
	PutSession(context.Context, domain.Session) error
	RetiredRefresh(context.Context, [32]byte) (*domain.ID, error)
	RetireRefresh(context.Context, [32]byte, domain.ID, int64, int64) error
	Sessions(context.Context, domain.ID) ([]domain.Session, error)
	RevokeSessions(context.Context, domain.ID, int64) error
	Dataset(context.Context, domain.Scope) (domain.Dataset, error)
	Datasets(context.Context, domain.ID) ([]domain.Dataset, error)
	InsertDataset(context.Context, domain.Dataset, int64) error
	LoadGraph(context.Context, domain.Scope) (domain.Graph, error)
	Apply(context.Context, domain.Scope, domain.Plan) error
	Receipt(context.Context, domain.Scope, domain.ID) (*domain.StoredReceipt, error)
	AppendReceipt(context.Context, domain.Scope, domain.StoredReceipt) error
	AppendBatch(context.Context, domain.Scope, domain.Batch) error
	Batches(context.Context, domain.Scope, domain.Counter, domain.Counter, int) ([]domain.Batch, error)
	Progress(context.Context, domain.Scope, domain.Counter, int64) error
	Compact(context.Context, domain.Scope, domain.Counter, int64) (domain.Counter, error)
	Job(context.Context, domain.ID) (*domain.Job, error)
	JobByOperation(context.Context, string) (*domain.Job, error)
	InsertJob(context.Context, domain.Job) error
	UpdateJob(context.Context, domain.Job, string, int64) error
	RunnableJobs(context.Context, int64, int, ...string) ([]domain.Job, error)
	AllJobs(context.Context, *domain.ID) ([]domain.Job, error)
	Chunks(context.Context, domain.ID) ([]domain.Chunk, error)
	PutChunk(context.Context, domain.ID, domain.Chunk) error
	PutManifest(context.Context, domain.ID, domain.Manifest) error
	Manifest(context.Context, domain.ID) (*domain.Manifest, error)
	Pin(context.Context, domain.ID, domain.ID, domain.Counter, int64) error
	ReleasePin(context.Context, domain.ID) error
	ReadLease(context.Context, domain.ID, domain.ID, string, int64, int64) error
	ReleaseReadLease(context.Context, domain.ID) error
	HasReadLease(context.Context, domain.ID, int64) (bool, error)
	ReadLeaseCount(context.Context, domain.ID, int64) (int, error)
	Deletion(context.Context, domain.ID) (*domain.Deletion, error)
	PutDeletion(context.Context, domain.Deletion) error
	CancelIntents(context.Context, domain.ID, int64) error
	EraseAccount(context.Context, domain.ID, int64) error
	PurgeUserJobs(context.Context, domain.ID, domain.ID) error
	ReplayDeletion(context.Context, domain.LedgerEvent) error
}
type CommitState uint8

const (
	NotCommitted CommitState = iota
	Committed
	CommitUnknown
)

type Store interface {
	View(context.Context, func(Tx) error) error
	Update(context.Context, func(Tx) error) (CommitState, error)
	Snapshot(context.Context, string) error
	InspectBackup(context.Context, string) (domain.BackupInfo, error)
	Ready() error
	Close() error
}
type Clock interface{ Now() time.Time }
type SystemClock struct{}

func (SystemClock) Now() time.Time { return time.Now().UTC() }

type Hasher interface {
	Hash(context.Context, []byte) (string, error)
	Verify(context.Context, string, []byte) (bool, error)
}

type Ledger interface {
	ID() domain.ID
	Append(context.Context, domain.LedgerEvent) (string, error)
	Read(context.Context) ([]domain.LedgerEvent, error)
}

type Artifacts interface {
	Put(context.Context, domain.ID, int, io.Reader, int64, string) (domain.Chunk, error)
	Read(context.Context, domain.ID, []domain.Chunk, int64) ([]byte, error)
	PublishAttempt(context.Context, domain.ID, int64, []byte) ([]domain.Chunk, string, error)
	CleanAttempts(domain.ID, int64) error
	OpenStored(domain.ID, domain.Chunk) (io.ReadCloser, error)
	RemoveJob(domain.ID) error
	Orphans(map[domain.ID]bool, time.Time) ([]domain.ID, error)
}
type RequestCodec interface {
	DecodeMutation([]byte, domain.ID) (domain.Mutation, error)
	Decode(string, []byte, any) error
	Validate(string, any) error
}
