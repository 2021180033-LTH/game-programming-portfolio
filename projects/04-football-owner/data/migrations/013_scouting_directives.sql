PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS save_scout_familiarities (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    scout_id TEXT NOT NULL REFERENCES scouting_staff_profiles(scout_id),
    category TEXT NOT NULL,
    familiarity_key TEXT NOT NULL,
    level INTEGER NOT NULL CHECK (level BETWEEN 0 AND 100),
    updated_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, scout_id, category, familiarity_key)
);

CREATE TABLE IF NOT EXISTS save_scouting_directives (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    directive_id TEXT NOT NULL,
    directive_type TEXT NOT NULL CHECK (directive_type IN ('range', 'target')),
    status TEXT NOT NULL CHECK (status IN ('active', 'completed', 'cancelled')),
    scout_id TEXT NOT NULL REFERENCES scouting_staff_profiles(scout_id),
    depth TEXT NOT NULL CHECK (depth IN ('quick', 'standard', 'comprehensive')),
    focus TEXT NOT NULL
        CHECK (focus IN ('overall', 'current_ability', 'potential', 'availability')),
    target_person_id TEXT REFERENCES people(id),
    generated_assignment_id TEXT,
    region TEXT NOT NULL DEFAULT '',
    country TEXT NOT NULL DEFAULT '',
    league TEXT NOT NULL DEFAULT '',
    position TEXT NOT NULL DEFAULT '',
    style TEXT NOT NULL DEFAULT '',
    min_age INTEGER NOT NULL DEFAULT 0 CHECK (min_age BETWEEN 0 AND 80),
    max_age INTEGER NOT NULL DEFAULT 0 CHECK (max_age BETWEEN 0 AND 80),
    homegrown_only INTEGER NOT NULL DEFAULT 0 CHECK (homegrown_only IN (0, 1)),
    max_budget_pence INTEGER NOT NULL DEFAULT 0 CHECK (max_budget_pence >= 0),
    matched_candidate_count INTEGER NOT NULL DEFAULT 0 CHECK (matched_candidate_count >= 0),
    matched_player_ids TEXT NOT NULL DEFAULT '',
    recommendation_score INTEGER NOT NULL DEFAULT 0
        CHECK (recommendation_score BETWEEN 0 AND 100),
    estimated_duration_days INTEGER NOT NULL DEFAULT 0
        CHECK (estimated_duration_days BETWEEN 0 AND 365),
    accuracy_modifier INTEGER NOT NULL DEFAULT 0
        CHECK (accuracy_modifier BETWEEN 0 AND 100),
    created_date TEXT NOT NULL,
    due_date TEXT NOT NULL,
    completed_date TEXT,
    summary TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, directive_id)
);

CREATE INDEX IF NOT EXISTS idx_save_scouting_directives_status
    ON save_scouting_directives(save_id, status, due_date);

CREATE INDEX IF NOT EXISTS idx_save_scout_familiarities_scout
    ON save_scout_familiarities(save_id, scout_id, category);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (13);
