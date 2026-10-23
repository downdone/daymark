ALTER TABLE events ADD COLUMN repeat_frequency TEXT NOT NULL DEFAULT 'NONE' CHECK(repeat_frequency IN ('NONE','DAILY','WEEKLY','MONTHLY','YEARLY'));
ALTER TABLE events ADD COLUMN repeat_interval INTEGER NOT NULL DEFAULT 1 CHECK(repeat_interval BETWEEN 1 AND 365);
ALTER TABLE events ADD COLUMN repeat_until TEXT;
ALTER TABLE events ADD COLUMN alert_minutes INTEGER NOT NULL DEFAULT -1 CHECK(alert_minutes IN (-1,0,5,15,30,60,1440));
CREATE TABLE event_reminder_history(id TEXT PRIMARY KEY,event_id TEXT NOT NULL REFERENCES events(id),occurrence_at_utc TEXT NOT NULL,lead_minutes INTEGER NOT NULL CHECK(lead_minutes IN (0,5,15,30,60,1440)),fired_at_utc TEXT NOT NULL,UNIQUE(event_id,occurrence_at_utc,lead_minutes));
CREATE INDEX event_reminder_fired ON event_reminder_history(fired_at_utc);
