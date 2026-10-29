// Design-only wire declarations derived from sync-v1.openapi.json.
// The strict Codec must implement Field/union parsing; default encoding/json
// does NOT implement these contracts (presence, unknown/duplicate keys, UTF-8).
package synccontracts

import "encoding/json"

type WireField[T any] struct {
	Present bool
	Null    bool
	Value   T
}

type WireSchedule interface{ wireSchedule() }
type WireReminder interface{ wireReminder() }
type WireScheduleTemplate interface{ wireScheduleTemplate() }
type WireReminderTemplate interface{ wireReminderTemplate() }
type WireCommand interface{ wireCommand() }
type WirePushRequest interface{ wirePushRequest() }
type WireChange interface{ wireChange() }
type WireCounter string

type WirePositiveCounter string

type WireUUID string

type WireID string

type WireDate string

type WireInstant string

type WireLocalTime string

type WireZone string

type WireHash string

type WireToken string

type WireCursor string

type WireTitle string

type WireColor string

type WirePlanningState string

type WireEntityKind string

type WireStructureKind string

type WireEntityVersion struct {
	Type     WireEntityKind `json:"type"`
	ID       WireID         `json:"id"`
	Revision WireCounter    `json:"revision"`
}

type WireStructureVersion struct {
	Kind     WireStructureKind `json:"kind"`
	ID       WireUUID          `json:"id"`
	Revision WireCounter       `json:"revision"`
}

type WirePreconditions struct {
	Entities   []WireEntityVersion    `json:"entities"`
	Structures []WireStructureVersion `json:"structures"`
}

type WireAllDaySchedule struct {
	Kind    string   `json:"kind"`
	StartOn WireDate `json:"start_on"`
	EndOn   WireDate `json:"end_on"`
}

func (WireAllDaySchedule) wireSchedule() {}

type WireTimedSchedule struct {
	Kind       string      `json:"kind"`
	StartAtUTC WireInstant `json:"start_at_utc"`
	EndAtUTC   WireInstant `json:"end_at_utc"`
	TimeZone   WireZone    `json:"time_zone"`
}

func (WireTimedSchedule) wireSchedule() {}

type WireAbsoluteReminder struct {
	Kind        string      `json:"kind"`
	AnchorAtUTC WireInstant `json:"anchor_at_utc"`
	Repeat      string      `json:"repeat"`
	Every       int         `json:"every"`
	TimeZone    WireZone    `json:"time_zone"`
}

func (WireAbsoluteReminder) wireReminder() {}

type WireRelativeReminder struct {
	Kind          string   `json:"kind"`
	MinutesBefore int      `json:"minutes_before"`
	Repeat        string   `json:"repeat"`
	Every         int      `json:"every"`
	TimeZone      WireZone `json:"time_zone"`
}

func (WireRelativeReminder) wireReminder()         {}
func (WireRelativeReminder) wireReminderTemplate() {}

type WireTaskLink struct {
	TargetID WireID `json:"target_id"`
	Role     string `json:"role"`
}

type WireTaskPatch struct {
	Title         WireField[WireTitle]         `json:"title,omitempty"`
	Notes         WireField[string]            `json:"notes,omitempty"`
	PlanningState WireField[WirePlanningState] `json:"planning_state,omitempty"`
	CategoryID    WireField[WireID]            `json:"category_id,omitempty"`
	ParentTaskID  WireField[WireID]            `json:"parent_task_id,omitempty"`
	StartOn       WireField[WireDate]          `json:"start_on,omitempty"`
	DeadlineOn    WireField[WireDate]          `json:"deadline_on,omitempty"`
	TodayFocus    WireField[bool]              `json:"today_focus,omitempty"`
	Evening       WireField[bool]              `json:"evening,omitempty"`
	Pinned        WireField[bool]              `json:"pinned,omitempty"`
	Important     WireField[bool]              `json:"important,omitempty"`
	Schedule      WireField[WireSchedule]      `json:"schedule,omitempty"`
	Reminder      WireField[WireReminder]      `json:"reminder,omitempty"`
	TagIDs        WireField[[]WireID]          `json:"tag_ids,omitempty"`
	Links         WireField[[]WireTaskLink]    `json:"links,omitempty"`
}

