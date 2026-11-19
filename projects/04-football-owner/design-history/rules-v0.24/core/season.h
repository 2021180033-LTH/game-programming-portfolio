#pragma once
#include "core/game_state.h"

namespace football_owner {
[[nodiscard]] std::string careerSeasonId(const GameDate& start);
[[nodiscard]] const CareerSeason* activeSeason(const GameState& state);
[[nodiscard]] const RuntimeCompetitionSeason* runtimeCompetition(const GameState& state, const std::string& id);
[[nodiscard]] bool isActiveFixture(const GameState& state, const Fixture& fixture);
[[nodiscard]] bool isPlayerFixture(const GameState& state, const Fixture& fixture);
[[nodiscard]] std::vector<Fixture> generateLeagueSchedule(
    const CareerSeason& season, const RuntimeCompetitionSeason& competition,
    std::vector<CompetitionSeasonTeam> teams);
[[nodiscard]] std::string validateSeasonState(const GameState& state);
void ensureSeasonSchedules(GameState& state);
void recalculateSeasonStandings(GameState& state);
// Runs after all ordinary due events and fixtures. Stages changes on a copy.
void processSeasonLifecycle(GameState& state);
[[nodiscard]] std::string seasonStatus(const GameState& state);
[[nodiscard]] std::uint32_t seasonStableHash(const std::string& text);
}
