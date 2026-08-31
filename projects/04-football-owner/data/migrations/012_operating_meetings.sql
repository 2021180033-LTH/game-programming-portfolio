CREATE TABLE IF NOT EXISTS save_operating_meeting_settings (
    save_id INTEGER PRIMARY KEY NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    cadence TEXT NOT NULL
        CHECK (cadence IN (
            'off',
            'urgent_only',
            'monthly',
            'quarterly',
            'transfer_windows',
            'season_milestones',
            'custom'
        )),
    attendee_mode TEXT NOT NULL
        CHECK (attendee_mode IN ('auto', 'manual')),
    include_manager INTEGER NOT NULL CHECK (include_manager IN (0, 1)),
    include_director INTEGER NOT NULL CHECK (include_director IN (0, 1)),
    include_finance INTEGER NOT NULL CHECK (include_finance IN (0, 1)),
    include_scouting INTEGER NOT NULL CHECK (include_scouting IN (0, 1)),
    include_medical INTEGER NOT NULL CHECK (include_medical IN (0, 1)),
    next_meeting_date TEXT,
    updated_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS save_operating_meetings (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    meeting_id TEXT NOT NULL,
    meeting_date TEXT NOT NULL,
    agenda_type TEXT NOT NULL
        CHECK (agenda_type IN (
            'general',
            'squad_planning',
            'transfer_market',
            'finance',
            'staff',
            'scouting',
            'crisis'
        )),
    status TEXT NOT NULL
        CHECK (status IN ('scheduled', 'held', 'cancelled')),
    attendee_mode TEXT NOT NULL
        CHECK (attendee_mode IN ('auto', 'manual')),
    summary TEXT NOT NULL,
    outcome_inbox_event_key TEXT NOT NULL,
    created_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, meeting_id)
);

CREATE TABLE IF NOT EXISTS save_operating_meeting_attendees (
    save_id INTEGER NOT NULL,
    meeting_id TEXT NOT NULL,
    attendee_role TEXT NOT NULL,
    display_name TEXT NOT NULL,
    auto_assigned INTEGER NOT NULL CHECK (auto_assigned IN (0, 1)),
    notes TEXT NOT NULL,
    PRIMARY KEY (save_id, meeting_id, attendee_role),
    FOREIGN KEY (save_id, meeting_id)
        REFERENCES save_operating_meetings(save_id, meeting_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_save_operating_meetings_date
    ON save_operating_meetings(save_id, meeting_date, status);

CREATE INDEX IF NOT EXISTS idx_save_operating_meetings_agenda
    ON save_operating_meetings(save_id, agenda_type, status);

INSERT OR IGNORE INTO schema_migrations(version) VALUES (12);
