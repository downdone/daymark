package storage

import (
	"context"
	"encoding/json"
	"sort"
	"strings"

	"daymark.local/server/internal/domain"
)

func decodeLegacy(r row) (*domain.Legacy, error) {
	if r["legacy_json"] == nil {
		return nil, nil
	}
	var l domain.Legacy
	if e := json.Unmarshal([]byte(r.s("legacy_json")), &l); e != nil {
		return nil, domain.Wrap("storage_corrupt", e)
	}
	return &l, nil
}
func (t *transaction) LoadGraph(ctx context.Context, s domain.Scope) (domain.Graph, error) {
	g := domain.EmptyGraph()
	if _, e := t.Dataset(ctx, s); e != nil {
		return g, e
	}
	for _, table := range []string{"categories", "tags", "recurrence_rules", "tasks", "ordering_scopes"} {
		rs, e := t.rows(ctx, "SELECT * FROM "+table+" WHERE dataset_id=?", s.DatasetID)
		if e != nil {
			return g, e
		}
		if len(rs) > 100000 {
			return g, domain.Fail("quota_exceeded")
		}
		for _, r := range rs {
			var value any
			var kind domain.Kind
			id := domain.ID(r.s("id"))
			rev := domain.Counter(r.n("revision"))
			switch table {
			case "categories":
				kind = domain.CategoryKind
				l, e := decodeLegacy(r)
				if e != nil {
					return g, e
				}
				value = &domain.Category{ID: id, ParentID: r.id("parent_id"), Title: r.s("title"), Description: r.s("description"), Color: r.s("color_key"), Archived: r.b("archived"), CreatedAt: r.instant("created_ms"), UpdatedAt: r.instant("updated_ms"), DeletedAt: r.instantPtr("deleted_ms"), Legacy: l}
			case "tags":
				kind = domain.TagKind
				value = &domain.Tag{ID: id, Title: r.s("title"), Color: r.s("color_key"), CreatedAt: r.instant("created_ms"), UpdatedAt: r.instant("updated_ms"), DeletedAt: r.instantPtr("deleted_ms")}
			case "recurrence_rules":
				kind = domain.RuleKind
				var defaults domain.TaskTemplate
				if e = json.Unmarshal([]byte(r.s("defaults_json")), &defaults); e != nil {
					return g, domain.Wrap("storage_corrupt", e)
				}
				value = &domain.Rule{ID: id, SourceTaskID: r.id("source_task_id"), Spec: domain.RuleSpec{Mode: r.s("mode"), Frequency: r.s("frequency"), Interval: int(r.n("interval_n")), AnchorOn: domain.Date(r.s("anchor_on")), EndOn: r.date("end_on"), WeekdaysMask: int(r.n("weekdays_mask")), MonthDay: int(r.n("month_day")), YearMonth: int(r.n("year_month")), TimeZone: r.s("time_zone"), Defaults: defaults}, Active: r.b("active"), CreatedAt: r.instant("created_ms"), UpdatedAt: r.instant("updated_ms"), DeletedAt: r.instantPtr("deleted_ms")}
			case "tasks":
				kind = domain.TaskKind
				l, e := decodeLegacy(r)
				if e != nil {
					return g, e
				}
				value = &domain.Task{ID: id, Title: r.s("title"), Notes: r.s("notes"), PlanningState: r.s("planning_state"), Status: r.s("status"), CategoryID: r.id("category_id"), ParentTaskID: r.id("parent_task_id"), StartOn: r.date("start_on"), DeadlineOn: r.date("deadline_on"), TodayFocus: r.b("today_focus"), Evening: r.b("evening"), Pinned: r.b("pinned"), Important: r.b("important"), CreatedAt: r.instant("created_ms"), UpdatedAt: r.instant("updated_ms"), CompletedAt: r.instantPtr("completed_ms"), CancelledAt: r.instantPtr("cancelled_ms"), DeletedAt: r.instantPtr("deleted_ms"), CompletionGroupID: r.id("completion_group_id"), DeletionGroupID: r.id("deletion_group_id"), RecurrenceID: r.id("recurrence_id"), OccurrenceOn: r.date("occurrence_on"), OccurrenceKey: r.text("occurrence_key"), InstanceOverridden: r.b("instance_overridden"), Legacy: l, TagIDs: []domain.ID{}, Links: []domain.Link{}}
			case "ordering_scopes":
				kind = domain.OrderingKind
				value = &domain.Ordering{ID: id, Kind: r.s("kind"), SubjectID: r.id("subject_id"), OrderedIDs: []domain.ID{}}
			}
			g.Entities[domain.Key{Type: kind, ID: id}] = domain.Entity{Type: kind, ID: id, Revision: rev, Operation: "replace", Value: value}
		}
	}
	for _, table := range []string{"task_schedules", "reminder_configs", "task_tags", "task_links", "order_entries", "structure_versions", "task_aliases", "recurrence_successors"} {
		q := "SELECT * FROM " + table + " WHERE dataset_id=?"
		if table == "order_entries" {
			q += " ORDER BY scope_id,position"
		}
		rs, e := t.rows(ctx, q, s.DatasetID)
		if e != nil {
			return g, e
		}
		for _, r := range rs {
			switch table {
			case "task_schedules":
				task, ok := g.Task(domain.ID(r.s("task_id")))
				if !ok {
					return g, domain.Fail("storage_corrupt")
				}
				sc := &domain.Schedule{}
				if r.b("all_day") {
					sc.Kind = "ALL_DAY"
					sc.StartOn = r.date("start_on")
					sc.EndOn = r.date("end_on")
				} else {
					sc.Kind = "TIMED"
					sc.StartUTC = r.instantPtr("start_ms")
					sc.EndUTC = r.instantPtr("end_ms")
					sc.TimeZone = r.text("time_zone")
				}
				task.Schedule = sc
			case "reminder_configs":
				task, ok := g.Task(domain.ID(r.s("task_id")))
				if !ok {
					return g, domain.Fail("storage_corrupt")
				}
				rem := &domain.Reminder{Kind: r.s("kind"), AnchorUTC: r.instantPtr("anchor_ms"), Repeat: r.s("repeat_kind"), Every: int(r.n("every_n")), TimeZone: r.s("time_zone")}
				if r["minutes_before"] != nil {
					rem.MinutesBefore = domain.Ptr(int(r.n("minutes_before")))
				}
				task.Reminder = rem
			case "task_tags":
				task, ok := g.Task(domain.ID(r.s("task_id")))
				if !ok {
					return g, domain.Fail("storage_corrupt")
				}
				task.TagIDs = append(task.TagIDs, domain.ID(r.s("tag_id")))
			case "task_links":
				task, ok := g.Task(domain.ID(r.s("source_id")))
				if !ok {
					return g, domain.Fail("storage_corrupt")
				}
				task.Links = append(task.Links, domain.Link{TargetID: domain.ID(r.s("target_id")), Role: r.s("role")})
			case "order_entries":
				en, ok := g.Entities[domain.Key{Type: domain.OrderingKind, ID: domain.ID(r.s("scope_id"))}]
				if !ok {
					return g, domain.Fail("storage_corrupt")
				}
				en.Value.(*domain.Ordering).OrderedIDs = append(en.Value.(*domain.Ordering).OrderedIDs, domain.ID(r.s("entity_id")))
			case "structure_versions":
				g.Structures[domain.StructureKey{Kind: r.s("kind"), ID: domain.ID(r.s("id"))}] = domain.Counter(r.n("revision"))
			case "task_aliases":
				g.Aliases = append(g.Aliases, domain.Alias{AliasID: domain.ID(r.s("alias_id")), CanonicalID: domain.ID(r.s("canonical_id"))})
			case "recurrence_successors":
				g.Edges = append(g.Edges, domain.Edge{RuleID: domain.ID(r.s("rule_id")), PredecessorID: domain.ID(r.s("predecessor_id")), SuccessorID: domain.ID(r.s("successor_id"))})
			}
		}
	}
	for _, e := range g.Entities {
		if task, ok := e.Value.(*domain.Task); ok {
			sort.Slice(task.TagIDs, func(i, j int) bool { return task.TagIDs[i] < task.TagIDs[j] })
			sort.Slice(task.Links, func(i, j int) bool { return task.Links[i].TargetID < task.Links[j].TargetID })
		}
	}
	return g, nil
}
func legacyJSON(l *domain.Legacy) any {
	if l == nil {
		return nil
	}
	b, _ := json.Marshal(l)
	return string(b)
}
func (t *transaction) writeEntity(ctx context.Context, s domain.Scope, d domain.Delta) error {
	e := d.After
	fields := map[string]any{"dataset_id": s.DatasetID, "id": e.ID, "revision": e.Revision}
	table := ""
	switch v := e.Value.(type) {
	case *domain.Task:
		table = "tasks"
		for k, val := range map[string]any{"title": v.Title, "notes": v.Notes, "planning_state": v.PlanningState, "status": v.Status, "category_id": nullableString(v.CategoryID), "parent_task_id": nullableString(v.ParentTaskID), "start_on": nullableString(v.StartOn), "deadline_on": nullableString(v.DeadlineOn), "today_focus": v.TodayFocus, "evening": v.Evening, "pinned": v.Pinned, "important": v.Important, "created_ms": milliseconds(v.CreatedAt), "updated_ms": milliseconds(v.UpdatedAt), "completed_ms": nullableTime(v.CompletedAt), "cancelled_ms": nullableTime(v.CancelledAt), "deleted_ms": nullableTime(v.DeletedAt), "completion_group_id": nullableString(v.CompletionGroupID), "deletion_group_id": nullableString(v.DeletionGroupID), "recurrence_id": nullableString(v.RecurrenceID), "occurrence_on": nullableString(v.OccurrenceOn), "occurrence_key": nullableString(v.OccurrenceKey), "instance_overridden": v.InstanceOverridden, "legacy_json": legacyJSON(v.Legacy)} {
			fields[k] = val
		}
	case *domain.Category:
		table = "categories"
		for k, val := range map[string]any{"parent_id": nullableString(v.ParentID), "title": v.Title, "description": v.Description, "color_key": v.Color, "archived": v.Archived, "created_ms": milliseconds(v.CreatedAt), "updated_ms": milliseconds(v.UpdatedAt), "deleted_ms": nullableTime(v.DeletedAt), "legacy_json": legacyJSON(v.Legacy)} {
			fields[k] = val
		}
	case *domain.Tag:
		table = "tags"
		for k, val := range map[string]any{"title": v.Title, "color_key": v.Color, "created_ms": milliseconds(v.CreatedAt), "updated_ms": milliseconds(v.UpdatedAt), "deleted_ms": nullableTime(v.DeletedAt)} {
			fields[k] = val
		}
	case *domain.Rule:
		table = "recurrence_rules"
		b, err := json.Marshal(v.Spec.Defaults)
		if err != nil {
			return err
		}
		for k, val := range map[string]any{"source_task_id": nullableString(v.SourceTaskID), "title": v.Spec.Defaults.Title, "notes": v.Spec.Defaults.Notes, "mode": v.Spec.Mode, "frequency": v.Spec.Frequency, "interval_n": v.Spec.Interval, "anchor_on": v.Spec.AnchorOn, "end_on": nullableString(v.Spec.EndOn), "weekdays_mask": v.Spec.WeekdaysMask, "month_day": v.Spec.MonthDay, "year_month": v.Spec.YearMonth, "time_zone": v.Spec.TimeZone, "defaults_json": string(b), "active": v.Active, "created_ms": milliseconds(v.CreatedAt), "updated_ms": milliseconds(v.UpdatedAt), "deleted_ms": nullableTime(v.DeletedAt)} {
			fields[k] = val
		}
	case *domain.Ordering:
		table = "ordering_scopes"
		fields["kind"] = v.Kind
		fields["subject_id"] = nullableString(v.SubjectID)
	default:
		return domain.Fail("internal_error")
	}
	if d.Before == nil {
		return t.upsertEntityInsert(ctx, table, fields)
	}
	names := []string{}
	for k := range fields {
		if k != "dataset_id" && k != "id" {
			names = append(names, k)
		}
	}
	sort.Strings(names)
	args := []any{}
	sets := []string{}
	for _, k := range names {
		sets = append(sets, k+"=?")
		args = append(args, fields[k])
	}
	args = append(args, s.DatasetID, e.ID, d.Before.Revision)
	r, err := t.tx.ExecContext(ctx, "UPDATE "+table+" SET "+strings.Join(sets, ",")+" WHERE dataset_id=? AND id=? AND revision=?", args...)
	if err != nil {
		return err
	}
	n, err := r.RowsAffected()
	if err != nil {
		return err
	}
	if n != 1 {
		return domain.ConflictError(e.Key(), d.Before.Revision, 0)
	}
	return nil
}
func (t *transaction) upsertEntityInsert(ctx context.Context, table string, fields map[string]any) error {
	names := []string{}
	for k := range fields {
		names = append(names, k)
	}
	sort.Strings(names)
	args := []any{}
	qs := []string{}
	for _, k := range names {
		args = append(args, fields[k])
		qs = append(qs, "?")
	}
	return t.exec(ctx, "INSERT INTO "+table+"("+strings.Join(names, ",")+") VALUES("+strings.Join(qs, ",")+")", args...)
}
func (t *transaction) components(ctx context.Context, s domain.Scope, e domain.Entity) error {
	switch v := e.Value.(type) {
	case *domain.Task:
		for _, table := range []string{"task_schedules", "reminder_configs", "task_tags"} {
			if err := t.exec(ctx, "DELETE FROM "+table+" WHERE dataset_id=? AND task_id=?", s.DatasetID, e.ID); err != nil {
				return err
			}
		}
		if err := t.exec(ctx, "DELETE FROM task_links WHERE dataset_id=? AND source_id=?", s.DatasetID, e.ID); err != nil {
			return err
		}
		if sc := v.Schedule; sc != nil {
			if err := t.exec(ctx, "INSERT INTO task_schedules VALUES(?,?,?,?,?,?,?,?)", s.DatasetID, e.ID, sc.Kind == "ALL_DAY", nullableString(sc.StartOn), nullableString(sc.EndOn), nullableTime(sc.StartUTC), nullableTime(sc.EndUTC), nullableString(sc.TimeZone)); err != nil {
				return err
			}
		}
		if r := v.Reminder; r != nil {
			if err := t.exec(ctx, "INSERT INTO reminder_configs VALUES(?,?,?,?,?,?,?,?)", s.DatasetID, e.ID, r.Kind, nullableTime(r.AnchorUTC), r.MinutesBefore, r.Repeat, r.Every, r.TimeZone); err != nil {
				return err
			}
		}
		for _, id := range v.TagIDs {
			if err := t.exec(ctx, "INSERT INTO task_tags VALUES(?,?,?)", s.DatasetID, e.ID, id); err != nil {
				return err
			}
		}
		for _, link := range v.Links {
			if err := t.exec(ctx, "INSERT INTO task_links VALUES(?,?,?,?)", s.DatasetID, e.ID, link.TargetID, link.Role); err != nil {
				return err
			}
		}
	case *domain.Ordering:
		if err := t.exec(ctx, "DELETE FROM order_entries WHERE dataset_id=? AND scope_id=?", s.DatasetID, e.ID); err != nil {
			return err
		}
		for i, id := range v.OrderedIDs {
			if err := t.exec(ctx, "INSERT INTO order_entries VALUES(?,?,?,?)", s.DatasetID, e.ID, id, i); err != nil {
				return err
			}
		}
	}
	return nil
}
func (t *transaction) Apply(ctx context.Context, s domain.Scope, p domain.Plan) error {
	if _, err := t.Dataset(ctx, s); err != nil {
		return err
	}
	for _, d := range p.Changes {
		if err := t.writeEntity(ctx, s, d); err != nil {
			return err
		}
	}
	for _, d := range p.Changes {
		if err := t.components(ctx, s, d.After); err != nil {
			return err
		}
	}
	for _, v := range p.Structures {
		if err := t.upsert(ctx, "structure_versions", map[string]any{"dataset_id": s.DatasetID, "kind": v.Kind, "id": v.ID, "revision": v.Revision}, []string{"dataset_id", "kind", "id"}); err != nil {
			return err
		}
	}
	for _, v := range p.Aliases {
		if err := t.exec(ctx, "INSERT INTO task_aliases VALUES(?,?,?) ON CONFLICT(dataset_id,alias_id) DO NOTHING", s.DatasetID, v.AliasID, v.CanonicalID); err != nil {
			return err
		}
	}
	for _, v := range p.Edges {
		if err := t.exec(ctx, "INSERT INTO recurrence_successors VALUES(?,?,?,?) ON CONFLICT(dataset_id,rule_id,predecessor_id) DO NOTHING", s.DatasetID, v.RuleID, v.PredecessorID, v.SuccessorID); err != nil {
			return err
		}
	}
	return nil
}
