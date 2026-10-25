ALTER TABLE projects ADD COLUMN parent_id TEXT REFERENCES projects(id);
ALTER TABLE projects ADD COLUMN archived INTEGER NOT NULL DEFAULT 0 CHECK(archived IN (0,1));
INSERT INTO projects(id,title,description,planning_state,status,sort_order,row_version,created_at_utc,updated_at_utc,deleted_at_utc,archived)
SELECT id,name,'','ACTIVE','OPEN',sort_order,row_version,created_at_utc,updated_at_utc,deleted_at_utc,archived FROM areas;
UPDATE projects SET parent_id=area_id,area_id=NULL WHERE area_id IS NOT NULL;
UPDATE tasks SET project_id=direct_area_id,direct_area_id=NULL WHERE direct_area_id IS NOT NULL;
UPDATE areas SET deleted_at_utc=COALESCE(deleted_at_utc,updated_at_utc);
CREATE INDEX idx_category_parent ON projects(parent_id,sort_order);
