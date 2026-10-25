CREATE TABLE task_schedules (
 task_id TEXT PRIMARY KEY REFERENCES tasks(id),
 all_day INTEGER NOT NULL CHECK(all_day IN (0,1)),
 start_on TEXT,end_on TEXT,start_at_utc TEXT,end_at_utc TEXT,time_zone TEXT,
 alert_minutes INTEGER NOT NULL DEFAULT -1 CHECK(alert_minutes IN (-1,0,5,15,30,60,1440)),
 CHECK((all_day=1 AND start_on IS NOT NULL AND end_on>start_on AND start_at_utc IS NULL AND end_at_utc IS NULL AND time_zone IS NULL)
 OR (all_day=0 AND start_on IS NULL AND end_on IS NULL AND start_at_utc IS NOT NULL AND end_at_utc IS NOT NULL AND time_zone IS NOT NULL))
);
CREATE TABLE task_links(source_id TEXT NOT NULL REFERENCES tasks(id),target_id TEXT NOT NULL REFERENCES tasks(id),role TEXT NOT NULL CHECK(role IN ('preparation','followup')),PRIMARY KEY(source_id,target_id),CHECK(source_id<>target_id));
CREATE INDEX idx_task_links_target ON task_links(target_id);
ALTER TABLE recurrence_templates ADD COLUMN source_task_id TEXT REFERENCES tasks(id);
ALTER TABLE events ADD COLUMN task_migrated INTEGER NOT NULL DEFAULT 0 CHECK(task_migrated IN (0,1));
