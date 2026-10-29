-- DESIGN ONLY: additive sync metadata, not a delivered desktop migration.
-- Run with foreign_keys=ON on each connection. Business tables remain existing
-- Daymark tables; metadata and business writes share the SAME connection/Tx.
CREATE TABLE sync_profiles (
 profile_id TEXT PRIMARY KEY NOT NULL,
 mode TEXT NOT NULL CHECK(mode IN ('GUEST','ACCOUNT')),
 server_origin TEXT, server_instance_id TEXT, user_id TEXT, dataset_id TEXT, data_epoch TEXT,
 generation INTEGER NOT NULL CHECK(generation>=1),
 auth_state TEXT NOT NULL CHECK(auth_state IN ('SIGNED_OUT','AUTHENTICATED','AUTH_REQUIRED','REVOKED')),
 next_local_seq INTEGER NOT NULL DEFAULT 1 CHECK(next_local_seq>=1),
 CHECK((mode='GUEST' AND server_origin IS NULL AND server_instance_id IS NULL AND user_id IS NULL AND dataset_id IS NULL AND data_epoch IS NULL AND auth_state='SIGNED_OUT')
 OR (mode='ACCOUNT' AND server_origin IS NOT NULL AND server_instance_id IS NOT NULL AND user_id IS NOT NULL AND dataset_id IS NOT NULL AND data_epoch IS NOT NULL)),
 UNIQUE(profile_id,data_epoch)
) STRICT;
CREATE TABLE sync_cursor (
 profile_id TEXT PRIMARY KEY NOT NULL, data_epoch TEXT NOT NULL,
 opaque_cursor TEXT NOT NULL, applied_seq INTEGER NOT NULL CHECK(applied_seq>=0),
 observed_head INTEGER NOT NULL CHECK(observed_head>=applied_seq),
 FOREIGN KEY(profile_id,data_epoch) REFERENCES sync_profiles(profile_id,data_epoch)
) STRICT;
CREATE TABLE sync_base (
 profile_id TEXT NOT NULL, data_epoch TEXT NOT NULL, entity_type TEXT NOT NULL
 CHECK(entity_type IN ('task','category','tag','recurrence_rule','ordering_scope')),
 entity_id TEXT NOT NULL, server_revision INTEGER NOT NULL CHECK(server_revision>=1),
 canonical_snapshot TEXT NOT NULL, last_origin_mutation TEXT,
 PRIMARY KEY(profile_id,data_epoch,entity_type,entity_id),
 FOREIGN KEY(profile_id,data_epoch) REFERENCES sync_profiles(profile_id,data_epoch)
) STRICT;
CREATE TABLE sync_structures (
 profile_id TEXT NOT NULL, data_epoch TEXT NOT NULL, kind TEXT NOT NULL, entity_id TEXT NOT NULL,
 server_revision INTEGER NOT NULL CHECK(server_revision>=1),
 PRIMARY KEY(profile_id,data_epoch,kind,entity_id),
 FOREIGN KEY(profile_id,data_epoch) REFERENCES sync_profiles(profile_id,data_epoch)
) STRICT;
CREATE TABLE sync_outbox (
 profile_id TEXT NOT NULL, mutation_id TEXT NOT NULL, origin_epoch TEXT NOT NULL,
 local_seq INTEGER NOT NULL CHECK(local_seq>=1),
 state TEXT NOT NULL CHECK(state IN ('PENDING','IN_FLIGHT','RETRY_WAIT','ACKNOWLEDGED','BLOCKED_CONFLICT','BLOCKED_VALIDATION','RECONCILE_REQUIRED','SUPERSEDED')),
 kind TEXT NOT NULL CHECK(kind IN ('COMMANDS','IMPORT')),
 intent_json TEXT NOT NULL, base_snapshot_json TEXT NOT NULL,
 preconditions_json TEXT, frozen_payload BLOB, canonical_hash BLOB,
 attempt INTEGER NOT NULL DEFAULT 0 CHECK(attempt>=0), not_before_ms INTEGER NOT NULL,
 created_ms INTEGER NOT NULL, safe_error_code TEXT,
 PRIMARY KEY(profile_id,mutation_id), UNIQUE(profile_id,local_seq),
 FOREIGN KEY(profile_id) REFERENCES sync_profiles(profile_id),
 CHECK((frozen_payload IS NULL AND canonical_hash IS NULL AND preconditions_json IS NULL)
 OR (frozen_payload IS NOT NULL AND canonical_hash IS NOT NULL AND length(canonical_hash)=32 AND preconditions_json IS NOT NULL)),
 CHECK(state NOT IN ('IN_FLIGHT','RETRY_WAIT','ACKNOWLEDGED') OR frozen_payload IS NOT NULL)
) STRICT;
CREATE INDEX sync_outbox_runnable ON sync_outbox(profile_id,state,not_before_ms,local_seq);
CREATE UNIQUE INDEX sync_outbox_single_flight ON sync_outbox(profile_id) WHERE state='IN_FLIGHT';
-- frozen fields stay immutable once nonnull, even after a conflict/epoch reset.
CREATE TRIGGER sync_outbox_frozen_immutable BEFORE UPDATE ON sync_outbox
WHEN OLD.frozen_payload IS NOT NULL AND
 (NEW.frozen_payload IS NOT OLD.frozen_payload OR NEW.canonical_hash IS NOT OLD.canonical_hash
  OR NEW.preconditions_json IS NOT OLD.preconditions_json OR NEW.intent_json IS NOT OLD.intent_json
  OR NEW.mutation_id<>OLD.mutation_id OR NEW.origin_epoch<>OLD.origin_epoch
  OR NEW.profile_id<>OLD.profile_id OR NEW.local_seq<>OLD.local_seq)