type WireTaskInitial struct {
	Notes         WireField[string]            `json:"notes,omitempty"`
	PlanningState WireField[WirePlanningState] `json:"planning_state,omitempty"`
	CategoryID    WireField[WireID]            `json:"category_id,omitempty"`
	ParentTaskID  WireField[WireID]            `json:"parent_task_id,omitempty"`
	StartOn       WireField[WireDate]          `json:"start_on,omitempty"`
	DeadlineOn    WireField[WireDate]          `json:"deadline_on,omitempty"`
	TodayFocus    WireField[bool]              `json:"today_focus,omitempty"`
	Evening       WireField[bool]              `json:"evening,omitempty"`
	Pinned        WireField[bool]              `json:"pinned,omitempty"`
	Important     WireField[bool]              `json:"important,omitempty"`
	Schedule      WireField[WireSchedule]      `json:"schedule,omitempty"`
	Reminder      WireField[WireReminder]      `json:"reminder,omitempty"`
	TagIDs        WireField[[]WireID]          `json:"tag_ids,omitempty"`
	Links         WireField[[]WireTaskLink]    `json:"links,omitempty"`
}

type WireAllDayTemplate struct {
	Kind           string `json:"kind"`
	StartDayOffset int    `json:"start_day_offset"`
	EndDayOffset   int    `json:"end_day_offset"`
}

func (WireAllDayTemplate) wireScheduleTemplate() {}

type WireTimedTemplate struct {
	Kind           string        `json:"kind"`
	StartDayOffset int           `json:"start_day_offset"`
	EndDayOffset   int           `json:"end_day_offset"`
	StartLocalTime WireLocalTime `json:"start_local_time"`
	EndLocalTime   WireLocalTime `json:"end_local_time"`
	TimeZone       WireZone      `json:"time_zone"`
}

func (WireTimedTemplate) wireScheduleTemplate() {}

type WireAbsoluteReminderTemplate struct {
	Kind      string        `json:"kind"`
	DayOffset int           `json:"day_offset"`
	LocalTime WireLocalTime `json:"local_time"`
	Repeat    string        `json:"repeat"`
	Every     int           `json:"every"`
	TimeZone  WireZone      `json:"time_zone"`
}

func (WireAbsoluteReminderTemplate) wireReminderTemplate() {}

type WireTaskTemplate struct {
	TemplateVersion   int                  `json:"template_version"`
	Title             WireTitle            `json:"title"`
	Notes             string               `json:"notes"`
	PlanningState     WirePlanningState    `json:"planning_state"`
	CategoryID        *WireID              `json:"category_id"`
	ParentTaskID      *WireID              `json:"parent_task_id"`
	TodayFocus        bool                 `json:"today_focus"`
	Evening           bool                 `json:"evening"`
	Pinned            bool                 `json:"pinned"`
	Important         bool                 `json:"important"`
	TagIDs            []WireID             `json:"tag_ids"`
	Links             []WireTaskLink       `json:"links"`
	StartDayOffset    *int                 `json:"start_day_offset"`
	DeadlineDayOffset *int                 `json:"deadline_day_offset"`
	Schedule          WireScheduleTemplate `json:"schedule"`
	Reminder          WireReminderTemplate `json:"reminder"`
}

type WireRuleSpec struct {
	Mode         string           `json:"mode"`
	Frequency    string           `json:"frequency"`
	Interval     int              `json:"interval"`
	AnchorOn     WireDate         `json:"anchor_on"`
	EndOn        *WireDate        `json:"end_on"`
	WeekdaysMask int              `json:"weekdays_mask"`
	MonthDay     int              `json:"month_day"`
	YearMonth    int              `json:"year_month"`
	TimeZone     WireZone         `json:"time_zone"`
	Defaults     WireTaskTemplate `json:"defaults"`
}

