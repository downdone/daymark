package domain

import "encoding/json"

type User struct {
	ID                   ID
	Login, PasswordHash  string
	CredentialVersion    int64
	Status               string
	CreatedMS, UpdatedMS int64
}
type Session struct {
	ID, UserID, InstallationID, AuthEpoch      ID
	DeviceName, Platform                       string
	CredentialVersion, Generation              int64
	AccessHash, RefreshHash                    [32]byte
	CreatedMS, AccessExpiryMS, RefreshExpiryMS int64
	RevokedMS                                  *int64
}
type TokenPair struct {
	ProtocolVersion int     `json:"protocol_version"`
	Access          string  `json:"access_token"`
	Refresh         string  `json:"refresh_token"`
	AccessExpiry    Instant `json:"access_expires_at_utc"`
	RefreshExpiry   Instant `json:"refresh_expires_at_utc"`
	SessionID       ID      `json:"session_id"`
	Generation      Counter `json:"generation"`
}
type SessionView struct {
	ID             ID      `json:"id"`
	InstallationID ID      `json:"installation_id"`
	DeviceName     string  `json:"device_name"`
	Platform       string  `json:"platform"`
	CreatedAt      Instant `json:"created_at_utc"`
	ExpiresAt      Instant `json:"expires_at_utc"`
	Current        bool    `json:"current"`
}
type State struct {
	ServerID, AuthEpoch ID
	SchemaVersion       int
	LedgerID            *ID
}

type LedgerEvent struct {
	OperationID ID     `json:"operation_id"`
	UserID      ID     `json:"user_id"`
	RequestedMS int64  `json:"requested_ms"`
	Receipt     string `json:"receipt,omitempty"`
}
type BackupRecord struct {
	JobID         ID
	SnapshotMS    int64
	Hash          [32]byte
	ArtifactKey   string
	TargetID      *string
	VerifiedMS    *int64
	SchemaVersion int
}
type BackupInfo struct {
	State     State
	Datasets  []Dataset
	TaskCount int64
}
type StoredReceipt struct {
	Receipt         Receipt
	Hash, OuterHash [32]byte
	PayloadRef      *ID
	Response        []byte
}
type Chunk struct {
	Index       int     `json:"index"`
	Bytes       Counter `json:"byte_count"`
	Hash        string  `json:"sha256"`
	ArtifactKey string  `json:"-"`
}
type Job struct {
	ProtocolVersion      int             `json:"protocol_version"`
	ID                   ID              `json:"id"`
	OwnerID              *ID             `json:"-"`
	DatasetID            *ID             `json:"dataset_id"`
	DataEpoch            *ID             `json:"data_epoch"`
	Kind                 string          `json:"kind"`
	State                string          `json:"state"`
	ExpiresAt            Instant         `json:"expires_at_utc"`
	Reserved             Counter         `json:"bytes_reserved"`
	SafeError            *string         `json:"safe_error_code"`
	OperationKey         string          `json:"-"`
	Metadata             json.RawMessage `json:"-"`
	Attempt              int64           `json:"-"`
	LeaseOwner           *string         `json:"-"`
	LeaseUntil           *int64          `json:"-"`
	CreatedMS, UpdatedMS int64           `json:"-"`
}
type Manifest struct {
	ProtocolVersion int      `json:"protocol_version"`
	JobID           ID       `json:"job_id"`
	DatasetID       *ID      `json:"dataset_id"`
	DataEpoch       *ID      `json:"data_epoch"`
	Kind            string   `json:"kind"`
	DTOVersion      int      `json:"dto_version"`
	Cutoff          *Counter `json:"cutoff_seq"`
	EntityCount     Counter  `json:"entity_count"`
	TotalBytes      Counter  `json:"total_bytes"`
	Chunks          []Chunk  `json:"chunks"`
	PayloadHash     string   `json:"payload_sha256"`
	ManifestHash    string   `json:"manifest_sha256"`
	ExpiresAt       Instant  `json:"expires_at_utc"`
	ResumeCursor    *string  `json:"resume_cursor"`
}
type Snapshot struct {
	DTOVersion int                `json:"dto_version"`
	DatasetID  ID                 `json:"dataset_id"`
	DataEpoch  ID                 `json:"data_epoch"`
	Cutoff     Counter            `json:"cutoff_seq"`
	Entities   []Entity           `json:"entities"`
	Structures []StructureVersion `json:"structure_revisions"`
	Aliases    []Alias            `json:"aliases"`
	Edges      []Edge             `json:"successor_edges"`
}
type ImportPayload struct {
	DTOVersion int      `json:"dto_version"`
	BaseHead   Counter  `json:"base_head_seq"`
	Entities   []Entity `json:"entities"`
	Aliases    []Alias  `json:"aliases"`
	Edges      []Edge   `json:"successor_edges"`
}
type Deletion struct {
	OperationID, UserID, AuthEpoch ID
	CredentialVersion              int64
	State                          string
	TokenHash                      [32]byte
	ConfirmUntilMS, ExpiresMS      int64
	JobID                          *ID
	LedgerReceipt                  *string
	RequestedMS, UpdatedMS         int64
}
