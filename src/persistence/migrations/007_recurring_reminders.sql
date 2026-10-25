ALTER TABLE tasks ADD COLUMN reminder_repeat TEXT NOT NULL DEFAULT 'NONE' CHECK(reminder_repeat IN ('NONE','DAILY','WEEKLY','MONTHLY','YEARLY'));
ALTER TABLE tasks ADD COLUMN reminder_every INTEGER NOT NULL DEFAULT 1 CHECK(reminder_every BETWEEN 1 AND 36500);
ALTER TABLE tasks ADD COLUMN reminder_anchor_utc TEXT;
ALTER TABLE tasks ADD COLUMN reminder_zone TEXT NOT NULL DEFAULT 'UTC';
UPDATE tasks SET reminder_anchor_utc=remind_at_utc;
CREATE TABLE task_reminder_history(id TEXT PRIMARY KEY,task_id TEXT NOT NULL REFERENCES tasks(id),scheduled_at_utc TEXT NOT NULL,fired_at_utc TEXT NOT NULL,UNIQUE(task_id,scheduled_at_utc));
INSERT INTO task_reminder_history(id,task_id,scheduled_at_utc,fired_at_utc) SELECT id,id,remind_at_utc,reminder_fired_at_utc FROM tasks WHERE remind_at_utc IS NOT NULL AND reminder_fired_at_utc IS NOT NULL;
CREATE INDEX idx_task_reminder_history ON task_reminder_history(fired_at_utc);
