CREATE TABLE IF NOT EXISTS world_baseline_imports (
    dataset_id TEXT PRIMARY KEY NOT NULL,
    version TEXT NOT NULL,
    source_resource TEXT NOT NULL,
    logical_digest TEXT NOT NULL,
    imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL,
    clubs INTEGER NOT NULL,
    playable_clubs INTEGER NOT NULL,
    people INTEGER NOT NULL,
    active_start_registrations INTEGER NOT NULL,
    opening_contract_states INTEGER NOT NULL,
    external_registered_players INTEGER NOT NULL,
    staff_records INTEGER NOT NULL,
    future_transfer_events INTEGER NOT NULL,
    provenance_rows INTEGER NOT NULL,
    notes TEXT NOT NULL
);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (9);
