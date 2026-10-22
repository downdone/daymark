CREATE TABLE IF NOT EXISTS areas (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    color_key TEXT NOT NULL DEFAULT 'neutral',
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    area_id TEXT REFERENCES areas(id),
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    description TEXT NOT NULL DEFAULT '',
    planning_state TEXT NOT NULL DEFAULT 'ACTIVE'
        CHECK(planning_state IN ('ACTIVE','SOMEDAY')),
    status TEXT NOT NULL DEFAULT 'OPEN'
        CHECK(status IN ('OPEN','COMPLETED','CANCELLED')),
    start_on TEXT,
    deadline_on TEXT,
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    completed_at_utc TEXT,
    deleted_at_utc TEXT,
    CHECK(start_on IS NULL OR deadline_on IS NULL OR start_on <= deadline_on)
);

CREATE TABLE IF NOT EXISTS headings (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id),
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS recurrence_templates (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    note_text TEXT NOT NULL DEFAULT '',
    task_defaults_json TEXT NOT NULL DEFAULT '{}',
    mode TEXT NOT NULL CHECK(mode IN ('FIXED','AFTER_COMPLETION')),
    frequency TEXT NOT NULL CHECK(frequency IN ('DAILY','WEEKLY','MONTHLY','YEARLY','AFTER_DAYS')),
    interval_n INTEGER NOT NULL DEFAULT 1 CHECK(interval_n >= 1),
    anchor_on TEXT NOT NULL,
    weekdays_mask INTEGER NOT NULL DEFAULT 0 CHECK(weekdays_mask BETWEEN 0 AND 127),
    month_day INTEGER CHECK(month_day BETWEEN 1 AND 31),
    year_month INTEGER CHECK(year_month BETWEEN 1 AND 12),
    end_on TEXT,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    last_materialized_on TEXT,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT,
    CHECK(end_on IS NULL OR anchor_on <= end_on)
);

CREATE TABLE IF NOT EXISTS tasks (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    note_text TEXT NOT NULL DEFAULT '',
    project_id TEXT REFERENCES projects(id),
    direct_area_id TEXT REFERENCES areas(id),
    heading_id TEXT REFERENCES headings(id),
    planning_state TEXT NOT NULL DEFAULT 'INBOX'
        CHECK(planning_state IN ('INBOX','ACTIVE','SOMEDAY')),
    status TEXT NOT NULL DEFAULT 'OPEN'
        CHECK(status IN ('OPEN','COMPLETED','CANCELLED')),
    today_focus INTEGER NOT NULL DEFAULT 0 CHECK(today_focus IN (0,1)),
    evening INTEGER NOT NULL DEFAULT 0 CHECK(evening IN (0,1)),
    start_on TEXT,
    deadline_on TEXT,
    remind_at_utc TEXT,
    reminder_fired_at_utc TEXT,
    sort_order INTEGER NOT NULL DEFAULT 0,
    recurrence_template_id TEXT REFERENCES recurrence_templates(id),
    occurrence_on TEXT,
    instance_overridden INTEGER NOT NULL DEFAULT 0
        CHECK(instance_overridden IN (0,1)),
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    completed_at_utc TEXT,
    cancelled_at_utc TEXT,
    deleted_at_utc TEXT,
    CHECK(NOT (project_id IS NOT NULL AND direct_area_id IS NOT NULL)),
    CHECK(heading_id IS NULL OR project_id IS NOT NULL),
    CHECK(start_on IS NULL OR deadline_on IS NULL OR start_on <= deadline_on),
    CHECK((recurrence_template_id IS NULL AND occurrence_on IS NULL)
        OR (recurrence_template_id IS NOT NULL AND occurrence_on IS NOT NULL)),
    UNIQUE(recurrence_template_id, occurrence_on)
);

CREATE TABLE IF NOT EXISTS checklist_items (
    id TEXT PRIMARY KEY,
    task_id TEXT NOT NULL REFERENCES tasks(id),
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    is_done INTEGER NOT NULL DEFAULT 0 CHECK(is_done IN (0,1)),
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS tags (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    normalized_name TEXT NOT NULL UNIQUE,
    color_key TEXT NOT NULL DEFAULT 'neutral',
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS task_tags (
    task_id TEXT NOT NULL REFERENCES tasks(id),
    tag_id TEXT NOT NULL REFERENCES tags(id),
    PRIMARY KEY(task_id, tag_id)
);

CREATE TABLE IF NOT EXISTS task_view_order (
    view_code TEXT NOT NULL,
    task_id TEXT NOT NULL REFERENCES tasks(id),
    sort_order INTEGER NOT NULL,
    PRIMARY KEY(view_code, task_id)
);

CREATE INDEX IF NOT EXISTS idx_tasks_planning ON tasks(planning_state, status, start_on);
CREATE INDEX IF NOT EXISTS idx_tasks_deadline ON tasks(deadline_on, status);
CREATE INDEX IF NOT EXISTS idx_tasks_project ON tasks(project_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_tasks_area ON tasks(direct_area_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_tasks_heading ON tasks(heading_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_tasks_updated ON tasks(updated_at_utc);
CREATE INDEX IF NOT EXISTS idx_tasks_reminder ON tasks(remind_at_utc, reminder_fired_at_utc);
CREATE INDEX IF NOT EXISTS idx_tasktags_tag ON task_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_checklist_task ON checklist_items(task_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_project_area ON projects(area_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_heading_project ON headings(project_id, sort_order);
