CREATE TABLE IF NOT EXISTS fact_safe_competition_bridge_imports (
    dataset_id TEXT PRIMARY KEY NOT NULL,
    version TEXT NOT NULL,
    source_dataset_id TEXT NOT NULL,
    imported_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status TEXT NOT NULL,
    competition_definitions INTEGER NOT NULL CHECK (competition_definitions >= 0),
    competition_id_bridges INTEGER NOT NULL CHECK (competition_id_bridges >= 0),
    competition_seasons INTEGER NOT NULL CHECK (competition_seasons >= 0),
    competition_participants INTEGER NOT NULL CHECK (competition_participants >= 0),
    fixture_seed_rows INTEGER NOT NULL CHECK (fixture_seed_rows >= 0),
    active_fixture_rows INTEGER NOT NULL CHECK (active_fixture_rows >= 0),
    premier_league_fixture_rows INTEGER NOT NULL CHECK (premier_league_fixture_rows >= 0),
    charity_shield_fixture_rows INTEGER NOT NULL CHECK (charity_shield_fixture_rows >= 0),
    result_seeded_rows INTEGER NOT NULL CHECK (result_seeded_rows >= 0),
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS fact_safe_competition_id_bridge (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_competition_bridge_imports(dataset_id) ON DELETE CASCADE,
    fact_safe_competition_id TEXT NOT NULL,
    fact_safe_season_id TEXT NOT NULL,
    gameplay_competition_id TEXT NOT NULL REFERENCES competitions(id),
    gameplay_season_id TEXT NOT NULL REFERENCES competition_seasons(id),
    competition_name TEXT NOT NULL,
    competition_type TEXT NOT NULL,
    runtime_seed_policy TEXT NOT NULL,
    bridge_status TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, fact_safe_competition_id)
);

CREATE TABLE IF NOT EXISTS fact_safe_fixture_id_bridge (
    dataset_id TEXT NOT NULL REFERENCES fact_safe_competition_bridge_imports(dataset_id) ON DELETE CASCADE,
    fact_safe_fixture_id TEXT NOT NULL,
    gameplay_fixture_id TEXT NOT NULL REFERENCES fixtures(id),
    fact_safe_competition_id TEXT NOT NULL,
    gameplay_competition_id TEXT NOT NULL REFERENCES competitions(id),
    gameplay_season_id TEXT NOT NULL REFERENCES competition_seasons(id),
    match_date TEXT NOT NULL,
    home_locked_club_id TEXT NOT NULL,
    away_locked_club_id TEXT NOT NULL,
    runtime_result_policy TEXT NOT NULL,
    bridge_status TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (dataset_id, fact_safe_fixture_id),
    UNIQUE (dataset_id, gameplay_fixture_id)
);

CREATE INDEX IF NOT EXISTS idx_fact_safe_fixture_bridge_gameplay
    ON fact_safe_fixture_id_bridge(dataset_id, gameplay_fixture_id);

CREATE INDEX IF NOT EXISTS idx_fixtures_competition_date
    ON fixtures(competition_id, fixture_date);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (15);