BEGIN SELECT RAISE(ABORT,'frozen mutation is immutable'); END;
CREATE TABLE sync_dependencies (
 profile_id TEXT NOT NULL, mutation_id TEXT NOT NULL, depends_on_id TEXT NOT NULL,
 PRIMARY KEY(profile_id,mutation_id,depends_on_id), CHECK(mutation_id<>depends_on_id),
 FOREIGN KEY(profile_id,mutation_id) REFERENCES sync_outbox(profile_id,mutation_id),
 FOREIGN KEY(profile_id,depends_on_id) REFERENCES sync_outbox(profile_id,mutation_id)
) STRICT;
-- App additionally requires dependency.local_seq < child.local_seq, forbidding cycles.
CREATE TRIGGER sync_dependencies_forward_only BEFORE INSERT ON sync_dependencies
WHEN (SELECT local_seq FROM sync_outbox WHERE profile_id=NEW.profile_id AND mutation_id=NEW.depends_on_id)
 >= (SELECT local_seq FROM sync_outbox WHERE profile_id=NEW.profile_id AND mutation_id=NEW.mutation_id)
BEGIN SELECT RAISE(ABORT,'dependency must precede child'); END;
CREATE TABLE sync_dirty_entities (
 profile_id TEXT NOT NULL, mutation_id TEXT NOT NULL, entity_type TEXT NOT NULL, entity_id TEXT NOT NULL,
 captured_row_version INTEGER NOT NULL CHECK(captured_row_version>=1),
 PRIMARY KEY(profile_id,mutation_id,entity_type,entity_id),
 FOREIGN KEY(profile_id,mutation_id) REFERENCES sync_outbox(profile_id,mutation_id)
) STRICT;
CREATE TABLE sync_ack (
 profile_id TEXT NOT NULL, mutation_id TEXT NOT NULL, origin_epoch TEXT NOT NULL,
 batch_seq INTEGER CHECK(batch_seq IS NULL OR batch_seq>=1),
 receipt_json TEXT NOT NULL, confirmed_revisions_json TEXT NOT NULL, confirmed_ms INTEGER NOT NULL,
 PRIMARY KEY(profile_id,mutation_id), UNIQUE(profile_id,origin_epoch,batch_seq),
 FOREIGN KEY(profile_id,mutation_id) REFERENCES sync_outbox(profile_id,mutation_id)
) STRICT;
CREATE TABLE sync_conflicts (
 profile_id TEXT NOT NULL, conflict_id TEXT NOT NULL, mutation_id TEXT NOT NULL,
 source_epoch TEXT NOT NULL, conflict_group_id TEXT NOT NULL,
 entity_type TEXT NOT NULL, entity_id TEXT NOT NULL,
 base_json TEXT, local_json TEXT NOT NULL, remote_json TEXT,
 remote_revision INTEGER NOT NULL CHECK(remote_revision>=0),
 state TEXT NOT NULL CHECK(state IN ('OPEN','RESOLVED','SUPERSEDED')),
 decision_json TEXT, replacement_mutation_id TEXT,
 PRIMARY KEY(profile_id,conflict_id),
 FOREIGN KEY(profile_id,mutation_id) REFERENCES sync_outbox(profile_id,mutation_id),
 FOREIGN KEY(profile_id,replacement_mutation_id) REFERENCES sync_outbox(profile_id,mutation_id),
 CHECK(state<>'RESOLVED' OR decision_json IS NOT NULL)
) STRICT;
CREATE INDEX sync_conflicts_open ON sync_conflicts(profile_id,state,conflict_group_id);
CREATE TABLE sync_incoming_manifests (
 profile_id TEXT NOT NULL, data_epoch TEXT NOT NULL, batch_seq INTEGER NOT NULL CHECK(batch_seq>=1),
 job_id TEXT NOT NULL, expected_cursor TEXT NOT NULL, manifest_json TEXT NOT NULL,
 manifest_hash BLOB NOT NULL CHECK(length(manifest_hash)=32),
 state TEXT NOT NULL CHECK(state IN ('DOWNLOADING','VERIFIED')),
 PRIMARY KEY(profile_id,data_epoch,batch_seq),
 FOREIGN KEY(profile_id,data_epoch) REFERENCES sync_profiles(profile_id,data_epoch)
) STRICT;
CREATE TABLE sync_incoming (
 profile_id TEXT NOT NULL, data_epoch TEXT NOT NULL, batch_seq INTEGER NOT NULL,
 chunk_index INTEGER NOT NULL CHECK(chunk_index>=0), sha256 BLOB NOT NULL CHECK(length(sha256)=32),
 byte_count INTEGER NOT NULL CHECK(byte_count>=0), bytes BLOB NOT NULL CHECK(length(bytes)=byte_count),
 PRIMARY KEY(profile_id,data_epoch,batch_seq,chunk_index),
 FOREIGN KEY(profile_id,data_epoch,batch_seq) REFERENCES sync_incoming_manifests(profile_id,data_epoch,batch_seq)
) STRICT;
CREATE TABLE reminder_execution (
 task_id TEXT NOT NULL, config_fingerprint BLOB NOT NULL CHECK(length(config_fingerprint)=32),
 next_due_ms INTEGER, last_fired_ms INTEGER, claim_token TEXT, claim_until_ms INTEGER,
 PRIMARY KEY(task_id,config_fingerprint),
 CHECK((claim_token IS NULL)=(claim_until_ms IS NULL))
) STRICT;
-- Profile binding changes use explicit save-gate+worker transaction, not a login
-- callback UPDATE owner. Before epoch replacement archive cursor/base/incoming;
-- outbox/conflicts retain origin_epoch and become RECONCILE_REQUIRED. Remove all
-- current-epoch FK children before changing sync_profiles.data_epoch, in one Tx.
-- Never store access/refresh/deletion tokens here or in legacy task/export JSON.
