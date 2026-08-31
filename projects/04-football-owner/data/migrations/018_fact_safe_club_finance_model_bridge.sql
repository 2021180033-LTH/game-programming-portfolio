PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS fact_safe_club_finance_bridge_imports (
    dataset_id TEXT PRIMARY KEY NOT NULL,
    version TEXT NOT NULL,
    source_dataset_id TEXT NOT NULL REFERENCES fact_safe_handoff_imports(dataset_id) ON DELETE CASCADE,
    source_file_path TEXT NOT NULL,
    imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL,
    club_model_rows INTEGER NOT NULL CHECK (club_model_rows >= 0),
    playable_model_rows INTEGER NOT NULL CHECK (playable_model_rows >= 0),
    active_finance_rows INTEGER NOT NULL CHECK (active_finance_rows >= 0),
    historical_snapshot_rows INTEGER NOT NULL CHECK (historical_snapshot_rows >= 0),
    active_gameplay_estimate_rows INTEGER NOT NULL CHECK (active_gameplay_estimate_rows >= 0),
    historical_fact_rows INTEGER NOT NULL CHECK (historical_fact_rows >= 0),
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS fact_safe_club_finance_bridge (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_club_finance_bridge_imports(dataset_id) ON DELETE CASCADE,
    fact_safe_club_id TEXT NOT NULL,
    gameplay_club_id TEXT REFERENCES clubs(id),
    club_name TEXT NOT NULL,
    competition_name TEXT NOT NULL,
    playable_status TEXT NOT NULL,
    runtime_scope TEXT NOT NULL,
    cash_gbp TEXT NOT NULL,
    cash_pence INTEGER NOT NULL CHECK (cash_pence >= 0),
    transfer_budget_pence INTEGER NOT NULL CHECK (transfer_budget_pence >= 0),
    weekly_wage_budget_pence INTEGER NOT NULL CHECK (weekly_wage_budget_pence >= 0),
    weekly_wage_commitment_pence INTEGER NOT NULL CHECK (weekly_wage_commitment_pence >= 0),
    debt_pence INTEGER NOT NULL CHECK (debt_pence >= 0),
    reputation_score INTEGER NOT NULL CHECK (reputation_score BETWEEN 0 AND 100),
    training_level INTEGER NOT NULL CHECK (training_level >= 0),
    finance_data_status TEXT NOT NULL,
    cash_gbp_fact_status TEXT NOT NULL,
    historical_finance_source_url TEXT NOT NULL,
    finance_model_fallback_policy TEXT NOT NULL,
    activation_status TEXT NOT NULL CHECK (activation_status IN (
        'activated_model_snapshot',
        'reference_only',
        'skipped_unmapped',
        'skipped_invalid_cash_model'
    )),
    skip_reason TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    source_url TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, fact_safe_club_id)
);

CREATE INDEX IF NOT EXISTS idx_fact_safe_club_finance_gameplay
    ON fact_safe_club_finance_bridge(dataset_id, gameplay_club_id, activation_status);

CREATE INDEX IF NOT EXISTS idx_fact_safe_club_finance_status
    ON fact_safe_club_finance_bridge(dataset_id, playable_status, runtime_scope, activation_status);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (18);
