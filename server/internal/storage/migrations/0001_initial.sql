-- DESIGN ARTIFACT ONLY. Not a delivered migration; never run on a user database.
-- Runtime must enable foreign_keys/WAL/FULL on the appropriate connections.
-- Instants: UTC milliseconds. Calendar dates: validated ISO TEXT.
CREATE TABLE schema_migrations (
 version INTEGER PRIMARY KEY, checksum TEXT NOT NULL, applied_ms INTEGER NOT NULL
) STRICT;
CREATE TABLE service_state (
 singleton INTEGER PRIMARY KEY CHECK(singleton=1), server_instance_id TEXT NOT NULL,
 auth_epoch TEXT NOT NULL, schema_version INTEGER NOT NULL CHECK(schema_version>=1)
) STRICT;
CREATE TABLE users (
 id TEXT PRIMARY KEY NOT NULL, login_normalized TEXT NOT NULL UNIQUE,
 password_hash TEXT NOT NULL, credential_version INTEGER NOT NULL CHECK(credential_version>=1),
 status TEXT NOT NULL CHECK(status IN ('ACTIVE','DISABLED','DELETING','DELETED')),
 created_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL
) STRICT;
CREATE TABLE sessions (
 id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL REFERENCES users(id),
 installation_id TEXT NOT NULL, device_label TEXT NOT NULL,
 platform TEXT NOT NULL CHECK(platform IN ('windows','macos')), auth_epoch TEXT NOT NULL,
 credential_version INTEGER NOT NULL, generation INTEGER NOT NULL CHECK(generation>=1),
 access_hash BLOB NOT NULL UNIQUE CHECK(length(access_hash)=32),
 refresh_hash BLOB NOT NULL UNIQUE CHECK(length(refresh_hash)=32),
 created_ms INTEGER NOT NULL, access_expires_ms INTEGER NOT NULL,
 refresh_expires_ms INTEGER NOT NULL, revoked_ms INTEGER,
 CHECK(access_expires_ms>created_ms AND refresh_expires_ms>created_ms)
) STRICT;
CREATE INDEX sessions_user ON sessions(user_id,revoked_ms,refresh_expires_ms);
CREATE TABLE refresh_history (
 token_hash BLOB PRIMARY KEY NOT NULL CHECK(length(token_hash)=32),
 session_id TEXT NOT NULL REFERENCES sessions(id), retired_ms INTEGER NOT NULL,
 expires_ms INTEGER NOT NULL
) STRICT;
CREATE INDEX refresh_history_expiry ON refresh_history(expires_ms);
CREATE TABLE datasets (
 id TEXT PRIMARY KEY NOT NULL, owner_id TEXT NOT NULL REFERENCES users(id),
 data_epoch TEXT NOT NULL, head_seq INTEGER NOT NULL DEFAULT 0 CHECK(head_seq>=0),
 retained_after_seq INTEGER NOT NULL DEFAULT 0 CHECK(retained_after_seq>=0),
 created_ms INTEGER NOT NULL, UNIQUE(id,owner_id), CHECK(retained_after_seq<=head_seq)
) STRICT;
CREATE INDEX datasets_owner ON datasets(owner_id,id);
CREATE TABLE categories (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), id TEXT NOT NULL, parent_id TEXT,
 title TEXT NOT NULL CHECK(length(trim(title))>0), description TEXT NOT NULL DEFAULT '',
 color_key TEXT NOT NULL DEFAULT 'neutral', archived INTEGER NOT NULL CHECK(archived IN (0,1)),
 revision INTEGER NOT NULL CHECK(revision>=1), created_ms INTEGER NOT NULL,
 updated_ms INTEGER NOT NULL, deleted_ms INTEGER, legacy_json TEXT,
 PRIMARY KEY(dataset_id,id), CHECK(parent_id IS NULL OR parent_id<>id),
 FOREIGN KEY(dataset_id,parent_id) REFERENCES categories(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE INDEX categories_parent ON categories(dataset_id,parent_id,deleted_ms);
CREATE TABLE tags (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), id TEXT NOT NULL, title TEXT NOT NULL,
 color_key TEXT NOT NULL DEFAULT 'neutral', revision INTEGER NOT NULL CHECK(revision>=1),
 created_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL, deleted_ms INTEGER,
 PRIMARY KEY(dataset_id,id), CHECK(length(trim(title))>0)
) STRICT;
CREATE TABLE recurrence_rules (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), id TEXT NOT NULL, source_task_id TEXT,
 title TEXT NOT NULL, notes TEXT NOT NULL, mode TEXT NOT NULL CHECK(mode IN ('FIXED','AFTER_COMPLETION')),
 frequency TEXT NOT NULL CHECK(frequency IN ('DAILY','WEEKLY','MONTHLY','YEARLY','AFTER_DAYS')),
 interval_n INTEGER NOT NULL CHECK(interval_n BETWEEN 1 AND 36500),
 anchor_on TEXT NOT NULL, end_on TEXT, weekdays_mask INTEGER NOT NULL CHECK(weekdays_mask BETWEEN 0 AND 127),
 month_day INTEGER NOT NULL CHECK(month_day BETWEEN 1 AND 31), year_month INTEGER NOT NULL CHECK(year_month BETWEEN 1 AND 12),
 time_zone TEXT NOT NULL, defaults_json TEXT NOT NULL,
 active INTEGER NOT NULL CHECK(active IN (0,1)), revision INTEGER NOT NULL CHECK(revision>=1),
 created_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL, deleted_ms INTEGER,
 PRIMARY KEY(dataset_id,id), CHECK(end_on IS NULL OR end_on>=anchor_on),
 CHECK((mode='AFTER_COMPLETION')=(frequency='AFTER_DAYS')),
 FOREIGN KEY(dataset_id,source_task_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE TABLE tasks (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), id TEXT NOT NULL,
 title TEXT NOT NULL CHECK(length(trim(title))>0 AND length(title)<=512), notes TEXT NOT NULL DEFAULT '',
 planning_state TEXT NOT NULL CHECK(planning_state IN ('INBOX','ACTIVE','SOMEDAY')),
 status TEXT NOT NULL CHECK(status IN ('OPEN','COMPLETED','CANCELLED')),
 category_id TEXT, parent_task_id TEXT, start_on TEXT, deadline_on TEXT,
 today_focus INTEGER NOT NULL CHECK(today_focus IN (0,1)), evening INTEGER NOT NULL CHECK(evening IN (0,1)),
 pinned INTEGER NOT NULL CHECK(pinned IN (0,1)), important INTEGER NOT NULL CHECK(important IN (0,1)),
 created_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL, completed_ms INTEGER,
 cancelled_ms INTEGER, deleted_ms INTEGER, completion_group_id TEXT, deletion_group_id TEXT,
 recurrence_id TEXT, occurrence_on TEXT, occurrence_key TEXT,
 instance_overridden INTEGER NOT NULL CHECK(instance_overridden IN (0,1)),
 revision INTEGER NOT NULL CHECK(revision>=1), legacy_json TEXT,
 PRIMARY KEY(dataset_id,id), CHECK(parent_task_id IS NULL OR parent_task_id<>id),
 CHECK(start_on IS NULL OR deadline_on IS NULL OR start_on<=deadline_on),
 CHECK((status='OPEN' AND completed_ms IS NULL AND cancelled_ms IS NULL)
    OR (status='COMPLETED' AND completed_ms IS NOT NULL AND cancelled_ms IS NULL)
    OR (status='CANCELLED' AND cancelled_ms IS NOT NULL AND completed_ms IS NULL)),
 CHECK((recurrence_id IS NULL AND occurrence_key IS NULL) OR (recurrence_id IS NOT NULL AND occurrence_key IS NOT NULL)),
 FOREIGN KEY(dataset_id,category_id) REFERENCES categories(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(dataset_id,parent_task_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(dataset_id,recurrence_id) REFERENCES recurrence_rules(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 UNIQUE(dataset_id,recurrence_id,occurrence_key)
) STRICT;
CREATE INDEX tasks_parent ON tasks(dataset_id,parent_task_id,deleted_ms);
CREATE INDEX tasks_category ON tasks(dataset_id,category_id,deleted_ms);
CREATE INDEX tasks_deletion ON tasks(dataset_id,deletion_group_id);
CREATE INDEX tasks_completion ON tasks(dataset_id,completion_group_id);
CREATE TABLE task_schedules (
 dataset_id TEXT NOT NULL, task_id TEXT NOT NULL, all_day INTEGER NOT NULL CHECK(all_day IN (0,1)),
 start_on TEXT, end_on TEXT, start_ms INTEGER, end_ms INTEGER, time_zone TEXT,
 PRIMARY KEY(dataset_id,task_id),
 CHECK((all_day=1 AND start_on IS NOT NULL AND end_on IS NOT NULL AND end_on>start_on AND start_ms IS NULL AND end_ms IS NULL AND time_zone IS NULL)
 OR (all_day=0 AND start_on IS NULL AND end_on IS NULL AND start_ms IS NOT NULL AND end_ms IS NOT NULL AND end_ms>start_ms AND time_zone IS NOT NULL)),
 FOREIGN KEY(dataset_id,task_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE TABLE reminder_configs (
 dataset_id TEXT NOT NULL, task_id TEXT NOT NULL,
 kind TEXT NOT NULL CHECK(kind IN ('ABSOLUTE','RELATIVE')), anchor_ms INTEGER, minutes_before INTEGER,
 repeat_kind TEXT NOT NULL CHECK(repeat_kind IN ('NONE','DAILY','WEEKLY','MONTHLY','YEARLY')),
 every_n INTEGER NOT NULL CHECK(every_n BETWEEN 1 AND 36500), time_zone TEXT NOT NULL,
 PRIMARY KEY(dataset_id,task_id),
 CHECK((kind='ABSOLUTE' AND anchor_ms IS NOT NULL AND minutes_before IS NULL)
 OR (kind='RELATIVE' AND anchor_ms IS NULL AND minutes_before IS NOT NULL AND minutes_before IN (0,5,15,30,60,1440))),
 FOREIGN KEY(dataset_id,task_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE TABLE task_tags (
 dataset_id TEXT NOT NULL, task_id TEXT NOT NULL, tag_id TEXT NOT NULL,
 PRIMARY KEY(dataset_id,task_id,tag_id),
 FOREIGN KEY(dataset_id,task_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(dataset_id,tag_id) REFERENCES tags(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE INDEX task_tags_reverse ON task_tags(dataset_id,tag_id,task_id);
CREATE TABLE task_links (
 dataset_id TEXT NOT NULL, source_id TEXT NOT NULL, target_id TEXT NOT NULL,
 role TEXT NOT NULL CHECK(role IN ('preparation','followup')),
 PRIMARY KEY(dataset_id,source_id,target_id), CHECK(source_id<>target_id),
 FOREIGN KEY(dataset_id,source_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(dataset_id,target_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE INDEX task_links_reverse ON task_links(dataset_id,target_id);
CREATE TABLE recurrence_successors (
 dataset_id TEXT NOT NULL, rule_id TEXT NOT NULL, predecessor_id TEXT NOT NULL, successor_id TEXT NOT NULL,
 PRIMARY KEY(dataset_id,rule_id,predecessor_id), UNIQUE(dataset_id,successor_id), CHECK(predecessor_id<>successor_id),
 FOREIGN KEY(dataset_id,rule_id) REFERENCES recurrence_rules(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(dataset_id,predecessor_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED,
 FOREIGN KEY(dataset_id,successor_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE TABLE task_aliases (
 dataset_id TEXT NOT NULL, alias_id TEXT NOT NULL, canonical_id TEXT NOT NULL,
 PRIMARY KEY(dataset_id,alias_id), CHECK(alias_id<>canonical_id),
 FOREIGN KEY(dataset_id,canonical_id) REFERENCES tasks(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE TABLE structure_versions (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), kind TEXT NOT NULL, id TEXT NOT NULL,
 revision INTEGER NOT NULL CHECK(revision>=1), PRIMARY KEY(dataset_id,kind,id)
) STRICT;
CREATE TABLE ordering_scopes (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), id TEXT NOT NULL,
 kind TEXT NOT NULL CHECK(kind IN ('category_siblings','task_siblings','today')),
 subject_id TEXT, revision INTEGER NOT NULL CHECK(revision>=1), PRIMARY KEY(dataset_id,id)
) STRICT;
CREATE TABLE order_entries (
 dataset_id TEXT NOT NULL, scope_id TEXT NOT NULL, entity_id TEXT NOT NULL, position INTEGER NOT NULL CHECK(position>=0),
 PRIMARY KEY(dataset_id,scope_id,entity_id), UNIQUE(dataset_id,scope_id,position),
 FOREIGN KEY(dataset_id,scope_id) REFERENCES ordering_scopes(dataset_id,id) DEFERRABLE INITIALLY DEFERRED
) STRICT;
CREATE TABLE change_batches (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), seq INTEGER NOT NULL CHECK(seq>=1),
 data_epoch TEXT NOT NULL, mutation_id TEXT NOT NULL, installation_id TEXT NOT NULL,
 committed_ms INTEGER NOT NULL, payload_json TEXT NOT NULL, payload_hash BLOB NOT NULL CHECK(length(payload_hash)=32),
 PRIMARY KEY(dataset_id,seq), UNIQUE(dataset_id,mutation_id)
) STRICT;
CREATE INDEX change_batches_age ON change_batches(committed_ms,dataset_id,seq);
CREATE TABLE mutation_receipts (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), mutation_id TEXT NOT NULL,
 data_epoch TEXT NOT NULL, request_hash BLOB NOT NULL CHECK(length(request_hash)=32),
 outer_hash BLOB CHECK(outer_hash IS NULL OR length(outer_hash)=32), payload_ref TEXT,
 result TEXT NOT NULL CHECK(result IN ('committed','no_op')), batch_seq INTEGER,
 response_json TEXT NOT NULL, created_ms INTEGER NOT NULL,
 PRIMARY KEY(dataset_id,mutation_id),
 CHECK((result='committed' AND batch_seq IS NOT NULL AND batch_seq>=1) OR (result='no_op' AND batch_seq IS NULL))
) STRICT;
-- No receipt -> batch FK: batches can expire while deduplication must survive.
CREATE TABLE device_progress (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), installation_id TEXT NOT NULL,
 data_epoch TEXT NOT NULL, applied_seq INTEGER NOT NULL CHECK(applied_seq>=0),
 reported_ms INTEGER NOT NULL, PRIMARY KEY(dataset_id,installation_id)
) STRICT;
CREATE TABLE jobs (
 id TEXT PRIMARY KEY NOT NULL, owner_id TEXT REFERENCES users(id), dataset_id TEXT,
 kind TEXT NOT NULL CHECK(kind IN ('SNAPSHOT','STAGED','IMPORT','EXPORT','BATCH','BACKUP','DELETE_ACCOUNT')),
 state TEXT NOT NULL CHECK(state IN ('UPLOADING','SEALED','BUILDING','READY','PUBLISHING','COMMITTED','FAILED','CANCELLED','EXPIRED')),
 data_epoch TEXT, operation_key TEXT NOT NULL UNIQUE, metadata_json TEXT NOT NULL,
 attempt INTEGER NOT NULL DEFAULT 0 CHECK(attempt>=0), lease_owner TEXT, lease_until_ms INTEGER,
 expires_ms INTEGER NOT NULL, bytes_reserved INTEGER NOT NULL CHECK(bytes_reserved>=0),
 safe_error_code TEXT, created_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL,
 FOREIGN KEY(dataset_id,owner_id) REFERENCES datasets(id,owner_id),
 CHECK((kind='BACKUP' AND owner_id IS NULL AND dataset_id IS NULL) OR (kind<>'BACKUP' AND owner_id IS NOT NULL)),
 CHECK(kind NOT IN ('SNAPSHOT','STAGED','IMPORT','BATCH') OR dataset_id IS NOT NULL),
 CHECK(dataset_id IS NULL OR data_epoch IS NOT NULL)
) STRICT;
CREATE INDEX jobs_runnable ON jobs(state,lease_until_ms,expires_ms);
CREATE INDEX jobs_scope ON jobs(owner_id,dataset_id,kind,state);
CREATE TABLE job_read_leases (
 id TEXT PRIMARY KEY NOT NULL, job_id TEXT NOT NULL REFERENCES jobs(id),
 reader_id TEXT NOT NULL, created_ms INTEGER NOT NULL, expires_ms INTEGER NOT NULL,
 CHECK(expires_ms>created_ms)
) STRICT;
CREATE INDEX job_read_leases_expiry ON job_read_leases(job_id,expires_ms);
CREATE TABLE job_chunks (
 job_id TEXT NOT NULL REFERENCES jobs(id), chunk_index INTEGER NOT NULL CHECK(chunk_index>=0),
 byte_count INTEGER NOT NULL CHECK(byte_count>=0), sha256 BLOB NOT NULL CHECK(length(sha256)=32),
 artifact_key TEXT NOT NULL UNIQUE, PRIMARY KEY(job_id,chunk_index)
) STRICT;
CREATE TABLE retention_pins (
 dataset_id TEXT NOT NULL REFERENCES datasets(id), job_id TEXT NOT NULL REFERENCES jobs(id),
 after_seq INTEGER NOT NULL CHECK(after_seq>=0), expires_ms INTEGER NOT NULL,
 PRIMARY KEY(dataset_id,job_id)
) STRICT;
CREATE TABLE snapshot_manifests (
 job_id TEXT PRIMARY KEY NOT NULL REFERENCES jobs(id), cutoff_seq INTEGER NOT NULL CHECK(cutoff_seq>=0),
 dto_version INTEGER NOT NULL CHECK(dto_version>=1), entity_count INTEGER NOT NULL CHECK(entity_count>=0),
 total_bytes INTEGER NOT NULL CHECK(total_bytes>=0), manifest_json TEXT NOT NULL,
 manifest_hash BLOB NOT NULL CHECK(length(manifest_hash)=32)
) STRICT;
CREATE TABLE backup_records (
 job_id TEXT PRIMARY KEY NOT NULL REFERENCES jobs(id), snapshot_ms INTEGER NOT NULL,
 sha256 BLOB NOT NULL CHECK(length(sha256)=32), local_artifact_key TEXT NOT NULL,
 remote_target_id TEXT, remote_verified_ms INTEGER, schema_version INTEGER NOT NULL
) STRICT;
CREATE TABLE account_deletions (
 operation_id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL REFERENCES users(id),
 state TEXT NOT NULL CHECK(state IN ('INTENT','PENDING_LEDGER','LEDGER_CONFIRMED','COMPLETED','CANCELLED','EXPIRED')),
 token_hash BLOB NOT NULL UNIQUE CHECK(length(token_hash)=32), auth_epoch TEXT NOT NULL,
 credential_version INTEGER NOT NULL CHECK(credential_version>=1),
 confirm_until_ms INTEGER NOT NULL, expires_ms INTEGER NOT NULL,
 job_id TEXT REFERENCES jobs(id), ledger_receipt TEXT, requested_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL,
 CHECK(confirm_until_ms>requested_ms AND expires_ms>confirm_until_ms),
 CHECK(state IN ('INTENT','CANCELLED','EXPIRED') OR job_id IS NOT NULL)
) STRICT;
