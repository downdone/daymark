package domain

import (
	"fmt"
	"slices"
	"strings"
	"time"
)

func (p *planner) validateRule(r *Rule) error {
	s := r.Spec
	if _, e := s.AnchorOn.Time(); e != nil {
		return e
	}
	if s.EndOn != nil {
		if _, e := s.EndOn.Time(); e != nil {
			return e
		}
		if *s.EndOn < s.AnchorOn {
			return Fail("date_order")
		}
	}
	if _, e := location(s.TimeZone); e != nil {
		return e
	}
	if s.Interval < 1 || s.Interval > 36500 || s.MonthDay < 1 || s.MonthDay > 31 || s.YearMonth < 1 || s.YearMonth > 12 || s.WeekdaysMask < 0 || s.WeekdaysMask > 127 {
		return Fail("recurrence_invalid")
	}
	if (s.Mode == "AFTER_COMPLETION") != (s.Frequency == "AFTER_DAYS") || !slices.Contains([]string{"FIXED", "AFTER_COMPLETION"}, s.Mode) || !slices.Contains([]string{"DAILY", "WEEKLY", "MONTHLY", "YEARLY", "AFTER_DAYS"}, s.Frequency) {
		return Fail("recurrence_invalid")
	}
	if s.Frequency == "WEEKLY" && s.WeekdaysMask == 0 || s.Frequency != "WEEKLY" && s.WeekdaysMask != 0 {
		return Fail("recurrence_invalid")
	}
	anchor, _ := s.AnchorOn.Time()
	if s.Frequency != "MONTHLY" && s.Frequency != "YEARLY" && s.MonthDay != anchor.Day() {
		return Fail("recurrence_invalid")
	}
	if s.Frequency != "YEARLY" && s.YearMonth != int(anchor.Month()) {
		return Fail("recurrence_invalid")
	}
	t := s.Defaults
	if t.CategoryID != nil {
		if _, e := p.category(*t.CategoryID); e != nil {
			return e
		}
	}
	if t.ParentTaskID != nil {
		if _, e := p.task(*t.ParentTaskID); e != nil {
			return e
		}
	}
	seenTags := map[ID]bool{}
	for _, id := range t.TagIDs {
		if seenTags[id] {
			return Fail("recurrence_invalid")
		}
		seenTags[id] = true
		if _, e := p.entity(Key{TagKind, id}); e != nil {
			return e
		}
	}
	seenLinks := map[ID]bool{}
	for _, link := range t.Links {
		if seenLinks[link.TargetID] || !slices.Contains([]string{"preparation", "followup"}, link.Role) {
			return Fail("recurrence_invalid")
		}
		seenLinks[link.TargetID] = true
		if _, e := p.task(link.TargetID); e != nil {
			return e
		}
	}
	if t.Version != 1 {
		return Fail("protocol_upgrade_required")
	}
	if e := CheckTitle(t.Title); e != nil {
		return e
	}
	if !slices.Contains([]string{"INBOX", "ACTIVE", "SOMEDAY"}, t.PlanningState) {
		return Fail("recurrence_invalid")
	}
	for _, n := range []*int{t.StartDayOffset, t.DeadlineDayOffset} {
		if n != nil && (*n < -36500 || *n > 36500) {
			return Fail("recurrence_invalid")
		}
	}
	if t.Schedule != nil {
		sc := t.Schedule
		if sc.StartDayOffset < -36500 || sc.StartDayOffset > 36500 || sc.EndDayOffset < -36500 || sc.EndDayOffset > 36500 {
			return Fail("recurrence_invalid")
		}
		if sc.Kind == "ALL_DAY" {
			if sc.EndDayOffset <= sc.StartDayOffset || sc.StartLocalTime != nil || sc.EndLocalTime != nil || sc.TimeZone != nil {
				return Fail("recurrence_invalid")
			}
		} else if sc.Kind == "TIMED" {
			if sc.StartLocalTime == nil || sc.EndLocalTime == nil || sc.TimeZone == nil {
				return Fail("recurrence_invalid")
			}
			if _, e := location(*sc.TimeZone); e != nil {
				return e
			}
			for _, v := range []*string{sc.StartLocalTime, sc.EndLocalTime} {
				if _, e := time.Parse("15:04:05.000", *v); e != nil {
					return Fail("recurrence_invalid")
				}
			}
			if sc.EndDayOffset < sc.StartDayOffset || sc.EndDayOffset == sc.StartDayOffset && *sc.EndLocalTime <= *sc.StartLocalTime {
				return Fail("recurrence_invalid")
			}
		} else {
			return Fail("recurrence_invalid")
		}
	}
	if t.Reminder != nil {
		rem := t.Reminder
		if _, e := location(rem.TimeZone); e != nil {
			return e
		}
		if rem.Kind == "RELATIVE" {
			if t.Schedule == nil || rem.MinutesBefore == nil || !slices.Contains([]int{0, 5, 15, 30, 60, 1440}, *rem.MinutesBefore) || rem.Repeat != "NONE" || rem.Every != 1 || rem.DayOffset != nil || rem.LocalTime != nil {
				return Fail("recurrence_invalid")
			}
		} else if rem.Kind == "ABSOLUTE" {
			if rem.DayOffset == nil || rem.LocalTime == nil || rem.MinutesBefore != nil || *rem.DayOffset < -36500 || *rem.DayOffset > 36500 || rem.Every < 1 || rem.Every > 36500 || !slices.Contains([]string{"NONE", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"}, rem.Repeat) {
				return Fail("recurrence_invalid")
			}
			if _, e := time.Parse("15:04:05.000", *rem.LocalTime); e != nil {
				return e
			}
		} else {
			return Fail("recurrence_invalid")
		}
	}
	if r.SourceTaskID != nil {
		if _, e := p.task(*r.SourceTaskID); e != nil {
			return e
		}
	}
	return nil
}
func (p *planner) replaceRule(c *ReplaceRecurrence) error {
	source, e := p.task(c.SourceTaskID)
	if e != nil {
		return e
	}
	if source.DeletedAt != nil {
		return Invalid("/source_task_id")
	}
	if _, e = c.CutoffOn.Time(); e != nil {
		return e
	}
	// A conflict decision may restore the same immutable generation. A changed
	// specification still requires a new identity; source ownership never moves.
	if existing, ok := p.graph.Entities[Key{RuleKind, c.NewRuleID}]; ok {
		p.record(existing.Key())
		rule := existing.Value.(*Rule)
		if !Equal(rule.SourceTaskID, Ptr(source.ID)) || !Equal(rule.Spec, c.Rule) {
			return Fail("recurrence_invalid")
		}
		if rule.Active && source.RecurrenceID != nil && *source.RecurrenceID == rule.ID {
			return nil
		}
		if source.RecurrenceID != nil && *source.RecurrenceID != rule.ID {
			if e = p.stopRule(*source.RecurrenceID, true, c.CutoffOn, source.ID); e != nil {
				return e
			}
		}
		rule.Active = true
		rule.DeletedAt = nil
		rule.UpdatedAt = p.event
		source.RecurrenceID = Ptr(rule.ID)
		source.OccurrenceOn = Ptr(rule.Spec.AnchorOn)
		key := "D:" + string(rule.Spec.AnchorOn)
		source.OccurrenceKey = Ptr(key)
		source.InstanceOverridden = false
		source.UpdatedAt = p.event
		p.structure(StructureKey{"recurrence_members", rule.ID}, true)
		return nil
	}
	if source.RecurrenceID != nil {
		if e = p.stopRule(*source.RecurrenceID, true, c.CutoffOn, source.ID); e != nil {
			return e
		}
	}
	r := &Rule{ID: c.NewRuleID, SourceTaskID: Ptr(source.ID), Spec: c.Rule, Active: true, CreatedAt: p.event, UpdatedAt: p.event}
	if e = p.newEntity(Entity{Type: RuleKind, ID: r.ID, Value: r}); e != nil {
		return e
	}
	source.RecurrenceID = Ptr(r.ID)
	source.OccurrenceOn = Ptr(r.Spec.AnchorOn)
	key := "D:" + string(r.Spec.AnchorOn)
	source.OccurrenceKey = Ptr(key)
	source.InstanceOverridden = false
	source.UpdatedAt = p.event
	p.structure(StructureKey{"recurrence_members", r.ID}, true)
	return nil
}
func (p *planner) stopRule(id ID, remove bool, cutoff Date, exclude ID) error {
	if _, e := cutoff.Time(); e != nil {
		return e
	}
	r, e := p.rule(id)
	if e != nil {
		return e
	}
	p.structure(StructureKey{"recurrence_members", id}, false)
	if r.Active {
		r.Active = false
		r.UpdatedAt = p.event
		p.structure(StructureKey{"recurrence_members", id}, true)
	}
	if remove {
		for _, en := range p.graph.Entities {
			t, ok := en.Value.(*Task)
			if !ok || t.RecurrenceID == nil || *t.RecurrenceID != id || t.ID == exclude {
				continue
			}
			p.record(en.Key())
			if t.OccurrenceOn != nil && *t.OccurrenceOn >= cutoff && t.Status == "OPEN" && t.DeletedAt == nil && !t.InstanceOverridden {
				t.DeletedAt = Ptr(p.event)
				t.DeletionGroupID = nil
				t.UpdatedAt = p.event
				p.structure(StructureKey{"recurrence_members", id}, true)
				p.structure(StructKey("category_members", t.CategoryID), true)
				root, e := p.root(t.ID)
				if e != nil {
					return e
				}
				p.structure(StructureKey{"task_tree", root}, true)
			}
		}
	}
	return nil
}
func (p *planner) materialize(c *MaterializeRecurrence) error {
	from, e := c.FromOn.Time()
	if e != nil {
		return e
	}
	through, e := c.ThroughOn.Time()
	if e != nil {
		return e
	}
	if through.Before(from) || through.Sub(from) > 365*24*time.Hour {
		return Fail("recurrence_invalid")
	}
	r, e := p.rule(c.RuleID)
	if e != nil {
		return e
	}
	if e = p.validateRule(r); e != nil {
		return e
	}
	p.structure(StructureKey{"recurrence_members", r.ID}, false)
	if !r.Active || r.Spec.Mode != "FIXED" {
		return nil
	}
	count := 0
	for date := from; !date.After(through); date = date.AddDate(0, 0, 1) {
		on := Date(date.Format("2006-01-02"))
		if !ruleMatches(r.Spec, on) {
			continue
		}
		count++
		if count > 512 {
			return Fail("payload_too_large")
		}
		key := "D:" + string(on)
		if e = p.generate(r, on, key); e != nil {
			return e
		}
	}
	return nil
}
func ruleMatches(s RuleSpec, on Date) bool {
	if on < s.AnchorOn || s.EndOn != nil && on > *s.EndOn {
		return false
	}
	day, e := on.Time()
	if e != nil {
		return false
	}
	anchor, e := s.AnchorOn.Time()
	if e != nil {
		return false
	}
	switch s.Frequency {
	case "DAILY":
		return ((day.Unix()-anchor.Unix())/86400)%int64(s.Interval) == 0
	case "WEEKLY":
		aMonday := anchor.AddDate(0, 0, -(int(anchor.Weekday())+6)%7)
		dMonday := day.AddDate(0, 0, -(int(day.Weekday())+6)%7)
		weeks := (dMonday.Unix() - aMonday.Unix()) / 604800
		return weeks%int64(s.Interval) == 0 && (s.WeekdaysMask&(1<<((int(day.Weekday())+6)%7))) != 0
	case "MONTHLY":
		months := (day.Year()-anchor.Year())*12 + int(day.Month()-anchor.Month())
		return months%s.Interval == 0 && day.Day() == s.MonthDay
	case "YEARLY":
		return (day.Year()-anchor.Year())%s.Interval == 0 && int(day.Month()) == s.YearMonth && day.Day() == s.MonthDay
	}
	return false
}
func (p *planner) generate(r *Rule, on Date, key string) error {
	for _, en := range p.graph.Entities {
		if task, ok := en.Value.(*Task); ok && task.RecurrenceID != nil && *task.RecurrenceID == r.ID && task.OccurrenceKey != nil && *task.OccurrenceKey == key {
			p.record(en.Key())
			return nil
		}
	}
	id := StableID(string(r.ID) + ":" + key)
	if key == "D:"+string(r.Spec.AnchorOn) && r.SourceTaskID != nil {
		id = *r.SourceTaskID
	}
	seed := r.Spec.Defaults
	t := &Task{ID: id, Title: strings.TrimSpace(seed.Title), Notes: seed.Notes, PlanningState: seed.PlanningState, Status: "OPEN", CategoryID: seed.CategoryID, ParentTaskID: seed.ParentTaskID, TodayFocus: seed.TodayFocus, Evening: seed.Evening, Pinned: seed.Pinned, Important: seed.Important, CreatedAt: p.event, UpdatedAt: p.event, RecurrenceID: Ptr(r.ID), OccurrenceOn: Ptr(on), OccurrenceKey: Ptr(key), TagIDs: []ID{}, Links: append([]Link{}, seed.Links...)}
	if t.ParentTaskID != nil {
		parent, e := p.task(*t.ParentTaskID)
		if e != nil {
			return e
		}
		if parent.DeletedAt != nil || parent.Status != "OPEN" {
			return nil
		}
		t.CategoryID = parent.CategoryID
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
	for _, id := range seed.TagIDs {
		en, e := p.entity(Key{TagKind, id})
		if e != nil {
			return e
		}
		if en.Value.(*Tag).DeletedAt == nil {
			t.TagIDs = append(t.TagIDs, id)
		}
	}
	slices.Sort(t.TagIDs)
	if seed.StartDayOffset != nil {
		d, e := on.AddDays(*seed.StartDayOffset)
		if e != nil {
			return e
		}
		t.StartOn = Ptr(d)
	}
	if seed.DeadlineDayOffset != nil {
		d, e := on.AddDays(*seed.DeadlineDayOffset)
		if e != nil {
			return e
		}
		t.DeadlineOn = Ptr(d)
	}
	if s := seed.Schedule; s != nil {
		first, e := on.AddDays(s.StartDayOffset)
		if e != nil {
			return e
		}
		last, e := on.AddDays(s.EndDayOffset)
		if e != nil {
			return e
		}
		if s.Kind == "ALL_DAY" {
			t.Schedule = &Schedule{Kind: "ALL_DAY", StartOn: Ptr(first), EndOn: Ptr(last)}
			end, e := last.AddDays(-1)
			if e != nil {
				return e
			}
			t.StartOn = Ptr(first)
			t.DeadlineOn = Ptr(end)
		} else {
			start, e := LocalInstant(first, *s.StartLocalTime, *s.TimeZone)
			if e != nil {
				return nil
			}
			end, e := LocalInstant(last, *s.EndLocalTime, *s.TimeZone)
			if e != nil {
				return nil
			}
			t.Schedule = &Schedule{Kind: "TIMED", StartUTC: Ptr(start), EndUTC: Ptr(end), TimeZone: s.TimeZone}
			t.StartOn = Ptr(first)
			t.DeadlineOn = Ptr(last)
		}
	}
	if r := seed.Reminder; r != nil {
		rem := &Reminder{Kind: r.Kind, MinutesBefore: r.MinutesBefore, Repeat: r.Repeat, Every: r.Every, TimeZone: r.TimeZone}
		if r.Kind == "ABSOLUTE" {
			d, e := on.AddDays(*r.DayOffset)
			if e != nil {
				return e
			}
			anchor, e := LocalInstant(d, *r.LocalTime, r.TimeZone)
			if e != nil {
				return nil
			}
			rem.AnchorUTC = Ptr(anchor)
		}
		t.Reminder = rem
	}
	if t.PlanningState == "INBOX" && (t.StartOn != nil || t.DeadlineOn != nil || t.Schedule != nil || t.Reminder != nil) {
		t.PlanningState = "ACTIVE"
	}
	if e := p.newEntity(Entity{Type: TaskKind, ID: id, Value: t}); e != nil {
		return e
	}
	p.structure(StructureKey{"recurrence_members", r.ID}, true)
	p.structure(StructKey("category_members", t.CategoryID), true)
	root, e := p.root(id)
	if e != nil {
		return e
	}
	p.structure(StructureKey{"task_tree", root}, true)
	for _, tag := range t.TagIDs {
		p.structure(StructureKey{"tag_members", tag}, true)
	}
	return nil
}
func (p *planner) successor(t *Task) error {
	r, e := p.rule(*t.RecurrenceID)
	if e != nil {
		return e
	}
	if !r.Active || r.Spec.Mode != "AFTER_COMPLETION" {
		return nil
	}
	if e = p.validateRule(r); e != nil {
		return e
	}
	p.structure(StructureKey{"recurrence_members", r.ID}, false)
	for _, edge := range p.graph.Edges {
		if edge.RuleID == r.ID && edge.PredecessorID == t.ID {
			return nil
		}
	}
	loc, e := location(r.Spec.TimeZone)
	if e != nil {
		return e
	}
	completed, e := t.CompletedAt.Time()
	if e != nil {
		return e
	}
	day := Date(completed.In(loc).Format("2006-01-02"))
	next, e := day.AddDays(r.Spec.Interval)
	if e != nil {
		return e
	}
	if r.Spec.EndOn != nil && next > *r.Spec.EndOn {
		return nil
	}
	key := fmt.Sprintf("A:%s", t.ID)
	id := StableID(string(r.ID) + ":" + key)
	if e = p.generate(r, next, key); e != nil {
		return e
	}
	if _, ok := p.graph.Task(id); ok {
		p.graph.Edges = append(p.graph.Edges, Edge{RuleID: r.ID, PredecessorID: t.ID, SuccessorID: id})
	}
	return nil
}
