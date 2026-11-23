ALTER TABLE datasets ADD COLUMN minimum_dto_version INTEGER NOT NULL DEFAULT 1
    CHECK (minimum_dto_version IN (1, 2));
