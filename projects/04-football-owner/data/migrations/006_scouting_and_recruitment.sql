PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS scouting_staff_profiles (
    scout_id TEXT PRIMARY KEY,
    club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    full_name TEXT NOT NULL,
    role TEXT NOT NULL,
    region_specialism TEXT NOT NULL,
    judging_current_ability INTEGER NOT NULL
        CHECK (judging_current_ability BETWEEN 0 AND 100),
    judging_potential INTEGER NOT NULL
        CHECK (judging_potential BETWEEN 0 AND 100),
    adaptability INTEGER NOT NULL
        CHECK (adaptability BETWEEN 0 AND 100),
    assignment_capacity INTEGER NOT NULL
        CHECK (assignment_capacity BETWEEN 1 AND 10),
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS initial_scouting_knowledge (
    club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    person_id TEXT NOT NULL REFERENCES people(id),
    scout_id TEXT NOT NULL REFERENCES scouting_staff_profiles(scout_id),
    knowledge_level INTEGER NOT NULL CHECK (knowledge_level BETWEEN 0 AND 100),
    focus TEXT NOT NULL
        CHECK (focus IN ('overall', 'current_ability', 'potential', 'availability')),
    last_observed_date TEXT NOT NULL,
    PRIMARY KEY (club_id, snapshot_date, person_id)
);

CREATE TABLE IF NOT EXISTS save_scouting_reports (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    person_id TEXT NOT NULL REFERENCES people(id),
    knowledge_level INTEGER NOT NULL CHECK (knowledge_level BETWEEN 0 AND 100),
    current_ability_low INTEGER NOT NULL CHECK (current_ability_low BETWEEN 0 AND 100),
    current_ability_high INTEGER NOT NULL
        CHECK (current_ability_high BETWEEN current_ability_low AND 100),
    potential_low INTEGER NOT NULL CHECK (potential_low BETWEEN 0 AND 100),
    potential_high INTEGER NOT NULL
        CHECK (potential_high BETWEEN potential_low AND 100),
    market_value_low_pence INTEGER NOT NULL CHECK (market_value_low_pence >= 0),
    market_value_high_pence INTEGER NOT NULL
        CHECK (market_value_high_pence >= market_value_low_pence),
    asking_price_low_pence INTEGER NOT NULL CHECK (asking_price_low_pence >= 0),
    asking_price_high_pence INTEGER NOT NULL
        CHECK (asking_price_high_pence >= asking_price_low_pence),
    expected_wage_low_pence INTEGER NOT NULL CHECK (expected_wage_low_pence >= 0),
    expected_wage_high_pence INTEGER NOT NULL
        CHECK (expected_wage_high_pence >= expected_wage_low_pence),
    role_fit TEXT NOT NULL,
    summary TEXT NOT NULL,
    availability TEXT NOT NULL,
    last_observed_date TEXT NOT NULL,
    valid_until TEXT NOT NULL,
    scout_id TEXT NOT NULL REFERENCES scouting_staff_profiles(scout_id),
    focus TEXT NOT NULL
        CHECK (focus IN ('overall', 'current_ability', 'potential', 'availability')),
    report_version INTEGER NOT NULL CHECK (report_version > 0),
    stale_notified INTEGER NOT NULL DEFAULT 0 CHECK (stale_notified IN (0, 1)),
    PRIMARY KEY (save_id, person_id)
);

CREATE TABLE IF NOT EXISTS save_scouting_assignments (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    assignment_id TEXT NOT NULL,
    person_id TEXT NOT NULL REFERENCES people(id),
    scout_id TEXT NOT NULL REFERENCES scouting_staff_profiles(scout_id),
    depth TEXT NOT NULL CHECK (depth IN ('quick', 'standard', 'comprehensive')),
    focus TEXT NOT NULL
        CHECK (focus IN ('overall', 'current_ability', 'potential', 'availability')),
    started_date TEXT NOT NULL,
    next_update_date TEXT NOT NULL,
    due_date TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('active', 'completed', 'cancelled')),
    starting_knowledge INTEGER NOT NULL CHECK (starting_knowledge BETWEEN 0 AND 100),
    target_knowledge INTEGER NOT NULL CHECK (target_knowledge BETWEEN 0 AND 100),
    last_milestone INTEGER NOT NULL CHECK (last_milestone IN (0, 50, 100)),
    completed_date TEXT,
    PRIMARY KEY (save_id, assignment_id)
);

CREATE TABLE IF NOT EXISTS save_recruitment_plans (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    plan_id TEXT NOT NULL,
    title TEXT NOT NULL,
    requested_by TEXT NOT NULL,
    description TEXT NOT NULL,
    start_date TEXT NOT NULL,
    evaluation_date TEXT NOT NULL,
    target_signings INTEGER NOT NULL CHECK (target_signings > 0),
    minimum_acceptable_signings INTEGER NOT NULL
        CHECK (
            minimum_acceptable_signings >= 0
            AND minimum_acceptable_signings <= target_signings
        ),
    status TEXT NOT NULL
        CHECK (status IN ('active', 'achieved', 'acceptable', 'under_delivered', 'failed')),
    confidence_delta INTEGER NOT NULL DEFAULT 0
        CHECK (confidence_delta BETWEEN -100 AND 100),
    evaluated_date TEXT,
    PRIMARY KEY (save_id, plan_id)
);

CREATE INDEX IF NOT EXISTS idx_scouting_assignments_due
    ON save_scouting_assignments (save_id, status, next_update_date, due_date);
CREATE INDEX IF NOT EXISTS idx_scouting_reports_freshness
    ON save_scouting_reports (save_id, valid_until, stale_notified);
CREATE INDEX IF NOT EXISTS idx_recruitment_plans_evaluation
    ON save_recruitment_plans (save_id, status, evaluation_date);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (6);
