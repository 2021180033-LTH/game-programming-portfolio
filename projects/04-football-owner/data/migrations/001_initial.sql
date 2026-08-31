PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS competitions (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    level INTEGER NOT NULL,
    active_detail_level TEXT NOT NULL
        CHECK (active_detail_level IN ('playable', 'detailed', 'view_only', 'background', 'inactive'))
);

CREATE TABLE IF NOT EXISTS clubs (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    short_name TEXT NOT NULL,
    country_code TEXT NOT NULL,
    competition_id TEXT NOT NULL REFERENCES competitions(id)
);

CREATE TABLE IF NOT EXISTS historical_club_snapshots (
    club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    cash_pence INTEGER,
    transfer_budget_pence INTEGER,
    weekly_wage_budget_pence INTEGER,
    weekly_wage_commitment_pence INTEGER,
    debt_pence INTEGER,
    reputation INTEGER NOT NULL CHECK (reputation BETWEEN 0 AND 100),
    squad_strength INTEGER NOT NULL CHECK (squad_strength BETWEEN 0 AND 100),
    data_confidence TEXT NOT NULL,
    source_note TEXT NOT NULL,
    PRIMARY KEY (club_id, snapshot_date)
);

CREATE TABLE IF NOT EXISTS people (
    id TEXT PRIMARY KEY,
    full_name TEXT NOT NULL,
    birth_date TEXT NOT NULL,
    nationality TEXT NOT NULL,
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS source_records (
    id TEXT PRIMARY KEY,
    publisher TEXT NOT NULL,
    title TEXT NOT NULL,
    url TEXT NOT NULL UNIQUE,
    accessed_date TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS data_assertions (
    id TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,
    entity_id TEXT NOT NULL,
    field_name TEXT NOT NULL,
    effective_date TEXT,
    source_id TEXT NOT NULL REFERENCES source_records(id),
    confidence TEXT NOT NULL,
    note TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS player_profiles (
    person_id TEXT PRIMARY KEY REFERENCES people(id),
    primary_position TEXT NOT NULL,
    secondary_positions TEXT NOT NULL DEFAULT '',
    profile_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS historical_registrations (
    person_id TEXT NOT NULL REFERENCES people(id),
    club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    squad_group TEXT NOT NULL,
    registration_status TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (person_id, club_id, snapshot_date)
);

CREATE TABLE IF NOT EXISTS historical_staff_roles (
    person_id TEXT NOT NULL REFERENCES people(id),
    club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    role TEXT NOT NULL,
    board_confidence INTEGER NOT NULL CHECK (board_confidence BETWEEN 0 AND 100),
    tactical_identity TEXT NOT NULL,
    current_assessment TEXT NOT NULL,
    PRIMARY KEY (person_id, club_id, snapshot_date, role)
);

CREATE TABLE IF NOT EXISTS fixtures (
    id TEXT PRIMARY KEY,
    competition_id TEXT NOT NULL REFERENCES competitions(id),
    fixture_date TEXT NOT NULL,
    home_club_id TEXT NOT NULL REFERENCES clubs(id),
    away_club_id TEXT NOT NULL REFERENCES clubs(id),
    home_strength_estimate INTEGER NOT NULL CHECK (home_strength_estimate BETWEEN 0 AND 100),
    away_strength_estimate INTEGER NOT NULL CHECK (away_strength_estimate BETWEEN 0 AND 100),
    simulation_confidence TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'scheduled'
        CHECK (status IN ('scheduled', 'postponed', 'cancelled'))
);

CREATE TABLE IF NOT EXISTS save_games (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    display_name TEXT NOT NULL,
    game_date TEXT NOT NULL,
    player_club_id TEXT NOT NULL REFERENCES clubs(id),
    owner_name TEXT NOT NULL,
    rng_seed INTEGER NOT NULL,
    ruleset_tag TEXT NOT NULL DEFAULT 'mvp-historical',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS save_club_deltas (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    club_id TEXT NOT NULL REFERENCES clubs(id),
    cash_pence INTEGER,
    transfer_budget_pence INTEGER,
    weekly_wage_budget_pence INTEGER,
    weekly_wage_commitment_pence INTEGER,
    debt_pence INTEGER,
    manager_board_confidence INTEGER,
    PRIMARY KEY (save_id, club_id)
);

CREATE TABLE IF NOT EXISTS save_fixture_deltas (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    fixture_id TEXT NOT NULL REFERENCES fixtures(id),
    played INTEGER NOT NULL CHECK (played IN (0, 1)),
    home_goals INTEGER NOT NULL,
    away_goals INTEGER NOT NULL,
    PRIMARY KEY (save_id, fixture_id)
);

CREATE TABLE IF NOT EXISTS save_inbox_items (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    item_id INTEGER NOT NULL,
    event_key TEXT NOT NULL,
    item_date TEXT NOT NULL,
    category TEXT NOT NULL,
    title TEXT NOT NULL,
    body TEXT NOT NULL,
    unread INTEGER NOT NULL CHECK (unread IN (0, 1)),
    requires_decision INTEGER NOT NULL CHECK (requires_decision IN (0, 1)),
    PRIMARY KEY (save_id, item_id),
    UNIQUE (save_id, event_key)
);

CREATE TABLE IF NOT EXISTS save_event_queue (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    event_id INTEGER NOT NULL,
    event_key TEXT NOT NULL,
    due_date TEXT NOT NULL,
    category TEXT NOT NULL,
    title TEXT NOT NULL,
    body TEXT NOT NULL,
    requires_decision INTEGER NOT NULL CHECK (requires_decision IN (0, 1)),
    processed INTEGER NOT NULL CHECK (processed IN (0, 1)),
    PRIMARY KEY (save_id, event_id),
    UNIQUE (save_id, event_key)
);

CREATE INDEX IF NOT EXISTS idx_registrations_club_snapshot
    ON historical_registrations (club_id, snapshot_date);
CREATE INDEX IF NOT EXISTS idx_fixtures_date
    ON fixtures (fixture_date);
CREATE INDEX IF NOT EXISTS idx_assertions_entity
    ON data_assertions (entity_type, entity_id, field_name);
CREATE INDEX IF NOT EXISTS idx_inbox_save_date
    ON save_inbox_items (save_id, item_date DESC);
CREATE INDEX IF NOT EXISTS idx_event_queue_due
    ON save_event_queue (save_id, processed, due_date);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (1);