type WireTaskCreate struct {
	Type       string           `json:"type"`
	TaskID     WireID           `json:"task_id"`
	Title      WireTitle        `json:"title"`
	Initial    *WireTaskInitial `json:"initial,omitempty"`
	EventAtUTC WireInstant      `json:"event_at_utc"`
}

func (WireTaskCreate) wireCommand() {}

type WireTaskPatchCommand struct {
	Type       string        `json:"type"`
	TaskID     WireID        `json:"task_id"`
	Patch      WireTaskPatch `json:"patch"`
	EventAtUTC WireInstant   `json:"event_at_utc"`
}

func (WireTaskPatchCommand) wireCommand() {}

type WireTaskChangeState struct {
	Type       string      `json:"type"`
	TaskIDs    []WireID    `json:"task_ids"`
	Action     string      `json:"action"`
	GroupID    WireID      `json:"group_id"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireTaskChangeState) wireCommand() {}

type WireTaskMoveTree struct {
	Type         string      `json:"type"`
	TaskID       WireID      `json:"task_id"`
	CategoryID   *WireID     `json:"category_id"`
	ParentTaskID *WireID     `json:"parent_task_id"`
	EventAtUTC   WireInstant `json:"event_at_utc"`
}

func (WireTaskMoveTree) wireCommand() {}

type WireCategorySave struct {
	Type        string            `json:"type"`
	CategoryID  WireID            `json:"category_id"`
	Title       WireTitle         `json:"title"`
	Description string            `json:"description"`
	ColorKey    WireColor         `json:"color_key"`
	Archived    bool              `json:"archived"`
	ParentID    WireField[WireID] `json:"parent_id,omitempty"`
	EventAtUTC  WireInstant       `json:"event_at_utc"`
}

func (WireCategorySave) wireCommand() {}

type WireCategoryRelocate struct {
	Type       string      `json:"type"`
	CategoryID WireID      `json:"category_id"`
	TargetID   WireID      `json:"target_id"`
	Placement  string      `json:"placement"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireCategoryRelocate) wireCommand() {}

type WireCategoryRemove struct {
	Type       string      `json:"type"`
	CategoryID WireID      `json:"category_id"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireCategoryRemove) wireCommand() {}

type WireTagSave struct {
	Type       string      `json:"type"`
	TagID      WireID      `json:"tag_id"`
	Title      WireTitle   `json:"title"`
	ColorKey   WireColor   `json:"color_key"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireTagSave) wireCommand() {}

type WireTagRemove struct {
	Type       string      `json:"type"`
	TagID      WireID      `json:"tag_id"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireTagRemove) wireCommand() {}

type WireRecurrenceReplace struct {
	Type         string       `json:"type"`
	SourceTaskID WireID       `json:"source_task_id"`
	NewRuleID    WireID       `json:"new_rule_id"`
	Rule         WireRuleSpec `json:"rule"`
	CutoffOn     WireDate     `json:"cutoff_on"`
	EventAtUTC   WireInstant  `json:"event_at_utc"`
}

func (WireRecurrenceReplace) wireCommand() {}

type WireRecurrenceStop struct {
	Type         string      `json:"type"`
	RuleID       WireID      `json:"rule_id"`
	RemoveFuture bool        `json:"remove_future"`
	CutoffOn     WireDate    `json:"cutoff_on"`
	EventAtUTC   WireInstant `json:"event_at_utc"`
}

func (WireRecurrenceStop) wireCommand() {}

type WireRecurrenceMaterialize struct {
	Type       string      `json:"type"`
	RuleID     WireID      `json:"rule_id"`
	FromOn     WireDate    `json:"from_on"`
	ThroughOn  WireDate    `json:"through_on"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireRecurrenceMaterialize) wireCommand() {}

type WireOrderingReplace struct {
	Type       string      `json:"type"`
	ScopeID    WireID      `json:"scope_id"`
	Kind       string      `json:"kind"`
	SubjectID  *WireID     `json:"subject_id"`
	MemberIDs  []WireID    `json:"member_ids"`
	OrderedIDs []WireID    `json:"ordered_ids"`
	EventAtUTC WireInstant `json:"event_at_utc"`
}

