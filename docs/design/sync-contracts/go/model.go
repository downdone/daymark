// Package synccontracts contains design contracts only. It is not a server.
// Values are trusted only after codec/domain validation; constructors and all
// business, storage and transport implementations belong to later DEV slices.
package synccontracts

import "time"

type ID string
type Revision int64
type Seq int64
type Instant int64 // UTC Unix milliseconds, not local calendar time.
type Date string   // Validated proleptic Gregorian YYYY-MM-DD, years 0001..9999.
type Zone string   // Validated IANA name; version of tzdb is a deployment input.
type Digest [32]byte
type Token string // Secret: never put in logs, errors, URLs or persisted DTOs.
type Cursor string
type FieldState uint8

const (
	Unspecified FieldState = iota
	Clear
	Set
)

// Field cannot use encoding/json's default unmarshalling. The codec retains
// absent vs null vs value, rejects Clear on non-nullable fields and rejects
// non-zero Value for Unspecified/Clear. Empty slice Set means replace with [].
type Field[T any] struct {
	State FieldState
	Value T
}

type Principal struct {
	UserID, SessionID, InstallationID, AuthEpoch ID
	CredentialVersion                            int64
	ExpiresAt                                    Instant
}
type Scope struct {
	Principal            Principal
	DatasetID, DataEpoch ID
}
type EntityKind string

const (
	TaskKind     EntityKind = "task"
	CategoryKind EntityKind = "category"
	TagKind      EntityKind = "tag"
	RuleKind     EntityKind = "recurrence_rule"
	OrderingKind EntityKind = "ordering_scope"
)

type StructureKind string

const (
	TaskTree          StructureKind = "task_tree"
	CategoryTree      StructureKind = "category_tree"
	CategoryMembers   StructureKind = "category_members"
	TagMembers        StructureKind = "tag_members"
	RecurrenceMembers StructureKind = "recurrence_members"
	OrderingStructure StructureKind = "ordering_scope"
)

type EntityKey struct {
	Kind EntityKind
	ID   ID
}
type StructureKey struct {
	Kind StructureKind
	ID   ID
}
type EntityVersion struct {
	Key      EntityKey
	Revision Revision
}
type StructureVersion struct {
	Key      StructureKey
	Revision Revision
}
type Preconditions struct {
	Entities   []EntityVersion
	Structures []StructureVersion
}
type PlanState string

const (
	Inbox   PlanState = "INBOX"
	Active  PlanState = "ACTIVE"
	Someday PlanState = "SOMEDAY"
)

type TaskState string

const (
	Open      TaskState = "OPEN"
	Completed TaskState = "COMPLETED"
	Cancelled TaskState = "CANCELLED"
)

type Color string

const (
	Neutral Color = "neutral"
	Blue    Color = "blue"
	Green   Color = "green"
	Orange  Color = "orange"
	Purple  Color = "purple"
)

type Flags struct{ TodayFocus, Evening, Pinned, Important bool }

type Schedule interface{ schedule() }
type AllDaySchedule struct{ StartOn, EndOn Date } // EndOn exclusive.
type TimedSchedule struct {
	StartUTC, EndUTC Instant
	TimeZone         Zone
}

func (AllDaySchedule) schedule() {}
func (TimedSchedule) schedule()  {}

type Reminder interface{ reminder() }
type ReminderRepeat string

const (
	Once    ReminderRepeat = "NONE"
	Daily   ReminderRepeat = "DAILY"
	Weekly  ReminderRepeat = "WEEKLY"
	Monthly ReminderRepeat = "MONTHLY"
	Yearly  ReminderRepeat = "YEARLY"
)

type AbsoluteReminder struct {
	AnchorUTC Instant
	Repeat    ReminderRepeat
	Every     int
	TimeZone  Zone
}
type RelativeReminder struct {
	MinutesBefore int
	TimeZone      Zone
}

func (AbsoluteReminder) reminder() {}
func (RelativeReminder) reminder() {}

type LinkRole string

const (
	Preparation LinkRole = "preparation"
	Followup    LinkRole = "followup"
)

type TaskLink struct {
	TargetID ID
	Role     LinkRole
}

// Legacy canonical JSON is a validated compatibility capsule, never an untyped
// current entity or a source of executable expressions.
type LegacyMetadata struct {
	FormatVersion, SourceExportVersion int
	CanonicalJSON                      []byte
}

type Task struct {
	ID                                  ID
	Revision                            Revision
	Title, Notes                        string
	PlanningState                       PlanState
	Status                              TaskState
	Flags                               Flags
	CategoryID, ParentTaskID            *ID
	StartOn, DeadlineOn                 *Date
	CreatedAt, UpdatedAt                Instant
	CompletedAt, CancelledAt, DeletedAt *Instant
	DeletionGroupID, CompletionGroupID  *ID
	Schedule                            Schedule
	Reminder                            Reminder // nil means absent; never typed nil.
	RecurrenceID                        *ID
	OccurrenceOn                        *Date
	OccurrenceKey                       *string
	InstanceOverridden                  bool
	Legacy                              *LegacyMetadata
	TagIDs                              []ID
	Links                               []TaskLink
}
type Category struct {
	ID                   ID
	Revision             Revision
	ParentID             *ID
	Title, Description   string
	Color                Color
	Archived             bool
	CreatedAt, UpdatedAt Instant
	DeletedAt            *Instant
	Legacy               *LegacyMetadata
}
type Tag struct {
	ID                   ID
	Revision             Revision
	Title                string
	Color                Color
	CreatedAt, UpdatedAt Instant
	DeletedAt            *Instant
}
type OrderingScope struct {
	ID         ID
	Revision   Revision
	Kind       string
	SubjectID  *ID
	OrderedIDs []ID
}

