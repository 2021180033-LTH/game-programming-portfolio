PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS transfer_rule_profiles (
    rule_profile_id TEXT PRIMARY KEY,
    competition_id TEXT NOT NULL REFERENCES competitions(id),
    effective_from TEXT NOT NULL,
    effective_to TEXT,
    transfer_windows_active INTEGER NOT NULL
        CHECK (transfer_windows_active IN (0, 1)),
    registration_cutoff_month INTEGER NOT NULL
        CHECK (registration_cutoff_month BETWEEN 1 AND 12),
    registration_cutoff_day INTEGER NOT NULL
        CHECK (registration_cutoff_day BETWEEN 1 AND 31),
    free_movement_after_contract INTEGER NOT NULL
        CHECK (free_movement_after_contract IN (0, 1)),
    domestic_loan_model TEXT NOT NULL,
    data_confidence TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS save_loan_deals (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    loan_id TEXT NOT NULL,
    person_id TEXT NOT NULL REFERENCES people(id),
    direction TEXT NOT NULL CHECK (direction IN ('incoming', 'outgoing')),
    parent_club_id TEXT NOT NULL REFERENCES clubs(id),
    loan_club_id TEXT NOT NULL REFERENCES clubs(id),
    purpose TEXT NOT NULL,
    proposed_date TEXT NOT NULL,
    decision_deadline TEXT NOT NULL,
    planned_start_date TEXT NOT NULL,
    stage TEXT NOT NULL CHECK (stage IN (
        'decision_pending',
        'awaiting_club_response',
        'club_countered',
        'awaiting_start',
        'active',
        'recall_pending',
        'recalled',
        'completed',
        'rejected',
        'withdrawn',
        'expired'
    )),
    loan_fee_pence INTEGER NOT NULL CHECK (loan_fee_pence >= 0),
    borrowing_wage_percentage INTEGER NOT NULL
        CHECK (borrowing_wage_percentage BETWEEN 0 AND 100),
    promised_role TEXT NOT NULL,
    end_date TEXT NOT NULL,
    can_recall INTEGER NOT NULL CHECK (can_recall IN (0, 1)),
    recall_notice_days INTEGER NOT NULL CHECK (recall_notice_days BETWEEN 0 AND 30),
    future_fee_type TEXT NOT NULL
        CHECK (future_fee_type IN ('none', 'option', 'mandatory')),
    future_fee_pence INTEGER NOT NULL CHECK (future_fee_pence >= 0),
    parent_match_restricted INTEGER NOT NULL
        CHECK (parent_match_restricted IN (0, 1)),
    unused_player_fee_pence INTEGER NOT NULL CHECK (unused_player_fee_pence >= 0),
    treatment_responsibility TEXT NOT NULL
        CHECK (treatment_responsibility IN ('parent_club', 'loan_club', 'shared')),
    counter_loan_fee_pence INTEGER NOT NULL CHECK (counter_loan_fee_pence >= 0),
    counter_borrowing_wage_percentage INTEGER NOT NULL
        CHECK (counter_borrowing_wage_percentage BETWEEN 0 AND 100),
    counter_promised_role TEXT NOT NULL,
    counter_end_date TEXT NOT NULL,
    counter_can_recall INTEGER NOT NULL CHECK (counter_can_recall IN (0, 1)),
    counter_recall_notice_days INTEGER NOT NULL
        CHECK (counter_recall_notice_days BETWEEN 0 AND 30),
    counter_future_fee_type TEXT NOT NULL
        CHECK (counter_future_fee_type IN ('none', 'option', 'mandatory')),
    counter_future_fee_pence INTEGER NOT NULL CHECK (counter_future_fee_pence >= 0),
    counter_parent_match_restricted INTEGER NOT NULL
        CHECK (counter_parent_match_restricted IN (0, 1)),
    counter_unused_player_fee_pence INTEGER NOT NULL
        CHECK (counter_unused_player_fee_pence >= 0),
    counter_treatment_responsibility TEXT NOT NULL
        CHECK (counter_treatment_responsibility IN ('parent_club', 'loan_club', 'shared')),
    next_action_date TEXT,
    actual_start_date TEXT,
    actual_end_date TEXT,
    last_report_date TEXT,
    parent_squad_group TEXT NOT NULL,
    negotiation_round INTEGER NOT NULL CHECK (negotiation_round >= 0),
    appearances INTEGER NOT NULL CHECK (appearances >= 0),
    starts INTEGER NOT NULL CHECK (starts >= 0),
    response_note TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, loan_id)
);

