#pragma once

// G3-a (plan §66.8 "인수 후 승계와 첫 100일"): what the acquisition set up starts to matter.
// Briefings arrive in the inbox on fixed days after the takeover, hidden risks surface on their
// dates with real effects on the club, and inauguration pledges are judged kept or broken against
// the career's actual facts. Everything is derived from saved state (no new tables): pledge
// status/evidence (schema 35), risk status (36), the inbox, debts, operating obligations, owner
// cases, player injuries and the owner action log. Explicit gameplay model, never history. No Qt.
#include "core/game_state.h"

#include <optional>
#include <string>
#include <vector>

namespace football_owner {

constexpr int kFirstHundredDays = 100;

// A briefing the owner receives N days after the takeover. The body is written on the day it is
// sent, from the state of that day; the inbox eventKey (firstDaysBriefingKey) keeps it to one copy.
struct FirstDaysBriefing {
    std::string id;
    int dayOffset {};
    InboxCategory category {InboxCategory::Board};
    std::string title;
    bool legacyCovered {false}; // the built-in 1992-93 scenario already scripts an equivalent item
};
[[nodiscard]] const std::vector<FirstDaysBriefing>& firstDaysBriefings();
[[nodiscard]] std::string firstDaysBriefingKey(const std::string& id);
[[nodiscard]] bool firstDaysBriefingApplies(const GameState& state, const FirstDaysBriefing& briefing);
[[nodiscard]] std::string firstDaysBriefingBody(const GameState& state, const std::string& id);

// Verdict on one open pledge today: a break is found as soon as it happens, a keep is confirmed at
// the deadline. `newDeadline` asks to wait (football-top-half waits for the season to finish).
struct PledgeVerdict {
    bool decided {false};
    bool kept {false};
    bool withdrawn {false}; // no rule or no evidence can exist: closed without blame
    std::string evidence;
    std::optional<GameDate> newDeadline;
};
[[nodiscard]] PledgeVerdict evaluatePledge(const GameState& state, const OwnerPledge& pledge);

// Applies a latent risk to the club (legacy liability, overdue wages, scheduled costs, a transfer
// request or a long injury), marks it surfaced and returns the inbox body. The daily processor adds
// the inbox item and the owner action log entry.
std::string surfaceHiddenRisk(GameState& state, OwnerHiddenRisk& risk);

// For a save from before schema 40, once: finds the ledger object each surfaced risk created
// (linkSurfacedHiddenRisks), adds a hidden debt that surfaced after the baseline was taken to the
// open debt-reduction baseline, and derives what became of each risk. Risks that already carry a
// link are left alone, so loading the upgraded save again changes nothing. Returns whether
// anything was restored.
bool restoreHiddenRiskLinks(GameState& state);

// Once per simulated day, after owner governance: baselines, briefings, risks, pledge verdicts.
// A career without an acquisition profile (pre-35 save, acquisition-free entry) is left alone.
void processFirstHundredDaysDay(GameState& state);
[[nodiscard]] std::optional<GameDate> nextFirstHundredDaysDate(const GameState& state);

// Computed view for the home page; nothing here is stored.
struct FirstHundredDaysView {
    bool active {false};
    int day {};
    std::string phaseId;    // emergency | first_month | first_window | first_season | complete
    std::string phaseLabel;
    struct Briefing {
        std::string id;
        std::string title;
        GameDate date;
        bool delivered {false};
        bool skipped {false};
    };
    std::vector<Briefing> briefings;
    int pledgesOpen {};
    int pledgesKept {};
    int pledgesBroken {};
    std::optional<GameDate> nextPledgeDeadline;
    int risksTotal {};
    int risksDisclosed {};
    int risksSurfaced {};
    std::optional<GameDate> nextDate;
};
[[nodiscard]] FirstHundredDaysView firstHundredDaysView(const GameState& state);

} // namespace football_owner
