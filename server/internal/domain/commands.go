package domain

import (
	"bytes"
	"encoding/json"
)

type Field[T any] struct {
	Present bool
	Null    bool
	Value   T
}

func (f *Field[T]) UnmarshalJSON(b []byte) error {
	f.Present = true
	f.Null = bytes.Equal(bytes.TrimSpace(b), []byte("null"))
	if f.Null {
		var zero T
		f.Value = zero
		return nil
	}
	return json.Unmarshal(b, &f.Value)
}
func (f Field[T]) IsZero() bool { return !f.Present }
func (f Field[T]) MarshalJSON() ([]byte, error) {
	if f.Null || !f.Present {
		return []byte("null"), nil
	}
	return json.Marshal(f.Value)
}

type Patch struct {
	Title         Field[string]   `json:"title,omitzero"`
	Notes         Field[string]   `json:"notes,omitzero"`
	PlanningState Field[string]   `json:"planning_state,omitzero"`
	CategoryID    Field[ID]       `json:"category_id,omitzero"`
	ParentTaskID  Field[ID]       `json:"parent_task_id,omitzero"`
	StartOn       Field[Date]     `json:"start_on,omitzero"`
	DeadlineOn    Field[Date]     `json:"deadline_on,omitzero"`
	TodayFocus    Field[bool]     `json:"today_focus,omitzero"`
	Evening       Field[bool]     `json:"evening,omitzero"`
	Pinned        Field[bool]     `json:"pinned,omitzero"`
	Important     Field[bool]     `json:"important,omitzero"`
	Schedule      Field[Schedule] `json:"schedule,omitzero"`
	Reminder      Field[Reminder] `json:"reminder,omitzero"`
	TagIDs        Field[[]ID]     `json:"tag_ids,omitzero"`
	Links         Field[[]Link]   `json:"links,omitzero"`
}
type Command interface {
	CommandType() string
	EventTime() Instant
	command()
}
type Event struct {
	Type    string  `json:"type"`
	EventAt Instant `json:"event_at_utc"`
}

func (e Event) CommandType() string { return e.Type }
func (e Event) EventTime() Instant  { return e.EventAt }
func (Event) command()              {}

type CreateTask struct {
	Event
	TaskID  ID     `json:"task_id"`
	Title   string `json:"title"`
	Initial Patch  `json:"initial"`
}
type PatchTask struct {
	Event
	TaskID ID    `json:"task_id"`
	Patch  Patch `json:"patch"`
}
type ChangeState struct {
	Event
	TaskIDs []ID   `json:"task_ids"`
	Action  string `json:"action"`
	GroupID ID     `json:"group_id"`
}
type MoveTree struct {
	Event
	TaskID       ID  `json:"task_id"`
	CategoryID   *ID `json:"category_id"`
	ParentTaskID *ID `json:"parent_task_id"`
}
type SaveCategory struct {
	Event
	CategoryID  ID        `json:"category_id"`
	Title       string    `json:"title"`
	Description string    `json:"description"`
	Color       string    `json:"color_key"`
	Archived    bool      `json:"archived"`
	ParentID    Field[ID] `json:"parent_id,omitzero"`
}
type RelocateCategory struct {
	Event
	CategoryID ID     `json:"category_id"`
	TargetID   ID     `json:"target_id"`
	Placement  string `json:"placement"`
}
type RemoveCategory struct {
	Event
	CategoryID ID `json:"category_id"`
}
type SaveTag struct {
	Event
	TagID ID     `json:"tag_id"`
	Title string `json:"title"`
	Color string `json:"color_key"`
}
type RemoveTag struct {
	Event
	TagID ID `json:"tag_id"`
}
type ReplaceRecurrence struct {
	Event
	SourceTaskID ID       `json:"source_task_id"`
	NewRuleID    ID       `json:"new_rule_id"`
	Rule         RuleSpec `json:"rule"`
	CutoffOn     Date     `json:"cutoff_on"`
}
type StopRecurrence struct {
	Event
	RuleID       ID   `json:"rule_id"`
	RemoveFuture bool `json:"remove_future"`
	CutoffOn     Date `json:"cutoff_on"`
}
type MaterializeRecurrence struct {
	Event
	RuleID    ID   `json:"rule_id"`
	FromOn    Date `json:"from_on"`
	ThroughOn Date `json:"through_on"`
}
type ReplaceOrdering struct {
	Event
	ScopeID    ID     `json:"scope_id"`
	Kind       string `json:"kind"`
	SubjectID  *ID    `json:"subject_id"`
	MemberIDs  []ID   `json:"member_ids"`
	OrderedIDs []ID   `json:"ordered_ids"`
}

var CommandTypes = []string{"task.create", "task.patch", "task.change_state", "task.move_tree", "category.save", "category.relocate", "category.remove", "tag.save", "tag.remove", "recurrence.replace", "recurrence.stop", "recurrence.materialize", "ordering.replace"}

type PayloadRef struct {
	JobID         ID     `json:"job_id"`
	CanonicalHash string `json:"canonical_sha256"`
}
type Mutation struct {
	ProtocolVersion                                  int
	DatasetID, DataEpoch, MutationID, InstallationID ID
	ClientSeq                                        Counter
	Preconditions                                    Preconditions
	Commands                                         []Command
	Ref                                              *PayloadRef
	Canonical                                        []byte
	Hash, OuterHash                                  [32]byte
}