func (WireOrderingReplace) wireCommand() {}

type WireInlinePayload struct {
	Preconditions WirePreconditions `json:"preconditions"`
	Commands      []WireCommand     `json:"commands"`
}

type WireInlinePush struct {
	ProtocolVersion int               `json:"protocol_version"`
	DataEpoch       WireID            `json:"data_epoch"`
	MutationID      WireID            `json:"mutation_id"`
	InstallationID  WireID            `json:"installation_id"`
	ClientSeq       WireCounter       `json:"client_seq"`
	Preconditions   WirePreconditions `json:"preconditions"`
	Commands        []WireCommand     `json:"commands"`
}

func (WireInlinePush) wirePushRequest() {}

type WirePayloadRef struct {
	JobID           WireID   `json:"job_id"`
	CanonicalSHA256 WireHash `json:"canonical_sha256"`
}

type WireStagedPush struct {
	ProtocolVersion int            `json:"protocol_version"`
	DataEpoch       WireID         `json:"data_epoch"`
	MutationID      WireID         `json:"mutation_id"`
	InstallationID  WireID         `json:"installation_id"`
	ClientSeq       WireCounter    `json:"client_seq"`
	PayloadRef      WirePayloadRef `json:"payload_ref"`
}

func (WireStagedPush) wirePushRequest() {}

type WireLegacyMetadata struct {
	FormatVersion       int    `json:"format_version"`
	SourceExportVersion int    `json:"source_export_version"`
	CanonicalJSON       string `json:"canonical_json"`
}

type WireTask struct {
	ID                 WireID              `json:"id"`
	Title              WireTitle           `json:"title"`
	Notes              string              `json:"notes"`
	PlanningState      WirePlanningState   `json:"planning_state"`
	CategoryID         *WireID             `json:"category_id"`
	ParentTaskID       *WireID             `json:"parent_task_id"`
	StartOn            *WireDate           `json:"start_on"`
	DeadlineOn         *WireDate           `json:"deadline_on"`
	TodayFocus         bool                `json:"today_focus"`
	Evening            bool                `json:"evening"`
	Pinned             bool                `json:"pinned"`
	Important          bool                `json:"important"`
	Schedule           WireSchedule        `json:"schedule"`
	Reminder           WireReminder        `json:"reminder"`
	TagIDs             []WireID            `json:"tag_ids"`
	Links              []WireTaskLink      `json:"links"`
	Status             string              `json:"status"`
	CreatedAtUTC       WireInstant         `json:"created_at_utc"`
	UpdatedAtUTC       WireInstant         `json:"updated_at_utc"`
	CompletedAtUTC     *WireInstant        `json:"completed_at_utc"`
	CancelledAtUTC     *WireInstant        `json:"cancelled_at_utc"`
	DeletedAtUTC       *WireInstant        `json:"deleted_at_utc"`
	DeletionGroupID    *WireID             `json:"deletion_group_id"`
	CompletionGroupID  *WireID             `json:"completion_group_id"`
	RecurrenceID       *WireID             `json:"recurrence_id"`
	OccurrenceOn       *WireDate           `json:"occurrence_on"`
	OccurrenceKey      *string             `json:"occurrence_key"`
	InstanceOverridden bool                `json:"instance_overridden"`
	LegacyMetadata     *WireLegacyMetadata `json:"legacy_metadata"`
}

type WireCategory struct {
	ID             WireID              `json:"id"`
	CreatedAtUTC   WireInstant         `json:"created_at_utc"`
	UpdatedAtUTC   WireInstant         `json:"updated_at_utc"`
	DeletedAtUTC   *WireInstant        `json:"deleted_at_utc"`
	ParentID       *WireID             `json:"parent_id"`
	Title          WireTitle           `json:"title"`
	Description    string              `json:"description"`
	ColorKey       WireColor           `json:"color_key"`
	Archived       bool                `json:"archived"`
	LegacyMetadata *WireLegacyMetadata `json:"legacy_metadata"`
}

