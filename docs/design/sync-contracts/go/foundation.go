package synccontracts

import (
	"context"
	"time"
)

// SecretRef names a protected deployment secret, not its cleartext contents.
type SecretRef string
type Limits struct {
	WriterQueue, ReadPool, HTTPConcurrency, MaxHeaderBytes int
	InlineBytes                                            int64
	InlineCommands, InlineEntities, ChunkBytes, Workers    int
	StagedBytes, SnapshotBytes, MaxArtifactDiskBytes       int64
	MaxJobsPerUser, MaxReadLeasesPerJob                    int
}
type Timeouts struct {
	ReadHeader, Request, Busy, Idle, Shutdown                       time.Duration
	WorkerLease, WorkerHeartbeat, ArtifactTTL, UploadTTL, ReadLease time.Duration
	AccessTTL, RefreshAbsoluteTTL, CursorTTL, LogRetention          time.Duration
}
type Config struct {
	DataDir, ListenAddress, PublicOrigin string
	Production                           bool
	Limits                               Limits
	Timeouts                             Timeouts
	CursorActiveKeyID                    string
	CursorKeys                           map[string]SecretRef
	BackupKey, DeletionLedgerTarget      SecretRef
	BackupEvery, BackupRetention         time.Duration
	KDFParallelism                       int
}
type ValidConfig struct{ Config Config } // Created only by ConfigLoader.Validate.
type Readiness struct {
	Ready           bool
	ReasonCode      string
	DegradedReasons []string
}
type StopReport struct {
	Drained                      bool
	PendingRequests, PendingJobs int
}
type InstanceLease interface{ Release() error }
type InstanceLock interface {
	Acquire(context.Context, string) (InstanceLease, error)
}
type ConfigLoader interface {
	Load(context.Context, string, map[string]string) (Config, error)
	Validate(Config) (ValidConfig, error)
}
type Runtime interface {
	Start(context.Context, ValidConfig) error
	Readiness() Readiness
	Stop(context.Context) (StopReport, error)
}
type Scheduler interface {
	Wait(context.Context, time.Duration) error
}
type SchemaState struct {
	Version      int
	Checksums    map[int]Digest
	NeedsUpgrade bool
}
type MigrationRunner interface {
	Inspect(context.Context, AdminCapability) (SchemaState, error)
	Upgrade(context.Context, AdminCapability, SchemaState, BackupReport) (SchemaState, error)
}
type AdminIdentityService interface {
	CreateUser(context.Context, AdminCapability, string, []byte) (ID, error)
	SetDisabled(context.Context, AdminCapability, ID, bool) error
}
