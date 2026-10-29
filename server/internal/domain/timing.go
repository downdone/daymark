package domain

import (
	"slices"
	"time"
	_ "time/tzdata"
)

func location(zone string) (*time.Location, error) {
	if zone == "" || zone == "Local" {
		return nil, Invalid("/time_zone")
	}
	l, e := time.LoadLocation(zone)
	if e != nil {
		return nil, Invalid("/time_zone")
	}
	return l, nil
}
func validateTiming(t *Task) error {
	if s := t.Schedule; s != nil {
		switch s.Kind {
		case "ALL_DAY":
			if s.StartOn == nil || s.EndOn == nil || s.StartUTC != nil || s.EndUTC != nil || s.TimeZone != nil {
				return Invalid("/schedule")
			}
			if _, e := s.StartOn.Time(); e != nil {
				return e
			}
			if _, e := s.EndOn.Time(); e != nil {
				return e
			}
			if *s.EndOn <= *s.StartOn {
				return Fail("date_order")
			}
			end, e := s.EndOn.AddDays(-1)
			if e != nil {
				return e
			}
			if !Equal(t.StartOn, s.StartOn) || t.DeadlineOn == nil || *t.DeadlineOn != end {
				return Invalid("/schedule")
			}
		case "TIMED":
			if s.StartUTC == nil || s.EndUTC == nil || s.TimeZone == nil || s.StartOn != nil || s.EndOn != nil {
				return Invalid("/schedule")
			}
			first, e := s.StartUTC.Time()
			if e != nil {
				return e
			}
			last, e := s.EndUTC.Time()
			if e != nil {
				return e
			}
			if !last.After(first) {
				return Fail("date_order")
			}
			loc, e := location(*s.TimeZone)
			if e != nil {
				return e
			}
			if t.StartOn == nil || t.DeadlineOn == nil || string(*t.StartOn) != first.In(loc).Format("2006-01-02") || string(*t.DeadlineOn) != last.In(loc).Format("2006-01-02") {
				return Invalid("/schedule")
			}
		default:
			return Invalid("/schedule/kind")
		}
	}
	if r := t.Reminder; r != nil {
		if _, e := location(r.TimeZone); e != nil {
			return e
		}
		if r.Every < 1 || r.Every > 36500 || !slices.Contains([]string{"NONE", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"}, r.Repeat) {
			return Invalid("/reminder")
		}
		switch r.Kind {
		case "ABSOLUTE":
			if r.AnchorUTC == nil || r.MinutesBefore != nil {
				return Invalid("/reminder")
			}
			if _, e := r.AnchorUTC.Time(); e != nil {
				return e
			}
		case "RELATIVE":
			if t.Schedule == nil || r.AnchorUTC != nil || r.MinutesBefore == nil || !slices.Contains([]int{0, 5, 15, 30, 60, 1440}, *r.MinutesBefore) || r.Repeat != "NONE" || r.Every != 1 {
				return Invalid("/reminder")
			}
		default:
			return Invalid("/reminder/kind")
		}
	}
	return nil
}

// LocalInstant implements Reject for missing AND ambiguous local times. It
// enumerates real tzdb transition intervals near the requested wall clock,
// rather than relying on time.Date's choice of one side of a DST fold.
func LocalInstant(day Date, wall, zone string) (Instant, error) {
	loc, e := location(zone)
	if e != nil {
		return "", e
	}
	naive, e := time.Parse("2006-01-02 15:04:05.000", string(day)+" "+wall)
	if e != nil || naive.Year() < 1 {
		return "", Invalid("/local_time")
	}
	offsets := map[int]bool{}
	start, end := naive.Add(-72*time.Hour), naive.Add(72*time.Hour)
	for probe, steps := start, 0; !probe.After(end) && steps < 32; steps++ {
		local := probe.In(loc)
		_, offset := local.Zone()
		offsets[offset] = true
		_, until := local.ZoneBounds()
		if until.IsZero() || until.After(end) {
			break
		}
		if !until.After(probe) {
			return "", Invalid("/local_time")
		}
		probe = until
	}
	found := []time.Time{}
	for offset := range offsets {
		candidate := naive.Add(-time.Duration(offset) * time.Second)
		if candidate.In(loc).Format("2006-01-02 15:04:05.000") == naive.Format("2006-01-02 15:04:05.000") {
			found = append(found, candidate)
		}
	}
	if len(found) != 1 {
		return "", Invalid("/local_time")
	}
	return At(found[0]), nil
}