CREATE TABLE IF NOT EXISTS save_contract_renewals (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    renewal_id TEXT NOT NULL,
    person_id TEXT NOT NULL REFERENCES people(id),
    player_name TEXT NOT NULL,
    requested_by TEXT NOT NULL,
    rationale TEXT NOT NULL,
    current_contract_end_date TEXT NOT NULL,
    decision_deadline TEXT NOT NULL,
    stage TEXT NOT NULL CHECK (stage IN (
        'recommended',
        'awaiting_player_response',
        'player_countered',
        'agreement_reached',
        'completed',
        'rejected',
        'withdrawn',
        'expired'
    )),
    recommended_weekly_wage_pence INTEGER NOT NULL CHECK (recommended_weekly_wage_pence >= 0),
    recommended_contract_years INTEGER NOT NULL CHECK (recommended_contract_years BETWEEN 0 AND 5),
    recommended_signing_bonus_pence INTEGER NOT NULL CHECK (recommended_signing_bonus_pence >= 0),
    recommended_loyalty_bonus_pence INTEGER NOT NULL CHECK (recommended_loyalty_bonus_pence >= 0),
    recommended_appearance_fee_pence INTEGER NOT NULL CHECK (recommended_appearance_fee_pence >= 0),
    recommended_goal_bonus_pence INTEGER NOT NULL CHECK (recommended_goal_bonus_pence >= 0),
    recommended_agent_fee_pence INTEGER NOT NULL CHECK (recommended_agent_fee_pence >= 0),
    recommended_release_clause_pence INTEGER NOT NULL CHECK (recommended_release_clause_pence >= 0),
    recommended_squad_role TEXT NOT NULL,
    offer_weekly_wage_pence INTEGER NOT NULL CHECK (offer_weekly_wage_pence >= 0),
    offer_contract_years INTEGER NOT NULL CHECK (offer_contract_years BETWEEN 0 AND 5),
    offer_signing_bonus_pence INTEGER NOT NULL CHECK (offer_signing_bonus_pence >= 0),
    offer_loyalty_bonus_pence INTEGER NOT NULL CHECK (offer_loyalty_bonus_pence >= 0),
    offer_appearance_fee_pence INTEGER NOT NULL CHECK (offer_appearance_fee_pence >= 0),
    offer_goal_bonus_pence INTEGER NOT NULL CHECK (offer_goal_bonus_pence >= 0),
    offer_agent_fee_pence INTEGER NOT NULL CHECK (offer_agent_fee_pence >= 0),
    offer_release_clause_pence INTEGER NOT NULL CHECK (offer_release_clause_pence >= 0),
    offer_squad_role TEXT NOT NULL,
    counter_weekly_wage_pence INTEGER NOT NULL CHECK (counter_weekly_wage_pence >= 0),
    counter_contract_years INTEGER NOT NULL CHECK (counter_contract_years BETWEEN 0 AND 5),
    counter_signing_bonus_pence INTEGER NOT NULL CHECK (counter_signing_bonus_pence >= 0),
    counter_loyalty_bonus_pence INTEGER NOT NULL CHECK (counter_loyalty_bonus_pence >= 0),
    counter_appearance_fee_pence INTEGER NOT NULL CHECK (counter_appearance_fee_pence >= 0),
    counter_goal_bonus_pence INTEGER NOT NULL CHECK (counter_goal_bonus_pence >= 0),
    counter_agent_fee_pence INTEGER NOT NULL CHECK (counter_agent_fee_pence >= 0),
    counter_release_clause_pence INTEGER NOT NULL CHECK (counter_release_clause_pence >= 0),
    counter_squad_role TEXT NOT NULL,
    next_action_date TEXT,
    last_update_date TEXT,
    signed_date TEXT,
    player_interest INTEGER NOT NULL CHECK (player_interest BETWEEN 0 AND 100),
    agent_patience INTEGER NOT NULL CHECK (agent_patience BETWEEN 1 AND 10),
    negotiation_round INTEGER NOT NULL CHECK (negotiation_round >= 0),
    response_note TEXT NOT NULL,
    data_confidence TEXT NOT NULL,
    PRIMARY KEY (save_id, renewal_id)
);

CREATE TABLE IF NOT EXISTS save_player_contract_lifecycle (
    save_id INTEGER NOT NULL,
    person_id TEXT NOT NULL,
    club_id TEXT NOT NULL,
    lifecycle_status TEXT NOT NULL CHECK (lifecycle_status IN (
        'active',
        'expired_retained',
        'expired_free_agent'
    )),
    expiry_notified INTEGER NOT NULL CHECK (expiry_notified IN (0, 1)),
    PRIMARY KEY (save_id, person_id, club_id),
    FOREIGN KEY (save_id, person_id, club_id)
        REFERENCES save_player_contracts (save_id, person_id, club_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_loan_deals_next_action
    ON save_loan_deals (save_id, stage, next_action_date, decision_deadline);
CREATE INDEX IF NOT EXISTS idx_contract_renewals_next_action
    ON save_contract_renewals (save_id, stage, next_action_date, decision_deadline);
CREATE INDEX IF NOT EXISTS idx_contract_lifecycle
    ON save_player_contract_lifecycle (save_id, lifecycle_status);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (7);