type WireTag struct {
	ID           WireID       `json:"id"`
	CreatedAtUTC WireInstant  `json:"created_at_utc"`
	UpdatedAtUTC WireInstant  `json:"updated_at_utc"`
	DeletedAtUTC *WireInstant `json:"deleted_at_utc"`
	Title        WireTitle    `json:"title"`
	ColorKey     WireColor    `json:"color_key"`
}

type WireRecurrenceRule struct {
	ID           WireID       `json:"id"`
	CreatedAtUTC WireInstant  `json:"created_at_utc"`
	UpdatedAtUTC WireInstant  `json:"updated_at_utc"`
	DeletedAtUTC *WireInstant `json:"deleted_at_utc"`
	SourceTaskID *WireID      `json:"source_task_id"`
	Rule         WireRuleSpec `json:"rule"`
	Active       bool         `json:"active"`
}

type WireOrderingScope struct {
	ID         WireID   `json:"id"`
	Kind       string   `json:"kind"`
	SubjectID  *WireID  `json:"subject_id"`
	OrderedIDs []WireID `json:"ordered_ids"`
}

type WireTaskChange struct {
	Type      string              `json:"type"`
	ID        WireID              `json:"id"`
	Revision  WirePositiveCounter `json:"revision"`
	Operation string              `json:"operation"`
	Value     WireTask            `json:"value"`
}

func (WireTaskChange) wireChange() {}

type WireCategoryChange struct {
	Type      string              `json:"type"`
	ID        WireID              `json:"id"`
	Revision  WirePositiveCounter `json:"revision"`
	Operation string              `json:"operation"`
	Value     WireCategory        `json:"value"`
}

func (WireCategoryChange) wireChange() {}

type WireTagChange struct {
	Type      string              `json:"type"`
	ID        WireID              `json:"id"`
	Revision  WirePositiveCounter `json:"revision"`
	Operation string              `json:"operation"`
	Value     WireTag             `json:"value"`
}

func (WireTagChange) wireChange() {}

type WireRecurrenceRuleChange struct {
	Type      string              `json:"type"`
	ID        WireID              `json:"id"`
	Revision  WirePositiveCounter `json:"revision"`
	Operation string              `json:"operation"`
	Value     WireRecurrenceRule  `json:"value"`
}

func (WireRecurrenceRuleChange) wireChange() {}

type WireOrderingScopeChange struct {
	Type      string              `json:"type"`
	ID        WireID              `json:"id"`
	Revision  WirePositiveCounter `json:"revision"`
	Operation string              `json:"operation"`
	Value     WireOrderingScope   `json:"value"`
}

func (WireOrderingScopeChange) wireChange() {}

type WireAlias struct {
	AliasID     WireID `json:"alias_id"`
	CanonicalID WireID `json:"canonical_id"`
}

type WireSuccessorEdge struct {
	RuleID        WireID `json:"rule_id"`
	PredecessorID WireID `json:"predecessor_id"`
	SuccessorID   WireID `json:"successor_id"`
}

type WireChangeBatch struct {
	Seq                  WirePositiveCounter    `json:"seq"`
	OriginMutationID     WireID                 `json:"origin_mutation_id"`
	OriginInstallationID WireID                 `json:"origin_installation_id"`
	CommittedAtUTC       WireInstant            `json:"committed_at_utc"`
	Changes              []WireChange           `json:"changes"`
	StructureRevisions   []WireStructureVersion `json:"structure_revisions"`
	Aliases              []WireAlias            `json:"aliases"`
	SuccessorEdges       []WireSuccessorEdge    `json:"successor_edges"`
}

type WireBatchRef struct {
	Seq      WirePositiveCounter `json:"seq"`
	BatchRef json.RawMessage     `json:"batch_ref"`
}

type WireDeliveredBatch struct {
	Batch       json.RawMessage `json:"batch"`
	AfterCursor WireCursor      `json:"after_cursor"`
}

