// G3-a first 100 days: briefings, surfacing hidden risks and pledge verdicts are explicit gameplay
// models; these tests pin the schedule, the effects and the thirteen pledge rules on synthetic states.
#include "core/first_hundred_days.h"

#include "core/ownership.h"
#include "core/simulation_engine.h"
#include "core/start_scenario.h"
#include "development_test_support.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace football_owner;

namespace {

void require(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}

OwnerPledge pledgeFor(const GameState& s, const std::string& id)
{
    OwnerPledge pledge;
    pledge.id = id;
    pledge.status = "open";
    pledge.madeDate = s.currentDate;
    if (const auto* option = ownerPledgeOption(id)) {
        pledge.area = option->area;
        pledge.title = option->title;
        pledge.detail = option->detail;
        pledge.deadline = s.currentDate.addDays(option->horizonDays);
    } else {
        pledge.title = id;
        pledge.detail = "test";
        pledge.deadline = s.currentDate.addDays(30);
    }
    return pledge;
}

OwnerHiddenRisk riskFor(const GameState& s, const std::string& id, const std::string& area, Money amount, int days, bool disclosed)
{
    OwnerHiddenRisk risk;
    risk.id = id;
    risk.area = area;
    risk.title = id;
    risk.detail = "test risk";
    risk.amountPence = amount;
    risk.severity = 2;
    risk.detectability = 2;
    risk.surfacesOn = s.currentDate.addDays(days);
    risk.disclosed = disclosed;
    risk.status = "latent";
    return risk;
}

// The match fixture's club "home" (22 first-team players born 1966) right after an acquisition.
GameState acquiredState(std::vector<std::string> pledgeIds = {"invest-first-window", "finance-no-new-debt", "fans-identity"})
{
    auto s = development_test::basis();
    s.startScenario.packageId = "test_package";
    s.fixtures.clear();
    for (auto& player : s.players) player.squadGroup = "first_team";
    for (int index = 0; index < 22; ++index) {
        PayrollInput contract;
        contract.playerId = "home-" + std::to_string(index);
        contract.clubId = "home";
        contract.startDate = GameDate {1990, 1, 1};
        contract.endDate = GameDate {1994, 6, 30};
        contract.weeklyWagePence = (2200 - index * 50) * 100;
        contract.provenance = "test";
        s.payrollInputs.push_back(contract);
    }
    s.club.finance.debtPence = 2'000'000 * 100;
    s.club.finance.weeklyWageBudgetPence = 30'000 * 100;
    s.club.finance.weeklyWageCommitmentPence = 25'000 * 100;
    s.ownerProfile.initialized = true;
    s.ownerProfile.tier = AssetTier::LargeTycoon;
    s.ownerProfile.structure = AcquisitionStructure::SoleOwnership;
    s.ownerProfile.acquisitionDate = s.currentDate;
    s.ownerProfile.tierCashPence = 20'000'000 * 100;
    s.ownerProfile.personalCashPence = 10'000'000 * 100;
    s.ownerProfile.purchasePricePence = 10'000'000 * 100;
    s.ownerProfile.cashPaidPence = 10'000'000 * 100;
    s.ownerProfile.askingPricePence = 10'000'000 * 100;
    s.ownerProfile.equityValuePence = 8'000'000 * 100;
    s.ownerProfile.difficulty = 2;
    s.ownerProfile.fanReaction = "기대";
    for (const auto& id : pledgeIds) s.ownerPledges.push_back(pledgeFor(s, id));
    s.gameSettings.tags = settingsTags(s.gameSettings);
    return s;
}

void days(GameState& s, int count)
{
    for (int index = 0; index < count; ++index) {
        s.currentDate = s.currentDate.addDays(1);
        processFirstHundredDaysDay(s);
    }
}

bool hasKey(const GameState& s, const std::string& key)
{
    return std::any_of(s.inbox.begin(), s.inbox.end(), [&](const auto& item) { return item.eventKey == key; });
}

const InboxItem& inboxItem(const GameState& s, const std::string& key)
{
    for (const auto& item : s.inbox) if (item.eventKey == key) return item;
    throw std::runtime_error("missing inbox item " + key);
}

const OwnerPledge& pledgeOf(const GameState& s, const std::string& id)
{
    for (const auto& pledge : s.ownerPledges) if (pledge.id == id) return pledge;
    throw std::runtime_error("missing pledge " + id);
}

StaffContract staffContract(const std::string& id, const std::string& role, const GameDate& start, const std::string& status)
{
    StaffContract contract;
    contract.id = id;
    contract.personId = id + "-person";
    contract.clubId = "home";
    contract.roles = {role};
    contract.startDate = start;
    contract.status = status;
    contract.weeklyPence = 500 * 100;
    return contract;
}

} // namespace

