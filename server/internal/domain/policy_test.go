package domain

import (
	"encoding/json"
	"slices"
	"strings"
	"testing"
)

const testDataset ID = "20000000-0000-4000-8000-000000000001"
const testNow Instant = "2026-10-28T08:00:00.000Z"

func testID(s string) ID { return StableID("fixture:" + s) }
func taskFixture(name string) *Task {
	return &Task{ID: testID(name), Title: name, Notes: "", PlanningState: "INBOX", Status: "OPEN", CreatedAt: testNow, UpdatedAt: testNow, TagIDs: []ID{}, Links: []Link{}}
}
func putTask(g *Graph, t *Task) {
	g.Entities[Key{TaskKind, t.ID}] = Entity{TaskKind, t.ID, 1, "replace", t}
}
func event(kind string) Event { return Event{kind, testNow} }
func set[T any](v T) Field[T] { return Field[T]{Present: true, Value: v} }
func runPlan(t *testing.T, g Graph, c ...Command) (Graph, Plan) {
	t.Helper()
	p, e := PlanCommands(g, c, testDataset)
	if e != nil {
		t.Fatal(e)
	}
	next, e := g.Clone()
	if e != nil {
		t.Fatal(e)
	}
	for _, d := range p.Changes {
		next.Entities[d.After.Key()] = d.After
	}
	for _, s := range p.Structures {
		next.Structures[s.Key()] = s.Revision
	}
	next.Aliases = append(next.Aliases, p.Aliases...)
	next.Edges = append(next.Edges, p.Edges...)
	return next, p
}
func TestTitlePatchAndDateValidation(t *testing.T) {
	for _, title := range []string{"", strings.Repeat("x", 513), strings.Repeat("😀", 513)} {
		_, e := PlanCommands(EmptyGraph(), []Command{&CreateTask{Event: event("task.create"), TaskID: testID("new"), Title: title}}, testDataset)
		if Code(e) != "title_invalid" {
			t.Fatal(e)
		}
	}
	g, p := runPlan(t, EmptyGraph(), &CreateTask{Event: event("task.create"), TaskID: testID("new"), Title: " " + strings.Repeat("😀", 512) + " "})
	if len(p.Changes) != 1 {
		t.Fatal(p)
	}
	task, _ := g.Task(testID("new"))
	if len([]rune(task.Title)) != 512 {
		t.Fatal("scalar length")
	}
	before, _ := json.Marshal(task)
	_, e := PlanCommands(g, []Command{&PatchTask{Event: event("task.patch"), TaskID: task.ID, Patch: Patch{Notes: Field[string]{Present: true, Null: true}}}}, testDataset)
	if e == nil {
		t.Fatal("null string accepted")
	}
	after, _ := json.Marshal(task)
	if string(before) != string(after) {
		t.Fatal("input mutated")
	}
	_, e = PlanCommands(g, []Command{&PatchTask{Event: event("task.patch"), TaskID: task.ID, Patch: Patch{StartOn: set(Date("2030-02-30"))}}}, testDataset)
	if e == nil {
		t.Fatal("invalid date")
	}
	_, e = PlanCommands(g, []Command{&PatchTask{Event: event("task.patch"), TaskID: task.ID, Patch: Patch{StartOn: set(Date("2030-02-02")), DeadlineOn: set(Date("2030-02-01"))}}}, testDataset)
	if Code(e) != "date_order" {
		t.Fatal(e)
	}
}
func TestWholeGroupNoOpAndFailureArePure(t *testing.T) {
	id := testID("one")
	g, p := runPlan(t, EmptyGraph(), &CreateTask{Event: event("task.create"), TaskID: id, Title: "first"}, &PatchTask{Event: event("task.patch"), TaskID: id, Patch: Patch{Title: set("second")}})
	if len(p.Changes) != 1 || p.Changes[0].After.Revision != 1 {
		t.Fatal("one revision per group")
	}
	_, p = runPlan(t, g, &PatchTask{Event: event("task.patch"), TaskID: id, Patch: Patch{Title: set("second")}})
	if len(p.Changes) != 0 || len(p.Structures) != 0 {
		t.Fatal("same value changed facts")
	}
	_, e := PlanCommands(g, []Command{&PatchTask{Event: event("task.patch"), TaskID: id, Patch: Patch{Title: set("third")}}, &RemoveTag{Event: event("tag.remove"), TagID: testID("missing")}}, testDataset)
	if e == nil {
		t.Fatal("last command should fail")
	}
	old, _ := g.Task(id)
	if old.Title != "second" {
		t.Fatal("failed plan mutated input")
	}
}
func TestStateGroupingAndIndependentHistory(t *testing.T) {
	g := EmptyGraph()
	root, a, b, c := taskFixture("root"), taskFixture("a"), taskFixture("b"), taskFixture("c")
	for _, child := range []*Task{a, b, c} {
		child.ParentTaskID = Ptr(root.ID)
	}
	b.Status = "COMPLETED"
	b.CompletedAt = Ptr(testNow)
	b.CompletionGroupID = Ptr(testID("old-completion"))
	c.DeletedAt = Ptr(testNow)
	c.DeletionGroupID = Ptr(testID("old-delete"))
	for _, task := range []*Task{root, a, b, c} {
		putTask(&g, task)
	}
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID, a.ID}, Action: "complete", GroupID: testID("complete")})
	ta, _ := g.Task(a.ID)
	tb, _ := g.Task(b.ID)
	if ta.Status != "COMPLETED" || Value(tb.CompletionGroupID) != testID("old-completion") {
		t.Fatal("completion group")
	}
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "reopen", GroupID: testID("reopen")})
	ta, _ = g.Task(a.ID)
	tb, _ = g.Task(b.ID)
	if ta.Status != "OPEN" || tb.Status != "COMPLETED" {
		t.Fatal("reopen independent item")
	}
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "delete", GroupID: testID("delete")})
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "restore", GroupID: testID("restore")})
	tc, _ := g.Task(c.ID)
	if tc.DeletedAt == nil {
		t.Fatal("independent delete restored")
	}
	// A null completion group must never group unrelated legacy descendants.
	r, _ := g.Task(root.ID)
	r.Status = "COMPLETED"
	r.CompletedAt = Ptr(testNow)
	r.CompletionGroupID = nil
	ta, _ = g.Task(a.ID)
	ta.Status = "COMPLETED"
	ta.CompletedAt = Ptr(testNow)
	ta.CompletionGroupID = nil
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "reopen", GroupID: testID("legacy-reopen")})
	ta, _ = g.Task(a.ID)
	if ta.Status != "COMPLETED" {
		t.Fatal("null group broadened")
	}
}
func TestMoveDepthCycleAndCategoryRemoval(t *testing.T) {
	g := EmptyGraph()
	root, child := taskFixture("root"), taskFixture("child")
	child.ParentTaskID = Ptr(root.ID)
	putTask(&g, root)
	putTask(&g, child)
	_, e := PlanCommands(g, []Command{&CreateTask{Event: event("task.create"), TaskID: testID("deep"), Title: "deep", Initial: Patch{ParentTaskID: set(child.ID)}}}, testDataset)
	if Code(e) != "category_depth" {
		t.Fatal(e)
	}
	_, e = PlanCommands(g, []Command{&MoveTree{Event: event("task.move_tree"), TaskID: root.ID, ParentTaskID: Ptr(child.ID)}}, testDataset)
	if Code(e) != "category_cycle" {
		t.Fatal(e)
	}
	catID := testID("cat")
	g, _ = runPlan(t, g, &SaveCategory{Event: event("category.save"), CategoryID: catID, Title: "category", Color: "neutral"}, &MoveTree{Event: event("task.move_tree"), TaskID: root.ID, CategoryID: Ptr(catID)})
	g, _ = runPlan(t, g, &RemoveCategory{Event: event("category.remove"), CategoryID: catID})
	for _, id := range []ID{root.ID, child.ID} {
		v, _ := g.Task(id)
		if v.CategoryID != nil || v.DeletedAt != nil {
			t.Fatal("category removal lost task")
		}
	}
}
func TestSubsetOrderingAndMembership(t *testing.T) {
	g := EmptyGraph()
	a, h, b := taskFixture("a"), taskFixture("hidden"), taskFixture("b")
	for _, v := range []*Task{a, h, b} {
		putTask(&g, v)
	}
	scope := testID("scope")
	g.Entities[Key{OrderingKind, scope}] = Entity{OrderingKind, scope, 1, "replace", &Ordering{ID: scope, Kind: "today", OrderedIDs: []ID{a.ID, h.ID, b.ID}}}
	g.Structures[StructureKey{"ordering_scope", scope}] = 1
	g, p := runPlan(t, g, &ReplaceOrdering{Event: event("ordering.replace"), ScopeID: scope, Kind: "today", MemberIDs: []ID{a.ID, b.ID}, OrderedIDs: []ID{b.ID, a.ID}})
	o := g.Entities[Key{OrderingKind, scope}].Value.(*Ordering)
	if !slices.Equal(o.OrderedIDs, []ID{b.ID, h.ID, a.ID}) || len(p.Changes) != 1 || p.Changes[0].After.Type != OrderingKind {
		t.Fatal("subset slots")
	}
	if g.Entities[Key{TaskKind, a.ID}].Revision != 1 {
		t.Fatal("ordering changed task revision")
	}
}
func TestReadSetIncludesEmptyCollectionsAndConcurrentMembers(t *testing.T) {
	id := testID("one")
	g := EmptyGraph()
	c := &CreateTask{Event: event("task.create"), TaskID: id, Title: "one"}
	_, p := runPlan(t, g, c)
	if e := CheckPreconditions(g, Preconditions{Entities: []Version{{TaskKind, id, 0}}}, p.Reads); Code(e) != "read_set_incomplete" {
		t.Fatal(e)
	}
	if e := CheckPreconditions(g, p.Reads, p.Reads); e != nil {
		t.Fatal(e)
	}
	g.Structures[StructKey("category_members", nil)] = 1
	if e := CheckPreconditions(g, p.Reads, p.Reads); Code(e) != "structure_conflict" {
		t.Fatal(e)
	}
}
func testRule(id ID, mode, freq string) Rule {
	interval := 1
	return Rule{ID: id, Spec: RuleSpec{Mode: mode, Frequency: freq, Interval: interval, AnchorOn: "2030-01-01", MonthDay: 1, YearMonth: 1, TimeZone: "Asia/Shanghai", Defaults: TaskTemplate{Version: 1, Title: "repeat", PlanningState: "ACTIVE", TagIDs: []ID{}, Links: []Link{}, StartDayOffset: Ptr(0), DeadlineDayOffset: Ptr(0)}}, Active: true, CreatedAt: testNow, UpdatedAt: testNow}
}
func TestFixedCalendarIdentityAndDSTReject(t *testing.T) {
	r := testRule(testID("rule"), "FIXED", "MONTHLY")
	r.Spec.MonthDay = 31
	g := EmptyGraph()
	g.Entities[Key{RuleKind, r.ID}] = Entity{RuleKind, r.ID, 1, "replace", &r}
	g, p := runPlan(t, g, &MaterializeRecurrence{Event: event("recurrence.materialize"), RuleID: r.ID, FromOn: "2030-01-01", ThroughOn: "2030-03-31"})
	if len(p.Changes) != 2 {
		t.Fatalf("31st should skip February: %d", len(p.Changes))
	}
	for _, d := range p.Changes {
		v := d.After.Value.(*Task)
		if v.ID != StableID(string(r.ID)+":"+Value(v.OccurrenceKey)) {
			t.Fatal("identity")
		}
	}
	_, p = runPlan(t, g, &MaterializeRecurrence{Event: event("recurrence.materialize"), RuleID: r.ID, FromOn: "2030-01-01", ThroughOn: "2030-03-31"})
	if len(p.Changes) != 0 {
		t.Fatal("materialization not idempotent")
	}
	for _, v := range []struct {
		day  Date
		wall string
	}{{"2026-03-08", "02:30:00.000"}, {"2026-11-01", "01:30:00.000"}} {
		if _, e := LocalInstant(v.day, v.wall, "America/New_York"); e == nil {
			t.Fatal("DST gap/fold accepted")
		}
	}
	if v, e := LocalInstant("2026-03-08", "03:30:00.000", "America/New_York"); e != nil || v != "2026-03-08T07:30:00.000Z" {
		t.Fatal(v, e)
	}
}
func TestAfterCompletionOneSuccessorAndReopenGuard(t *testing.T) {
	g := EmptyGraph()
	source := taskFixture("source")
	putTask(&g, source)
	r := testRule(testID("after"), "AFTER_COMPLETION", "AFTER_DAYS")
	g, _ = runPlan(t, g, &ReplaceRecurrence{Event: event("recurrence.replace"), SourceTaskID: source.ID, NewRuleID: r.ID, Rule: r.Spec, CutoffOn: r.Spec.AnchorOn})
	g, p := runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{source.ID}, Action: "complete", GroupID: testID("after-complete")})
	if len(g.Edges) != 1 || len(p.Changes) != 2 {
		t.Fatal("successor transaction")
	}
	_, p = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{source.ID}, Action: "complete", GroupID: testID("again")})
	if len(p.Changes) != 0 || len(p.Edges) != 0 {
		t.Fatal("duplicate successor")
	}
	_, e := PlanCommands(g, []Command{&ChangeState{Event: event("task.change_state"), TaskIDs: []ID{source.ID}, Action: "reopen", GroupID: testID("reopen-after")}}, testDataset)
	if Code(e) != "recurrence_open_instance" {
		t.Fatal(e)
	}
}
func TestTimingEndpointAndExpiredReminder(t *testing.T) {
	id := testID("timed")
	_, e := PlanCommands(EmptyGraph(), []Command{&CreateTask{Event: event("task.create"), TaskID: id, Title: "timed", Initial: Patch{Schedule: set(Schedule{Kind: "ALL_DAY", StartOn: Ptr(Date("2030-01-01")), EndOn: Ptr(Date("2030-01-02"))})}}}, testDataset)
	if e == nil {
		t.Fatal("inconsistent task endpoints accepted")
	}
	_, _ = runPlan(t, EmptyGraph(), &CreateTask{Event: event("task.create"), TaskID: id, Title: "expired offline reminder", Initial: Patch{Reminder: set(Reminder{Kind: "ABSOLUTE", AnchorUTC: Ptr(Instant("2000-01-01T00:00:00.000Z")), Repeat: "NONE", Every: 1, TimeZone: "Asia/Shanghai"})}})
}
