package synccontracts

// Command is a closed union. Markers implement type membership only.
type Command interface{ command() }
type TaskPatch struct {
	Title, Notes                           Field[string]
	PlanningState                          Field[PlanState]
	CategoryID, ParentTaskID               Field[ID]
	StartOn, DeadlineOn                    Field[Date]
	TodayFocus, Evening, Pinned, Important Field[bool]
	Schedule                               Field[Schedule]
	Reminder                               Field[Reminder]
	TagIDs                                 Field[[]ID]
	Links                                  Field[[]TaskLink]
}

// Initial uses TaskPatch's fields except Title; Title.State must Unspecified.
// State/identity/revision/legacy/group fields cannot be patched or initialized.
type TaskCreate struct {
	TaskID  ID
	Title   string
	Initial TaskPatch
	EventAt Instant
}
type TaskPatchCommand struct {
	TaskID  ID
	Patch   TaskPatch
	EventAt Instant
}
type StateAction string

const (
	Complete StateAction = "complete"
	Reopen   StateAction = "reopen"
	Delete   StateAction = "delete"
	Restore  StateAction = "restore"
	Cancel   StateAction = "cancel"
)

type TaskChangeState struct {
	TaskIDs []ID
	Action  StateAction
	GroupID ID
	EventAt Instant
}
type TaskMoveTree struct {
	TaskID                   ID
	CategoryID, ParentTaskID *ID
	EventAt                  Instant
}
type CategorySave struct {
	CategoryID         ID
	Title, Description string
	Color              Color
	Archived           bool
	ParentID           Field[ID]
	EventAt            Instant
}
type CategoryRelocate struct {
	CategoryID, TargetID ID
	Placement            string
	EventAt              Instant
}
type CategoryRemove struct {
	CategoryID ID
	EventAt    Instant
}
type TagSave struct {
	TagID   ID
	Title   string
	Color   Color
	EventAt Instant
}
type TagRemove struct {
	TagID   ID
	EventAt Instant
}
type RecurrenceReplace struct {
	SourceTaskID, NewRuleID ID
	Rule                    RuleSpec
	CutoffOn                Date
	EventAt                 Instant
}
type RecurrenceStop struct {
	RuleID       ID
	RemoveFuture bool
	CutoffOn     Date
	EventAt      Instant
}
type RecurrenceMaterialize struct {
	RuleID            ID
	FromOn, ThroughOn Date
	EventAt           Instant
}
type OrderingReplace struct {
	ScopeID               ID
	Kind                  string
	SubjectID             *ID
	MemberIDs, OrderedIDs []ID
	EventAt               Instant
}

func (TaskCreate) command()            {}
func (TaskPatchCommand) command()      {}
func (TaskChangeState) command()       {}
func (TaskMoveTree) command()          {}
func (CategorySave) command()          {}
func (CategoryRelocate) command()      {}
func (CategoryRemove) command()        {}
func (TagSave) command()               {}
func (TagRemove) command()             {}
func (RecurrenceReplace) command()     {}
func (RecurrenceStop) command()        {}
func (RecurrenceMaterialize) command() {}
func (OrderingReplace) command()       {}
