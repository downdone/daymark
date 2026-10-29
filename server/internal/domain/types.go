package domain

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"
)

type ID string

const NilID ID = "00000000-0000-0000-0000-000000000000"

func NewID() (ID, error) {
	var b [16]byte
	if _, e := rand.Read(b[:]); e != nil {
		return "", Wrap("token_source_failed", e)
	}
	b[6] = b[6]&15 | 64
	b[8] = b[8]&63 | 128
	h := hex.EncodeToString(b[:])
	return ID(h[:8] + "-" + h[8:12] + "-" + h[12:16] + "-" + h[16:20] + "-" + h[20:]), nil
}
func ValidID(s string, allowNil bool) bool {
	if len(s) != 36 || (!allowNil && ID(s) == NilID) {
		return false
	}
	for i, c := range s {
		if i == 8 || i == 13 || i == 18 || i == 23 {
			if c != '-' {
				return false
			}
		} else if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'f') {
			return false
		}
	}
	return true
}

// Counter uses an exact decimal string at the JSON boundary.
type Counter int64

func (c Counter) MarshalJSON() ([]byte, error) {
	if c < 0 {
		return nil, Invalid("/counter")
	}
	return json.Marshal(strconv.FormatInt(int64(c), 10))
}
func (c *Counter) UnmarshalJSON(b []byte) error {
	var s string
	if json.Unmarshal(b, &s) != nil {
		return Invalid("/counter")
	}
	n, e := strconv.ParseInt(s, 10, 64)
	if e != nil || n < 0 || strconv.FormatInt(n, 10) != s {
		return Invalid("/counter")
	}
	*c = Counter(n)
	return nil
}
func (c Counter) String() string { return strconv.FormatInt(int64(c), 10) }

type Date string

func (d Date) Time() (time.Time, error) {
	t, e := time.Parse("2006-01-02", string(d))
	if e != nil || t.Year() < 1 || t.Format("2006-01-02") != string(d) {
		return time.Time{}, Invalid("/date")
	}
	return t, nil
}
func (d Date) AddDays(n int) (Date, error) {
	t, e := d.Time()
	if e != nil {
		return "", e
	}
	t = t.AddDate(0, 0, n)
	if t.Year() < 1 || t.Year() > 9999 {
		return "", Invalid("/date")
	}
	return Date(t.Format("2006-01-02")), nil
}

type Instant string

func At(t time.Time) Instant { return Instant(t.UTC().Format("2006-01-02T15:04:05.000Z")) }
func (v Instant) Time() (time.Time, error) {
	t, e := time.Parse("2006-01-02T15:04:05.000Z", string(v))
	if e != nil || t.Year() < 1 || At(t) != v {
		return time.Time{}, Invalid("/instant")
	}
	return t, nil
}
func (v Instant) Millis() (int64, error) { t, e := v.Time(); return t.UnixMilli(), e }
func FromMillis(n int64) Instant         { return At(time.UnixMilli(n)) }
func Ptr[T any](v T) *T                  { return &v }
func Value[T any](v *T) T {
	if v == nil {
		var z T
		return z
	}
	return *v
}

type Kind string

const (
	TaskKind     Kind = "task"
	CategoryKind Kind = "category"
	TagKind      Kind = "tag"
	RuleKind     Kind = "recurrence_rule"
	OrderingKind Kind = "ordering_scope"
)

type Key struct {
	Type Kind `json:"type"`
	ID   ID   `json:"id"`
}

func (k Key) String() string { return string(k.Type) + ":" + string(k.ID) }

type Version struct {
	Type     Kind    `json:"type"`
	ID       ID      `json:"id"`
	Revision Counter `json:"revision"`
}

func (v Version) Key() Key { return Key{v.Type, v.ID} }

type StructureKey struct {
	Kind string `json:"kind"`
	ID   ID     `json:"id"`
}
type StructureVersion struct {
	Kind     string  `json:"kind"`
	ID       ID      `json:"id"`
	Revision Counter `json:"revision"`
}

