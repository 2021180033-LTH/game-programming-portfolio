CREATE TABLE IF NOT EXISTS save_owner_reputation_state (
    save_id INTEGER PRIMARY KEY NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    overall_score INTEGER NOT NULL CHECK (overall_score BETWEEN 0 AND 100),
    sporting_ambition INTEGER NOT NULL CHECK (sporting_ambition BETWEEN 0 AND 100),
    financial_prudence INTEGER NOT NULL CHECK (financial_prudence BETWEEN 0 AND 100),
    squad_loyalty INTEGER NOT NULL CHECK (squad_loyalty BETWEEN 0 AND 100),
    autonomy_respect INTEGER NOT NULL CHECK (autonomy_respect BETWEEN 0 AND 100),
    headline TEXT NOT NULL,
    updated_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS save_fan_tendency_state (
    save_id INTEGER PRIMARY KEY NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    trust_score INTEGER NOT NULL CHECK (trust_score BETWEEN 0 AND 100),
    patience_score INTEGER NOT NULL CHECK (patience_score BETWEEN 0 AND 100),
    skepticism_score INTEGER NOT NULL CHECK (skepticism_score BETWEEN 0 AND 100),
    star_signing_preference INTEGER NOT NULL CHECK (star_signing_preference BETWEEN 0 AND 100),
    homegrown_preference INTEGER NOT NULL CHECK (homegrown_preference BETWEEN 0 AND 100),
    youth_development_preference INTEGER NOT NULL CHECK (youth_development_preference BETWEEN 0 AND 100),
    balanced_squad_preference INTEGER NOT NULL CHECK (balanced_squad_preference BETWEEN 0 AND 100),
    mood TEXT NOT NULL
        CHECK (mood IN (
            'backing_owner',
            'cautiously_supportive',
            'cautiously_optimistic',
            'skeptical',
            'hostile'
        )),
    updated_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS save_owner_action_log (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    action_id TEXT NOT NULL,
    action_date TEXT NOT NULL,
    action_type TEXT NOT NULL
        CHECK (action_type IN (
            'approve_transfer',
            'reject_transfer',
            'target_scouting_directive',
            'range_scouting_directive',
            'set_squad_opinion',
            'clear_squad_opinion',
            'call_operating_meeting'
        )),
    category TEXT NOT NULL
        CHECK (category IN (
            'transfer_market',
            'scouting',
            'squad_planning',
            'owner_ops'
        )),
    subject_id TEXT NOT NULL,
    subject_name TEXT NOT NULL,
    summary TEXT NOT NULL,
    reputation_delta INTEGER NOT NULL CHECK (reputation_delta BETWEEN -100 AND 100),
    fan_trust_delta INTEGER NOT NULL CHECK (fan_trust_delta BETWEEN -100 AND 100),
    ambition_delta INTEGER NOT NULL CHECK (ambition_delta BETWEEN -100 AND 100),
    prudence_delta INTEGER NOT NULL CHECK (prudence_delta BETWEEN -100 AND 100),
    loyalty_delta INTEGER NOT NULL CHECK (loyalty_delta BETWEEN -100 AND 100),
    autonomy_delta INTEGER NOT NULL CHECK (autonomy_delta BETWEEN -100 AND 100),
    star_preference_delta INTEGER NOT NULL CHECK (star_preference_delta BETWEEN -100 AND 100),
    homegrown_preference_delta INTEGER NOT NULL CHECK (homegrown_preference_delta BETWEEN -100 AND 100),
    youth_preference_delta INTEGER NOT NULL CHECK (youth_preference_delta BETWEEN -100 AND 100),
    balanced_preference_delta INTEGER NOT NULL CHECK (balanced_preference_delta BETWEEN -100 AND 100),
    skepticism_delta INTEGER NOT NULL CHECK (skepticism_delta BETWEEN -100 AND 100),
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, action_id)
);

CREATE INDEX IF NOT EXISTS idx_save_owner_action_log_date
    ON save_owner_action_log(save_id, action_date DESC, action_id DESC);

CREATE INDEX IF NOT EXISTS idx_save_owner_action_log_category
    ON save_owner_action_log(save_id, category, action_type);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (17);
