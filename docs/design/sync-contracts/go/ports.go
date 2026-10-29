package synccontracts

import (
	"context"
	"io"
	"time"
)

type Clock interface{ NowUTC() Instant }
type TokenSource interface{ NewBytes(int) ([]byte, error) }
type PasswordHasher interface {
	Hash(context.Context, []byte) (string, error)
	Verify(context.Context, string, []byte) (bool, error)
	NeedsRehash(string) (bool, error)
}
type Canonicalizer interface {
	Encode(Mutation) ([]byte, error)
	Digest(Mutation) (Digest, error)
	EncodeImport(ImportMutation) ([]byte, error)
	DigestImport(ImportMutation) (Digest, error)
}
type CursorCodec interface {
	Encode(CursorClaims) (Cursor, error)
	Decode(Cursor, Instant) (CursorClaims, error)
}
type Codec interface {
	DecodePush(io.Reader, ID, int64) (Mutation, *PayloadRef, error)
	DecodeCommand([]byte) (Command, error)
	EncodeBatch(ChangeBatch) ([]byte, error)
	EncodeReceipt(Receipt) ([]byte, error)
}
type PayloadRef struct {
	JobID         ID
	CanonicalHash Digest
}
type DatasetState struct {
	ID, OwnerID, DataEpoch ID
	Head, RetainedAfter    Seq
}
type DatasetSnapshot struct {
	State DatasetState
	Graph Graph
}
type ReadTx interface {
	Dataset(context.Context, Scope) (DatasetState, error)
	Receipt(context.Context, Scope, ID) (*StoredReceipt, error)
	LoadGraph(context.Context, Scope, ReadRequirements) (Graph, error)
	Batches(context.Context, Scope, Seq, Seq, PageBudget) ([]ChangeBatch, error)
	Job(context.Context, Scope, ID) (StoredJob, error)
	CaptureDataset(context.Context, Scope) (DatasetSnapshot, error)
}
type WriteTx interface {
	ReadTx
	RevalidatePrincipal(context.Context, Principal, Instant) error
	ApplyPlan(context.Context, Scope, Plan) error
	AppendBatch(context.Context, Scope, ChangeBatch) error
	InsertReceipt(context.Context, Scope, StoredReceipt) error
	CompareAndSetHead(context.Context, Scope, Seq, Seq) error
	ReportProgress(context.Context, Scope, Seq, Instant) error
	SetJobCommitted(context.Context, Scope, ID, int64, ID) error
}
type CommitDisposition uint8

const (
	NotCommitted CommitDisposition = iota
	Committed
	CommitUnknown
)

type CommitOutcome struct{ Disposition CommitDisposition }

// A callback error always rolls back. Outcome.CommitUnknown survives a ctx
// timeout: the caller must query the receipt independently before classifying.
type UnitOfWork interface {
	Read(context.Context, func(ReadTx) error) error
	Write(context.Context, func(WriteTx) error) (CommitOutcome, error)
}
type StoredReceipt struct {
	Receipt           Receipt
	CanonicalHash     Digest
	OuterHash         *Digest
	PayloadJobID      *ID
	CanonicalResponse []byte
}
type PageBudget struct {
	MaxBatches int
	MaxBytes   int64
}
type WorkspacePolicy interface {
	RequiredReads([]Command, Graph) (ReadRequirements, error)
	Plan(Graph, []Command, EventContext) (Plan, error)
}
type SyncService interface {
	Push(context.Context, Scope, Mutation) (Receipt, error)
	GetReceipt(context.Context, Scope, ID) (*Receipt, error)
	Pull(context.Context, Scope, Cursor, PageBudget) (PullPage, error)
	ReportProgress(context.Context, Scope, Seq) error
}
type LoginInput struct {
	Login                string
	Password             []byte
	InstallationID       ID
	DeviceName, Platform string
}
type TokenPair struct {
	Access, Refresh                   Token
	AccessExpiresAt, RefreshExpiresAt Instant
	SessionID                         ID
	Generation                        int64
}
type SessionView struct {
	ID, InstallationID   ID
	DeviceName, Platform string
	CreatedAt, ExpiresAt Instant
	Current              bool
}
type UserView struct {
	ID       ID
	Login    string
	Datasets []DatasetState
}
type AuthService interface {
	Login(context.Context, LoginInput) (TokenPair, error)
	Refresh(context.Context, Token) (TokenPair, error)
	Authenticate(context.Context, Token) (Principal, error)
	Logout(context.Context, Principal) error
	ListSessions(context.Context, Principal) ([]SessionView, error)
	RevokeSession(context.Context, Principal, ID) error
	ChangePassword(context.Context, Principal, []byte, []byte) error
	Me(context.Context, Principal) (UserView, error)
}