func (v StructureVersion) Key() StructureKey { return StructureKey{v.Kind, v.ID} }

type Preconditions struct {
	Entities   []Version          `json:"entities"`
	Structures []StructureVersion `json:"structures"`
}
type Principal struct {
	UserID, SessionID, InstallationID, AuthEpoch ID
	CredentialVersion                            int64
	ExpiresAt                                    int64
}
type Scope struct {
	Principal            Principal
	DatasetID, DataEpoch ID
}
type Dataset struct {
	ID            ID      `json:"id"`
	DataEpoch     ID      `json:"data_epoch"`
	Head          Counter `json:"head_seq"`
	OwnerID       ID      `json:"-"`
	RetainedAfter Counter `json:"-"`
}

type Schedule struct {
	Kind     string   `json:"kind"`
	StartOn  *Date    `json:"start_on,omitempty"`
	EndOn    *Date    `json:"end_on,omitempty"`
	StartUTC *Instant `json:"start_at_utc,omitempty"`
	EndUTC   *Instant `json:"end_at_utc,omitempty"`
	TimeZone *string  `json:"time_zone,omitempty"`
}
type Reminder struct {
	Kind          string   `json:"kind"`
	AnchorUTC     *Instant `json:"anchor_at_utc,omitempty"`
	MinutesBefore *int     `json:"minutes_before,omitempty"`
	Repeat        string   `json:"repeat"`
	Every         int      `json:"every"`
	TimeZone      string   `json:"time_zone"`
}
type Link struct {
	TargetID ID     `json:"target_id"`
	Role     string `json:"role"`
}
type Legacy struct {
	FormatVersion       int    `json:"format_version"`
	SourceExportVersion int    `json:"source_export_version"`
	CanonicalJSON       string `json:"canonical_json"`
}
type Task struct {
	ID                 ID        `json:"id"`
	Title              string    `json:"title"`
	Notes              string    `json:"notes"`
	PlanningState      string    `json:"planning_state"`
	Status             string    `json:"status"`
	TodayFocus         bool      `json:"today_focus"`
	Evening            bool      `json:"evening"`
	Pinned             bool      `json:"pinned"`
	Important          bool      `json:"important"`
	CategoryID         *ID       `json:"category_id"`
	ParentTaskID       *ID       `json:"parent_task_id"`
	StartOn            *Date     `json:"start_on"`
	DeadlineOn         *Date     `json:"deadline_on"`
	CreatedAt          Instant   `json:"created_at_utc"`
	UpdatedAt          Instant   `json:"updated_at_utc"`
	CompletedAt        *Instant  `json:"completed_at_utc"`
	CancelledAt        *Instant  `json:"cancelled_at_utc"`
	DeletedAt          *Instant  `json:"deleted_at_utc"`
	DeletionGroupID    *ID       `json:"deletion_group_id"`
	CompletionGroupID  *ID       `json:"completion_group_id"`
	Schedule           *Schedule `json:"schedule"`
	Reminder           *Reminder `json:"reminder"`
	RecurrenceID       *ID       `json:"recurrence_id"`
	OccurrenceOn       *Date     `json:"occurrence_on"`
	OccurrenceKey      *string   `json:"occurrence_key"`
	InstanceOverridden bool      `json:"instance_overridden"`
	Legacy             *Legacy   `json:"legacy_metadata"`
	TagIDs             []ID      `json:"tag_ids"`
	Links              []Link    `json:"links"`
}
type Category struct {
	ID          ID       `json:"id"`
	ParentID    *ID      `json:"parent_id"`
	Title       string   `json:"title"`
	Description string   `json:"description"`
	Color       string   `json:"color_key"`
	Archived    bool     `json:"archived"`
	CreatedAt   Instant  `json:"created_at_utc"`
	UpdatedAt   Instant  `json:"updated_at_utc"`
	DeletedAt   *Instant `json:"deleted_at_utc"`
	Legacy      *Legacy  `json:"legacy_metadata"`
}
type Tag struct {
	ID        ID       `json:"id"`
	Title     string   `json:"title"`
	Color     string   `json:"color_key"`
	CreatedAt Instant  `json:"created_at_utc"`
	UpdatedAt Instant  `json:"updated_at_utc"`
	DeletedAt *Instant `json:"deleted_at_utc"`
}
type ScheduleTemplate struct {
	Kind           string  `json:"kind"`
	StartDayOffset int     `json:"start_day_offset"`
	EndDayOffset   int     `json:"end_day_offset"`
	StartLocalTime *string `json:"start_local_time,omitempty"`
	EndLocalTime   *string `json:"end_local_time,omitempty"`
	TimeZone       *string `json:"time_zone,omitempty"`
}
type ReminderTemplate struct {
	Kind          string  `json:"kind"`
	MinutesBefore *int    `json:"minutes_before,omitempty"`
	DayOffset     *int    `json:"day_offset,omitempty"`
	LocalTime     *string `json:"local_time,omitempty"`
	Repeat        string  `json:"repeat"`
	Every         int     `json:"every"`
	TimeZone      string  `json:"time_zone"`
}
type TaskTemplate struct {
	Version           int               `json:"template_version"`
	Title             string            `json:"title"`
	Notes             string            `json:"notes"`
	PlanningState     string            `json:"planning_state"`
	CategoryID        *ID               `json:"category_id"`
	ParentTaskID      *ID               `json:"parent_task_id"`
	TodayFocus        bool              `json:"today_focus"`
	Evening           bool              `json:"evening"`
	Pinned            bool              `json:"pinned"`
	Important         bool              `json:"important"`
	TagIDs            []ID              `json:"tag_ids"`
	Links             []Link            `json:"links"`
	StartDayOffset    *int              `json:"start_day_offset"`
	DeadlineDayOffset *int              `json:"deadline_day_offset"`
	Schedule          *ScheduleTemplate `json:"schedule"`
	Reminder          *ReminderTemplate `json:"reminder"`
}
type RuleSpec struct {
	Mode         string       `json:"mode"`
	Frequency    string       `json:"frequency"`
	Interval     int          `json:"interval"`
	AnchorOn     Date         `json:"anchor_on"`
	EndOn        *Date        `json:"end_on"`
	WeekdaysMask int          `json:"weekdays_mask"`
	MonthDay     int          `json:"month_day"`
	YearMonth    int          `json:"year_month"`
	TimeZone     string       `json:"time_zone"`
	Defaults     TaskTemplate `json:"defaults"`
}
type Rule struct {
	ID           ID       `json:"id"`
	SourceTaskID *ID      `json:"source_task_id"`
	Spec         RuleSpec `json:"rule"`
	Active       bool     `json:"active"`
	CreatedAt    Instant  `json:"created_at_utc"`
	UpdatedAt    Instant  `json:"updated_at_utc"`
	DeletedAt    *Instant `json:"deleted_at_utc"`
}
type Ordering struct {
	ID         ID     `json:"id"`
	Kind       string `json:"kind"`
	SubjectID  *ID    `json:"subject_id"`
	OrderedIDs []ID   `json:"ordered_ids"`
}
type Alias struct {
	AliasID     ID `json:"alias_id"`
	CanonicalID ID `json:"canonical_id"`
}
type Edge struct {
	RuleID        ID `json:"rule_id"`
	PredecessorID ID `json:"predecessor_id"`
	SuccessorID   ID `json:"successor_id"`
}

