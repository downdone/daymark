package domain

import "testing"

func TestPurgeIsTerminalAndAtomic(t *testing.T) {
	g := EmptyGraph()
	root := taskFixture("root")
	child := taskFixture("child")
	child.ParentTaskID = Ptr(root.ID)
	putTask(&g, root)
	putTask(&g, child)
	if _, e := PlanCommands(g, []Command{&PurgeTasks{Event: event("task.purge"), TaskIDs: []ID{root.ID}}}, testDataset); e == nil {
		t.Fatal("live task purged")
	}
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "delete", GroupID: testID("group")})
	next, plan := runPlan(t, g, &PurgeTasks{Event: event("task.purge"), TaskIDs: []ID{root.ID, child.ID}})
	if len(plan.Changes) != 2 {
		t.Fatal("deduplicated subtree")
	}
	for _, id := range []ID{root.ID, child.ID} {
		task, _ := next.Task(id)
		if task.PurgedAt == nil || task.Title != "[deleted]" || task.Notes != "" {
			t.Fatal("body retained")
		}
		if _, e := PlanCommands(next, []Command{&ChangeState{Event: event("task.change_state"), TaskIDs: []ID{id}, Action: "restore", GroupID: testID("restore")}}, testDataset); e == nil {
			t.Fatal("tombstone restored")
		}
	}
	_, again := runPlan(t, next, &PurgeTasks{Event: event("task.purge"), TaskIDs: []ID{root.ID}})
	if len(again.Changes) != 0 {
		t.Fatal("not idempotent")
	}
	original, _ := g.Task(root.ID)
	if original.PurgedAt != nil {
		t.Fatal("input mutated")
	}
}

func TestRestoreParentSkipsPreviouslyPurgedChild(t *testing.T) {
	g := EmptyGraph()
	root := taskFixture("restore root")
	child := taskFixture("purged child")
	child.ParentTaskID = Ptr(root.ID)
	putTask(&g, root)
	putTask(&g, child)
	g, _ = runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "delete", GroupID: testID("deletion")})
	g, _ = runPlan(t, g, &PurgeTasks{Event: event("task.purge"), TaskIDs: []ID{child.ID}})
	restored, _ := runPlan(t, g, &ChangeState{Event: event("task.change_state"), TaskIDs: []ID{root.ID}, Action: "restore", GroupID: testID("restoration")})
	r, _ := restored.Task(root.ID)
	c, _ := restored.Task(child.ID)
	if r.DeletedAt != nil || c.PurgedAt == nil || c.DeletedAt == nil {
		t.Fatal("restore crossed terminal boundary")
	}
}
