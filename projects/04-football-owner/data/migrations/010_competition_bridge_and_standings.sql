CREATE TABLE IF NOT EXISTS world_club_id_bridge (
    locked_club_id TEXT PRIMARY KEY NOT NULL,
    gameplay_club_id TEXT NOT NULL UNIQUE,
    club_name TEXT NOT NULL,
    short_name TEXT NOT NULL,
    competition_id TEXT NOT NULL,
    competition_name TEXT NOT NULL,
    tier INTEGER NOT NULL,
    playable_status TEXT NOT NULL,
    runtime_scope TEXT NOT NULL,
    bridge_status TEXT NOT NULL
        CHECK (bridge_status IN ('mapped_existing', 'reserved_from_locked_id', 'reference_only')),
    data_confidence TEXT NOT NULL,
    source_url TEXT NOT NULL,
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS world_person_id_bridge (
    locked_person_id TEXT PRIMARY KEY NOT NULL,
    gameplay_person_id TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    date_of_birth TEXT,
    active_start_locked_club_id TEXT,
    active_start_gameplay_club_id TEXT,
    runtime_scope TEXT NOT NULL,
    bridge_status TEXT NOT NULL
        CHECK (bridge_status IN ('mapped_existing', 'reserved_from_locked_id', 'reference_only', 'inactive')),
    data_confidence TEXT NOT NULL,
    source_url TEXT NOT NULL,
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS competition_seasons (
    id TEXT PRIMARY KEY,
    competition_id TEXT NOT NULL REFERENCES competitions(id),
    season_name TEXT NOT NULL,
    start_date TEXT NOT NULL,
    end_date TEXT NOT NULL,
    status TEXT NOT NULL
        CHECK (status IN ('planned', 'active', 'completed', 'archived')),
    source_status TEXT NOT NULL,
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS season_rule_profiles (
    id TEXT PRIMARY KEY,
    season_id TEXT NOT NULL UNIQUE REFERENCES competition_seasons(id) ON DELETE CASCADE,
    points_for_win INTEGER NOT NULL,
    points_for_draw INTEGER NOT NULL,
    points_for_loss INTEGER NOT NULL,
    ranking_tiebreakers TEXT NOT NULL,
    promotion_slots INTEGER NOT NULL,
    playoff_slots INTEGER NOT NULL,
    relegation_slots INTEGER NOT NULL,
    registration_deadline TEXT NOT NULL,
    transfer_windows_active INTEGER NOT NULL CHECK (transfer_windows_active IN (0, 1)),
    source_status TEXT NOT NULL,
    notes TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS competition_season_teams (
    season_id TEXT NOT NULL REFERENCES competition_seasons(id) ON DELETE CASCADE,
    club_id TEXT NOT NULL REFERENCES clubs(id),
    source_locked_club_id TEXT NOT NULL,
    seed_order INTEGER NOT NULL,
    starting_points INTEGER NOT NULL DEFAULT 0,
    source_status TEXT NOT NULL,
    notes TEXT NOT NULL,
    PRIMARY KEY (season_id, club_id),
    UNIQUE (season_id, source_locked_club_id),
    UNIQUE (season_id, seed_order)
);

CREATE TABLE IF NOT EXISTS fixture_generation_profiles (
    id TEXT PRIMARY KEY,
    season_id TEXT NOT NULL UNIQUE REFERENCES competition_seasons(id) ON DELETE CASCADE,
    method TEXT NOT NULL,
    rng_seed INTEGER NOT NULL,
    generated_fixture_count INTEGER NOT NULL DEFAULT 0,
    source_status TEXT NOT NULL,
    notes TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_world_club_bridge_gameplay
    ON world_club_id_bridge (gameplay_club_id);
CREATE INDEX IF NOT EXISTS idx_world_club_bridge_competition
    ON world_club_id_bridge (competition_id, playable_status);
CREATE INDEX IF NOT EXISTS idx_world_person_bridge_gameplay
    ON world_person_id_bridge (gameplay_person_id);
CREATE INDEX IF NOT EXISTS idx_world_person_bridge_start_club
    ON world_person_id_bridge (active_start_gameplay_club_id);
CREATE INDEX IF NOT EXISTS idx_competition_season_teams_club
    ON competition_season_teams (club_id);

INSERT OR IGNORE INTO competitions (
    id, name, country_code, level, active_detail_level
) VALUES
    ('eng-premier-league', 'Premier League', 'ENG', 1, 'playable'),
    ('eng-first-division', 'Football League First Division', 'ENG', 2, 'detailed');

INSERT OR REPLACE INTO world_club_id_bridge (
    locked_club_id, gameplay_club_id, club_name, short_name,
    competition_id, competition_name, tier, playable_status, runtime_scope,
    bridge_status, data_confidence, source_url, notes
)
SELECT
    locked.club_id,
    COALESCE(existing.id, 'fo92-' || LOWER(locked.club_id)),
    locked.club_name,
    COALESCE(existing.short_name, locked.club_name),
    CASE locked.competition_name
        WHEN 'FA Premier League' THEN 'eng-premier-league'
        WHEN 'Football League First Division' THEN 'eng-first-division'
        ELSE 'fo92-reference'
    END,
    locked.competition_name,
    locked.tier,
    locked.playable_status,
    locked.runtime_scope,
    CASE
        WHEN locked.playable_status <> 'playable' THEN 'reference_only'
        WHEN existing.id IS NOT NULL THEN 'mapped_existing'
        ELSE 'reserved_from_locked_id'
    END,
    locked.confidence,
    COALESCE(locked.source_url, ''),
    CASE
        WHEN existing.id IS NOT NULL THEN 'Existing gameplay club id preserved from clubs.id.'
        WHEN locked.playable_status = 'playable' THEN 'Stable gameplay id reserved from LOCKED v11 club_id.'
        ELSE 'Reference-only club excluded from active competition and AI loops.'
    END
FROM fo92_locked_clubs AS locked
LEFT JOIN clubs AS existing ON existing.name = locked.club_name;

INSERT OR IGNORE INTO clubs (
    id, name, short_name, country_code, competition_id
)
SELECT
    bridge.gameplay_club_id,
    bridge.club_name,
    bridge.short_name,
    'ENG',
    bridge.competition_id
FROM world_club_id_bridge AS bridge
WHERE bridge.playable_status = 'playable'
  AND bridge.competition_id IN ('eng-premier-league', 'eng-first-division');

INSERT OR REPLACE INTO competition_seasons (
    id, competition_id, season_name, start_date, end_date, status, source_status, notes
) VALUES
    (
        'eng-premier-league-1992-93',
        'eng-premier-league',
        '1992-93',
        '1992-07-01',
        '1993-06-30',
        'active',
        'locked_v11_participants_fixture_source_pending',
        'Participants come from LOCKED v11. Full fixture/results source is pending.'
    ),
    (
        'eng-first-division-1992-93',
        'eng-first-division',
        '1992-93',
        '1992-07-01',
        '1993-06-30',
        'active',
        'locked_v11_participants_fixture_source_pending',
        'Participants come from LOCKED v11. Full fixture/results source is pending.'
    );

INSERT OR REPLACE INTO season_rule_profiles (
    id, season_id, points_for_win, points_for_draw, points_for_loss,
    ranking_tiebreakers, promotion_slots, playoff_slots, relegation_slots,
    registration_deadline, transfer_windows_active, source_status, notes
) VALUES
    (
        'eng-premier-league-1992-93-rules',
        'eng-premier-league-1992-93',
        3,
        1,
        0,
        'points,goal_difference,goals_for,club_name',
        0,
        0,
        3,
        '1993-03-31',
        0,
        'competition_rules_provisional_pending_source_pack',
        '1992-93 uses the pre-window registration deadline already modeled by era rules.'
    ),
    (
        'eng-first-division-1992-93-rules',
        'eng-first-division-1992-93',
        3,
        1,
        0,
        'points,goal_difference,goals_for,club_name',
        2,
        4,
        3,
        '1993-03-31',
        0,
        'competition_rules_provisional_pending_source_pack',
        'Promotion/playoff/relegation slots are provisional until the dedicated competition source pack is imported.'
    );

INSERT OR REPLACE INTO competition_season_teams (
    season_id, club_id, source_locked_club_id, seed_order, starting_points, source_status, notes
)
SELECT
    CASE bridge.competition_id
        WHEN 'eng-premier-league' THEN 'eng-premier-league-1992-93'
        WHEN 'eng-first-division' THEN 'eng-first-division-1992-93'
    END,
    bridge.gameplay_club_id,
    bridge.locked_club_id,
    CAST(SUBSTR(bridge.locked_club_id, 4) AS INTEGER),
    0,
    'locked_v11_participant',
    'Active participant imported from LOCKED v11 club baseline.'
FROM world_club_id_bridge AS bridge
WHERE bridge.playable_status = 'playable'
  AND bridge.competition_id IN ('eng-premier-league', 'eng-first-division');

INSERT OR REPLACE INTO fixture_generation_profiles (
    id, season_id, method, rng_seed, generated_fixture_count, source_status, notes
) VALUES
    (
        'eng-premier-league-1992-93-fixture-source',
        'eng-premier-league-1992-93',
        'pending_real_source_then_deterministic_round_robin_fallback',
        1992070101,
        0,
        'awaiting_fixture_source_pack',
        'Only currently seeded fixtures are active until a dedicated fixture/results source pack is imported.'
    ),
    (
        'eng-first-division-1992-93-fixture-source',
        'eng-first-division-1992-93',
        'pending_real_source_then_deterministic_round_robin_fallback',
        1992070102,
        0,
        'awaiting_fixture_source_pack',
        'No generated fixtures are inserted yet; this profile fixes the fallback seed and method.'
    );

INSERT OR REPLACE INTO world_person_id_bridge (
    locked_person_id, gameplay_person_id, display_name, date_of_birth,
    active_start_locked_club_id, active_start_gameplay_club_id, runtime_scope,
    bridge_status, data_confidence, source_url, notes
)
SELECT
    person.person_id,
    COALESCE(existing.id, LOWER(person.person_id)),
    person.display_name,
    person.date_of_birth,
    registration.club_id,
    club_bridge.gameplay_club_id,
    COALESCE(club_bridge.runtime_scope, person.availability_state, 'inactive'),
    CASE
        WHEN club_bridge.playable_status = 'playable' AND existing.id IS NOT NULL THEN 'mapped_existing'
        WHEN club_bridge.playable_status = 'playable' THEN 'reserved_from_locked_id'
        WHEN club_bridge.locked_club_id IS NOT NULL THEN 'reference_only'
        ELSE 'inactive'
    END,
    person.confidence,
    COALESCE(person.source_url, ''),
    CASE
        WHEN existing.id IS NOT NULL THEN 'Existing gameplay person id preserved from people.id.'
        WHEN club_bridge.playable_status = 'playable' THEN 'Stable gameplay id reserved from LOCKED v11 person_id.'
        WHEN club_bridge.locked_club_id IS NOT NULL THEN 'Reference-only person excluded from active squad, AI, finance, and training loops.'
        ELSE 'Inactive or future person staged for later activation rules.'
    END
FROM fo92_locked_people AS person
LEFT JOIN fo92_locked_start_registrations AS registration
    ON registration.person_id = person.person_id
LEFT JOIN world_club_id_bridge AS club_bridge
    ON club_bridge.locked_club_id = registration.club_id
LEFT JOIN people AS existing
    ON existing.full_name = person.display_name
   AND existing.birth_date = COALESCE(person.date_of_birth, '');

INSERT OR IGNORE INTO schema_migrations(version) VALUES (10);
