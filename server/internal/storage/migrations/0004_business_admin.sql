CREATE TABLE admin_identities (
 id TEXT PRIMARY KEY NOT NULL, login TEXT NOT NULL UNIQUE, password_hash TEXT NOT NULL,
 secret_cipher BLOB NOT NULL, recovery_hashes TEXT NOT NULL CHECK(json_valid(recovery_hashes)),
 roles TEXT NOT NULL CHECK(json_valid(roles)), status TEXT NOT NULL CHECK(status IN ('ACTIVE','DISABLED')),
 version INTEGER NOT NULL CHECK(version>0), last_step INTEGER NOT NULL DEFAULT -1, created_ms INTEGER NOT NULL
) STRICT;
CREATE TABLE admin_sessions (
 token_hash BLOB PRIMARY KEY NOT NULL CHECK(length(token_hash)=32), admin_id TEXT NOT NULL REFERENCES admin_identities(id),
 version INTEGER NOT NULL, auth_epoch TEXT NOT NULL, created_ms INTEGER NOT NULL, expires_ms INTEGER NOT NULL
) STRICT;
CREATE INDEX admin_sessions_expiry ON admin_sessions(expires_ms);
CREATE TABLE business_notices (
 id TEXT PRIMARY KEY NOT NULL, title TEXT NOT NULL, body TEXT NOT NULL,
 state TEXT NOT NULL CHECK(state IN ('draft','scheduled','published','revoked')),
 action TEXT NOT NULL, platform TEXT NOT NULL, min_version TEXT NOT NULL, max_version TEXT NOT NULL,
 targets TEXT NOT NULL CHECK(json_valid(targets)), version INTEGER NOT NULL CHECK(version>0),
 publish_ms INTEGER NOT NULL, expire_ms INTEGER NOT NULL, updated_ms INTEGER NOT NULL, created_by TEXT NOT NULL REFERENCES admin_identities(id)
) STRICT;
CREATE TABLE admin_audit (
 seq INTEGER PRIMARY KEY AUTOINCREMENT, actor TEXT NOT NULL, action TEXT NOT NULL, target TEXT NOT NULL,
 reason TEXT NOT NULL, request_id TEXT NOT NULL, at_ms INTEGER NOT NULL
) STRICT;
CREATE INDEX admin_audit_time ON admin_audit(at_ms,seq);
CREATE TABLE business_operations (
 actor TEXT NOT NULL, operation_id TEXT NOT NULL, request_hash TEXT NOT NULL, result TEXT NOT NULL,
 at_ms INTEGER NOT NULL, PRIMARY KEY(actor,operation_id)
) STRICT;
CREATE TABLE business_cleanup (
 user_id TEXT PRIMARY KEY NOT NULL, at_ms INTEGER NOT NULL
) STRICT;
CREATE INDEX business_operations_time ON business_operations(at_ms);
CREATE INDEX business_notices_expiry ON business_notices(expire_ms);
INSERT INTO business_cleanup(user_id,at_ms) SELECT id,updated_ms FROM users WHERE status IN ('DELETING','DELETED');
