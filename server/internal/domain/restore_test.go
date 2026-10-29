package domain

import "testing"

func TestExplicitCatalogSaveRestoresIdentityWithoutRestoringMembership(t *testing.T) {
	category, tag, task := testID("restore-category"), testID("restore-tag"), testID("restore-task")
	g, _ := runPlan(t, EmptyGraph(),
		&SaveCategory{Event: event("category.save"), CategoryID: category, Title: "category", Color: "blue"},
		&SaveTag{Event: event("tag.save"), TagID: tag, Title: "tag", Color: "blue"},
		&CreateTask{Event: event("task.create"), TaskID: task, Title: "member", Initial: Patch{CategoryID: set(category), TagIDs: set([]ID{tag})}})
	g, _ = runPlan(t, g, &RemoveCategory{Event: event("category.remove"), CategoryID: category}, &RemoveTag{Event: event("tag.remove"), TagID: tag})
	g, _ = runPlan(t, g, &SaveCategory{Event: event("category.save"), CategoryID: category, Title: "restored", Color: "blue"}, &SaveTag{Event: event("tag.save"), TagID: tag, Title: "restored", Color: "blue"})
	c := g.Entities[Key{CategoryKind, category}].Value.(*Category)
	tagValue := g.Entities[Key{TagKind, tag}].Value.(*Tag)
	member, _ := g.Task(task)
	if c.DeletedAt != nil || tagValue.DeletedAt != nil || c.CreatedAt != testNow || tagValue.CreatedAt != testNow || member.CategoryID != nil || len(member.TagIDs) != 0 {
		t.Fatal("restoring an identity must preserve creation and leave removed memberships detached")
	}
}

func TestConflictCanRestoreOnlyTheSameImmutableRecurrenceGeneration(t *testing.T) {
	source := taskFixture("restore-source")
	g := EmptyGraph()
	putTask(&g, source)
	r := testRule(testID("restore-rule"), "FIXED", "DAILY")
	command := &ReplaceRecurrence{Event: event("recurrence.replace"), SourceTaskID: source.ID, NewRuleID: r.ID, Rule: r.Spec, CutoffOn: r.Spec.AnchorOn}
	g, _ = runPlan(t, g, command)
	g, _ = runPlan(t, g, &StopRecurrence{Event: event("recurrence.stop"), RuleID: r.ID, CutoffOn: r.Spec.AnchorOn})
	g, _ = runPlan(t, g, command)
	if !g.Entities[Key{RuleKind, r.ID}].Value.(*Rule).Active {
		t.Fatal("same generation was not restored")
	}
	_, p := runPlan(t, g, command)
	if len(p.Changes) != 0 {
		t.Fatal("repeat restore must be idempotent")
	}
	changed := *command
	changed.Rule.Interval++
	_, err := PlanCommands(g, []Command{&changed}, testDataset)
	if Code(err) != "recurrence_invalid" {
		t.Fatal(err)
	}
}
