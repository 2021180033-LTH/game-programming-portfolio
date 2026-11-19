#pragma once
#include "core/game_state.h"
namespace football_owner {
struct StaffQuote {
    bool allowed {false};
    std::string token, reason, action, personId, clubId, replacedContract;
    std::vector<std::string> roles;
    int years {1};
    Money payNowPence {}, weeklyPence {}, compensationPence {};
};
void initializeStaffOperations(GameState&);
void projectStaffRoles(GameState&);
void processStaffDay(GameState&);
const StaffContract* activeStaffContract(const GameState&, const std::string& person);
const StaffPerson* staffForRole(const GameState&, const std::string& club, const std::string& role);
int staffRoleAbility(const GameState&, const std::string& club, const std::string& role);
Money staffWeeklyPayroll(const GameState&, const std::string& club);
Money staffCashReserve(const GameState&, const std::string& club, int days);
std::string staffRoleLabel(const std::string&);
StaffQuote quoteStaffAction(const GameState&, const std::string& club, const std::string& person,
                           const std::string& action, std::vector<std::string> roles={}, int years=1);
bool confirmStaffAction(GameState&, const StaffQuote&, std::string* error=nullptr);
bool decideStaffRequest(GameState&, const std::string& id, bool approve, const std::string& reason,
                        const std::string& quoteToken, std::string* error=nullptr);
bool setStaffPolicy(GameState&, const std::string& club, Money weeklyLimit, bool delegateHiring,
                    Money delegationLimit, std::string* error=nullptr);
bool setStaffMeetingPeople(GameState&, const std::string& club, std::vector<std::string>, std::string* error=nullptr);
std::optional<GameDate> nextStaffDate(const GameState&);
std::optional<GameDate> nextStaffMeetingDate(const GameDate&, OperatingMeetingCadence);
std::vector<OperatingMeetingAttendee> staffMeetingAttendees(const GameState&, OperatingMeetingAgendaType,
                                                           const OperatingMeetingSettings&);
void processStaffMeetings(GameState&);
std::string validateStaffOperations(const GameState&);
}
