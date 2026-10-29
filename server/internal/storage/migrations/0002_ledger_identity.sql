-- The ledger is independent of database snapshots; this identity prevents a
-- restored database from silently accepting a new empty deletion ledger.
ALTER TABLE service_state ADD COLUMN ledger_id TEXT;