type WirePullResponse struct {
	ProtocolVersion int                  `json:"protocol_version"`
	DataEpoch       WireID               `json:"data_epoch"`
	ThroughSeq      WireCounter          `json:"through_seq"`
	Batches         []WireDeliveredBatch `json:"batches"`
	NextCursor      WireCursor           `json:"next_cursor"`
	HasMore         bool                 `json:"has_more"`
	Metadata        *WireMetadata        `json:"metadata,omitempty"`
}

type WireReceipt struct {
	ProtocolVersion    int                    `json:"protocol_version"`
	MutationID         WireID                 `json:"mutation_id"`
	DataEpoch          WireID                 `json:"data_epoch"`
	Result             string                 `json:"result"`
	BatchSeq           *WirePositiveCounter   `json:"batch_seq"`
	ObservedHeadSeq    WireCounter            `json:"observed_head_seq"`
	EntityRevisions    []WireEntityVersion    `json:"entity_revisions"`
	StructureRevisions []WireStructureVersion `json:"structure_revisions"`
	CommittedAtUTC     WireInstant            `json:"committed_at_utc"`
	Metadata           *WireMetadata          `json:"metadata,omitempty"`
}

type WireConflict struct {
	Type             WireEntityKind `json:"type"`
	ID               WireID         `json:"id"`
	ExpectedRevision WireCounter    `json:"expected_revision"`
	ActualRevision   WireCounter    `json:"actual_revision"`
}

type WireProblem struct {
	Type               string                  `json:"type"`
	Title              string                  `json:"title"`
	Status             int                     `json:"status"`
	Instance           string                  `json:"instance"`
	Code               string                  `json:"code"`
	Retryable          bool                    `json:"retryable"`
	FieldPath          *string                 `json:"field_path,omitempty"`
	RetryAfterMs       *int                    `json:"retry_after_ms,omitempty"`
	Conflicts          *[]WireConflict         `json:"conflicts,omitempty"`
	StructureConflicts *[]WireStructureVersion `json:"structure_conflicts,omitempty"`
	Metadata           *WireMetadata           `json:"metadata,omitempty"`
}

type WireCapabilities struct {
	ProtocolVersion           int                 `json:"protocol_version"`
	ServerInstanceID          WireID              `json:"server_instance_id"`
	SupportedProtocolVersions []int               `json:"supported_protocol_versions"`
	SupportedDTOVersions      []int               `json:"supported_dto_versions"`
	DTOVersion                int                 `json:"dto_version"`
	CommandTypes              []string            `json:"command_types"`
	MaxInlineBytes            int                 `json:"max_inline_bytes"`
	MaxInlineCommands         int                 `json:"max_inline_commands"`
	MaxInlineEntities         int                 `json:"max_inline_entities"`
	MaxChunkBytes             int                 `json:"max_chunk_bytes"`
	MaxJobBytes               WirePositiveCounter `json:"max_job_bytes"`
	MaxBatchReassemblyBytes   WirePositiveCounter `json:"max_batch_reassembly_bytes"`
	Features                  json.RawMessage     `json:"features"`
	Metadata                  *WireMetadata       `json:"metadata,omitempty"`
}

type WireLoginRequest struct {
	ProtocolVersion int    `json:"protocol_version"`
	Login           string `json:"login"`
	Password        string `json:"password"`
	InstallationID  WireID `json:"installation_id"`
	DeviceName      string `json:"device_name"`
	Platform        string `json:"platform"`
}

type WireRefreshRequest struct {
	ProtocolVersion int       `json:"protocol_version"`
	RefreshToken    WireToken `json:"refresh_token"`
}

type WireTokenPair struct {
	ProtocolVersion     int                 `json:"protocol_version"`
	AccessToken         WireToken           `json:"access_token"`
	RefreshToken        WireToken           `json:"refresh_token"`
	AccessExpiresAtUTC  WireInstant         `json:"access_expires_at_utc"`
	RefreshExpiresAtUTC WireInstant         `json:"refresh_expires_at_utc"`
	SessionID           WireID              `json:"session_id"`
	Generation          WirePositiveCounter `json:"generation"`
	Metadata            *WireMetadata       `json:"metadata,omitempty"`
}

