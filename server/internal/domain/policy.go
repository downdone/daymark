package domain

import (
	"cmp"
	"encoding/json"
	"slices"
	"strings"
	"unicode/utf8"
)

type planner struct {
	before, graph Graph
	reads         map[Key]Counter
	structures    map[StructureKey]Counter
	dirty         map[StructureKey]bool
	event         Instant
	dataset       ID
}

// PlanCommands is deterministic: no SQL, network, randomness, clock or input mutation.
// A full bounded graph is loaded by the adapter; reads retain only its required
// entity/collection closure, including empty collections.
func PlanCommands(before Graph, commands []Command, dataset ID) (Plan, error) {
	copy, e := before.Clone()
	if e != nil {
		return Plan{}, e
	}
	p := &planner{before: before, graph: copy, reads: map[Key]Counter{}, structures: map[StructureKey]Counter{}, dirty: map[StructureKey]bool{}, dataset: dataset}
	for _, cmd := range commands {
		p.event = cmd.EventTime()
		if _, e = p.event.Time(); e != nil {
			return Plan{}, e
		}
		if e = p.apply(cmd); e != nil {
			return Plan{}, e
		}
	}
	if e = p.reconcileOrderings(); e != nil {
		return Plan{}, e
	}
	return p.finish()
}
func (p *planner) record(k Key) {
	if _, ok := p.reads[k]; !ok {
		p.reads[k] = p.before.Entities[k].Revision
	}
}
func (p *planner) entity(k Key) (Entity, error) {
	p.record(k)
	v, ok := p.graph.Entities[k]
	if !ok {
		return Entity{}, Fail("not_found")
	}
	return v, nil
}
func (p *planner) task(id ID) (*Task, error) {
	e, err := p.entity(Key{TaskKind, id})
	if err != nil {
		return nil, err
	}
	v, ok := e.Value.(*Task)
	if !ok {
		return nil, Fail("internal_error")
	}
	return v, nil
}
func (p *planner) category(id ID) (*Category, error) {
	e, err := p.entity(Key{CategoryKind, id})
	if err != nil {
		return nil, err
	}
	v, ok := e.Value.(*Category)
	if !ok {
		return nil, Fail("internal_error")
	}
	return v, nil
}
func (p *planner) rule(id ID) (*Rule, error) {
	e, err := p.entity(Key{RuleKind, id})
	if err != nil {
		return nil, err
	}
	v, ok := e.Value.(*Rule)
	if !ok {
		return nil, Fail("internal_error")
	}
	return v, nil
}
func (p *planner) structure(k StructureKey, dirty bool) {
	if _, ok := p.structures[k]; !ok {
		p.structures[k] = p.before.Structures[k]
	}
	if dirty {
		p.dirty[k] = true
	}
}
func (p *planner) newEntity(e Entity) error {
	p.record(e.Key())
	if old, ok := p.graph.Entities[e.Key()]; ok {
		return ConflictError(e.Key(), 0, old.Revision)
	}
	for _, a := range p.graph.Aliases {
		if a.AliasID == e.ID {
			return Fail("revision_conflict")
		}
	}
	e.Revision = 1
	e.Operation = "replace"
	p.graph.Entities[e.Key()] = e
	return nil
}
func (p *planner) root(id ID) (ID, error) {
	seen := map[ID]bool{}
	for {
		if seen[id] {
			return "", Fail("category_cycle")
		}
		seen[id] = true
		t, e := p.task(id)
		if e != nil {
			return "", e
		}
		if t.ParentTaskID == nil {
			return id, nil
		}
		id = *t.ParentTaskID
	}
}
func (p *planner) tree(id ID) ([]*Task, error) {
	root, e := p.root(id)
	if e != nil {
		return nil, e
	}
	p.structure(StructureKey{"task_tree", root}, false)
	out := []*Task{}
	seen := map[ID]bool{}
	var walk func(ID) error
	walk = func(id ID) error {
		if seen[id] {
			return Fail("category_cycle")
		}
		seen[id] = true
		t, e := p.task(id)
		if e != nil {
			return e
		}
		out = append(out, t)
		children := []ID{}
		for _, en := range p.graph.Entities {
			if child, ok := en.Value.(*Task); ok && child.ParentTaskID != nil && *child.ParentTaskID == id {
				children = append(children, child.ID)
			}
		}
		slices.Sort(children)
		for _, child := range children {
			if e = walk(child); e != nil {
				return e
			}
		}
		return nil
	}
	err := walk(id)
	return out, err
}
func (p *planner) changedTask(t *Task, old Task) {
	if Equal(t, &old) {
		return
	}
	t.UpdatedAt = p.event
	if t.RecurrenceID != nil {
		t.InstanceOverridden = true
		p.structure(StructureKey{"recurrence_members", *t.RecurrenceID}, true)
	}
}
func (p *planner) apply(cmd Command) error {
	switch c := cmd.(type) {
	case *CreateTask:
		if !ValidID(string(c.TaskID), false) || c.Initial.Title.Present {
			return Invalid("/task_id")
		}
		if e := CheckTitle(c.Title); e != nil {
			return e
		}
		t := &Task{ID: c.TaskID, Title: strings.TrimSpace(c.Title), Notes: "", PlanningState: "INBOX", Status: "OPEN", CreatedAt: p.event, UpdatedAt: p.event, TagIDs: []ID{}, Links: []Link{}}
		if e := p.patchValues(t, c.Initial); e != nil {
			return e
		}
		if t.ParentTaskID != nil {
			parent, e := p.task(*t.ParentTaskID)
			if e != nil {
				return e
			}
			t.CategoryID = parent.CategoryID
			root, e := p.root(parent.ID)
			if e != nil {
				return e
			}
			p.structure(StructureKey{"task_tree", root}, true)
		}
		p.structure(StructKey("category_members", t.CategoryID), true)
		p.structure(StructureKey{"task_tree", t.ID}, true)
		for _, id := range t.TagIDs {
			p.structure(StructureKey{"tag_members", id}, true)
		}
		return p.newEntity(Entity{Type: TaskKind, ID: t.ID, Value: t})
	case *PatchTask:
		t, e := p.task(c.TaskID)
		if e != nil {
			return e
		}
		old := cloneTask(t)
		if t.DeletedAt != nil {
			return Invalid("/task_id")
		}
		newParent, newCategory := t.ParentTaskID, t.CategoryID
		if c.Patch.ParentTaskID.Present {
			newParent = fieldPointer(c.Patch.ParentTaskID)
		}
		if c.Patch.CategoryID.Present {
			newCategory = fieldPointer(c.Patch.CategoryID)
		}
		if !Equal(newParent, t.ParentTaskID) || !Equal(newCategory, t.CategoryID) {
			if e = p.move(t.ID, newCategory, newParent); e != nil {
				return e
			}
		}
		patch := c.Patch
		patch.CategoryID = Field[ID]{}
		patch.ParentTaskID = Field[ID]{}
		if e = p.patchValues(t, patch); e != nil {
			return e
		}
		p.changedTask(t, old)
		for _, id := range old.TagIDs {
			if !slices.Contains(t.TagIDs, id) {
				p.structure(StructureKey{"tag_members", id}, true)
			}
		}
		for _, id := range t.TagIDs {
			if !slices.Contains(old.TagIDs, id) {
				p.structure(StructureKey{"tag_members", id}, true)
			}
		}
		return nil
	case *MoveTree:
		return p.move(c.TaskID, c.CategoryID, c.ParentTaskID)
	case *ChangeState:
		return p.changeState(c)
	case *SaveCategory:
		return p.saveCategory(c)
	case *RelocateCategory:
		return p.relocate(c)
	case *RemoveCategory:
		return p.removeCategory(c)
	case *SaveTag:
		if e := CheckTitle(c.Title); e != nil {
			return e
		}
		key := Key{TagKind, c.TagID}
		p.record(key)
		old, ok := p.graph.Entities[key]
		if !ok {
			return p.newEntity(Entity{Type: TagKind, ID: c.TagID, Value: &Tag{ID: c.TagID, Title: strings.TrimSpace(c.Title), Color: c.Color, CreatedAt: p.event, UpdatedAt: p.event}})
		}
		v := old.Value.(*Tag)
		restoring := v.DeletedAt != nil
		if restoring || v.Title != strings.TrimSpace(c.Title) || v.Color != c.Color {
			if restoring {
				p.structure(StructureKey{"tag_members", v.ID}, true)
			}
			v.Title = strings.TrimSpace(c.Title)
			v.Color = c.Color
			v.UpdatedAt = p.event
			v.DeletedAt = nil
		}
		return nil
	case *RemoveTag:
		e, err := p.entity(Key{TagKind, c.TagID})
		if err != nil {
			return err
		}
		tag := e.Value.(*Tag)
		p.structure(StructureKey{"tag_members", c.TagID}, false)
		if tag.DeletedAt != nil {
			return nil
		}
		tag.DeletedAt = Ptr(p.event)
		tag.UpdatedAt = p.event
		p.structure(StructureKey{"tag_members", c.TagID}, true)
		for _, en := range p.graph.Entities {
			if t, ok := en.Value.(*Task); ok && slices.Contains(t.TagIDs, c.TagID) {
				p.record(en.Key())
				old := cloneTask(t)
				t.TagIDs = slices.DeleteFunc(t.TagIDs, func(id ID) bool { return id == c.TagID })
				p.changedTask(t, old)
			}
		}
		return nil
	case *ReplaceRecurrence:
		return p.replaceRule(c)
	case *StopRecurrence:
		return p.stopRule(c.RuleID, c.RemoveFuture, c.CutoffOn, "")
	case *MaterializeRecurrence:
		return p.materialize(c)
	case *ReplaceOrdering:
		return p.order(c)
	default:
		return Fail("protocol_upgrade_required")
	}
}
func cloneTask(t *Task) Task {
	b, _ := json.Marshal(t)
	var out Task
	_ = json.Unmarshal(b, &out)
	return out
}
func fieldPointer[T any](f Field[T]) *T {
	if f.Null {
		return nil
	}
	return Ptr(f.Value)
}
func applyScalar[T any](f Field[T], out *T) error {
	if !f.Present {
		return nil
	}
	if f.Null {
		return Invalid("/patch")
	}
	*out = f.Value
	return nil
}
func (p *planner) patchValues(t *Task, v Patch) error {
	if e := applyScalar(v.Title, &t.Title); e != nil {
		return e
	}
	if e := applyScalar(v.Notes, &t.Notes); e != nil {
		return e
	}
	if e := applyScalar(v.PlanningState, &t.PlanningState); e != nil {
		return e
	}
	t.Title = strings.TrimSpace(t.Title)
	for _, pair := range []struct {
		f   Field[bool]
		out *bool
	}{{v.TodayFocus, &t.TodayFocus}, {v.Evening, &t.Evening}, {v.Pinned, &t.Pinned}, {v.Important, &t.Important}} {
		if e := applyScalar(pair.f, pair.out); e != nil {
			return e
		}
	}
	if v.CategoryID.Present {
		t.CategoryID = fieldPointer(v.CategoryID)
	}
	if v.ParentTaskID.Present {
		t.ParentTaskID = fieldPointer(v.ParentTaskID)
	}
	if v.StartOn.Present {
		t.StartOn = fieldPointer(v.StartOn)
	}
	if v.DeadlineOn.Present {
		t.DeadlineOn = fieldPointer(v.DeadlineOn)
	}
	if v.Schedule.Present {
		t.Schedule = fieldPointer(v.Schedule)
	}
	if v.Reminder.Present {
		t.Reminder = fieldPointer(v.Reminder)
	}
	if v.TagIDs.Present {
		t.TagIDs = []ID{}
		if !v.TagIDs.Null {
			t.TagIDs = append(t.TagIDs, v.TagIDs.Value...)
		}
		slices.Sort(t.TagIDs)
	}
	if v.Links.Present {
		t.Links = []Link{}
		if !v.Links.Null {
			t.Links = append(t.Links, v.Links.Value...)
		}
		slices.SortFunc(t.Links, func(a, b Link) int { return cmp.Compare(a.TargetID, b.TargetID) })
	}
	if t.Schedule == nil && t.Reminder != nil && t.Reminder.Kind == "RELATIVE" {
		if v.Reminder.Present && !v.Reminder.Null {
			return Invalid("/reminder")
		}
		t.Reminder = nil
	}
	if t.PlanningState == "INBOX" && (t.StartOn != nil || t.DeadlineOn != nil || t.Schedule != nil || t.Reminder != nil) {
		t.PlanningState = "ACTIVE"
	}
	return nil
}
func (p *planner) move(id ID, category, parent *ID) error {
	t, e := p.task(id)
	if e != nil {
		return e
	}
	if t.DeletedAt != nil {
		return Invalid("/task_id")
	}
	oldRoot, e := p.root(id)
	if e != nil {
		return e
	}
	members, e := p.tree(id)
	if e != nil {
		return e
	}
	oldCategory := t.CategoryID
	beforeMove := cloneTask(t)
	if parent != nil {
		for _, m := range members {
			if m.ID == *parent {
				return Fail("category_cycle")
			}
		}
		target, e := p.task(*parent)
		if e != nil {
			return e
		}
		if target.DeletedAt != nil || target.Status != "OPEN" {
			return Invalid("/parent_task_id")
		}
		category = target.CategoryID
	}
	if category != nil {
		cat, e := p.category(*category)
		if e != nil {
			return e
		}
		if cat.DeletedAt != nil {
			return Invalid("/category_id")
		}
	}
	if Equal(t.ParentTaskID, parent) && Equal(t.CategoryID, category) {
		return nil
	}
	t.ParentTaskID = parent
	for _, m := range members {
		if m.DeletedAt == nil {
			key := Key{TaskKind, m.ID}
			depth := depthIn(p.graph, key)
			if depth > 2 && depth > depthIn(p.before, key) {
				return Fail("category_depth")
			}
		}
	}
	for _, m := range members {
		if m.DeletedAt != nil {
			continue
		}
		old := cloneTask(m)
		m.CategoryID = category
		p.changedTask(m, old)
	}
	t.UpdatedAt = p.event
	p.changedTask(t, beforeMove)
	p.structure(StructureKey{"task_tree", oldRoot}, true)
	newRoot, e := p.root(id)
	if e != nil {
		return e
	}
	p.structure(StructureKey{"task_tree", newRoot}, true)
	p.structure(StructKey("category_members", oldCategory), true)
	p.structure(StructKey("category_members", category), true)
	return nil
}
func (p *planner) finish() (Plan, error) {
	out := Plan{Changes: []Delta{}, Structures: []StructureVersion{}, Aliases: []Alias{}, Edges: []Edge{}, Reads: Preconditions{Entities: []Version{}, Structures: []StructureVersion{}}}
	keys := []Key{}
	for k := range p.graph.Entities {
		keys = append(keys, k)
	}
	slices.SortFunc(keys, func(a, b Key) int { return strings.Compare(a.String(), b.String()) })
	for _, k := range keys {
		after := p.graph.Entities[k]
		before, ok := p.before.Entities[k]
		if ok && Equal(before.Value, after.Value) {
			continue
		}
		p.record(k)
		if e := p.validateEntity(after, before); e != nil {
			return Plan{}, e
		}
		if ok {
			if before.Revision == Counter(1<<63-1) {
				return Plan{}, Fail("quota_exceeded")
			}
			after.Revision = before.Revision + 1
			out.Changes = append(out.Changes, Delta{Before: Ptr(before), After: after})
		} else {
			after.Revision = 1
			out.Changes = append(out.Changes, Delta{After: after})
		}
	}
	for k := range p.dirty {
		old := p.before.Structures[k]
		if old == Counter(1<<63-1) {
			return Plan{}, Fail("quota_exceeded")
		}
		out.Structures = append(out.Structures, StructureVersion{k.Kind, k.ID, old + 1})
	}
	slices.SortFunc(out.Structures, func(a, b StructureVersion) int {
		if a.Kind != b.Kind {
			return strings.Compare(a.Kind, b.Kind)
		}
		return cmp.Compare(a.ID, b.ID)
	})
	for k, v := range p.reads {
		out.Reads.Entities = append(out.Reads.Entities, Version{k.Type, k.ID, v})
	}
	for k, v := range p.structures {
		out.Reads.Structures = append(out.Reads.Structures, StructureVersion{k.Kind, k.ID, v})
	}
	slices.SortFunc(out.Reads.Entities, func(a, b Version) int { return strings.Compare(a.Key().String(), b.Key().String()) })
	slices.SortFunc(out.Reads.Structures, func(a, b StructureVersion) int {
		if a.Kind != b.Kind {
			return strings.Compare(a.Kind, b.Kind)
		}
		return cmp.Compare(a.ID, b.ID)
	})
	for _, a := range p.graph.Aliases {
		if !slices.Contains(p.before.Aliases, a) {
			out.Aliases = append(out.Aliases, a)
		}
	}
	for _, edge := range p.graph.Edges {
		if !slices.Contains(p.before.Edges, edge) {
			out.Edges = append(out.Edges, edge)
		}
	}
	return out, nil
}
func CheckPreconditions(graph Graph, provided, required Preconditions) error {
	entities := map[Key]Counter{}
	structures := map[StructureKey]Counter{}
	for _, v := range provided.Entities {
		k := v.Key()
		if _, ok := entities[k]; ok {
			return Invalid("/preconditions/entities")
		}
		entities[k] = v.Revision
		if actual := graph.Entities[k].Revision; actual != v.Revision {
			return ConflictError(k, v.Revision, actual)
		}
	}
	for _, v := range provided.Structures {
		k := v.Key()
		if _, ok := structures[k]; ok {
			return Invalid("/preconditions/structures")
		}
		structures[k] = v.Revision
		if actual := graph.Structures[k]; actual != v.Revision {
			return &Fault{Code: "structure_conflict", Structures: []StructureVersion{{k.Kind, k.ID, actual}}}
		}
	}
	for _, v := range required.Entities {
		if _, ok := entities[v.Key()]; !ok {
			return Missing(v.Key())
		}
	}
	for _, v := range required.Structures {
		if _, ok := structures[v.Key()]; !ok {
			return &Fault{Code: "read_set_incomplete", Field: "/preconditions/structures/" + v.Kind + "/" + string(v.ID)}
		}
	}
	return nil
}
func (p *planner) validateEntity(e, before Entity) error {
	if !ValidID(string(e.ID), false) {
		return Invalid("/id")
	}
	switch v := e.Value.(type) {
	case *Task:
		if v.ID != e.ID {
			return Invalid("/id")
		}
		return p.validateTask(v, before)
	case *Category:
		if v.ID != e.ID {
			return Invalid("/id")
		}
		if err := CheckTitle(v.Title); err != nil {
			return err
		}
		if !slices.Contains([]string{"neutral", "blue", "green", "orange", "purple"}, v.Color) {
			return Invalid("/color_key")
		}
		depth, err := p.categoryDepth(v.ID)
		if err != nil {
			return err
		}
		oldDepth := depthIn(p.before, Key{CategoryKind, v.ID})
		if depth > 2 && depth > oldDepth {
			return Fail("category_depth")
		}
	case *Tag:
		if v.ID != e.ID {
			return Invalid("/id")
		}
		if err := CheckTitle(v.Title); err != nil {
			return err
		}
		if !slices.Contains([]string{"neutral", "blue", "green", "orange", "purple"}, v.Color) {
			return Invalid("/color_key")
		}
	case *Rule:
		if v.ID != e.ID {
			return Invalid("/id")
		}
		return p.validateRule(v)
	case *Ordering:
		if v.ID != e.ID {
			return Invalid("/id")
		}
		if !slices.Contains([]string{"category_siblings", "task_siblings", "today"}, v.Kind) {
			return Invalid("/kind")
		}
	default:
		return Fail("internal_error")
	}
	return nil
}
func depthIn(g Graph, k Key) int {
	seen := map[ID]bool{}
	depth := 0
	for {
		if seen[k.ID] {
			return 1 << 20
		}
		seen[k.ID] = true
		e, ok := g.Entities[k]
		if !ok {
			return 0
		}
		depth++
		var parent *ID
		switch v := e.Value.(type) {
		case *Task:
			parent = v.ParentTaskID
		case *Category:
			parent = v.ParentID
		}
		if parent == nil {
			return depth
		}
		k.ID = *parent
	}
}
func (p *planner) categoryDepth(id ID) (int, error) {
	seen := map[ID]bool{}
	n := 0
	for {
		if seen[id] {
			return 0, Fail("category_cycle")
		}
		seen[id] = true
		c, e := p.category(id)
		if e != nil {
			return 0, e
		}
		n++
		if c.ParentID == nil {
			return n, nil
		}
		id = *c.ParentID
	}
}
func (p *planner) validateTask(t *Task, before Entity) error {
	if e := CheckTitle(t.Title); e != nil {
		return e
	}
	if !utf8.ValidString(t.Title) || !utf8.ValidString(t.Notes) {
		return Invalid("/text")
	}
	if !slices.Contains([]string{"INBOX", "ACTIVE", "SOMEDAY"}, t.PlanningState) || !slices.Contains([]string{"OPEN", "COMPLETED", "CANCELLED"}, t.Status) {
		return Invalid("/status")
	}
	if (t.Status == "OPEN" && (t.CompletedAt != nil || t.CancelledAt != nil)) || (t.Status == "COMPLETED" && (t.CompletedAt == nil || t.CancelledAt != nil)) || (t.Status == "CANCELLED" && (t.CancelledAt == nil || t.CompletedAt != nil)) {
		return Invalid("/status")
	}
	for _, v := range []*Instant{&t.CreatedAt, &t.UpdatedAt, t.CompletedAt, t.CancelledAt, t.DeletedAt} {
		if v != nil {
			if _, e := v.Time(); e != nil {
				return e
			}
		}
	}
	for _, d := range []*Date{t.StartOn, t.DeadlineOn, t.OccurrenceOn} {
		if d != nil {
			if _, e := d.Time(); e != nil {
				return e
			}
		}
	}
	if t.StartOn != nil && t.DeadlineOn != nil && *t.StartOn > *t.DeadlineOn {
		return Fail("date_order")
	}
	if t.CategoryID != nil {
		c, e := p.category(*t.CategoryID)
		if e != nil {
			return e
		}
		if c.DeletedAt != nil {
			return Invalid("/category_id")
		}
	}
	if t.ParentTaskID != nil {
		parent, e := p.task(*t.ParentTaskID)
		if e != nil {
			return e
		}
		if !Equal(parent.CategoryID, t.CategoryID) {
			return Invalid("/category_id")
		}
		if parent.ID == t.ID {
			return Fail("category_cycle")
		}
	}
	root, e := p.root(t.ID)
	if e != nil {
		return e
	}
	_ = root
	depth := depthIn(p.graph, Key{TaskKind, t.ID})
	oldDepth := depthIn(p.before, Key{TaskKind, t.ID})
	if depth > 2 && depth > oldDepth {
		return Fail("category_depth")
	}
	if (t.RecurrenceID == nil) != (t.OccurrenceKey == nil) {
		return Invalid("/recurrence_id")
	}
	if t.RecurrenceID != nil {
		if _, e := p.rule(*t.RecurrenceID); e != nil {
			return e
		}
	}
	seenTags := map[ID]bool{}
	for _, id := range t.TagIDs {
		if seenTags[id] {
			return Invalid("/tag_ids")
		}
		seenTags[id] = true
		e, err := p.entity(Key{TagKind, id})
		if err != nil {
			return err
		}
		if e.Value.(*Tag).DeletedAt != nil {
			return Invalid("/tag_ids")
		}
	}
	seenLinks := map[ID]bool{}
	for _, l := range t.Links {
		if l.TargetID == t.ID || seenLinks[l.TargetID] || !slices.Contains([]string{"preparation", "followup"}, l.Role) {
			return Invalid("/links")
		}
		seenLinks[l.TargetID] = true
		if _, e := p.task(l.TargetID); e != nil {
			return e
		}
	}
	return validateTiming(t)
}
