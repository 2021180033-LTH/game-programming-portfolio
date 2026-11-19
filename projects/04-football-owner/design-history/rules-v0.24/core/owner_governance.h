#pragma once
#include "core/game_state.h"
namespace football_owner {
void initializeOwnerGovernance(GameState&);
void processOwnerGovernanceDay(GameState&);
const FanTendencyState* governanceFans(const GameState&, const std::string& club);
int fanMatchIncomePercent(const GameState&, const std::string& club);
int playerMoraleAdjustment(const GameState&, const std::string& player, const std::string& club);
std::string ownerCaseLabel(const std::string&);
std::string ownerDirectionLabel(const std::string&);
std::string ownerCaseEligibility(const GameState&, const std::string& club, const std::string& kind,
                                 const std::string& subject, int days);
bool makeOwnerPromise(GameState&, const std::string& club, const std::string& kind, const std::string& subject,
                      int days, std::string* error = nullptr);
bool respondOwnerCase(GameState&, const std::string& id, bool promise, int days, const std::string& reason,
                      std::string* error = nullptr);
bool withdrawOwnerPromise(GameState&, const std::string& id, std::string* error = nullptr);
bool declareOwnerDirection(GameState&, const std::string& club, const std::string& direction, std::string* error = nullptr);
std::optional<GameDate> nextOwnerGovernanceDate(const GameState&);
std::string validateOwnerGovernance(const GameState&);
}