type ScheduleTemplate struct {
	AllDay                       bool
	StartDayOffset, EndDayOffset int
	StartLocalTime, EndLocalTime *string // HH:mm:ss.SSS, absent for all-day.
	TimeZone                     *Zone   // absent for all-day; validated disjoint variants by codec.
}
type ReminderTemplate struct {
	Kind          string
	MinutesBefore *int
	DayOffset     *int
	LocalTime     *string
	Repeat        ReminderRepeat
	Every         int
	TimeZone      Zone
}
type TaskTemplate struct {
	Title, Notes                      string
	PlanningState                     PlanState
	Flags                             Flags
	CategoryID, ParentTaskID          *ID
	StartDayOffset, DeadlineDayOffset *int
	Schedule                          *ScheduleTemplate
	Reminder                          *ReminderTemplate
	TagIDs                            []ID
	Links                             []TaskLink
}
type RuleSpec struct {
	Mode, Frequency                   string
	Interval                          int
	AnchorOn                          Date
	EndOn                             *Date
	WeekdaysMask, MonthDay, YearMonth int
	TimeZone                          Zone
	Defaults                          TaskTemplate
}
type Rule struct {
	ID                   ID
	Revision             Revision
	SourceTaskID         *ID
	Spec                 RuleSpec
	Active               bool
	CreatedAt, UpdatedAt Instant
	DeletedAt            *Instant
}
type Alias struct{ AliasID, CanonicalID ID }
type SuccessorEdge struct{ RuleID, PredecessorID, SuccessorID ID }
type EntityValue interface{ entityValue() }

func (Task) entityValue()          {}
func (Category) entityValue()      {}
func (Tag) entityValue()           {}
func (Rule) entityValue()          {}
func (OrderingScope) entityValue() {}

type EntitySnapshot struct {
	Key      EntityKey
	Revision Revision
	Value    EntityValue
}
type Graph struct {
	Entities   map[EntityKey]EntitySnapshot
	Structures map[StructureKey]Revision
	Aliases    []Alias
	Successors []SuccessorEdge
	Loaded     ReadRequirements
}

// Expansion keys request complete relation closures, including empty sets.
type Expansion struct {
	Kind StructureKind
	ID   ID
}
type ReadRequirements struct {
	Entities   []EntityKey
	Expansions []Expansion
	Structures []StructureKey
}
type EntityDelta struct {
	Before *EntitySnapshot
	After  EntitySnapshot
}
type Plan struct {
	ReadSet    Preconditions
	Changes    []EntityDelta
	Structures []StructureVersion
	Aliases    []Alias
	Successors []SuccessorEdge
	NoOp       bool
}
type EventContext struct{ ReceivedAt Instant } // Per-command EventAt is frozen.
type Mutation struct {
	ProtocolVersion                                  int
	DatasetID, DataEpoch, MutationID, InstallationID ID
	ClientSeq                                        Seq
	Preconditions                                    Preconditions
	Commands                                         []Command
	CanonicalHash                                    Digest
}
type ImportGraph struct {
	DTOVersion int
	BaseHead   Seq
	Entities   []EntitySnapshot
	Aliases    []Alias
	Successors []SuccessorEdge
}
type ImportMutation struct {
	ProtocolVersion                                  int
	DatasetID, DataEpoch, MutationID, InstallationID ID
	ClientSeq                                        Seq
	SourceExportVersion                              int
	Payload                                          ImportGraph
}
type Receipt struct {
	MutationID, DataEpoch ID
	Result                string
	BatchSeq              *Seq
	ObservedHead          Seq
	Entities              []EntityVersion
	Structures            []StructureVersion
	CommittedAt           Instant
}
type ChangeBatch struct {
	Seq                                    Seq
	OriginMutationID, OriginInstallationID ID
	CommittedAt                            Instant
	Changes                                []EntitySnapshot
	Structures                             []StructureVersion
	Aliases                                []Alias
	Successors                             []SuccessorEdge
}
type BatchReference struct {
	Seq   Seq
	JobID ID
	Hash  Digest
	Bytes int64
}
type DeliveredBatch struct {
	Batch       *ChangeBatch
	Reference   *BatchReference
	AfterCursor Cursor
}
type PullPage struct {
	DataEpoch ID
	Through   Seq
	Batches   []DeliveredBatch
	Next      Cursor
	HasMore   bool
}
type CursorClaims struct {
	Version                                 int
	KeyID                                   string
	OwnerID, ServerID, DatasetID, DataEpoch ID
	After                                   Seq
	Through                                 *Seq
	ExpiresAt                               Instant
}
type Fault struct {
	Code       string
	FieldPath  string
	RetryAfter time.Duration
	SafeArgs   map[string]string
}

// Ports return error whose public form is mapped to Fault. Implementations must
// preserve errors.Is/As; SQL text and secret data never enter the public Fault.
