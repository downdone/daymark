ALTER TABLE tasks ADD COLUMN parent_task_id TEXT REFERENCES tasks(id);
ALTER TABLE tasks ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0 CHECK(pinned IN (0,1));
ALTER TABLE tasks ADD COLUMN important INTEGER NOT NULL DEFAULT 0 CHECK(important IN (0,1));
INSERT INTO tasks(id,title,note_text,project_id,direct_area_id,planning_state,status,parent_task_id,sort_order,row_version,created_at_utc,updated_at_utc,completed_at_utc,deleted_at_utc)
SELECT c.id,c.title,'',t.project_id,t.direct_area_id,t.planning_state,CASE WHEN c.is_done=1 THEN 'COMPLETED' ELSE 'OPEN' END,c.task_id,c.sort_order,c.row_version,c.created_at_utc,c.updated_at_utc,CASE WHEN c.is_done=1 THEN c.updated_at_utc ELSE NULL END,c.deleted_at_utc FROM checklist_items c JOIN tasks t ON t.id=c.task_id;
UPDATE checklist_items SET deleted_at_utc=COALESCE(deleted_at_utc,updated_at_utc);
CREATE INDEX idx_task_parent ON tasks(parent_task_id,sort_order);
CREATE TABLE task_edit_history(id TEXT PRIMARY KEY,task_id TEXT NOT NULL REFERENCES tasks(id),edited_at_utc TEXT NOT NULL,fields_json TEXT NOT NULL);
CREATE INDEX idx_task_edits ON task_edit_history(task_id,edited_at_utc);
