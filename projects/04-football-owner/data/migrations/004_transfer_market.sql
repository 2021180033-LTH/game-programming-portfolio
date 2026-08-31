PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS transfer_market_profiles (
    person_id TEXT PRIMARY KEY REFERENCES people(id),
    current_club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    recommendation_rank INTEGER NOT NULL CHECK (recommendation_rank > 0),
    current_ability_low INTEGER NOT NULL CHECK (current_ability_low BETWEEN 0 AND 100),
    current_ability_high INTEGER NOT NULL CHECK (current_ability_high BETWEEN current_ability_low AND 100),
    potential_low INTEGER NOT NULL CHECK (potential_low BETWEEN 0 AND 100),
    potential_high INTEGER NOT NULL CHECK (potential_high BETWEEN potential_low AND 100),
    manager_priority INTEGER NOT NULL CHECK (manager_priority BETWEEN 0 AND 100),
    player_interest INTEGER NOT NULL CHECK (player_interest BETWEEN 0 AND 100),
    market_value_pence INTEGER NOT NULL CHECK (market_value_pence >= 0),
    opening_offer_pence INTEGER NOT NULL CHECK (opening_offer_pence >= 0),
    seller_minimum_pence INTEGER NOT NULL CHECK (seller_minimum_pence >= 0),
    asking_price_pence INTEGER NOT NULL CHECK (asking_price_pence >= seller_minimum_pence),
    expected_weekly_wage_pence INTEGER NOT NULL CHECK (expected_weekly_wage_pence >= 0),
    expected_contract_years INTEGER NOT NULL CHECK (expected_contract_years > 0),
    role_fit TEXT NOT NULL,
    scout_summary TEXT NOT NULL,
    availability TEXT NOT NULL,
    rival_club_id TEXT REFERENCES clubs(id),
    rival_bid_pence INTEGER NOT NULL DEFAULT 0 CHECK (rival_bid_pence >= 0),
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS historical_player_contracts (
    person_id TEXT NOT NULL REFERENCES people(id),
    club_id TEXT NOT NULL REFERENCES clubs(id),
    snapshot_date TEXT NOT NULL,
    contract_start_date TEXT,
    contract_end_date TEXT,
    weekly_wage_pence INTEGER,
    squad_role TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (person_id, club_id, snapshot_date)
);

CREATE TABLE IF NOT EXISTS save_transfer_pursuits (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    pursuit_id TEXT NOT NULL,
    target_person_id TEXT NOT NULL REFERENCES people(id),
    stage TEXT NOT NULL
        CHECK (stage IN (
            'awaiting_club_response',
            'club_countered',
            'awaiting_player_terms',
            'awaiting_owner_approval',
            'awaiting_completion',
            'club_rejected',
            'player_rejected',
            'owner_rejected',
            'withdrawn',
            'completed',
            'expired'
        )),
    current_offer_pence INTEGER NOT NULL CHECK (current_offer_pence >= 0),
    club_counter_pence INTEGER NOT NULL CHECK (club_counter_pence >= 0),
    negotiation_round INTEGER NOT NULL CHECK (negotiation_round >= 0),
    started_date TEXT NOT NULL,
    next_action_date TEXT NOT NULL,
    decision_deadline TEXT NOT NULL,
    last_update_date TEXT,
    response_note TEXT NOT NULL,
    rival_bid_active INTEGER NOT NULL CHECK (rival_bid_active IN (0, 1)),
    rival_club_id TEXT REFERENCES clubs(id),
    rival_bid_pence INTEGER NOT NULL DEFAULT 0 CHECK (rival_bid_pence >= 0),
    linked_proposal_id TEXT,
    PRIMARY KEY (save_id, pursuit_id)
);

CREATE TABLE IF NOT EXISTS save_outgoing_transfer_offers (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    offer_id TEXT NOT NULL,
    player_person_id TEXT NOT NULL REFERENCES people(id),
    buyer_club_id TEXT NOT NULL REFERENCES clubs(id),
    fee_pence INTEGER NOT NULL CHECK (fee_pence >= 0),
    weekly_wage_relief_pence INTEGER NOT NULL DEFAULT 0
        CHECK (weekly_wage_relief_pence >= 0),
    proposed_date TEXT NOT NULL,
    decision_deadline TEXT NOT NULL,
    decision_date TEXT,
    completion_date TEXT,
    status TEXT NOT NULL
        CHECK (status IN ('pending', 'accepted', 'rejected', 'completed', 'expired')),
    requested_role TEXT NOT NULL,
    rationale TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, offer_id)
);

CREATE TABLE IF NOT EXISTS save_player_contracts (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    person_id TEXT NOT NULL REFERENCES people(id),
    club_id TEXT NOT NULL REFERENCES clubs(id),
    contract_start_date TEXT NOT NULL,
    contract_end_date TEXT NOT NULL,
    weekly_wage_pence INTEGER NOT NULL CHECK (weekly_wage_pence >= 0),
    squad_role TEXT NOT NULL,
    source_proposal_id TEXT,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, person_id, club_id)
);

CREATE TABLE IF NOT EXISTS save_player_departures (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    person_id TEXT NOT NULL REFERENCES people(id),
    from_club_id TEXT NOT NULL REFERENCES clubs(id),
    to_club_id TEXT NOT NULL REFERENCES clubs(id),
    departure_date TEXT NOT NULL,
    source_offer_id TEXT NOT NULL,
    PRIMARY KEY (save_id, person_id, from_club_id)
);

CREATE INDEX IF NOT EXISTS idx_transfer_market_rank
    ON transfer_market_profiles (snapshot_date, recommendation_rank);
CREATE INDEX IF NOT EXISTS idx_transfer_pursuits_status
    ON save_transfer_pursuits (save_id, stage, next_action_date);
CREATE INDEX IF NOT EXISTS idx_outgoing_offers_status
    ON save_outgoing_transfer_offers (save_id, status, decision_deadline);
CREATE INDEX IF NOT EXISTS idx_player_departures
    ON save_player_departures (save_id, from_club_id, departure_date);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (4);