// Entity's discriminant and value are checked together on decode. Domain code
// handles concrete values, never a permissive generic upsert map.
type Entity struct {
	Type      Kind    `json:"type"`
	ID        ID      `json:"id"`
	Revision  Counter `json:"revision"`
	Operation string  `json:"operation"`
	Value     any     `json:"value"`
}

func (e Entity) Key() Key { return Key{e.Type, e.ID} }
func (e *Entity) UnmarshalJSON(b []byte) error {
	var v struct {
		Type      Kind            `json:"type"`
		ID        ID              `json:"id"`
		Revision  Counter         `json:"revision"`
		Operation string          `json:"operation"`
		Value     json.RawMessage `json:"value"`
	}
	if err := json.Unmarshal(b, &v); err != nil {
		return err
	}
	var target any
	switch v.Type {
	case TaskKind:
		target = &Task{}
	case CategoryKind:
		target = &Category{}
	case TagKind:
		target = &Tag{}
	case RuleKind:
		target = &Rule{}
	case OrderingKind:
		target = &Ordering{}
	default:
		return Fail("protocol_upgrade_required")
	}
	if err := json.Unmarshal(v.Value, target); err != nil {
		return err
	}
	*e = Entity{v.Type, v.ID, v.Revision, v.Operation, target}
	return nil
}

