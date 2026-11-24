package domain

import (
	"crypto/sha256"
	"encoding/hex"
	"slices"
	"time"
)

const TaskCycleCommand = "task.cycle_replace"

type TaskCycle struct {
	Frequency    string            `json:"frequency"`
	Interval     int               `json:"interval_n"`
	AnchorOn     Date              `json:"anchor_on"`
	Weekdays     int               `json:"weekdays_mask"`
	MonthDay     int               `json:"month_day"`
	YearMonth    int               `json:"year_month"`
	UntilOn      *Date             `json:"until_on"`
	Zone         string            `json:"zone"`
	Active       bool              `json:"active"`
	CurrentOn    Date              `json:"current_on"`
	CurrentStart Instant           `json:"current_start_utc"`
	CurrentEnd   *Instant          `json:"current_end_utc"`
	NextOn       *Date             `json:"next_on"`
	NextStart    *Instant          `json:"next_start_utc"`
	RuleRevision Counter           `json:"rule_revision"`
	Completions  []CycleCompletion `json:"completions"`
}

type CycleCompletion struct {
	Key           string   `json:"occurrence_key"`
	RuleRevision  Counter  `json:"rule_revision"`
	Start         Instant  `json:"scheduled_start_utc"`
	End           *Instant `json:"scheduled_end_utc"`
	Outcome       string   `json:"outcome"`
	PerformedAt   Instant  `json:"performed_at_utc"`
	MutationID    ID       `json:"mutation_id"`
	GroupID       *ID      `json:"completion_group_id"`
	Members       []ID     `json:"members"`
	RevokedAt     *Instant `json:"revoked_at_utc"`
	PriorReminder *Instant `json:"prior_reminder_utc"`
}

func cycleOccurrence(revision Counter, start Instant) string {
	sum := sha256.Sum256([]byte(revision.String() + "/" + string(start)))
	return hex.EncodeToString(sum[:])
}

func (p *planner) replaceTaskCycle(c *ReplaceTaskCycle) error {
	t, err := p.task(c.TaskID)
	if err != nil {
		return err
	}
	if t.PurgedAt != nil || t.DeletedAt != nil {
		return Invalid("/task_id")
	}
	before := cloneTask(t)
	if t.RecurrenceID != nil {
		rule, err := p.rule(*t.RecurrenceID)
		if err != nil {
			return err
		}
		// Conversion is explicit. Keep the old rule and generated tasks, and
		// detach only the task selected in the arrangement editor.
		if rule.Active || t.Cycle != nil {
			return Invalid("/cycle")
		}
		p.structure(StructureKey{"recurrence_members", rule.ID}, true)
		t.RecurrenceID, t.OccurrenceOn, t.OccurrenceKey = nil, nil, nil
		t.InstanceOverridden = false
	}
	t.Cycle = &c.Cycle
	p.changedTask(t, before)
	return nil
}

func validCycleTimes(start Instant, end *Instant, other ...*Instant) bool {
	if _, err := start.Time(); err != nil {
		return false
	}
	if end != nil && *end <= start {
		return false
	}
	for _, v := range append(other, end) {
		if v != nil {
			if _, err := v.Time(); err != nil {
				return false
			}
		}
	}
	return true
}