// Auth transactions are separate from dataset Tx. A rejection may itself need
// to COMMIT (refresh replay revocation). Return the policy error after Write.
type Credential struct {
	UserID      ID
	EncodedHash string
	Version     int64
	Status      string
}
type SessionRecord struct {
	Principal                   Principal
	AccessHash, RefreshHash     Digest
	Generation                  int64
	AccessExpiry, RefreshExpiry Instant
	RevokedAt                   *Instant
}
type AuthTx interface {
	UserByID(context.Context, ID) (UserView, error)
	SessionsForUser(context.Context, ID) ([]SessionView, error)
	CredentialByLogin(context.Context, string) (*Credential, error)
	SessionByAccessHash(context.Context, Digest) (*SessionRecord, error)
	SessionByRefreshHash(context.Context, Digest) (*SessionRecord, error)
	RetiredRefreshOwner(context.Context, Digest) (*ID, error)
	InsertSession(context.Context, SessionRecord, string, string) error
	RotatePair(context.Context, ID, int64, Digest, SessionRecord, Instant) error
	RevokeSession(context.Context, ID, ID, Instant) error
	ReplacePasswordAndRevokeAll(context.Context, ID, int64, string, Instant) error
}
type AuthUnitOfWork interface {
	Read(context.Context, func(AuthTx) error) error
	Write(context.Context, func(AuthTx) error) (CommitOutcome, error)
}
type JobKind string
type JobState string
type Job struct {
	ID, OwnerID          ID
	DatasetID, DataEpoch *ID
	Kind                 JobKind
	State                JobState
	OperationKey         string
	Attempt              int64
	LeaseOwner           *string
	LeaseUntil           *Instant
	ExpiresAt            Instant
	BytesReserved        int64
	SafeError            *string
}
type JobInput interface{ jobInput() }
type SnapshotJobInput struct{ OperationID ID }
type StagedJobInput struct {
	OperationID ID
	Input       StagedInput
}
type ImportJobInput struct {
	OperationID ID
	Input       ImportInput
}
type ExportJobInput struct{ OperationID ID }
type BatchJobInput struct {
	Seq       Seq
	BatchHash Digest
}

func (SnapshotJobInput) jobInput() {}
func (StagedJobInput) jobInput()   {}
func (ImportJobInput) jobInput()   {}
func (ExportJobInput) jobInput()   {}
func (BatchJobInput) jobInput()    {}

type StoredJob struct {
	Job                  Job
	Input                JobInput
	InputHash            Digest
	Manifest             *Manifest
	Chunks               []Chunk
	CanonicalPayloadHash *Digest
}

