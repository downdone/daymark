package domain

import (
	"crypto/sha1"
	"encoding/hex"
	"slices"
	"strings"
)

func StableID(name string) ID {
	namespace, _ := hex.DecodeString("43e1f2a48d214a8cbb7016b813d3f5ab")
	h := sha1.New()
	_, _ = h.Write(namespace)
	_, _ = h.Write([]byte(name))
	b := h.Sum(nil)[:16]
	b[6] = b[6]&15 | 80
	b[8] = b[8]&63 | 128
	s := hex.EncodeToString(b)
	return ID(s[:8] + "-" + s[8:12] + "-" + s[12:16] + "-" + s[16:20] + "-" + s[20:])
}
func OrderingID(kind string, subject *ID) ID {
	id := Value(subject)
	if id == "" {
		id = NilID
	}
	return StableID("ordering:" + kind + ":" + string(id))
}
func (p *planner) saveCategory(c *SaveCategory) error {
	if e := CheckTitle(c.Title); e != nil {
		return e
	}
	p.structure(StructureKey{"category_tree", p.dataset}, false)
	key := Key{CategoryKind, c.CategoryID}
	p.record(key)
	if old, ok := p.graph.Entities[key]; ok {
		v := old.Value.(*Category)
		restoring := v.DeletedAt != nil
		if c.ParentID.Present && !Equal(fieldPointer(c.ParentID), v.ParentID) {
			return Invalid("/parent_id")
		}
		if restoring || v.Title != strings.TrimSpace(c.Title) || v.Description != c.Description || v.Color != c.Color || v.Archived != c.Archived {
			if restoring || v.Archived != c.Archived {
				p.structure(StructureKey{"category_tree", p.dataset}, true)
			}
			v.Title = strings.TrimSpace(c.Title)
			v.Description = c.Description
			v.Color = c.Color
			v.Archived = c.Archived
			v.DeletedAt = nil
			v.UpdatedAt = p.event
		}
		return nil
	}
	var parent *ID
	if c.ParentID.Present {
		parent = fieldPointer(c.ParentID)
	}
	if parent != nil {
		v, e := p.category(*parent)
		if e != nil {
			return e
		}
		if v.DeletedAt != nil {
			return Invalid("/parent_id")
		}
	}
	p.structure(StructureKey{"category_tree", p.dataset}, true)
	return p.newEntity(Entity{Type: CategoryKind, ID: c.CategoryID, Value: &Category{ID: c.CategoryID, ParentID: parent, Title: strings.TrimSpace(c.Title), Description: c.Description, Color: c.Color, Archived: c.Archived, CreatedAt: p.event, UpdatedAt: p.event}})
}
func (p *planner) removeCategory(c *RemoveCategory) error {
	cat, e := p.category(c.CategoryID)
	if e != nil {
		return e
	}
	p.structure(StructureKey{"category_tree", p.dataset}, false)
	p.structure(StructureKey{"category_members", cat.ID}, false)
	if cat.DeletedAt != nil {
		return nil
	}
	cat.DeletedAt = Ptr(p.event)
	cat.UpdatedAt = p.event
	p.structure(StructureKey{"category_tree", p.dataset}, true)
	p.structure(StructureKey{"category_members", cat.ID}, true)
	p.structure(StructKey("category_members", nil), true)
	for _, en := range p.graph.Entities {
		switch v := en.Value.(type) {
		case *Task:
			if v.CategoryID != nil && *v.CategoryID == cat.ID {
				p.record(en.Key())
				old := cloneTask(v)
				v.CategoryID = nil
				p.changedTask(v, old)
			}
		case *Category:
			if v.ParentID != nil && *v.ParentID == cat.ID {
				p.record(en.Key())
				v.ParentID = nil
				v.UpdatedAt = p.event
			}
		}
	}
	return nil
}
func (p *planner) relocate(c *RelocateCategory) error {
	if c.CategoryID == c.TargetID {
		return Invalid("/target_id")
	}
	v, e := p.category(c.CategoryID)
	if e != nil {
		return e
	}
	target, e := p.category(c.TargetID)
	if e != nil {
		return e
	}
	if v.DeletedAt != nil || target.DeletedAt != nil {
		return Invalid("/category_id")
	}
	p.structure(StructureKey{"category_tree", p.dataset}, false)
	parent := target.ParentID
	if c.Placement == "inside" {
		if target.ParentID != nil {
			return Fail("category_depth")
		}
		parent = Ptr(target.ID)
	} else if c.Placement != "before" && c.Placement != "after" {
		return Invalid("/placement")
	}
	if parent != nil {
		for _, en := range p.graph.Entities {
			if child, ok := en.Value.(*Category); ok && child.ParentID != nil && *child.ParentID == v.ID && child.DeletedAt == nil {
				p.record(en.Key())
				return Fail("category_has_children")
			}
		}
	}
	oldParent := v.ParentID
	if !Equal(parent, v.ParentID) {
		v.ParentID = parent
		v.UpdatedAt = p.event
		p.structure(StructureKey{"category_tree", p.dataset}, true)
	}
	order, e := p.findOrdering("category_siblings", parent, true)
	if e != nil {
		return e
	}
	members := []ID{}
	for _, en := range p.graph.Entities {
		if item, ok := en.Value.(*Category); ok && Equal(item.ParentID, parent) && item.DeletedAt == nil {
			p.record(en.Key())
			members = append(members, item.ID)
		}
	}
	slices.Sort(members)
	ids := append([]ID{}, order.OrderedIDs...)
	for _, id := range members {
		if !slices.Contains(ids, id) {
			ids = append(ids, id)
		}
	}
	ids = slices.DeleteFunc(ids, func(id ID) bool { return id == v.ID })
	at := len(ids)
	if c.Placement != "inside" {
		at = slices.Index(ids, target.ID)
		if at < 0 {
			return Invalid("/target_id")
		}
		if c.Placement == "after" {
			at++
		}
	}
	ids = slices.Insert(ids, at, v.ID)
	if !slices.Equal(order.OrderedIDs, ids) {
		order.OrderedIDs = ids
		p.structure(StructureKey{"ordering_scope", order.ID}, true)
	}
	if !Equal(oldParent, parent) {
		if old, e := p.findOrdering("category_siblings", oldParent, false); e != nil {
			return e
		} else if old != nil {
			old.OrderedIDs = slices.DeleteFunc(old.OrderedIDs, func(id ID) bool { return id == v.ID })
			p.structure(StructureKey{"ordering_scope", old.ID}, true)
		}
	}
	return nil
}
func (p *planner) findOrdering(kind string, subject *ID, create bool) (*Ordering, error) {
	var found *Ordering
	for _, en := range p.graph.Entities {
		if v, ok := en.Value.(*Ordering); ok && v.Kind == kind && Equal(v.SubjectID, subject) {
			if found != nil {
				return nil, Fail("storage_corrupt")
			}
			found = v
		}
	}
	if found != nil {
		p.record(Key{OrderingKind, found.ID})
		p.structure(StructureKey{"ordering_scope", found.ID}, false)
		return found, nil
	}
	if !create {
		return nil, nil
	}
	id := OrderingID(kind, subject)
	o := &Ordering{ID: id, Kind: kind, SubjectID: subject, OrderedIDs: []ID{}}
	if e := p.newEntity(Entity{Type: OrderingKind, ID: id, Value: o}); e != nil {
		return nil, e
	}
	p.structure(StructureKey{"ordering_scope", id}, true)
	return o, nil
}
func (p *planner) belongs(o *Ordering, id ID) bool {
	switch o.Kind {
	case "category_siblings":
		v, ok := p.graph.Entities[Key{CategoryKind, id}]
		return ok && Equal(v.Value.(*Category).ParentID, o.SubjectID)
	case "task_siblings":
		v, ok := p.graph.Entities[Key{TaskKind, id}]
		return ok && Equal(v.Value.(*Task).ParentTaskID, o.SubjectID)
	case "today":
		_, ok := p.graph.Entities[Key{TaskKind, id}]
		return ok
	}
	return false
}
func (p *planner) order(c *ReplaceOrdering) error {
	key := Key{OrderingKind, c.ScopeID}
	p.record(key)
	var o *Ordering
	if en, ok := p.graph.Entities[key]; ok {
		o = en.Value.(*Ordering)
		if o.Kind != c.Kind || !Equal(o.SubjectID, c.SubjectID) {
			return Invalid("/kind")
		}
	} else {
		if old, e := p.findOrdering(c.Kind, c.SubjectID, false); e != nil {
			return e
		} else if old != nil {
			return Invalid("/scope_id")
		}
		o = &Ordering{ID: c.ScopeID, Kind: c.Kind, SubjectID: c.SubjectID, OrderedIDs: []ID{}}
		if e := p.newEntity(Entity{Type: OrderingKind, ID: o.ID, Value: o}); e != nil {
			return e
		}
		p.structure(StructureKey{"ordering_scope", o.ID}, true)
	}
	p.structure(StructureKey{"ordering_scope", o.ID}, false)
	if c.Kind == "today" && c.SubjectID != nil {
		return Invalid("/subject_id")
	}
	members := append([]ID{}, c.MemberIDs...)
	ordered := append([]ID{}, c.OrderedIDs...)
	slices.Sort(members)
	slices.Sort(ordered)
	if !slices.Equal(members, ordered) {
		return Invalid("/ordered_ids")
	}
	for i, id := range members {
		if i > 0 && members[i-1] == id {
			return Invalid("/member_ids")
		}
		if !p.belongs(o, id) {
			return Invalid("/member_ids")
		}
		kind := TaskKind
		if o.Kind == "category_siblings" {
			kind = CategoryKind
		}
		p.record(Key{kind, id})
	}
	ids := append([]ID{}, o.OrderedIDs...)
	for _, id := range members {
		if !slices.Contains(ids, id) {
			ids = append(ids, id)
		}
	}
	at := 0
	for i, id := range ids {
		if slices.Contains(members, id) {
			ids[i] = c.OrderedIDs[at]
			at++
		}
	}
	if !slices.Equal(ids, o.OrderedIDs) {
		o.OrderedIDs = ids
		p.structure(StructureKey{"ordering_scope", o.ID}, true)
	}
	return nil
}
func (p *planner) reconcileOrderings() error {
	for _, en := range p.graph.Entities {
		o, ok := en.Value.(*Ordering)
		if !ok || o.Kind == "today" {
			continue
		}
		membershipChanged := false
		for key, cur := range p.graph.Entities {
			prev, exists := p.before.Entities[key]
			switch v := cur.Value.(type) {
			case *Task:
				if o.Kind == "task_siblings" {
					var before *ID
					if exists {
						before = prev.Value.(*Task).ParentTaskID
					}
					if (!exists || !Equal(before, v.ParentTaskID)) && (Equal(before, o.SubjectID) || Equal(v.ParentTaskID, o.SubjectID)) {
						membershipChanged = true
					}
				}
			case *Category:
				if o.Kind == "category_siblings" {
					var before *ID
					if exists {
						before = prev.Value.(*Category).ParentID
					}
					if (!exists || !Equal(before, v.ParentID)) && (Equal(before, o.SubjectID) || Equal(v.ParentID, o.SubjectID)) {
						membershipChanged = true
					}
				}
			}
		}
		if !membershipChanged {
			continue
		}
		p.record(en.Key())
		p.structure(StructureKey{"ordering_scope", o.ID}, false)
		ids := []ID{}
		for _, id := range o.OrderedIDs {
			if p.belongs(o, id) {
				ids = append(ids, id)
			}
		}
		extra := []ID{}
		for key := range p.graph.Entities {
			if p.belongs(o, key.ID) && ((o.Kind == "category_siblings" && key.Type == CategoryKind) || (o.Kind == "task_siblings" && key.Type == TaskKind)) && !slices.Contains(ids, key.ID) {
				extra = append(extra, key.ID)
			}
		}
		slices.Sort(extra)
		ids = append(ids, extra...)
		if !slices.Equal(ids, o.OrderedIDs) {
			o.OrderedIDs = ids
			p.structure(StructureKey{"ordering_scope", o.ID}, true)
		}
	}
	return nil
}
