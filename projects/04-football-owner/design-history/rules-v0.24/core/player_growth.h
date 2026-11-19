#pragma once
#include "core/game_state.h"

namespace football_owner {
struct DirectoryPlayer;
void initializePlayerGrowth(GameState& state);
void addPlayerGrowth(GameState& state, const std::string& id, const std::string& club, const std::string& position);
void trainPlayerGrowth(GameState& state, const std::string& id, const std::string& club,
                       const std::string& position, std::optional<int> age, bool eligible);
void recordGrowthMinutes(GameState& state, const std::string& id, const std::string& club,
                         const GameDate& date, int minutes);
std::string validatePlayerGrowth(const GameState& state);
} // namespace football_owner