// Owner-only export and dataset-bound jobs use the same storage checks. Both
// optional scope fields are either nil or nonnil; clients never supply Principal.
type JobScope struct {
	Principal            Principal
	DatasetID, DataEpoch *ID
}
type SealDescriptor struct {
	Chunks        []Chunk
	TotalBytes    int64
	PayloadHash   Digest
	CanonicalHash *Digest
}
type TransferReadTx interface {
	GetJob(context.Context, JobScope, ID) (StoredJob, error)
	FindOperation(context.Context, JobScope, JobKind, ID) (*StoredJob, error)
	ActiveReadLeases(context.Context, ID, Instant) ([]ReadLease, error)
}
type TransferWriteTx interface {
	TransferReadTx
	ReserveJob(context.Context, JobScope, StoredJob) error
	RegisterChunk(context.Context, JobScope, ID, Chunk, string, Instant) error
	SealJob(context.Context, JobScope, ID, SealDescriptor, Instant) error
	RenewJob(context.Context, JobScope, ID, Instant, Instant) (Job, error)
	CancelJob(context.Context, JobScope, ID, Instant) (Job, error)
	PutPin(context.Context, Scope, ID, Seq, Instant) error
	ReleasePin(context.Context, Scope, ID) error
	AcquireReadLease(context.Context, JobScope, ID, ReadLease) error
	ReleaseReadLease(context.Context, JobScope, ID, ID) error
	MarkReady(context.Context, JobScope, JobFence, Manifest, Instant) error
}
type TransferUnitOfWork interface {
	Read(context.Context, func(TransferReadTx) error) error
	Write(context.Context, func(TransferWriteTx) error) (CommitOutcome, error)
}
type Chunk struct {
	Index  int
	Bytes  int64
	SHA256 Digest
}
type Manifest struct {
	JobID                     ID
	DatasetID, DataEpoch      *ID
	Kind                      JobKind
	Cutoff                    *Seq
	DTOVersion                int
	EntityCount, TotalBytes   int64
	Chunks                    []Chunk
	PayloadHash, ManifestHash Digest
	ExpiresAt                 Instant
	ResumeCursor              *Cursor
}
type StagedInput struct {
	MutationID, InstallationID ID
	ClientSeq                  Seq
	ExpectedBytes              int64
}
type ImportInput struct {
	StagedInput
	BaseHead            Seq
	DTOVersion          int
	SourceExportVersion int
}
type SealInput struct {
	Chunks      []Chunk
	TotalBytes  int64
	PayloadHash Digest
}
type SealedPayload struct {
	Job           Job
	CanonicalHash Digest
	Ref           *PayloadRef
}
type ReadLease struct {
	ID, JobID    ID
	ManifestHash Digest
	ExpiresAt    Instant
}
type ArtifactMeta struct {
	Bytes int64
	Hash  Digest
}
type ArtifactStore interface {
	PutAtomic(context.Context, string, io.Reader, int64, Digest) (ArtifactMeta, error)
	Open(context.Context, string) (io.ReadCloser, error)
	Stat(context.Context, string) (ArtifactMeta, error)
	Remove(context.Context, string) error
}
type TransferService interface {
	RequestSnapshot(context.Context, Scope, ID) (Job, error)
	CreateStaged(context.Context, Scope, ID, StagedInput) (Job, error)
	CreateImport(context.Context, Scope, ID, ImportInput) (Job, error)
	PutChunk(context.Context, Scope, ID, int, io.Reader, int64, Digest) (Chunk, error)
	Seal(context.Context, Scope, ID, SealInput) (SealedPayload, error)
	PublishImport(context.Context, Scope, ID) (Receipt, error)
	GetJob(context.Context, Scope, ID) (Job, error)
	GetManifest(context.Context, Scope, ID) (Manifest, error)
	OpenChunk(context.Context, Scope, ID, int) (ReadLease, io.ReadCloser, error)
	Renew(context.Context, Scope, ID, time.Duration) (Job, error)
	Cancel(context.Context, Scope, ID) (Job, error)
	ReleaseSnapshot(context.Context, Scope, ID, Seq) error
}
type JobFence struct {
	JobID   ID
	Attempt int64
	Owner   string
}
type JobStore interface {
	Claim(context.Context, JobKind, string, Instant) (*JobFence, error)
	Heartbeat(context.Context, JobFence, Instant) error
	CompareAndSetState(context.Context, JobFence, JobState, JobState) error
}

// AdminCapability is supplied by process startup, never HTTP request decoding.
type AdminCapability struct {
	InstanceID ID
	LockNonce  Digest
}
type BackupReport struct {
	JobID            ID
	SnapshotAt       Instant
	Hash             Digest
	SchemaVersion    int
	RemoteVerifiedAt *Instant
}
type RestoreReport struct {
	NewAuthEpoch                   ID
	NewDataEpochs                  map[ID]ID
	LostWindowStart, LostWindowEnd Instant
	ReadyForSwitch                 bool
}
type DeletionIntent struct {
	OperationID             ID
	Capability              Token
	ConfirmUntil, ExpiresAt Instant
}
type DeletionStatus struct {
	OperationID ID
	State       string
	UpdatedAt   Instant
}
type AccountService interface {
	RequestExport(context.Context, Principal, ID) (Job, error)
	GetExport(context.Context, Principal, ID) (Job, error)
	GetExportManifest(context.Context, Principal, ID) (Manifest, error)
	OpenExportChunk(context.Context, Principal, ID, int) (ReadLease, io.ReadCloser, error)
	PrepareDeletion(context.Context, Principal, []byte, ID) (DeletionIntent, error)
	ConfirmDeletion(context.Context, Token, ID) (DeletionStatus, error)
	DeletionStatus(context.Context, Token, ID) (DeletionStatus, error)
}
type Maintenance interface {
	Backup(context.Context, AdminCapability, ID) (BackupReport, error)
	VerifyBackup(context.Context, AdminCapability, ID) (BackupReport, error)
	Restore(context.Context, AdminCapability, ID, string) (RestoreReport, error)
	Compact(context.Context, AdminCapability, ID, Seq) (Seq, error)
	ReconcileArtifacts(context.Context, AdminCapability) (int, error)
}
