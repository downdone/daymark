package domain

import "slices"

// PlanImport merges identity-preserving complete facts. Different content under
// the same identity is a conflict; missing input never means deletion.
func PlanImport(before Graph, payload ImportPayload, dataset ID) (Plan, error) {
	copy, e := before.Clone()
	if e != nil {
		return Plan{}, e
	}
	p := &planner{before: before, graph: copy, reads: map[Key]Counter{}, structures: map[StructureKey]Counter{}, dirty: map[StructureKey]bool{}, dataset: dataset}
	seen := map[Key]bool{}
	aliases := map[ID]ID{}
	for _, en := range payload.Entities {
		// Identity checks precede any compatibility rewrite, otherwise a forged
		// outer ID could be hidden by recurrence alias reconciliation.
		var inner ID
		switch v := en.Value.(type) {
		case *Task:
			inner = v.ID
		case *Category:
			inner = v.ID
		case *Tag:
			inner = v.ID
		case *Rule:
			inner = v.ID
		case *Ordering:
			inner = v.ID
		default:
			return Plan{}, Invalid("/entities/type")
		}
		if inner != en.ID || !ValidID(string(inner), false) {
			return Plan{}, Invalid("/entities/id")
		}
		if seen[en.Key()] {
			return Plan{}, Invalid("/entities")
		}
		seen[en.Key()] = true
		if en.Operation != "replace" || en.Revision < 1 {
			return Plan{}, Invalid("/entities")
		}
		if t, ok := en.Value.(*Task); ok && t.RecurrenceID != nil && t.OccurrenceKey != nil {
			for _, old := range before.Entities {
				remote, ok := old.Value.(*Task)
				if ok && remote.ID != t.ID && Equal(remote.RecurrenceID, t.RecurrenceID) && Equal(remote.OccurrenceKey, t.OccurrenceKey) {
					incoming := cloneTask(t)
					incoming.ID = remote.ID
					if !Equal(&incoming, remote) {
						return Plan{}, ConflictError(en.Key(), en.Revision, old.Revision)
					}
					aliases[t.ID] = remote.ID
				}
			}
		}
	}
	resolve := func(id *ID) *ID {
		if id == nil {
			return nil
		}
		if canonical, ok := aliases[*id]; ok {
			return Ptr(canonical)
		}
		return id
	}
	for _, en := range payload.Entities {
		// Clone every input entity before rewriting compatibility identities.
		g := EmptyGraph()
		g.Entities[en.Key()] = en
		g, e = g.Clone()
		if e != nil {
			return Plan{}, e
		}
		en = g.Entities[en.Key()]
		if v, ok := en.Value.(*Task); ok {
			if canonical, ok := aliases[v.ID]; ok {
				v.ID = canonical
				en.ID = canonical
			}
			v.ParentTaskID = resolve(v.ParentTaskID)
			for i := range v.Links {
				v.Links[i].TargetID = Value(resolve(&v.Links[i].TargetID))
			}
			if v.ID != en.ID {
				return Plan{}, Invalid("/entities/id")
			}
		}
		if v, ok := en.Value.(*Rule); ok {
			v.SourceTaskID = resolve(v.SourceTaskID)
			v.Spec.Defaults.ParentTaskID = resolve(v.Spec.Defaults.ParentTaskID)
			for i := range v.Spec.Defaults.Links {
				v.Spec.Defaults.Links[i].TargetID = Value(resolve(&v.Spec.Defaults.Links[i].TargetID))
			}
		}
		if v, ok := en.Value.(*Ordering); ok {
			if v.Kind != "category_siblings" {
				v.SubjectID = resolve(v.SubjectID)
				for i := range v.OrderedIDs {
					v.OrderedIDs[i] = Value(resolve(&v.OrderedIDs[i]))
				}
			}
		}
		if old, exists := p.graph.Entities[en.Key()]; exists {
			if !Equal(old.Value, en.Value) {
				return Plan{}, ConflictError(en.Key(), en.Revision, old.Revision)
			}
			continue
		}
		if e = p.newEntity(en); e != nil {
			return Plan{}, e
		}
		switch v := en.Value.(type) {
		case *Task:
			p.structure(StructKey("category_members", v.CategoryID), true)
			for _, id := range v.TagIDs {
				p.structure(StructureKey{"tag_members", id}, true)
			}
			if v.RecurrenceID != nil {
				p.structure(StructureKey{"recurrence_members", *v.RecurrenceID}, true)
			}
		case *Category:
			p.structure(StructureKey{"category_tree", dataset}, true)
		case *Rule:
			p.structure(StructureKey{"recurrence_members", v.ID}, true)
		case *Ordering:
			p.structure(StructureKey{"ordering_scope", v.ID}, true)
		}
	}
	for alias, canonical := range aliases {
		payload.Aliases = append(payload.Aliases, Alias{alias, canonical})
	}
	for _, a := range payload.Aliases {
		if a.AliasID == a.CanonicalID || !ValidID(string(a.AliasID), false) || !ValidID(string(a.CanonicalID), false) {
			return Plan{}, Invalid("/aliases")
		}
		a.CanonicalID = Value(resolve(&a.CanonicalID))
		if a.AliasID == a.CanonicalID {
			continue
		}
		if _, exists := p.graph.Task(a.CanonicalID); !exists {
			return Plan{}, Invalid("/aliases")
		}
		if _, exists := p.graph.Task(a.AliasID); exists {
			return Plan{}, Invalid("/aliases")
		}
		for _, old := range p.graph.Aliases {
			if old.AliasID == a.AliasID && old.CanonicalID != a.CanonicalID {
				return Plan{}, Fail("revision_conflict")
			}
		}
		if !slices.Contains(p.graph.Aliases, a) {
			p.graph.Aliases = append(p.graph.Aliases, a)
		}
	}
	for _, edge := range payload.Edges {
		edge.PredecessorID = Value(resolve(&edge.PredecessorID))
		edge.SuccessorID = Value(resolve(&edge.SuccessorID))
		if edge.PredecessorID == edge.SuccessorID {
			return Plan{}, Invalid("/successor_edges")
		}
		if _, e = p.rule(edge.RuleID); e != nil {
			return Plan{}, e
		}
		if _, e = p.task(edge.PredecessorID); e != nil {
			return Plan{}, e
		}
		if _, e = p.task(edge.SuccessorID); e != nil {
			return Plan{}, e
		}
		for _, old := range p.graph.Edges {
			if old.RuleID == edge.RuleID && old.PredecessorID == edge.PredecessorID && old.SuccessorID != edge.SuccessorID || old.SuccessorID == edge.SuccessorID && old != edge {
				return Plan{}, Fail("revision_conflict")
			}
		}
		if !slices.Contains(p.graph.Edges, edge) {
			p.graph.Edges = append(p.graph.Edges, edge)
		}
	}
	for _, en := range p.graph.Entities {
		if t, ok := en.Value.(*Task); ok {
			if _, existed := before.Entities[en.Key()]; !existed {
				root, e := p.root(t.ID)
				if e != nil {
					return Plan{}, e
				}
				p.structure(StructureKey{"task_tree", root}, true)
			}
		}
	}
	if e = p.reconcileOrderings(); e != nil {
		return Plan{}, e
	}
	return p.finish()
}