type Graph struct {
	Entities   map[Key]Entity
	Structures map[StructureKey]Counter
	Aliases    []Alias
	Edges      []Edge
}

func EmptyGraph() Graph {
	return Graph{map[Key]Entity{}, map[StructureKey]Counter{}, []Alias{}, []Edge{}}
}
func (g Graph) Task(id ID) (*Task, bool) {
	e, ok := g.Entities[Key{TaskKind, id}]
	if !ok {
		return nil, false
	}
	v, ok := e.Value.(*Task)
	return v, ok
}
func (g Graph) Clone() (Graph, error) {
	out := EmptyGraph()
	for k, e := range g.Entities {
		b, err := json.Marshal(e)
		if err != nil {
			return out, err
		}
		var c Entity
		if err = json.Unmarshal(b, &c); err != nil {
			return out, err
		}
		out.Entities[k] = c
	}
	for k, v := range g.Structures {
		out.Structures[k] = v
	}
	out.Aliases = append(out.Aliases, g.Aliases...)
	out.Edges = append(out.Edges, g.Edges...)
	return out, nil
}
func Equal(a, b any) bool {
	x, e := json.Marshal(a)
	if e != nil {
		return false
	}
	y, e := json.Marshal(b)
	return e == nil && string(x) == string(y)
}

type Delta struct {
	Before *Entity
	After  Entity
}
type Plan struct {
	Changes    []Delta
	Structures []StructureVersion
	Aliases    []Alias
	Edges      []Edge
	Reads      Preconditions
}
type Batch struct {
	Seq            Counter            `json:"seq"`
	MutationID     ID                 `json:"origin_mutation_id"`
	InstallationID ID                 `json:"origin_installation_id"`
	CommittedAt    Instant            `json:"committed_at_utc"`
	Changes        []Entity           `json:"changes"`
	Structures     []StructureVersion `json:"structure_revisions"`
	Aliases        []Alias            `json:"aliases"`
	Edges          []Edge             `json:"successor_edges"`
}
type Receipt struct {
	ProtocolVersion int                `json:"protocol_version"`
	MutationID      ID                 `json:"mutation_id"`
	DataEpoch       ID                 `json:"data_epoch"`
	Result          string             `json:"result"`
	BatchSeq        *Counter           `json:"batch_seq"`
	ObservedHead    Counter            `json:"observed_head_seq"`
	Entities        []Version          `json:"entity_revisions"`
	Structures      []StructureVersion `json:"structure_revisions"`
	CommittedAt     Instant            `json:"committed_at_utc"`
}

func EmptyReceipt(mid, epoch ID, head Counter, at Instant) Receipt {
	return Receipt{1, mid, epoch, "no_op", nil, head, []Version{}, []StructureVersion{}, at}
}
func StructKey(kind string, id *ID) StructureKey {
	v := Value(id)
	if v == "" {
		v = NilID
	}
	return StructureKey{kind, v}
}
func CheckTitle(title string) error {
	if !utf8.ValidString(title) {
		return Fail("title_invalid")
	}
	n := len([]rune(strings.TrimSpace(title)))
	if n < 1 || n > 512 {
		return Fail("title_invalid")
	}
	return nil
}
func ContextField(i int, f string) string { return fmt.Sprintf("/commands/%d/%s", i, f) }
