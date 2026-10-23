CREATE TABLE events (
 id TEXT PRIMARY KEY NOT NULL,
 title TEXT NOT NULL CHECK(length(trim(title)) BETWEEN 1 AND 512),
 note_text TEXT NOT NULL DEFAULT '',
 all_day INTEGER NOT NULL CHECK(all_day IN (0,1)),
 start_on TEXT, end_on TEXT,
 start_at_utc TEXT, end_at_utc TEXT, time_zone TEXT,
 created_at_utc TEXT NOT NULL, updated_at_utc TEXT NOT NULL, deleted_at_utc TEXT,
 row_version INTEGER NOT NULL DEFAULT 1 CHECK(row_version>=1),
 CHECK((all_day=1 AND start_on IS NOT NULL AND end_on>start_on AND end_on IS NOT NULL AND start_at_utc IS NULL AND end_at_utc IS NULL AND time_zone IS NULL)
 OR (all_day=0 AND start_on IS NULL AND end_on IS NULL AND start_at_utc IS NOT NULL AND end_at_utc IS NOT NULL AND time_zone IS NOT NULL))
);
CREATE INDEX idx_events_dates ON events(start_on,end_on) WHERE deleted_at_utc IS NULL;
CREATE INDEX idx_events_instants ON events(start_at_utc,end_at_utc) WHERE deleted_at_utc IS NULL;
CREATE TABLE event_tasks (
 event_id TEXT NOT NULL REFERENCES events(id),
 task_id TEXT NOT NULL REFERENCES tasks(id),
 role TEXT NOT NULL CHECK(role IN ('preparation','followup')),
 PRIMARY KEY(event_id,task_id)
);
CREATE INDEX idx_event_tasks_task ON event_tasks(task_id);
