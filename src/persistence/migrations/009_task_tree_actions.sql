ALTER TABLE tasks ADD COLUMN deletion_group_id TEXT;
ALTER TABLE tasks ADD COLUMN completion_group_id TEXT;
CREATE INDEX idx_tasks_deletion_group ON tasks(deletion_group_id);
CREATE INDEX idx_tasks_completion_group ON tasks(completion_group_id);
