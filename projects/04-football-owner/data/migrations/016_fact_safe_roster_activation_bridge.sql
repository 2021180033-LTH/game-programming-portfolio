PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS fact_safe_roster_bridge_imports (
    dataset_id TEXT PRIMARY KEY NOT NULL,
    version TEXT NOT NULL,
    source_dataset_id TEXT NOT NULL,
    imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL,
    people_source_rows INTEGER NOT NULL CHECK (people_source_rows >= 0),
    person_bridge_rows INTEGER NOT NULL CHECK (person_bridge_rows >= 0),
    people_identity_rows_processed INTEGER NOT NULL CHECK (people_identity_rows_processed >= 0),
    player_profile_rows_processed INTEGER NOT NULL CHECK (player_profile_rows_processed >= 0),
    opening_registration_source_rows INTEGER NOT NULL CHECK (opening_registration_source_rows >= 0),
    registration_bridge_rows INTEGER NOT NULL CHECK (registration_bridge_rows >= 0),
    active_registration_rows INTEGER NOT NULL CHECK (active_registration_rows >= 0),
    skipped_registration_rows INTEGER NOT NULL CHECK (skipped_registration_rows >= 0),
    contract_source_rows INTEGER NOT NULL CHECK (contract_source_rows >= 0),
    contract_bridge_rows INTEGER NOT NULL CHECK (contract_bridge_rows >= 0),
    active_contract_rows INTEGER NOT NULL CHECK (active_contract_rows >= 0),
    skipped_contract_rows INTEGER NOT NULL CHECK (skipped_contract_rows >= 0),
    loan_review_rows INTEGER NOT NULL CHECK (loan_review_rows >= 0),
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS fact_safe_person_activation_bridge (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_roster_bridge_imports(dataset_id) ON DELETE CASCADE,
    fact_safe_person_id TEXT NOT NULL,
    gameplay_person_id TEXT NOT NULL REFERENCES people(id),
    display_name TEXT NOT NULL,
    date_of_birth TEXT NOT NULL,
    nationality TEXT NOT NULL,
    primary_position TEXT NOT NULL,
    availability_state TEXT NOT NULL,
    activation_status TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    source_url TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, fact_safe_person_id)
);

CREATE TABLE IF NOT EXISTS fact_safe_roster_registration_bridge (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_roster_bridge_imports(dataset_id) ON DELETE CASCADE,
    fact_safe_registration_id TEXT NOT NULL,
    fact_safe_person_id TEXT NOT NULL,
    gameplay_person_id TEXT NOT NULL REFERENCES people(id),
    fact_safe_club_id TEXT NOT NULL,
    gameplay_club_id TEXT REFERENCES clubs(id),
    display_name TEXT NOT NULL,
    club_name TEXT NOT NULL,
    snapshot_date TEXT NOT NULL,
    squad_group_source TEXT NOT NULL,
    squad_group_gameplay TEXT NOT NULL,
    registration_status_source TEXT NOT NULL,
    registration_status_gameplay TEXT NOT NULL,
    codex_import_policy TEXT NOT NULL,
    operational_status TEXT NOT NULL,
    activation_status TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, fact_safe_registration_id)
);

CREATE TABLE IF NOT EXISTS fact_safe_contract_bridge (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_roster_bridge_imports(dataset_id) ON DELETE CASCADE,
    fact_safe_contract_id TEXT NOT NULL,
    fact_safe_person_id TEXT NOT NULL,
    gameplay_person_id TEXT NOT NULL REFERENCES people(id),
    fact_safe_club_id TEXT NOT NULL,
    gameplay_club_id TEXT REFERENCES clubs(id),
    display_name TEXT NOT NULL,
    club_name TEXT NOT NULL,
    registration_scope TEXT NOT NULL,
    contract_start_date TEXT NOT NULL,
    contract_end_date TEXT NOT NULL,
    weekly_wage_pence INTEGER NOT NULL CHECK (weekly_wage_pence >= 0),
    squad_role_source TEXT NOT NULL,
    squad_role_gameplay TEXT NOT NULL,
    contract_status TEXT NOT NULL,
    codex_import_policy TEXT NOT NULL,
    activation_status TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, fact_safe_contract_id)
);

CREATE TABLE IF NOT EXISTS fact_safe_loan_review_queue (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_roster_bridge_imports(dataset_id) ON DELETE CASCADE,
    source_file_path TEXT NOT NULL,
    source_row_number INTEGER NOT NULL CHECK (source_row_number >= 1),
    fact_safe_record_id TEXT NOT NULL,
    fact_safe_person_id TEXT NOT NULL,
    gameplay_person_id TEXT NOT NULL REFERENCES people(id),
    fact_safe_club_id TEXT NOT NULL,
    gameplay_club_id TEXT REFERENCES clubs(id),
    display_name TEXT NOT NULL,
    review_status TEXT NOT NULL,
    evidence_summary TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, source_file_path, source_row_number)
);

CREATE INDEX IF NOT EXISTS idx_fact_safe_person_activation_gameplay
    ON fact_safe_person_activation_bridge(dataset_id, gameplay_person_id);

CREATE INDEX IF NOT EXISTS idx_fact_safe_roster_registration_gameplay_club
    ON fact_safe_roster_registration_bridge(dataset_id, gameplay_club_id, activation_status);

CREATE INDEX IF NOT EXISTS idx_fact_safe_contract_gameplay_club
    ON fact_safe_contract_bridge(dataset_id, gameplay_club_id, activation_status);

CREATE INDEX IF NOT EXISTS idx_fact_safe_loan_review_status
    ON fact_safe_loan_review_queue(dataset_id, review_status);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (16);
