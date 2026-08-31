PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS save_transfer_pursuit_details (
    save_id INTEGER NOT NULL,
    pursuit_id TEXT NOT NULL,
    detailed_stage TEXT NOT NULL
        CHECK (detailed_stage IN (
            'awaiting_club_response',
            'club_countered',
            'awaiting_player_terms',
            'awaiting_player_response',
            'player_countered',
            'awaiting_medical',
            'awaiting_registration',
            'awaiting_owner_approval',
            'awaiting_completion',
            'club_rejected',
            'player_rejected',
            'medical_failed',
            'registration_failed',
            'owner_rejected',
            'withdrawn',
            'completed',
            'expired'
        )),
    offer_upfront_pence INTEGER NOT NULL DEFAULT 0 CHECK (offer_upfront_pence >= 0),
    offer_installment_total_pence INTEGER NOT NULL DEFAULT 0
        CHECK (offer_installment_total_pence >= 0),
    offer_installment_count INTEGER NOT NULL DEFAULT 0
        CHECK (offer_installment_count BETWEEN 0 AND 4),
    offer_appearance_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (offer_appearance_bonus_pence >= 0),
    offer_appearance_threshold INTEGER NOT NULL DEFAULT 0
        CHECK (offer_appearance_threshold BETWEEN 0 AND 100),
    offer_sell_on_percentage INTEGER NOT NULL DEFAULT 0
        CHECK (offer_sell_on_percentage BETWEEN 0 AND 50),
    counter_upfront_pence INTEGER NOT NULL DEFAULT 0 CHECK (counter_upfront_pence >= 0),
    counter_installment_total_pence INTEGER NOT NULL DEFAULT 0
        CHECK (counter_installment_total_pence >= 0),
    counter_installment_count INTEGER NOT NULL DEFAULT 0
        CHECK (counter_installment_count BETWEEN 0 AND 4),
    counter_appearance_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (counter_appearance_bonus_pence >= 0),
    counter_appearance_threshold INTEGER NOT NULL DEFAULT 0
        CHECK (counter_appearance_threshold BETWEEN 0 AND 100),
    counter_sell_on_percentage INTEGER NOT NULL DEFAULT 0
        CHECK (counter_sell_on_percentage BETWEEN 0 AND 50),
    player_offer_weekly_wage_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_weekly_wage_pence >= 0),
    player_offer_contract_years INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_contract_years BETWEEN 0 AND 5),
    player_offer_signing_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_signing_bonus_pence >= 0),
    player_offer_loyalty_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_loyalty_bonus_pence >= 0),
    player_offer_appearance_fee_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_appearance_fee_pence >= 0),
    player_offer_goal_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_goal_bonus_pence >= 0),
    player_offer_agent_fee_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_agent_fee_pence >= 0),
    player_offer_release_clause_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_release_clause_pence >= 0),
    player_offer_squad_role TEXT NOT NULL DEFAULT 'First Team',
    player_counter_weekly_wage_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_weekly_wage_pence >= 0),
    player_counter_contract_years INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_contract_years BETWEEN 0 AND 5),
    player_counter_signing_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_signing_bonus_pence >= 0),
    player_counter_loyalty_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_loyalty_bonus_pence >= 0),
    player_counter_appearance_fee_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_appearance_fee_pence >= 0),
    player_counter_goal_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_goal_bonus_pence >= 0),
    player_counter_agent_fee_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_agent_fee_pence >= 0),
    player_counter_release_clause_pence INTEGER NOT NULL DEFAULT 0
        CHECK (player_counter_release_clause_pence >= 0),
    player_counter_squad_role TEXT NOT NULL DEFAULT 'First Team',
    player_negotiation_round INTEGER NOT NULL DEFAULT 0
        CHECK (player_negotiation_round >= 0),
    player_offer_submitted INTEGER NOT NULL DEFAULT 0
        CHECK (player_offer_submitted IN (0, 1)),
    medical_risk_percent INTEGER NOT NULL DEFAULT 0
        CHECK (medical_risk_percent BETWEEN 0 AND 100),
    medical_status TEXT NOT NULL DEFAULT 'not_started',
    registration_status TEXT NOT NULL DEFAULT 'not_checked',
    PRIMARY KEY (save_id, pursuit_id),
    FOREIGN KEY (save_id, pursuit_id)
        REFERENCES save_transfer_pursuits (save_id, pursuit_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS save_transfer_proposal_details (
    save_id INTEGER NOT NULL,
    proposal_id TEXT NOT NULL,
    upfront_fee_pence INTEGER NOT NULL DEFAULT 0 CHECK (upfront_fee_pence >= 0),
    installment_total_pence INTEGER NOT NULL DEFAULT 0
        CHECK (installment_total_pence >= 0),
    installment_count INTEGER NOT NULL DEFAULT 0
        CHECK (installment_count BETWEEN 0 AND 4),
    appearance_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (appearance_bonus_pence >= 0),
    appearance_threshold INTEGER NOT NULL DEFAULT 0
        CHECK (appearance_threshold BETWEEN 0 AND 100),
    sell_on_percentage INTEGER NOT NULL DEFAULT 0
        CHECK (sell_on_percentage BETWEEN 0 AND 50),
    signing_bonus_pence INTEGER NOT NULL DEFAULT 0 CHECK (signing_bonus_pence >= 0),
    loyalty_bonus_pence INTEGER NOT NULL DEFAULT 0 CHECK (loyalty_bonus_pence >= 0),
    appearance_fee_pence INTEGER NOT NULL DEFAULT 0 CHECK (appearance_fee_pence >= 0),
    goal_bonus_pence INTEGER NOT NULL DEFAULT 0 CHECK (goal_bonus_pence >= 0),
    agent_fee_pence INTEGER NOT NULL DEFAULT 0 CHECK (agent_fee_pence >= 0),
    release_clause_pence INTEGER NOT NULL DEFAULT 0 CHECK (release_clause_pence >= 0),
    squad_role TEXT NOT NULL DEFAULT 'First Team',
    medical_status TEXT NOT NULL DEFAULT 'not_checked',
    registration_status TEXT NOT NULL DEFAULT 'not_checked',
    PRIMARY KEY (save_id, proposal_id),
    FOREIGN KEY (save_id, proposal_id)
        REFERENCES save_transfer_proposals (save_id, proposal_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS save_outgoing_transfer_details (
    save_id INTEGER NOT NULL,
    offer_id TEXT NOT NULL,
    upfront_fee_pence INTEGER NOT NULL DEFAULT 0 CHECK (upfront_fee_pence >= 0),
    installment_total_pence INTEGER NOT NULL DEFAULT 0
        CHECK (installment_total_pence >= 0),
    installment_count INTEGER NOT NULL DEFAULT 0
        CHECK (installment_count BETWEEN 0 AND 4),
    appearance_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (appearance_bonus_pence >= 0),
    appearance_threshold INTEGER NOT NULL DEFAULT 0
        CHECK (appearance_threshold BETWEEN 0 AND 100),
    sell_on_percentage INTEGER NOT NULL DEFAULT 0
        CHECK (sell_on_percentage BETWEEN 0 AND 50),
    buyer_counter_upfront_pence INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_counter_upfront_pence >= 0),
    buyer_counter_installment_total_pence INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_counter_installment_total_pence >= 0),
    buyer_counter_installment_count INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_counter_installment_count BETWEEN 0 AND 4),
    buyer_counter_appearance_bonus_pence INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_counter_appearance_bonus_pence >= 0),
    buyer_counter_appearance_threshold INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_counter_appearance_threshold BETWEEN 0 AND 100),
    buyer_counter_sell_on_percentage INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_counter_sell_on_percentage BETWEEN 0 AND 50),
    negotiation_stage TEXT NOT NULL DEFAULT 'decision_pending'
        CHECK (negotiation_stage IN (
            'decision_pending',
            'awaiting_buyer_response',
            'buyer_countered',
            'agreement_reached',
            'closed'
        )),
    negotiation_round INTEGER NOT NULL DEFAULT 0 CHECK (negotiation_round >= 0),
    next_action_date TEXT,
    buyer_maximum_value_pence INTEGER NOT NULL DEFAULT 0
        CHECK (buyer_maximum_value_pence >= 0),
    PRIMARY KEY (save_id, offer_id),
    FOREIGN KEY (save_id, offer_id)
        REFERENCES save_outgoing_transfer_offers (save_id, offer_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS save_player_contract_details (
    save_id INTEGER NOT NULL,
    person_id TEXT NOT NULL,
    club_id TEXT NOT NULL,
    contract_years INTEGER NOT NULL DEFAULT 0 CHECK (contract_years BETWEEN 0 AND 5),
    signing_bonus_pence INTEGER NOT NULL DEFAULT 0 CHECK (signing_bonus_pence >= 0),
    loyalty_bonus_pence INTEGER NOT NULL DEFAULT 0 CHECK (loyalty_bonus_pence >= 0),
    appearance_fee_pence INTEGER NOT NULL DEFAULT 0 CHECK (appearance_fee_pence >= 0),
    goal_bonus_pence INTEGER NOT NULL DEFAULT 0 CHECK (goal_bonus_pence >= 0),
    agent_fee_pence INTEGER NOT NULL DEFAULT 0 CHECK (agent_fee_pence >= 0),
    release_clause_pence INTEGER NOT NULL DEFAULT 0 CHECK (release_clause_pence >= 0),
    PRIMARY KEY (save_id, person_id, club_id),
    FOREIGN KEY (save_id, person_id, club_id)
        REFERENCES save_player_contracts (save_id, person_id, club_id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS save_transfer_payments (
    save_id INTEGER NOT NULL REFERENCES save_games(id) ON DELETE CASCADE,
    payment_id TEXT NOT NULL,
    source_id TEXT NOT NULL,
    counterparty_club_id TEXT NOT NULL,
    counterparty_name TEXT NOT NULL,
    direction TEXT NOT NULL CHECK (direction IN ('payable', 'receivable')),
    category TEXT NOT NULL,
    due_date TEXT NOT NULL,
    amount_pence INTEGER NOT NULL CHECK (amount_pence > 0),
    status TEXT NOT NULL
        CHECK (status IN ('scheduled', 'settled', 'overdue', 'cancelled')),
    settled_date TEXT,
    description TEXT NOT NULL,
    PRIMARY KEY (save_id, payment_id)
);

CREATE INDEX IF NOT EXISTS idx_detailed_pursuit_stage
    ON save_transfer_pursuit_details (save_id, detailed_stage);
CREATE INDEX IF NOT EXISTS idx_transfer_payments_due
    ON save_transfer_payments (save_id, status, due_date);

INSERT OR IGNORE INTO schema_migrations (version) VALUES (5);