func validateTaskCycle(t *Task, before Entity) error {
	c := t.Cycle
	old, _ := before.Value.(*Task)
	if c == nil {
		if old != nil && old.Cycle != nil && t.PurgedAt == nil {
			return Invalid("/cycle")
		}
		return nil
	}
	if t.RecurrenceID != nil || (c.Active && t.ParentTaskID != nil) ||
		(c.Active && t.Reminder != nil && t.Reminder.Repeat != "NONE") || c.RuleRevision < 1 ||
		c.Interval < 1 || c.Interval > 36500 || c.MonthDay < 1 || c.MonthDay > 31 ||
		c.YearMonth < 1 || c.YearMonth > 12 || c.Weekdays < 0 || c.Weekdays > 127 ||
		!slices.Contains([]string{"DAILY", "WEEKLY", "MONTHLY", "YEARLY", "AFTER_DAYS"}, c.Frequency) ||
		(c.Frequency == "WEEKLY") != (c.Weekdays != 0) || (c.NextOn == nil) != (c.NextStart == nil) ||
		!validCycleTimes(c.CurrentStart, c.CurrentEnd, c.NextStart) {
		return Invalid("/cycle")
	}
	loc, err := location(c.Zone)
	if err != nil {
		return Invalid("/cycle/zone")
	}
	for _, d := range []*Date{&c.AnchorOn, &c.CurrentOn, c.UntilOn, c.NextOn} {
		if d != nil {
			if _, err := d.Time(); err != nil {
				return Invalid("/cycle")
			}
		}
	}
	if c.CurrentOn < c.AnchorOn || (c.UntilOn != nil && c.CurrentOn > *c.UntilOn) ||
		dateAt(c.CurrentStart, loc) != c.CurrentOn {
		return Invalid("/cycle/current_on")
	}
	if c.NextStart != nil && (!c.Active || t.Status == "OPEN" || *c.NextOn <= c.CurrentOn ||
		*c.NextStart <= c.CurrentStart || dateAt(*c.NextStart, loc) != *c.NextOn ||
		(c.UntilOn != nil && *c.NextOn > *c.UntilOn)) {
		return Invalid("/cycle/next_start_utc")
	}
	if c.Completions == nil || len(c.Completions) > 100000 {
		return Invalid("/cycle/completions")
	}
	for i, h := range c.Completions {
		if h.RuleRevision < 1 || h.RuleRevision > c.RuleRevision || h.Key != cycleOccurrence(h.RuleRevision, h.Start) ||
			(i > 0 && h.Key <= c.Completions[i-1].Key) || !ValidID(string(h.MutationID), false) ||
			!slices.Contains([]string{"COMPLETED", "SKIPPED"}, h.Outcome) || h.Members == nil ||
			!validCycleTimes(h.Start, h.End, &h.PerformedAt, h.RevokedAt, h.PriorReminder) ||
			(h.RevokedAt != nil && *h.RevokedAt < h.PerformedAt) ||
			(h.GroupID != nil && !ValidID(string(*h.GroupID), false)) {
			return Invalid("/cycle/completions")
		}
		for j, id := range h.Members {
			if !ValidID(string(id), false) || id == t.ID || (j > 0 && id <= h.Members[j-1]) {
				return Invalid("/cycle/completions/members")
			}
		}
	}
	currentKey := cycleOccurrence(c.RuleRevision, c.CurrentStart)
	if c.Active && t.Status == "COMPLETED" {
		i, ok := slices.BinarySearchFunc(c.Completions, currentKey, compareCycleCompletion)
		if !ok || c.Completions[i].RevokedAt != nil || !Equal(c.Completions[i].GroupID, t.CompletionGroupID) {
			return Invalid("/cycle/completions")
		}
	}
	if old != nil && old.Cycle != nil {
		return validateCycleTransition(old.Cycle, c, currentKey)
	}
	return nil
}

func dateAt(v Instant, loc *time.Location) Date {
	t, _ := v.Time()
	return Date(t.In(loc).Format("2006-01-02"))
}

func compareCycleCompletion(h CycleCompletion, key string) int {
	if h.Key < key {
		return -1
	}
	if h.Key == key {
		return 0
	}
	return 1
}

func validateCycleTransition(old, next *TaskCycle, currentKey string) error {
	priorKey := cycleOccurrence(old.RuleRevision, old.CurrentStart)
	if next.RuleRevision < old.RuleRevision || next.RuleRevision > old.RuleRevision+1 ||
		(next.RuleRevision == old.RuleRevision && next.CurrentStart != old.CurrentStart &&
			(old.NextStart == nil || next.CurrentStart != *old.NextStart)) {
		return Invalid("/cycle/rule_revision")
	}
	if next.RuleRevision == old.RuleRevision {
		priorRule, nextRule := *old, *next
		priorRule.Active = nextRule.Active
		priorRule.CurrentOn, priorRule.CurrentStart, priorRule.CurrentEnd = nextRule.CurrentOn, nextRule.CurrentStart, nextRule.CurrentEnd
		priorRule.NextOn, priorRule.NextStart, priorRule.Completions = nextRule.NextOn, nextRule.NextStart, nextRule.Completions
		if (!old.Active && next.Active) || !Equal(priorRule, nextRule) {
			return Invalid("/cycle/rule_revision")
		}
	}
	for _, h := range next.Completions {
		if _, existed := slices.BinarySearchFunc(old.Completions, h.Key, compareCycleCompletion); !existed && h.Key != currentKey {
			return Invalid("/cycle/completions")
		}
	}
	for _, prior := range old.Completions {
		i, ok := slices.BinarySearchFunc(next.Completions, prior.Key, compareCycleCompletion)
		if !ok {
			return Invalid("/cycle/completions")
		}
		h := next.Completions[i]
		if Equal(prior, h) {
			continue
		}
		if prior.Key != currentKey || prior.Key != priorKey {
			return Invalid("/cycle/completions")
		}
		if prior.RevokedAt == nil {
			revoke := h.RevokedAt
			h.RevokedAt = nil
			if revoke == nil || !Equal(prior, h) {
				return Invalid("/cycle/completions")
			}
		} else if h.RevokedAt != nil {
			return Invalid("/cycle/completions")
		}
	}
	return nil
}

func validateCycleMutationIDs(graph Graph) error {
	seen := map[ID]bool{}
	for _, entity := range graph.Entities {
		task, ok := entity.Value.(*Task)
		if !ok || task.Cycle == nil {
			continue
		}
		for _, entry := range task.Cycle.Completions {
			if seen[entry.MutationID] {
				return Invalid("/cycle/completions/mutation_id")
			}
			seen[entry.MutationID] = true
		}
	}
	return nil
}