type WirePasswordChange struct {
	ProtocolVersion int    `json:"protocol_version"`
	CurrentPassword string `json:"current_password"`
	NewPassword     string `json:"new_password"`
}

type WireDataset struct {
	ID        WireID      `json:"id"`
	DataEpoch WireID      `json:"data_epoch"`
	HeadSeq   WireCounter `json:"head_seq"`
}

type WireMe struct {
	ProtocolVersion int           `json:"protocol_version"`
	UserID          WireID        `json:"user_id"`
	Login           string        `json:"login"`
	Datasets        []WireDataset `json:"datasets"`
	Metadata        *WireMetadata `json:"metadata,omitempty"`
}

type WireSession struct {
	ID             WireID      `json:"id"`
	InstallationID WireID      `json:"installation_id"`
	DeviceName     string      `json:"device_name"`
	Platform       string      `json:"platform"`
	CreatedAtUTC   WireInstant `json:"created_at_utc"`
	ExpiresAtUTC   WireInstant `json:"expires_at_utc"`
	Current        bool        `json:"current"`
}

type WireSessions struct {
	ProtocolVersion int           `json:"protocol_version"`
	Sessions        []WireSession `json:"sessions"`
	Metadata        *WireMetadata `json:"metadata,omitempty"`
}

type WireProgress struct {
	ProtocolVersion int         `json:"protocol_version"`
	DataEpoch       WireID      `json:"data_epoch"`
	AppliedSeq      WireCounter `json:"applied_seq"`
}

type WireOperationRequest struct {
	ProtocolVersion int    `json:"protocol_version"`
	DataEpoch       WireID `json:"data_epoch"`
	OperationID     WireID `json:"operation_id"`
}

type WireStagedCreate struct {
	ProtocolVersion int                 `json:"protocol_version"`
	DataEpoch       WireID              `json:"data_epoch"`
	OperationID     WireID              `json:"operation_id"`
	MutationID      WireID              `json:"mutation_id"`
	InstallationID  WireID              `json:"installation_id"`
	ClientSeq       WireCounter         `json:"client_seq"`
	ExpectedBytes   WirePositiveCounter `json:"expected_bytes"`
}

type WireImportCreate struct {
	ProtocolVersion     int                 `json:"protocol_version"`
	DataEpoch           WireID              `json:"data_epoch"`
	OperationID         WireID              `json:"operation_id"`
	MutationID          WireID              `json:"mutation_id"`
	InstallationID      WireID              `json:"installation_id"`
	ClientSeq           WireCounter         `json:"client_seq"`
	ExpectedBytes       WirePositiveCounter `json:"expected_bytes"`
	BaseHeadSeq         WireCounter         `json:"base_head_seq"`
	DTOVersion          int                 `json:"dto_version"`
	SourceExportVersion int                 `json:"source_export_version"`
}

type WireJob struct {
	ProtocolVersion int           `json:"protocol_version"`
	ID              WireID        `json:"id"`
	DatasetID       *WireID       `json:"dataset_id"`
	DataEpoch       *WireID       `json:"data_epoch"`
	Kind            string        `json:"kind"`
	State           string        `json:"state"`
	ExpiresAtUTC    WireInstant   `json:"expires_at_utc"`
	BytesReserved   WireCounter   `json:"bytes_reserved"`
	SafeErrorCode   *string       `json:"safe_error_code"`
	Metadata        *WireMetadata `json:"metadata,omitempty"`
}

type WireChunk struct {
	Index     int         `json:"index"`
	ByteCount WireCounter `json:"byte_count"`
	SHA256    WireHash    `json:"sha256"`
}

type WireSealRequest struct {
	ProtocolVersion int         `json:"protocol_version"`
	DataEpoch       WireID      `json:"data_epoch"`
	Chunks          []WireChunk `json:"chunks"`
	TotalBytes      WireCounter `json:"total_bytes"`
	PayloadSHA256   WireHash    `json:"payload_sha256"`
}

