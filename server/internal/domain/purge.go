package domain

// purge retains identity/relationship tombstones, never an editable task body.
func (p *planner) purge(c *PurgeTasks) error {
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
	if len(selected) == 0 {
		return Invalid("/task_ids")
	}
	members := map[ID]*Task{}
	for id := range selected {
		tree, e := p.tree(id)
		if e != nil {
			return e
		}
		for _, t := range tree {
			if t.DeletedAt == nil {
				return Invalid("/task_ids")
			}
			members[t.ID] = t
		}
	}
	for _, t := range members {
		if t.PurgedAt != nil {
			continue
		}
		old := cloneTask(t)
		t.Title = "[deleted]"
		t.Notes = ""
		t.PurgedAt = Ptr(p.event)
		t.Schedule = nil
		t.Reminder = nil
		t.StartOn = nil
		t.DeadlineOn = nil
		t.TodayFocus = false
		t.Evening = false
		t.Pinned = false
		t.Important = false
		t.Legacy = nil
		for _, id := range t.TagIDs {
			p.structure(StructureKey{"tag_members", id}, true)
		}
		t.TagIDs = []ID{}
		t.Links = []Link{}
		p.changedTask(t, old)
		root, e := p.root(t.ID)
		if e != nil {
			return e
		}
		p.structure(StructureKey{"task_tree", root}, true)
	}
	return nil
}
