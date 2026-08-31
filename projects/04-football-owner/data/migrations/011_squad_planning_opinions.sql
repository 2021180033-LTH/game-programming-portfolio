CREATE TABLE IF NOT EXISTS save_squad_planning_opinions (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    opinion_id TEXT NOT NULL,
    season_id TEXT NOT NULL,
    club_id TEXT NOT NULL REFERENCES clubs(id),
    player_person_id TEXT REFERENCES people(id),
    position_group TEXT,
    source_role TEXT NOT NULL
        CHECK (source_role IN ('owner', 'manager', 'director')),
    opinion_type TEXT NOT NULL
        CHECK (opinion_type IN ('sale', 'release', 'reinforce', 'loan', 'keep', 'monitor')),
    priority INTEGER NOT NULL DEFAULT 3
        CHECK (priority BETWEEN 1 AND 5),
    reason TEXT NOT NULL,
    target_window TEXT NOT NULL,
    status TEXT NOT NULL
        CHECK (status IN ('active', 'resolved', 'dismissed', 'superseded')),
    created_date TEXT NOT NULL,
    updated_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, opinion_id),
    CHECK (
        player_person_id IS NOT NULL
        OR (position_group IS NOT NULL AND position_group <> '')
    )
);

CREATE INDEX IF NOT EXISTS idx_save_squad_planning_opinions_save_status
    ON save_squad_planning_opinions(save_id, status);

CREATE INDEX IF NOT EXISTS idx_save_squad_planning_opinions_player
    ON save_squad_planning_opinions(save_id, player_person_id, source_role, status);

CREATE INDEX IF NOT EXISTS idx_save_squad_planning_opinions_position
    ON save_squad_planning_opinions(save_id, position_group, source_role, status);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (11);
