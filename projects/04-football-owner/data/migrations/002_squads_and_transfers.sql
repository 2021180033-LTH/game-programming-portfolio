PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS historical_registration_periods (
    person_id TEXT NOT NULL REFERENCES people(id),
    club_id TEXT NOT NULL REFERENCES clubs(id),
    joined_date TEXT,
    departed_date TEXT,
    period_confidence TEXT NOT NULL,
    PRIMARY KEY (person_id, club_id)
);

CREATE TABLE IF NOT EXISTS save_transfer_proposals (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    proposal_id TEXT NOT NULL,
    target_person_id TEXT NOT NULL REFERENCES people(id),
    from_club_id TEXT NOT NULL REFERENCES clubs(id),
    fee_pence INTEGER NOT NULL CHECK (fee_pence >= 0),
    weekly_wage_pence INTEGER NOT NULL CHECK (weekly_wage_pence >= 0),
    contract_years INTEGER NOT NULL CHECK (contract_years > 0),
    requested_by TEXT NOT NULL,
    rationale TEXT NOT NULL,
    proposed_date TEXT NOT NULL,
    decision_deadline TEXT NOT NULL,
    decision_date TEXT,
    join_date TEXT,
    status TEXT NOT NULL
        CHECK (status IN ('pending', 'approved', 'rejected', 'completed', 'expired')),
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, proposal_id)
);

CREATE TABLE IF NOT EXISTS save_player_registrations (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    person_id TEXT NOT NULL REFERENCES people(id),
    club_id TEXT NOT NULL REFERENCES clubs(id),
    squad_group TEXT NOT NULL,
    registration_status TEXT NOT NULL,
    joined_date TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    source_proposal_id TEXT,
    PRIMARY KEY (save_id, person_id, club_id)
);

CREATE INDEX IF NOT EXISTS idx_transfer_proposals_save_status
    ON save_transfer_proposals (save_id, status, decision_deadline);
CREATE INDEX IF NOT EXISTS idx_save_player_registrations
    ON save_player_registrations (save_id, club_id, squad_group);

UPDATE save_inbox_items
SET requires_decision = 0
WHERE event_key IN (
    'first-100-days',
    'finance-opening-review',
    'manager-opening-meeting'
);

UPDATE save_event_queue
SET requires_decision = 0
WHERE event_key IN (
    'finance-opening-review',
    'manager-opening-meeting'
);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (2);
