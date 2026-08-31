CREATE TABLE IF NOT EXISTS fact_safe_handoff_imports (
    dataset_id TEXT PRIMARY KEY NOT NULL,
    version TEXT NOT NULL,
    source_resource_root TEXT NOT NULL,
    imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL,
    manifest_file_count INTEGER NOT NULL CHECK (manifest_file_count >= 0),
    runtime_file_count INTEGER NOT NULL CHECK (runtime_file_count >= 0),
    runtime_row_count INTEGER NOT NULL CHECK (runtime_row_count >= 0),
    competition_runtime_file_count INTEGER NOT NULL CHECK (competition_runtime_file_count >= 0),
    competition_runtime_row_count INTEGER NOT NULL CHECK (competition_runtime_row_count >= 0),
    clubs INTEGER NOT NULL CHECK (clubs >= 0),
    playable_clubs INTEGER NOT NULL CHECK (playable_clubs >= 0),
    people INTEGER NOT NULL CHECK (people >= 0),
    opening_registrations INTEGER NOT NULL CHECK (opening_registrations >= 0),
    opening_relationship_records INTEGER NOT NULL CHECK (opening_relationship_records >= 0),
    scheduled_future_generations INTEGER NOT NULL CHECK (scheduled_future_generations >= 0),
    future_family_relationships INTEGER NOT NULL CHECK (future_family_relationships >= 0),
    premier_league_schedule_fixtures INTEGER NOT NULL CHECK (premier_league_schedule_fixtures >= 0),
    charity_shield_schedule_fixtures INTEGER NOT NULL CHECK (charity_shield_schedule_fixtures >= 0),
    competition_participants INTEGER NOT NULL CHECK (competition_participants >= 0),
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS fact_safe_handoff_files (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_handoff_imports(dataset_id) ON DELETE CASCADE,
    file_path TEXT NOT NULL,
    source_area TEXT NOT NULL,
    import_role TEXT NOT NULL,
    runtime_import_allowed INTEGER NOT NULL CHECK (runtime_import_allowed IN (0, 1)),
    authoritative_version TEXT NOT NULL DEFAULT '',
    load_order INTEGER NOT NULL DEFAULT 0,
    sha256 TEXT NOT NULL DEFAULT '',
    row_count INTEGER NOT NULL CHECK (row_count >= 0),
    column_names TEXT NOT NULL,
    status TEXT NOT NULL,
    notes TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (dataset_id, file_path)
);

CREATE TABLE IF NOT EXISTS fo92_fact_safe_rows (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_handoff_imports(dataset_id) ON DELETE CASCADE,
    file_path TEXT NOT NULL,
    row_number INTEGER NOT NULL CHECK (row_number >= 1),
    row_json TEXT NOT NULL,
    PRIMARY KEY (dataset_id, file_path, row_number),
    FOREIGN KEY (dataset_id, file_path)
        REFERENCES fact_safe_handoff_files(dataset_id, file_path)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_fact_safe_handoff_files_area
    ON fact_safe_handoff_files(dataset_id, source_area, import_role);

CREATE INDEX IF NOT EXISTS idx_fo92_fact_safe_rows_file
    ON fo92_fact_safe_rows(dataset_id, file_path);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (14);