int runFirstHundredDaysTests()
{
    const std::vector<std::pair<std::string, std::function<void()>>> tests {
        {"a career without an acquisition profile is left alone", [] {
            auto s = development_test::basis();
            const auto inboxBefore = s.inbox.size();
            days(s, 5);
            require(s.inbox.size() == inboxBefore, "inbox changed without a profile");
            require(!nextFirstHundredDaysDate(s).has_value() && !firstHundredDaysView(s).active, "schedule without a profile");
        }},
        {"briefings arrive on their day, once, and the built-in start skips the scripted ones", [] {
            auto s = acquiredState();
            days(s, 1);
            require(hasKey(s, "first100-diligence-summary") && !hasKey(s, "first100-finance-rules"), "day one briefing");
            require(inboxItem(s, "first100-diligence-summary").body.find("실사에서 확인한 위험은 없었습니다") != std::string::npos, "diligence summary body");
            days(s, 2);
            require(hasKey(s, "first100-finance-rules"), "day three briefing");
            const auto size = s.inbox.size();
            processFirstHundredDaysDay(s);
            require(s.inbox.size() == size, "same day repeats a briefing");
            days(s, 97);
            int count = 0;
            for (const auto& item : s.inbox) if (item.eventKey.rfind("first100-", 0) == 0) ++count;
            require(count == 8 && hasKey(s, "first100-review"), "eight briefings by day 100: " + std::to_string(count));
            require(inboxItem(s, "first100-review").body.find("취임 100일") != std::string::npos, "review body");
            require(inboxItem(s, "first100-squad-plan").body.find("1군 22명") != std::string::npos, "squad plan counts the first team: " + inboxItem(s, "first100-squad-plan").body);

            auto legacy = acquiredState();
            legacy.startScenario.packageId = StartScenario::legacy1992().packageId;
            days(legacy, 100);
            require(!hasKey(legacy, "first100-finance-rules") && !hasKey(legacy, "first100-manager-meeting") && !hasKey(legacy, "first100-squad-plan"), "legacy skips scripted themes");
            require(hasKey(legacy, "first100-diligence-summary") && hasKey(legacy, "first100-fan-expectation") && hasKey(legacy, "first100-review"), "legacy keeps the rest");
            const auto view = firstHundredDaysView(legacy);
            int skipped = 0;
            for (const auto& briefing : view.briefings) if (briefing.skipped) ++skipped;
            require(skipped == 3, "view marks skipped briefings");
        }},
        {"the next date is the earliest briefing, risk or deadline and the engine stops there", [] {
            auto s = acquiredState();
            require(nextFirstHundredDaysDate(s) == s.currentDate.addDays(1), "first briefing is tomorrow");
            SimulationEngine engine(s);
            require(engine.nextPendingDate() == s.currentDate.addDays(1), "engine ignores the first-100-days schedule");
            days(s, 1);
            s.ownerHiddenRisks.push_back(riskFor(s, "stadium-repair", "facilities", 100'000 * 100, 1, true));
            require(nextFirstHundredDaysDate(s) == s.currentDate.addDays(1), "a risk due tomorrow comes before the day-three briefing");
            days(s, 1);
            // No operating ledger here: the cost left the cash on the day, so nothing is left open.
            require(s.ownerHiddenRisks.front().status == "resolved" && s.ownerHiddenRisks.front().resolution == "cash_paid", "risk surfaced on its day");
            require(nextFirstHundredDaysDate(s) == s.currentDate.addDays(1), "next is the day-three briefing");
        }},
        {"hidden debt surfaces as a club liability with repayments and an inbox note", [] {
            auto s = acquiredState();
            s.ownerHiddenRisks.push_back(riskFor(s, "hidden-debt", "finance", 500'000 * 100, 30, false));
            const Money debtBefore = s.club.finance.debtPence;
            const int trustBefore = s.fanTendency.trustScore;
            days(s, 29);
            require(s.ownerHiddenRisks.front().status == "latent" && !hasKey(s, "hidden-risk-surfaced-hidden-debt"), "surfaced early");
            days(s, 1);
            const auto& risk = s.ownerHiddenRisks.front();
            require(risk.status == "surfaced", "status after surfacing");
            const auto debt = std::find_if(s.debts.begin(), s.debts.end(), [](const auto& item) { return item.id == "legacy-liability-home"; });
            require(debt != s.debts.end() && debt->kind == "legacy_liability" && debt->remainingPrincipalPence == 500'000 * 100, "liability ledger row");
            require(std::count_if(s.debtRepayments.begin(), s.debtRepayments.end(), [](const auto& r) { return r.debtId == "legacy-liability-home"; }) == 24, "24 repayments");
            require(s.club.finance.debtPence >= debtBefore + 500'000 * 100, "club debt grew");
            const auto& mail = inboxItem(s, "hidden-risk-surfaced-hidden-debt");
            require(mail.category == InboxCategory::Finance && mail.body.find("실사가 보지 못한") != std::string::npos, "undisclosed risk wording");
            require(!s.ownerActionLog.empty() && s.ownerActionLog.back().actionType == "hidden_risk_surfaced",
                "action log: " + (s.ownerActionLog.empty() ? std::string("empty") : s.ownerActionLog.back().actionType));
            require(pledgeOf(s, "finance-no-new-debt").status == "open", "the ledger bootstrap row counted as new borrowing");
            require(s.fanTendency.trustScore < trustBefore, "fan trust fell");
            require(validateOwnership(s).empty(), validateOwnership(s));
            days(s, 1);
            require(std::count_if(s.inbox.begin(), s.inbox.end(), [](const auto& i) { return i.eventKey == "hidden-risk-surfaced-hidden-debt"; }) == 1, "surfaced twice");
        }},
        {"wage arrears and scheduled costs become obligations with an operating profile, cash otherwise", [] {
            auto s = acquiredState();
            s.ownerHiddenRisks.push_back(riskFor(s, "wage-arrears", "finance", 40'000 * 100, 14, true));
            const Money cashBefore = s.club.finance.cashPence;
            days(s, 14);
            require(s.club.finance.cashPence == cashBefore - 40'000 * 100 && s.operatingObligations.empty(), "cash path without a profile");
            require(inboxItem(s, "hidden-risk-surfaced-wage-arrears").body.find("실사에서 확인했던") != std::string::npos, "disclosed risk wording");

            auto p = acquiredState();
            OperatingProfile profile;
            profile.clubId = "home";
            profile.seasonId = "1992";
            profile.activatedDate = p.currentDate;
            p.operatingProfiles.push_back(profile);
            p.ownerHiddenRisks.push_back(riskFor(p, "stadium-repair", "facilities", 100'000 * 100, 20, true));
            p.ownerHiddenRisks.push_back(riskFor(p, "wage-arrears", "finance", 40'000 * 100, 20, true));
            const Money cash = p.club.finance.cashPence;
            days(p, 20);
            require(p.club.finance.cashPence == cash, "profile path touched cash directly");
            require(p.operatingObligations.size() == 2, "two obligations");
            const auto repair = std::find_if(p.operatingObligations.begin(), p.operatingObligations.end(), [](const auto& o) { return o.category == "stadium_repair"; });
            const auto wages = std::find_if(p.operatingObligations.begin(), p.operatingObligations.end(), [](const auto& o) { return o.category == "player_payroll"; });
            require(repair != p.operatingObligations.end() && repair->status == TransferPaymentStatus::Scheduled && repair->dueDate == p.currentDate.addDays(30)
                && repair->amountPence == 100'000 * 100, "scheduled repair cost");
            require(wages != p.operatingObligations.end() && wages->status == TransferPaymentStatus::Overdue && wages->dueDate == p.currentDate, "overdue wages");
        }},
        {"a transfer request opens a retain case and an injury backlog sidelines the next earner", [] {
            auto s = acquiredState();
            PlayerRelationship relation;
            relation.playerId = "home-0";
            relation.clubId = "home";
            relation.morale = 50;
            s.playerRelationships["home|home-0"] = relation;
            s.ownerHiddenRisks.push_back(riskFor(s, "key-player-exit", "football", 0, 7, true));
            s.ownerHiddenRisks.push_back(riskFor(s, "injury-backlog", "football", 0, 7, false));
            days(s, 7);
            require(s.ownerCases.size() == 1, "one owner case");
            const auto& item = s.ownerCases.front();
            require(item.kind == "retain_player" && item.source == "hidden_risk" && item.status == "requested" && item.subjectId == "home-0"
                && item.deadline == s.currentDate.addDays(28) && item.baselineContractEnd.has_value(), "retain request fields");
            require(s.playerRelationships.at("home|home-0").morale == 35, "morale fell by 15");
            const auto& development = s.playerDevelopment.at("home-1");
            require(development.injuredDate == s.currentDate && development.returnDate == s.currentDate.addDays(35) && development.programme == "rehab"
                && !development.injury.empty(), "injury backlog on the next earner");
            require(hasKey(s, "hidden-risk-surfaced-key-player-exit") && hasKey(s, "hidden-risk-surfaced-injury-backlog"), "both inbox notes");
            days(s, 7);
            require(s.ownerCases.size() == 1, "request duplicated");
        }},
        {"investment pledges: first-window signing and no fire sale", [] {
            auto broken = acquiredState({"invest-first-window"});
            const int trustBefore = broken.fanTendency.trustScore;
            days(broken, 119);
            require(pledgeOf(broken, "invest-first-window").status == "open", "decided before the deadline");
            days(broken, 1);
            require(pledgeOf(broken, "invest-first-window").status == "broken" && pledgeOf(broken, "invest-first-window").evidence.find("없었습니다") != std::string::npos, "no signing: broken");
            require(hasKey(broken, "pledge-verdict-invest-first-window") && broken.fanTendency.trustScore < trustBefore, "verdict mail and trust");
            require(broken.ownerActionLog.back().actionType == "pledge_broken", "action log");
            require(validateOwnership(broken).empty(), validateOwnership(broken));

            auto kept = acquiredState({"invest-first-window"});
            TransferProposal signing;
            signing.id = "in-1";
            signing.target = kept.players[5];
            signing.fromClubId = "away";
            signing.status = TransferStatus::Completed;
            signing.joinDate = kept.currentDate.addDays(10);
            signing.feePence = 1'000'000 * 100;
            kept.transferProposals.push_back(signing);
            days(kept, 120);
            require(pledgeOf(kept, "invest-first-window").status == "kept" && pledgeOf(kept, "invest-first-window").evidence.find("1명") != std::string::npos, "signing: kept");

            auto sale = acquiredState({"invest-no-fire-sale"});
            days(sale, 1);
            require(pledgeOf(sale, "invest-no-fire-sale").evidence.find("core=home-0|home-1") != std::string::npos, "core baseline: " + pledgeOf(sale, "invest-no-fire-sale").evidence);
            OutgoingTransferOffer offer;
            offer.id = "out-1";
            offer.playerId = "home-0";
            offer.playerName = "home-0";
            offer.buyerClubId = "away";
            offer.feePence = 2'000'000 * 100;
            offer.status = OutgoingTransferStatus::Completed;
            offer.completionDate = sale.currentDate.addDays(19);
            sale.outgoingTransferOffers.push_back(offer);
            days(sale, 19);
            require(pledgeOf(sale, "invest-no-fire-sale").status == "broken" && pledgeOf(sale, "invest-no-fire-sale").evidence.find("home-0") != std::string::npos, "core sale: broken early");
            require(validateOwnership(sale).empty(), validateOwnership(sale));
        }},
        {"football pledges: manager, youth minutes, top half", [] {
            auto manager = acquiredState({"football-keep-manager"});
            auto earlier = staffContract("old-boss", "manager", GameDate {1988, 7, 1}, "dismissed");
            earlier.endedDate = manager.currentDate.addDays(-100); // a dismissal before the takeover is not ours
            manager.staffContracts.push_back(earlier);
            days(manager, 14);
            require(pledgeOf(manager, "football-keep-manager").status == "open", "old dismissal counted");
            auto dismissed = staffContract("boss", "manager", GameDate {1990, 7, 1}, "dismissed");
            dismissed.endedDate = manager.currentDate.addDays(1);
            manager.staffContracts.push_back(dismissed);
            days(manager, 1);
            require(pledgeOf(manager, "football-keep-manager").status == "broken", "dismissal breaks the pledge");

            auto youth = acquiredState({"football-youth-minutes"});
            days(youth, 365);
            require(pledgeOf(youth, "football-youth-minutes").status == "broken", "no young appearances: broken");

            auto top = acquiredState({"football-top-half"});
            for (int position = 1; position <= 4; ++position) {
                FinalSeasonStanding standing;
                standing.row.seasonId = "league-1992";
                standing.row.competitionId = "league";
                standing.row.competition = "League";
                standing.row.clubId = position == 2 ? "home" : "other-" + std::to_string(position);
                standing.row.position = position;
                top.seasonHistory.push_back(standing);
            }
            days(top, 1);
            require(pledgeOf(top, "football-top-half").status == "kept" && pledgeOf(top, "football-top-half").evidence.find("2위/4팀") != std::string::npos, "second of four: kept");
            auto bottom = acquiredState({"football-top-half"});
            for (auto standing : top.seasonHistory) {
                if (standing.row.clubId == "home") standing.row.position = 3;
                else if (standing.row.position == 3) standing.row.position = 2;
                bottom.seasonHistory.push_back(standing);
            }
            days(bottom, 1);
            require(pledgeOf(bottom, "football-top-half").status == "broken", "third of four: broken");
            auto none = acquiredState({"football-top-half"});
            days(none, 365);
            require(pledgeOf(none, "football-top-half").status == "withdrawn", "no season record: withdrawn");
        }},
        {"finance pledges: new debt, wage discipline, debt reduction", [] {
            auto debt = acquiredState({"finance-no-new-debt"});
            ClubDebt legacy;
            legacy.id = "legacy-liability-home";
            legacy.clubId = "home";
            legacy.kind = "legacy_liability";
            legacy.originalPrincipalPence = legacy.remainingPrincipalPence = 100'000 * 100;
            legacy.originatedDate = debt.currentDate.addDays(5);
            debt.debts.push_back(legacy);
            days(debt, 9);
            require(pledgeOf(debt, "finance-no-new-debt").status == "open", "legacy liability counted as new borrowing");
            ClubDebt loan = legacy;
            loan.id = "loan-2";
            loan.kind = "bank_loan";
            loan.purpose = "운영 자금 대출";
            loan.originatedDate = debt.currentDate.addDays(1);
            debt.debts.push_back(loan);
            days(debt, 1);
            require(pledgeOf(debt, "finance-no-new-debt").status == "broken" && pledgeOf(debt, "finance-no-new-debt").evidence.find("운영 자금 대출") != std::string::npos, "new loan: broken");

            auto wages = acquiredState({"finance-wage-discipline"});
            wages.club.finance.weeklyWageCommitmentPence = wages.club.finance.weeklyWageBudgetPence * 120 / 100;
            days(wages, 28);
            require(pledgeOf(wages, "finance-wage-discipline").status == "open", "streak broke too early");
            days(wages, 1);
            require(pledgeOf(wages, "finance-wage-discipline").status == "broken" && pledgeOf(wages, "finance-wage-discipline").evidence.find("110%") != std::string::npos, "28-day overrun: broken");
            auto disciplined = acquiredState({"finance-wage-discipline"});
            days(disciplined, 365);
            require(pledgeOf(disciplined, "finance-wage-discipline").status == "kept", "within budget: kept");

            auto reduce = acquiredState({"finance-reduce-debt"});
            days(reduce, 1);
            reduce.club.finance.debtPence = 1'700'000 * 100;
            days(reduce, 364);
            require(pledgeOf(reduce, "finance-reduce-debt").status == "kept" && pledgeOf(reduce, "finance-reduce-debt").evidence.find("£") != std::string::npos, "15% cut: kept");
            auto stuck = acquiredState({"finance-reduce-debt"});
            days(stuck, 365);
            require(pledgeOf(stuck, "finance-reduce-debt").status == "broken", "no cut: broken");
        }},
        {"organisation and fan pledges", [] {
            auto staff = acquiredState({"organisation-staff"});
            auto scout = staffContract("eyes", "scout", staff.currentDate.addDays(5), "active");
            staff.staffContracts.push_back(scout);
            StaffPerson person;
            person.id = scout.personId;
            person.name = "New Scout";
            staff.staffPeople[person.id] = person;
            days(staff, 90);
            require(pledgeOf(staff, "organisation-staff").status == "kept" && pledgeOf(staff, "organisation-staff").evidence.find("New Scout") != std::string::npos, "scout hired: kept");
            auto noStaff = acquiredState({"organisation-staff"});
            days(noStaff, 90);
            require(pledgeOf(noStaff, "organisation-staff").status == "broken", "no hire: broken");

            auto meetings = acquiredState({"organisation-meetings"});
            for (int index = 0; index < 4; ++index) {
                OperatingMeeting meeting;
                meeting.id = "meeting-" + std::to_string(index);
                meeting.meetingDate = meetings.currentDate.addDays(10 + index * 30);
                meeting.createdDate = meeting.meetingDate;
                meetings.operatingMeetings.push_back(meeting);
            }
            days(meetings, 180);
            require(pledgeOf(meetings, "organisation-meetings").status == "kept", "four meetings: kept");

            auto identity = acquiredState({"fans-identity"});
            days(identity, 1);
            identity.fanTendency.trustScore = 46;
            days(identity, 364);
            require(pledgeOf(identity, "fans-identity").status == "kept", "trust within five points: kept");
            auto lost = acquiredState({"fans-identity"});
            days(lost, 1);
            lost.fanTendency.trustScore = 40;
            days(lost, 364);
            require(pledgeOf(lost, "fans-identity").status == "broken", "trust collapsed: broken");

            auto open = acquiredState({"fans-transparency"});
            for (int index = 0; index < 2; ++index) {
                open.governanceEvents.push_back({"statement-" + std::to_string(index), "home", "statement", "home", "test", open.currentDate.addDays(30 + index * 60), 0});
            }
            days(open, 365);
            require(pledgeOf(open, "fans-transparency").status == "kept", "two statements: kept");

            auto local = acquiredState({"fans-local-ties"});
            days(local, 365);
            require(pledgeOf(local, "fans-local-ties").status == "broken", "no young first-teamers: broken");
            auto young = acquiredState({"fans-local-ties"});
            for (int index = 0; index < 3; ++index) young.players[index].birthDate = GameDate {1972, 5, 1};
            days(young, 365);
            require(pledgeOf(young, "fans-local-ties").status == "kept", "three young first-teamers: kept");

            auto unknown = acquiredState({"future-pledge"});
            days(unknown, 30);
            require(pledgeOf(unknown, "future-pledge").status == "withdrawn", "unknown pledge withdrawn at its deadline");
            require(validateOwnership(unknown).empty(), validateOwnership(unknown));
        }},
        {"the home view tracks phases, briefings, pledges and risks", [] {
            auto s = acquiredState();
            s.ownerHiddenRisks.push_back(riskFor(s, "regulatory-probe", "legal", 50'000 * 100, 40, false));
            auto view = firstHundredDaysView(s);
            require(view.active && view.day == 0 && view.phaseId == "emergency" && view.briefings.size() == 8 && view.pledgesOpen == 3 && view.risksTotal == 1 && view.risksSurfaced == 0, "day zero view");
            days(s, 10);
            require(firstHundredDaysView(s).phaseId == "first_month", "first month phase");
            days(s, 30);
            view = firstHundredDaysView(s);
            require(view.phaseId == "first_window" && view.risksSurfaced == 1, "first window phase and surfaced risk");
            days(s, 30);
            require(firstHundredDaysView(s).phaseId == "first_season", "first season phase");
            days(s, 51);
            view = firstHundredDaysView(s);
            int delivered = 0;
            for (const auto& briefing : view.briefings) if (briefing.delivered) ++delivered;
            require(view.phaseId == "complete" && delivered == 8 && view.pledgesBroken == 1 && view.pledgesOpen == 2, "complete view: broken " + std::to_string(view.pledgesBroken));
            require(view.nextPledgeDeadline.has_value() && view.nextDate.has_value(), "remaining schedule");
            for (const auto& pledge : s.ownerPledges) require(pledge.status != "open" || pledge.evidence.rfind("baseline:", 0) == 0, "open pledge lost its baseline");
            require(pledgeOf(s, "invest-first-window").evidence.rfind("baseline:", 0) != 0, "verdict evidence still a baseline marker");
        }},
    };

    int failed = 0;
    for (const auto& [name, run] : tests) {
        try {
            run();
            std::cout << "[PASS] first100 " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] first100 " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failed << '/' << tests.size() << " first-100-days tests passed\n";
    return failed;
}