type WireSealed struct {
	Job        WireJob         `json:"job"`
	PayloadRef *WirePayloadRef `json:"payload_ref"`
	Metadata   *WireMetadata   `json:"metadata,omitempty"`
}

type WireManifest struct {
	ProtocolVersion int           `json:"protocol_version"`
	JobID           WireID        `json:"job_id"`
	DatasetID       *WireID       `json:"dataset_id"`
	DataEpoch       *WireID       `json:"data_epoch"`
	Kind            string        `json:"kind"`
	DTOVersion      int           `json:"dto_version"`
	CutoffSeq       *WireCounter  `json:"cutoff_seq"`
	EntityCount     WireCounter   `json:"entity_count"`
	TotalBytes      WireCounter   `json:"total_bytes"`
	Chunks          []WireChunk   `json:"chunks"`
	PayloadSHA256   WireHash      `json:"payload_sha256"`
	ManifestSHA256  WireHash      `json:"manifest_sha256"`
	ExpiresAtUTC    WireInstant   `json:"expires_at_utc"`
	ResumeCursor    *WireCursor   `json:"resume_cursor"`
	Metadata        *WireMetadata `json:"metadata,omitempty"`
}

type WireSnapshotPayload struct {
	DTOVersion         int                    `json:"dto_version"`
	DatasetID          WireID                 `json:"dataset_id"`
	DataEpoch          WireID                 `json:"data_epoch"`
	CutoffSeq          WireCounter            `json:"cutoff_seq"`
	Entities           []WireChange           `json:"entities"`
	StructureRevisions []WireStructureVersion `json:"structure_revisions"`
	Aliases            []WireAlias            `json:"aliases"`
	SuccessorEdges     []WireSuccessorEdge    `json:"successor_edges"`
}

type WireExportBundle struct {
	ExportFormatVersion int                   `json:"export_format_version"`
	Datasets            []WireSnapshotPayload `json:"datasets"`
}

type WireImportPayload struct {
	DTOVersion     int                 `json:"dto_version"`
	BaseHeadSeq    WireCounter         `json:"base_head_seq"`
	Entities       []WireChange        `json:"entities"`
	Aliases        []WireAlias         `json:"aliases"`
	SuccessorEdges []WireSuccessorEdge `json:"successor_edges"`
}

type WireRenewRequest struct {
	ProtocolVersion int    `json:"protocol_version"`
	DataEpoch       WireID `json:"data_epoch"`
	ExtendSeconds   int    `json:"extend_seconds"`
}

type WireReleaseRequest struct {
	ProtocolVersion int         `json:"protocol_version"`
	DataEpoch       WireID      `json:"data_epoch"`
	AppliedSeq      WireCounter `json:"applied_seq"`
}

type WireExportRequest struct {
	ProtocolVersion int    `json:"protocol_version"`
	OperationID     WireID `json:"operation_id"`
}

type WireDeletionPrepare struct {
	ProtocolVersion int    `json:"protocol_version"`
	OperationID     WireID `json:"operation_id"`
	CurrentPassword string `json:"current_password"`
}

type WireDeletionIntent struct {
	ProtocolVersion int           `json:"protocol_version"`
	OperationID     WireID        `json:"operation_id"`
	DeletionToken   WireToken     `json:"deletion_token"`
	ConfirmUntilUTC WireInstant   `json:"confirm_until_utc"`
	ExpiresAtUTC    WireInstant   `json:"expires_at_utc"`
	Metadata        *WireMetadata `json:"metadata,omitempty"`
}

type WireDeletionConfirm struct {
	ProtocolVersion int    `json:"protocol_version"`
	OperationID     WireID `json:"operation_id"`
}

type WireDeletionStatus struct {
	ProtocolVersion int           `json:"protocol_version"`
	OperationID     WireID        `json:"operation_id"`
	State           string        `json:"state"`
	UpdatedAtUTC    WireInstant   `json:"updated_at_utc"`
	Metadata        *WireMetadata `json:"metadata,omitempty"`
}

type WireHealth struct {
	Status string `json:"status"`
}

type WireMetadata map[string]json.RawMessage
