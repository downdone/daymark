package domain

import "slices"

func (p *planner) changeState(c *ChangeState) error {
	if !ValidID(string(c.GroupID), false) || !slices.Contains([]string{"complete", "reopen", "delete", "restore", "cancel"}, c.Action) {
		return Invalid("/action")
	}
	selected := map[ID]bool{}
	for _, id := range c.TaskIDs {
		if selected[id] {
			return Invalid("/task_ids")
		}
		selected[id] = true
		if _, e := p.task(id); e != nil {
			return e
		}
	}
	roots := []ID{}
	for _, id := range c.TaskIDs {
		t, _ := p.task(id)
		ancestor := t.ParentTaskID
		seen := map[ID]bool{}
		overlap := false
		for ancestor != nil {
			if seen[*ancestor] {
				return Fail("category_cycle")
			}
			seen[*ancestor] = true
			if selected[*ancestor] {
				overlap = true
				break
			}
			parent, e := p.task(*ancestor)
			if e != nil {
				return e
			}
			ancestor = parent.ParentTaskID
		}
		if !overlap || c.Action == "cancel" {
			roots = append(roots, id)
		}
	}
	slices.Sort(roots)
	all := map[ID]bool{}
	for _, id := range roots {
		members, e := p.tree(id)
		if e != nil {
			return e
		}
		for _, t := range members {
			all[t.ID] = true
		}
	}
	for _, en := range p.graph.Entities {
		if t, ok := en.Value.(*Task); ok && !all[t.ID] && ((t.CompletionGroupID != nil && *t.CompletionGroupID == c.GroupID) || (t.DeletionGroupID != nil && *t.DeletionGroupID == c.GroupID)) {
			return Invalid("/group_id")
		}
	}
	for _, id := range roots {
		root, e := p.task(id)
		if e != nil {
			return e
		}
		members, e := p.tree(id)
		if e != nil {
			return e
		}
		treeRoot, e := p.root(id)
		if e != nil {
			return e
		}
		group := root.CompletionGroupID
		if c.Action == "restore" {
			group = root.DeletionGroupID
		}
		for _, t := range members {
			if t.ID != root.ID {
				if c.Action == "cancel" {
					continue
				}
				if c.Action == "reopen" && (group == nil || !Equal(t.CompletionGroupID, group)) {
					continue
				}
				if c.Action == "restore" && (group == nil || !Equal(t.DeletionGroupID, group)) {
					continue
				}
			}
			old := cloneTask(t)
			switch c.Action {
			case "complete":
				if t.DeletedAt != nil || t.Status != "OPEN" {
					continue
				}
				t.Status = "COMPLETED"
				t.CompletedAt = Ptr(p.event)
				t.CancelledAt = nil
				t.CompletionGroupID = Ptr(c.GroupID)
			case "reopen":
				if t.DeletedAt != nil {
					return Invalid("/task_ids")
				}
				if t.Status == "OPEN" {
					continue
				}
				if e := p.canReopen(t); e != nil {
					return e
				}
				t.Status = "OPEN"
				t.CompletedAt = nil
				t.CancelledAt = nil
				t.CompletionGroupID = nil
			case "delete":
				if t.DeletedAt != nil {
					continue
				}
				t.DeletedAt = Ptr(p.event)
				t.DeletionGroupID = Ptr(c.GroupID)
				p.structure(StructKey("category_members", t.CategoryID), true)
			case "restore":
				if t.DeletedAt == nil {
					continue
				}
				t.DeletedAt = nil
				t.DeletionGroupID = nil
				if t.ParentTaskID != nil {
					parent, e := p.task(*t.ParentTaskID)
					if e != nil {
						return e
					}
					if parent.DeletedAt != nil {
						t.ParentTaskID = nil
					} else {
						t.CategoryID = parent.CategoryID
					}
				}
				if t.CategoryID != nil {
					cat, e := p.category(*t.CategoryID)
					if e != nil {
						return e
					}
					if cat.DeletedAt != nil {
						t.CategoryID = nil
					}
				}
				p.structure(StructKey("category_members", t.CategoryID), true)
			case "cancel":
				if t.DeletedAt != nil {
					return Invalid("/task_ids")
				}
				if t.Status == "CANCELLED" {
					continue
				}
				t.Status = "CANCELLED"
				t.CancelledAt = Ptr(p.event)
				t.CompletedAt = nil
				t.CompletionGroupID = nil
			}
			p.changedTask(t, old)
			p.structure(StructureKey{"task_tree", treeRoot}, true)
			if c.Action == "restore" {
				p.structure(StructKey("category_members", old.CategoryID), true)
				if newRoot, e := p.root(t.ID); e != nil {
					return e
				} else {
					p.structure(StructureKey{"task_tree", newRoot}, true)
				}
			}
			if c.Action == "complete" && t.RecurrenceID != nil {
				if e := p.successor(t); e != nil {
					return e
				}
			}
		}
	}
	return nil
}
func (p *planner) canReopen(t *Task) error {
	if t.RecurrenceID == nil {
		return nil
	}
	r, e := p.rule(*t.RecurrenceID)
	if e != nil {
		return e
	}
	if r.Spec.Mode != "AFTER_COMPLETION" {
		return nil
	}
	p.structure(StructureKey{"recurrence_members", r.ID}, false)
	for _, edge := range p.graph.Edges {
		if edge.RuleID == r.ID && edge.PredecessorID == t.ID {
			next, e := p.task(edge.SuccessorID)
			if e != nil {
				return e
			}
			if next.DeletedAt == nil && next.Status == "OPEN" {
				return Fail("recurrence_open_instance")
			}
		}
	}
	return nil
}
