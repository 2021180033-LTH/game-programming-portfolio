#include "core/first_hundred_days.h"

#include "core/owner_capital.h"

#include "core/due_diligence.h"
#include "core/funding.h"
#include "core/match_engine.h"
#include "core/operating_finance.h"
#include "core/ownership.h"
#include "core/player_directory.h"
#include "core/season.h"
#include "core/staff_operations.h"
#include "core/start_scenario.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace football_owner {
namespace {

constexpr int kLegacyLiabilityMonths = 24;
constexpr int kTransferRequestDays = 28;
constexpr int kBacklogInjuryDays = 28;
constexpr int kScheduledCostDays = 30;
constexpr int kWageOverrunStreakDays = 28;
constexpr int kWageOverrunPercent = 110;
constexpr int kCorePlayers = 5;
constexpr int kYouthAge = 23;
constexpr int kYouthPlayersRequired = 3;
constexpr int kMeetingsRequired = 4;
constexpr int kStatementsRequired = 2;
constexpr int kIdentityTrustTolerance = 5;
constexpr int kDebtReductionPercent = 90;
constexpr int kKeptFanTrust = 3;
constexpr int kKeptReputation = 2;
constexpr int kBrokenFanTrust = -5;
constexpr int kBrokenReputation = -3;
const char* const kProvenance = "first_hundred_days_v1_gameplay_not_historical";

bool acquired(const GameState& s) { return s.ownerProfile.initialized && s.ownerProfile.acquisitionDate.isValid(); }
GameDate acquisitionDate(const GameState& s) { return s.ownerProfile.acquisitionDate; }
bool legacyStart(const GameState& s) { return s.startScenario.packageId == StartScenario::legacy1992().packageId; }
std::string n(long long value) { return std::to_string(value); }

bool inboxHas(const GameState& s, const std::string& key)
{
    return std::any_of(s.inbox.begin(), s.inbox.end(), [&](const auto& item) { return item.eventKey == key; });
}

void pushInbox(GameState& s, const std::string& key, InboxCategory category, const std::string& title, const std::string& body)
{
    if (inboxHas(s, key)) return;
    int id = 1;
    for (const auto& item : s.inbox) id = std::max(id, item.id + 1);
    InboxItem item;
    item.id = id;
    item.eventKey = key;
    item.date = s.currentDate;
    item.category = category;
    item.title = title;
    item.body = body;
    item.unread = true;
    item.requiresDecision = false;
    s.inbox.push_back(std::move(item));
}

struct OwnPlayer {
    std::string id;
    const DirectoryPlayer* row {nullptr};
    Money wage {};
    int age {};
};

// Players the club owns and fields today, highest wage first.
std::vector<OwnPlayer> ownPlayers(const GameState& s, const std::map<std::string, DirectoryPlayer>& directory)
{
    std::vector<OwnPlayer> players;
    for (const auto& [id, row] : directory) {
        if (row.activeClubId != s.club.id || row.ownerClubId != s.club.id) continue;
        OwnPlayer player;
        player.id = id;
        player.row = &row;
        player.wage = row.contract ? row.contract->weeklyWagePence : 0;
        player.age = ageOnDate(row.player, s.currentDate).value_or(0);
        players.push_back(player);
    }
    std::sort(players.begin(), players.end(), [](const auto& a, const auto& b) {
        return a.wage != b.wage ? a.wage > b.wage : a.id < b.id;
    });
    return players;
}

// Packages write first_team / reserve / youth; the built-in 1992-93 data writes "First Team" and friends.
std::string canonicalGroup(std::string group)
{
    std::transform(group.begin(), group.end(), group.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(group.begin(), group.end(), ' ', '_');
    if (group.rfind("first", 0) == 0) return "first_team";
    if (group.rfind("reserve", 0) == 0) return "reserve";
    if (group.rfind("youth", 0) == 0 || group.rfind("academy", 0) == 0) return "youth";
    return group;
}

bool firstTeam(const OwnPlayer& player) { return canonicalGroup(player.row->player.squadGroup) == "first_team"; }

bool hasRole(const StaffContract& contract, const char* role)
{
    return std::find(contract.roles.begin(), contract.roles.end(), std::string(role)) != contract.roles.end();
}

// Baselines live in the open pledge's evidence string ("baseline:debt=...;trust=...;...") until the
// verdict replaces it with a sentence. No new column needed.
const char* const kBaselinePrefix = "baseline:";

std::string baselineField(const std::string& evidence, const std::string& key)
{
    if (evidence.rfind(kBaselinePrefix, 0) != 0) return {};
    const auto position = evidence.find(key + "=");
    if (position == std::string::npos) return {};
    const auto start = position + key.size() + 1;
    const auto end = evidence.find(';', start);
    return evidence.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

void setBaselineField(std::string& evidence, const std::string& key, const std::string& value)
{
    if (evidence.rfind(kBaselinePrefix, 0) != 0) return;
    const auto position = evidence.find(key + "=");
    if (position != std::string::npos) {
        const auto start = position + key.size() + 1;
        const auto end = evidence.find(';', start);
        evidence.erase(start, end == std::string::npos ? std::string::npos : end - start);
        evidence.insert(start, value);
        return;
    }
    evidence += ";" + key + "=" + value;
}

void clearBaselineField(std::string& evidence, const std::string& key)
{
    const auto position = evidence.find(";" + key + "=");
    if (position == std::string::npos) return;
    const auto end = evidence.find(';', position + 1);
    evidence.erase(position, end == std::string::npos ? std::string::npos : end - position);
}

std::vector<std::string> splitIds(const std::string& text)
{
    std::vector<std::string> ids;
    std::string current;
    for (const char character : text) {
        if (character == '|') {
            if (!current.empty()) ids.push_back(current);
            current.clear();
        } else {
            current += character;
        }
    }
    if (!current.empty()) ids.push_back(current);
    return ids;
}

Money toMoney(const std::string& text, Money fallback)
{
    try {
        return text.empty() ? fallback : static_cast<Money>(std::stoll(text));
    } catch (const std::exception&) {
        return fallback;
    }
}

int toInt(const std::string& text, int fallback)
{
    try {
        return text.empty() ? fallback : std::stoi(text);
    } catch (const std::exception&) {
        return fallback;
    }
}

void ensurePledgeBaselines(GameState& s)
{
    bool needed = false;
    for (const auto& pledge : s.ownerPledges) needed = needed || (pledge.status == "open" && pledge.evidence.empty());
    if (!needed) return;
    const auto directory = playerDirectory(s, s.currentDate);
    const auto players = ownPlayers(s, directory);
    std::string core;
    int count = 0;
    for (const auto& player : players) {
        if (!firstTeam(player)) continue;
        if (count++ == kCorePlayers) break;
        core += (core.empty() ? "" : "|") + player.id;
    }
    for (auto& pledge : s.ownerPledges) {
        if (pledge.status != "open" || !pledge.evidence.empty()) continue;
        pledge.evidence = std::string(kBaselinePrefix) + "date=" + s.currentDate.toIsoString() + ";debt=" + n(s.club.finance.debtPence)
            + ";trust=" + n(s.fanTendency.trustScore) + ";core=" + core;
    }
}

// A debt nobody knew of at the takeover is added to the debt-reduction baseline once, so the
// pledge is judged on the debt the owner actually inherited. Only a pledge that is still open and
// whose baseline was taken on or before the day the debt appeared: a verdict already given is
// never rewritten, and a baseline taken later already contains the debt.
bool addHiddenDebtToBaseline(GameState& s, Money principalPence, const GameDate& appeared)
{
    bool changed = false;
    for (auto& pledge : s.ownerPledges) {
        if (pledge.id != "finance-reduce-debt" || pledge.status != "open") continue;
        const auto baseline = baselineField(pledge.evidence, "debt");
        if (baseline.empty()) continue;
        const auto taken = GameDate::fromIsoString(baselineField(pledge.evidence, "date"));
        if (taken && *taken > appeared) continue;
        setBaselineField(pledge.evidence, "debt", n(toMoney(baseline, 0) + principalPence));
        changed = true;
    }
    return changed;
}

std::string pledgeList(const GameState& s, std::optional<PledgeArea> area)
{
    std::string text;
    for (const auto& pledge : s.ownerPledges) {
        if (area && pledge.area != *area) continue;
        text += (text.empty() ? "" : " · ") + pledge.title + "(" + pledge.deadline.toIsoString() + ")";
    }
    return text.empty() ? "없음" : text;
}

std::string warningList(const GameState& s)
{
    std::string text;
    int count = 0;
    for (const auto& risk : s.ownerHiddenRisks) {
        if (!risk.disclosed) continue;
        ++count;
        text += (text.empty() ? "" : " · ") + risk.title + "(" + severityLabel(risk.severity) + ")";
    }
    return count == 0 ? "실사에서 확인한 위험은 없었습니다." : "실사에서 확인한 위험 " + n(count) + "건: " + text + ".";
}

} // namespace

const std::vector<FirstDaysBriefing>& firstDaysBriefings()
{
    static const std::vector<FirstDaysBriefing> kBriefings {
        {"diligence-summary", 1, InboxCategory::Board, "실사 요약 · 인수 당시 확정값과 경고", false},
        {"finance-rules", 3, InboxCategory::Finance, "취임 재정 검토 · 현금·부채·주급·상환 일정", true},
        {"manager-meeting", 7, InboxCategory::Football, "감독 첫 면담 · 선수단 평가와 보강 우선순위", true},
        {"squad-plan", 14, InboxCategory::Football, "선수단 계획 · 구성과 계약 만료", true},
        {"organisation", 21, InboxCategory::Board, "조직 점검 · 직원 계약과 공석", false},
        {"fan-expectation", 30, InboxCategory::Board, "팬 기대 · 신뢰와 취임 약속", false},
        {"expiring-contracts", 45, InboxCategory::Football, "만료 계약 · 1년 내 끝나는 계약", false},
        {"review", kFirstHundredDays, InboxCategory::Board, "첫 100일 평가", false},
    };
    return kBriefings;
}

std::string firstDaysBriefingKey(const std::string& id) { return "first100-" + id; }

bool firstDaysBriefingApplies(const GameState& state, const FirstDaysBriefing& briefing)
{
    return acquired(state) && !(briefing.legacyCovered && legacyStart(state));
}

std::string firstDaysBriefingBody(const GameState& s, const std::string& id)
{
    const auto& finance = s.club.finance;
    const auto& profile = s.ownerProfile;
    const auto directory = playerDirectory(s, s.currentDate);
    const auto players = ownPlayers(s, directory);
    const std::string footer = " 모든 금액은 게임 모델 값이며 역사 사실이 아닙니다.";

    if (id == "diligence-summary") {
        return "인수 당시 확정값: 현금 " + formatMoney(finance.cashPence) + ", 부채 " + formatMoney(finance.debtPence) + ", 주급 약정 "
            + formatMoney(finance.weeklyWageCommitmentPence) + "/주 (예산 " + formatMoney(finance.weeklyWageBudgetPence) + "). "
            + warningList(s) + " 정보 모드 " + infoModeLabel(s.gameSettings.infoMode)
            + (s.gameSettings.infoMode == InfoMode::FullDisclosure ? "." : ". 실사 범위 밖 위험은 첫 100일 안에 드러날 수 있습니다.") + footer;
    }
    if (id == "finance-rules") {
        std::string loan = "인수 대출 없음.";
        if (!profile.acquisitionDebtId.empty()) {
            const auto debt = std::find_if(s.debts.begin(), s.debts.end(), [&](const auto& item) { return item.id == profile.acquisitionDebtId; });
            const DebtRepayment* next = nullptr;
            for (const auto& repayment : s.debtRepayments) {
                if (repayment.debtId != profile.acquisitionDebtId || repayment.status != TransferPaymentStatus::Scheduled) continue;
                if (next == nullptr || repayment.dueDate < next->dueDate) next = &repayment;
            }
            loan = "인수 대출 잔액 " + formatMoney(debt == s.debts.end() ? profile.acquisitionLoanPence : debt->remainingPrincipalPence)
                + (next ? ", 다음 상환 " + next->dueDate.toIsoString() + " " + formatMoney(next->principalPence + next->interestPence) : std::string()) + ".";
        }
        const Money reserve = std::max<Money>(finance.weeklyWageCommitmentPence, finance.weeklyWageBudgetPence) * 26;
        const int coverage = reserve > 0 ? static_cast<int>(finance.cashPence * 100 / reserve) : 0;
        const int usage = finance.weeklyWageBudgetPence > 0 ? static_cast<int>(finance.weeklyWageCommitmentPence * 100 / finance.weeklyWageBudgetPence) : 0;
        return "현금 " + formatMoney(finance.cashPence) + ", 부채 " + formatMoney(finance.debtPence) + ", 주급 약정 " + formatMoney(finance.weeklyWageCommitmentPence)
            + "/주 = 예산의 " + n(usage) + "%. " + loan + " 26주 급여 예비비 " + formatMoney(reserve) + " 대비 현금 " + n(coverage) + "%. 개인 자산 잔액 "
            + formatMoney(profile.personalCashPence) + ". 재정 약속: " + pledgeList(s, PledgeArea::Finance) + "." + footer;
    }
    if (id == "manager-meeting") {
        std::string plans;
        for (const auto& plan : s.recruitmentPlans) {
            if (plan.status != RecruitmentPlanStatus::Active) continue;
            plans += (plans.empty() ? "" : " · ") + plan.title + "(목표 " + n(plan.targetSignings) + "명, " + plan.evaluationDate.toIsoString() + ")";
        }
        return "감독 " + (s.club.manager.name.empty() ? std::string("공석") : s.club.manager.name) + " · " + s.club.manager.tacticalIdentity
            + ". 이사회 신뢰 " + n(s.club.manager.boardConfidence) + "/100. 현재 평가: " + s.club.manager.currentAssessment
            + " 영입 계획: " + (plans.empty() ? "없음" : plans) + ". 축구 방향 약속: " + pledgeList(s, PledgeArea::FootballDirection)
            + ". 선발·전술은 감독 몫이며 구단주는 방향과 예산으로 개입합니다.";
    }
    if (id == "squad-plan") {
        int first = 0, reserve = 0, youth = 0, young = 0, veteran = 0, expiring = 0;
        for (const auto& player : players) {
            const auto group = canonicalGroup(player.row->player.squadGroup);
            if (group == "first_team") ++first;
            else if (group == "reserve") ++reserve;
            else if (group == "youth") ++youth;
            if (player.age > 0 && player.age < kYouthAge) ++young;
            if (player.age >= 30) ++veteran;
            if (player.row->contract && s.currentDate.daysUntil(player.row->contract->endDate) <= 365) ++expiring;
        }
        std::string top;
        int count = 0;
        for (const auto& player : players) {
            if (!firstTeam(player) || player.wage <= 0 || count++ == 3) continue;
            top += (top.empty() ? "" : " · ") + player.row->player.fullName + " " + formatMoney(player.wage);
        }
        return "1군 " + n(first) + "명, 리저브 " + n(reserve) + "명, 유스 " + n(youth) + "명. 23세 미만 " + n(young) + "명, 30세 이상 " + n(veteran)
            + "명, 1년 내 계약 만료 " + n(expiring) + "명. 주급 상위: " + (top.empty() ? "자료 없음" : top)
            + ". 포지션별 진단은 선수단 화면의 선수단 구조 진단에서, 영입은 이적 시장에서 이어집니다.";
    }
    if (id == "organisation") {
        std::set<std::string> roles;
        int contracts = 0;
        for (const auto& contract : s.staffContracts) {
            if (contract.clubId != s.club.id || contract.status != "active") continue;
            ++contracts;
            roles.insert(contract.roles.begin(), contract.roles.end());
        }
        std::string missing;
        for (const char* role : {"manager", "coach", "scout", "medical", "director"}) {
            if (!roles.contains(role)) missing += (missing.empty() ? "" : ", ") + std::string(role);
        }
        return "활성 직원 계약 " + n(contracts) + "건, 역할 " + n(roles.size()) + "종. 공석: " + (missing.empty() ? "없음" : missing)
            + ". 시설·조직 약속: " + pledgeList(s, PledgeArea::FacilitiesOrganisation) + ". 채용·회의는 감독 화면의 직원 운영에서 합니다.";
    }
    if (id == "fan-expectation") {
        const auto governance = s.governanceClubs.find(s.club.id);
        return "팬 신뢰 " + n(s.fanTendency.trustScore) + "/100, 인내 " + n(s.fanTendency.patienceScore) + ", 의심 " + n(s.fanTendency.skepticismScore)
            + ", 분위기 " + s.fanTendency.mood + ". 인수 때 반응: " + profile.fanReaction
            + (governance != s.governanceClubs.end() ? " 공표한 운영 방향: " + governance->second.direction + "." : std::string())
            + " 취임 약속: " + pledgeList(s, std::nullopt) + ". 약속은 기한에 실제 결과로 평가되고 팬 신뢰와 구단주 평판에 반영됩니다.";
    }
    if (id == "expiring-contracts") {
        std::string list;
        int count = 0, total = 0;
        for (const auto& player : players) {
            if (!player.row->contract || s.currentDate.daysUntil(player.row->contract->endDate) > 365) continue;
            ++total;
            if (count++ >= 5) continue;
            list += (list.empty() ? "" : " · ") + player.row->player.fullName + "(" + player.row->contract->endDate.toIsoString() + ", " + formatMoney(player.wage) + ")";
        }
        return "1년 안에 계약이 끝나는 선수 " + n(total) + "명. 주급 상위: " + (list.empty() ? "없음" : list)
            + ". 재계약은 계약·임대 화면에서 시작하고, 핵심 선수는 구단주 운영에서 보유 약속을 걸 수 있습니다.";
    }
    if (id == "review") {
        int open = 0, kept = 0, broken = 0, surfaced = 0;
        std::string keptTitles, brokenTitles;
        for (const auto& pledge : s.ownerPledges) {
            if (pledge.status == "kept") { ++kept; keptTitles += (keptTitles.empty() ? "" : " · ") + pledge.title; }
            else if (pledge.status == "broken") { ++broken; brokenTitles += (brokenTitles.empty() ? "" : " · ") + pledge.title; }
            else if (pledge.status == "open") ++open;
        }
        for (const auto& risk : s.ownerHiddenRisks) if (risk.status != "latent") ++surfaced;
        std::string standing;
        for (const auto& row : s.leagueStandings) {
            if (row.clubId == s.club.id && row.seasonId == s.activeSeasonId) standing = " 리그 " + n(row.position) + "위(" + n(row.played) + "경기).";
        }
        return "취임 100일. 약속 지킴 " + n(kept) + (keptTitles.empty() ? "" : "(" + keptTitles + ")") + ", 어김 " + n(broken)
            + (brokenTitles.empty() ? "" : "(" + brokenTitles + ")") + ", 진행 중 " + n(open) + ". 드러난 숨은 위험 " + n(surfaced) + "건. 현금 "
            + formatMoney(finance.cashPence) + ", 부채 " + formatMoney(finance.debtPence) + ", 팬 신뢰 " + n(s.fanTendency.trustScore) + "/100, 구단주 평판 "
            + n(s.ownerReputation.overallScore) + "/100." + standing + " 남은 약속은 각 기한에 평가하고 첫 시즌 평가는 시즌 종료 때 이어집니다.";
    }
    return {};
}

PledgeVerdict evaluatePledge(const GameState& s, const OwnerPledge& pledge)
{
    PledgeVerdict verdict;
    if (!acquired(s) || pledge.status != "open") return verdict;
    const GameDate since = acquisitionDate(s);
    const bool due = pledge.deadline <= s.currentDate;
    const auto decide = [&](bool kept, std::string evidence) {
        verdict.decided = true;
        verdict.kept = kept;
        verdict.evidence = std::move(evidence);
        return verdict;
    };
    const auto& id = pledge.id;
    const auto& finance = s.club.finance;

    if (id == "invest-first-window") {
        int signings = 0;
        Money fees = 0;
        for (const auto& proposal : s.transferProposals) {
            if (proposal.status == TransferStatus::Completed && proposal.joinDate && *proposal.joinDate >= since && proposal.feePence > 0) {
                ++signings;
                fees += proposal.feePence;
            }
        }
        if (!due) return verdict;
        return signings > 0 ? decide(true, "첫 이적시장에 이적료 영입 " + n(signings) + "명(" + formatMoney(fees) + ")을 마쳤습니다.")
                            : decide(false, "기한까지 이적료를 낸 영입이 없었습니다.");
    }
    if (id == "invest-no-fire-sale") {
        const auto core = splitIds(baselineField(pledge.evidence, "core"));
        for (const auto& offer : s.outgoingTransferOffers) {
            if (offer.status != OutgoingTransferStatus::Completed || !offer.completionDate || *offer.completionDate < since) continue;
            if (std::find(core.begin(), core.end(), offer.playerId) != core.end()) {
                return decide(false, offer.playerName + " 매각(" + formatMoney(offer.feePence) + ", " + offer.completionDate->toIsoString() + ")으로 핵심 선수 매각 없음 약속을 어겼습니다.");
            }
        }
        if (!due) return verdict;
        return decide(true, "핵심 선수 " + n(core.size()) + "명을 기한까지 보유했습니다.");
    }
    if (id == "football-keep-manager") {
        for (const auto& contract : s.staffContracts) {
            if (contract.clubId != s.club.id || !hasRole(contract, "manager")) continue;
            if ((contract.status == "dismissed" || contract.status == "replaced") && contract.endedDate && *contract.endedDate >= since) {
                return decide(false, "감독 계약을 " + contract.endedDate->toIsoString() + "에 끝냈습니다(" + contract.status + ").");
            }
        }
        if (!due) return verdict;
        return decide(true, "첫 시즌 동안 감독 " + s.club.manager.name + "을(를) 유지했습니다.");
    }
    if (id == "football-youth-minutes") {
        if (!due) return verdict;
        const auto stats = playerSeasonStats(s, s.activeSeasonId);
        const auto directory = playerDirectory(s, s.currentDate);
        int young = 0;
        for (const auto& player : ownPlayers(s, directory)) {
            const auto stat = stats.find(player.id);
            if (stat != stats.end() && stat->second.appearances > 0 && player.age > 0 && player.age < kYouthAge) ++young;
        }
        return young >= kYouthPlayersRequired ? decide(true, "23세 미만 선수 " + n(young) + "명이 1군 경기에 나섰습니다.")
                                              : decide(false, "23세 미만 출전 선수가 " + n(young) + "명뿐이었습니다(기준 " + n(kYouthPlayersRequired) + "명).");
    }
    if (id == "football-top-half") {
        const FinalSeasonStanding* mine = nullptr;
        for (const auto& standing : s.seasonHistory) if (standing.row.clubId == s.club.id) mine = &standing;
        if (mine != nullptr) {
            int participants = 0;
            for (const auto& standing : s.seasonHistory) {
                if (standing.row.seasonId == mine->row.seasonId && standing.row.competitionId == mine->row.competitionId) ++participants;
            }
            const std::string where = mine->row.competition + " " + n(mine->row.position) + "위/" + n(participants) + "팀";
            return mine->row.position * 2 <= participants ? decide(true, "첫 시즌 " + where + "으로 상위권 약속을 지켰습니다.")
                                                          : decide(false, "첫 시즌 " + where + "으로 상위권에 들지 못했습니다.");
        }
        if (!due) return verdict;
        if (const auto* season = activeSeason(s); season != nullptr && !season->finalized && season->endDate >= s.currentDate) {
            verdict.newDeadline = season->endDate.addDays(1);
            return verdict;
        }
        verdict.decided = true;
        verdict.withdrawn = true;
        verdict.evidence = "평가할 시즌 기록이 없어 상위권 약속을 철회 처리했습니다.";
        return verdict;
    }
    if (id == "finance-no-new-debt") {
        for (const auto& debt : s.debts) {
            if (debt.clubId != s.club.id || debt.originatedDate <= since) continue;
            // The ledger bootstrap row (legacy_balance) and a surfaced hidden liability are inherited, not borrowed.
            if (debt.id == s.ownerProfile.acquisitionDebtId || debt.kind == "legacy_liability" || debt.kind == "legacy_balance") continue;
            return decide(false, debt.purpose + " " + formatMoney(debt.originalPrincipalPence) + "(" + debt.originatedDate.toIsoString() + ") 신규 차입으로 약속을 어겼습니다.");
        }
        if (!due) return verdict;
        return decide(true, "첫 시즌 동안 새 차입이 없었습니다.");
    }
    if (id == "finance-wage-discipline") {
        const auto overSince = baselineField(pledge.evidence, "over_since");
        if (!overSince.empty()) {
            if (const auto start = GameDate::fromIsoString(overSince); start && start->daysUntil(s.currentDate) >= kWageOverrunStreakDays) {
                return decide(false, "주급 약정 " + formatMoney(finance.weeklyWageCommitmentPence) + "이(가) 예산 " + formatMoney(finance.weeklyWageBudgetPence)
                    + "의 " + n(kWageOverrunPercent) + "%를 " + n(kWageOverrunStreakDays) + "일 넘게 넘었습니다.");
            }
        }
        if (!due) return verdict;
        return finance.weeklyWageCommitmentPence <= finance.weeklyWageBudgetPence
            ? decide(true, "기한에 주급 약정 " + formatMoney(finance.weeklyWageCommitmentPence) + "이(가) 예산 " + formatMoney(finance.weeklyWageBudgetPence) + " 안에 있었습니다.")
            : decide(false, "기한에 주급 약정 " + formatMoney(finance.weeklyWageCommitmentPence) + "이(가) 예산 " + formatMoney(finance.weeklyWageBudgetPence) + "을(를) 넘었습니다.");
    }
    if (id == "finance-reduce-debt") {
        if (!due) return verdict;
        const Money baseline = toMoney(baselineField(pledge.evidence, "debt"), finance.debtPence);
        const Money target = baseline * kDebtReductionPercent / 100;
        const std::string path = formatMoney(baseline) + " → " + formatMoney(finance.debtPence) + " (목표 " + formatMoney(target) + ")";
        return finance.debtPence <= target ? decide(true, "부채 " + path + "로 10% 감축을 지켰습니다.") : decide(false, "부채 " + path + "로 10% 감축에 못 미쳤습니다.");
    }
    if (id == "organisation-staff") {
        if (!due) return verdict;
        for (const auto& contract : s.staffContracts) {
            if (contract.clubId != s.club.id || contract.status != "active" || contract.startDate < since) continue;
            if (!hasRole(contract, "scout") && !hasRole(contract, "medical")) continue;
            const auto person = s.staffPeople.find(contract.personId);
            return decide(true, "인수 뒤 " + (person != s.staffPeople.end() ? person->second.name : contract.personId) + "을(를) 스카우팅·의료 담당으로 채용했습니다.");
        }
        return decide(false, "기한까지 스카우팅·의료 담당을 보강하지 않았습니다.");
    }
    if (id == "organisation-meetings") {
        if (!due) return verdict;
        int held = 0;
        for (const auto& meeting : s.operatingMeetings) if (meeting.status == OperatingMeetingStatus::Held && meeting.meetingDate >= since) ++held;
        return held >= kMeetingsRequired ? decide(true, "운영 회의를 " + n(held) + "번 열었습니다.")
                                         : decide(false, "운영 회의가 " + n(held) + "번뿐이었습니다(기준 " + n(kMeetingsRequired) + "번).");
    }
    if (id == "fans-identity") {
        if (!due) return verdict;
        const int baseline = toInt(baselineField(pledge.evidence, "trust"), 50);
        const std::string path = n(baseline) + " → " + n(s.fanTendency.trustScore);
        return s.fanTendency.trustScore >= baseline - kIdentityTrustTolerance ? decide(true, "팬 신뢰 " + path + "로 구단 정체성 신뢰를 지켰습니다.")
                                                                             : decide(false, "팬 신뢰 " + path + "로 정체성 약속에 대한 믿음이 무너졌습니다.");
    }
    if (id == "fans-transparency") {
        if (!due) return verdict;
        int statements = 0;
        for (const auto& event : s.governanceEvents) if (event.clubId == s.club.id && event.kind == "statement" && event.date >= since) ++statements;
        return statements >= kStatementsRequired ? decide(true, "운영 방향을 " + n(statements) + "번 공개 설명했습니다.")
                                                 : decide(false, "공개 설명이 " + n(statements) + "번뿐이었습니다(기준 " + n(kStatementsRequired) + "번).");
    }
    if (id == "fans-local-ties") {
        if (!due) return verdict;
        const auto directory = playerDirectory(s, s.currentDate);
        int young = 0;
        for (const auto& player : ownPlayers(s, directory)) if (firstTeam(player) && player.age > 0 && player.age < kYouthAge) ++young;
        return young >= kYouthPlayersRequired ? decide(true, "1군에 23세 미만 선수 " + n(young) + "명을 두어 지역·홈그로운 존중을 지켰습니다.")
                                              : decide(false, "1군의 23세 미만 선수가 " + n(young) + "명뿐이었습니다(기준 " + n(kYouthPlayersRequired) + "명).");
    }
    if (due) {
        verdict.decided = true;
        verdict.withdrawn = true;
        verdict.evidence = "평가 규칙이 없는 약속이라 기한에 철회 처리했습니다.";
    }
    return verdict;
}

std::string surfaceHiddenRisk(GameState& s, OwnerHiddenRisk& risk)
{
    const auto directory = playerDirectory(s, s.currentDate);
    const auto players = ownPlayers(s, directory);
    const auto* operating = operatingProfile(s, s.club.id);
    const auto obligation = [&](const std::string& category, Money amount, const GameDate& due, bool overdue) -> std::string {
        if (amount <= 0) return {};
        if (operating != nullptr) {
            OperatingObligation item;
            item.id = "hidden-risk:" + risk.id + ":" + s.currentDate.toIsoString();
            item.clubId = s.club.id;
            item.seasonId = operating->seasonId;
            item.dueDate = due;
            item.amountPence = amount;
            item.category = category;
            item.status = overdue ? TransferPaymentStatus::Overdue : TransferPaymentStatus::Scheduled;
            item.provenance = kProvenance;
            risk.linkedKind = "obligation";
            risk.linkedId = item.id;
            s.operatingObligations.push_back(std::move(item));
            return overdue ? "미지급 의무 " + formatMoney(amount) + "이(가) 즉시 잡혔고 운영 재정이 현금이 닿는 대로 정산합니다."
                           : formatMoney(amount) + "을(를) " + due.toIsoString() + "까지 지급해야 합니다.";
        }
        s.club.finance.cashPence -= amount;
        risk.linkedKind = "cash";
        return formatMoney(amount) + "을(를) 구단 현금에서 바로 지출했습니다.";
    };
    risk.linkedKind = "none";
    risk.linkedId.clear();

    std::string effect;
    if (risk.id == "hidden-debt" && risk.amountPence > 0) {
        ensureDebtLedger(s);
        const std::string debtId = "legacy-liability-" + s.club.id;
        if (std::none_of(s.debts.begin(), s.debts.end(), [&](const auto& debt) { return debt.id == debtId; })) {
            const int rate = loanAnnualRateBpsForYear(s.currentDate.year);
            std::vector<DebtRepayment> repayments;
            try {
                repayments = makeDebtRepayments(debtId, risk.amountPence, rate, kLegacyLiabilityMonths, s.currentDate);
            } catch (const std::invalid_argument&) {
                repayments.clear();
            }
            if (!repayments.empty()) {
                ClubDebt debt;
                debt.id = debtId;
                debt.clubId = s.club.id;
                debt.purpose = "인수 전 장부 밖 부채 (실사 미발견)";
                debt.kind = "legacy_liability";
                debt.originalPrincipalPence = risk.amountPence;
                debt.remainingPrincipalPence = risk.amountPence;
                debt.annualRateBps = rate;
                debt.originatedDate = s.currentDate;
                debt.firstRepaymentDate = repayments.front().dueDate;
                debt.maturityDate = repayments.back().dueDate;
                debt.status = "active";
                debt.model = "first_hundred_days_v1_gameplay";
                s.debts.push_back(debt);
                s.debtRepayments.insert(s.debtRepayments.end(), repayments.begin(), repayments.end());
                synchronizeDebtTotals(s);
                effect = "구단 부채가 " + formatMoney(risk.amountPence) + " 늘었고(연 " + n(rate / 100) + "." + n((rate % 100) / 10) + "%, "
                    + n(kLegacyLiabilityMonths) + "개월 상환) 부채 합계는 " + formatMoney(s.club.finance.debtPence) + "입니다.";
                if (addHiddenDebtToBaseline(s, risk.amountPence, s.currentDate)) effect += " 부채 감축 약속의 기준선에 이 금액을 더했습니다.";
            }
        }
        if (std::any_of(s.debts.begin(), s.debts.end(), [&](const auto& debt) { return debt.id == debtId; })) {
            risk.linkedKind = "debt";
            risk.linkedId = debtId;
        }
    } else if (risk.id == "wage-arrears") {
        effect = obligation("player_payroll", risk.amountPence, s.currentDate, true);
    } else if (risk.id == "stadium-repair") {
        effect = obligation("stadium_repair", risk.amountPence, s.currentDate.addDays(kScheduledCostDays), false);
    } else if (risk.id == "regulatory-probe") {
        effect = obligation("regulatory_fine", risk.amountPence, s.currentDate.addDays(kScheduledCostDays), false);
    } else if (risk.id == "key-player-exit") {
        const OwnPlayer* target = nullptr;
        for (const auto& player : players) {
            if (firstTeam(player) && player.row->contract) {
                target = &player;
                break;
            }
        }
        if (target != nullptr) {
            if (auto relation = s.playerRelationships.find(s.club.id + "|" + target->id); relation != s.playerRelationships.end()) {
                relation->second.morale = std::clamp(relation->second.morale - 15, 0, 100);
            }
            // The request joins an open case about keeping this player here, if there is one.
            // Any other open case of the player (unpaid wages, a contract renewal) is a different
            // matter: settling that one must not settle the transfer request.
            const auto retainCase = [&](const OwnerCase& item) {
                return item.clubId == s.club.id && item.kind == "retain_player" && item.subjectId == target->id
                    && (item.status == "requested" || item.status == "promised");
            };
            if (std::none_of(s.ownerCases.begin(), s.ownerCases.end(), retainCase)) {
                OwnerCase item;
                item.id = "owner-case:" + n(++s.governanceRevision);
                item.clubId = s.club.id;
                item.subjectId = target->id;
                item.subjectName = target->row->player.fullName;
                item.kind = "retain_player";
                item.source = "hidden_risk";
                item.status = "requested";
                item.createdDate = s.currentDate;
                item.deadline = s.currentDate.addDays(kTransferRequestDays);
                item.baselineContractEnd = target->row->contract->endDate;
                item.reason = "에이전트가 이적 허용을 요구합니다. 보유 약속으로 응답하거나 사유를 들어 거절할 수 있습니다(구단주 홈 → 약속·팬·선수단).";
                s.ownerCases.push_back(std::move(item));
            }
            if (const auto linked = std::find_if(s.ownerCases.begin(), s.ownerCases.end(), retainCase); linked != s.ownerCases.end()) {
                risk.linkedKind = "case";
                risk.linkedId = linked->id;
            }
            effect = target->row->player.fullName + "(주급 " + formatMoney(target->wage) + ")이(가) 이적을 요구했습니다. 사기가 떨어졌고 " + n(kTransferRequestDays) + "일 안에 응답해야 합니다.";
        } else {
            effect = "요구할 1군 선수가 없어 효과가 없었습니다.";
        }
    } else if (risk.id == "injury-backlog") {
        const OwnPlayer* target = nullptr;
        int rank = 0;
        for (const auto& player : players) {
            if (!firstTeam(player)) continue;
            if (rank++ == 0) continue; // the top earner is the transfer-request candidate; hurt the next one
            const auto development = s.playerDevelopment.find(player.id);
            if (development == s.playerDevelopment.end()) continue;
            if (development->second.returnDate && *development->second.returnDate > s.currentDate) continue;
            target = &player;
            break;
        }
        if (target != nullptr) {
            auto& development = s.playerDevelopment.at(target->id);
            development.injuredDate = s.currentDate;
            development.recoveryDate = s.currentDate.addDays(kBacklogInjuryDays);
            development.returnDate = s.currentDate.addDays(kBacklogInjuryDays + 7);
            development.injury = "숨겨져 있던 장기 부상 (게임 모델)";
            development.programme = "rehab";
            risk.linkedKind = "player";
            risk.linkedId = target->id;
            effect = target->row->player.fullName + "의 부상이 공개된 것보다 길어 " + development.returnDate->toIsoString() + "까지 뛸 수 없습니다.";
        } else {
            effect = "해당하는 선수가 없어 효과가 없었습니다.";
        }
    }

    risk.status = "surfaced";
    const std::string intro = risk.disclosed ? "실사에서 확인했던 위험이 현실이 됐습니다. " : "실사가 보지 못한 위험이 드러났습니다. ";
    return intro + risk.detail + (effect.empty() ? std::string() : " " + effect) + " 모든 금액은 게임 모델 값입니다.";
}

bool restoreHiddenRiskLinks(GameState& s)
{
    std::vector<std::string> unlinked;
    for (const auto& risk : s.ownerHiddenRisks)
        if (risk.status != "latent" && risk.linkedKind.empty()) unlinked.push_back(risk.id);
    if (unlinked.empty()) return false;
    linkSurfacedHiddenRisks(s);
    for (const auto& risk : s.ownerHiddenRisks) {
        if (risk.id != "hidden-debt" || risk.linkedKind != "debt"
            || std::find(unlinked.begin(), unlinked.end(), risk.id) == unlinked.end()) continue;
        const auto debt = std::find_if(s.debts.begin(), s.debts.end(), [&](const auto& item) { return item.id == risk.linkedId; });
        if (debt != s.debts.end()) (void)addHiddenDebtToBaseline(s, debt->originalPrincipalPence, debt->originatedDate);
    }
    refreshHiddenRiskResolutions(s);
    return true;
}

void processFirstHundredDaysDay(GameState& s)
{
    if (!acquired(s)) return;
    ensurePledgeBaselines(s);

    // Wage discipline watches a streak, so it needs a daily marker.
    for (auto& pledge : s.ownerPledges) {
        if (pledge.id != "finance-wage-discipline" || pledge.status != "open") continue;
        const auto& finance = s.club.finance;
        const bool over = finance.weeklyWageBudgetPence > 0 && finance.weeklyWageCommitmentPence > finance.weeklyWageBudgetPence * kWageOverrunPercent / 100;
        const auto marker = baselineField(pledge.evidence, "over_since");
        if (over && marker.empty()) setBaselineField(pledge.evidence, "over_since", s.currentDate.toIsoString());
        if (!over && !marker.empty()) clearBaselineField(pledge.evidence, "over_since");
    }

    const GameDate since = acquisitionDate(s);
    for (const auto& briefing : firstDaysBriefings()) {
        if (briefing.id == "review" || !firstDaysBriefingApplies(s, briefing)) continue;
        if (since.addDays(briefing.dayOffset) > s.currentDate) continue;
        const auto key = firstDaysBriefingKey(briefing.id);
        if (inboxHas(s, key)) continue;
        pushInbox(s, key, briefing.category, briefing.title, firstDaysBriefingBody(s, briefing.id));
    }

    for (auto& risk : s.ownerHiddenRisks) {
        if (risk.status != "latent" || risk.surfacesOn > s.currentDate) continue;
        const std::string body = surfaceHiddenRisk(s, risk);
        const auto category = risk.area == "finance" || risk.area == "legal" ? InboxCategory::Finance : InboxCategory::Board;
        pushInbox(s, "hidden-risk-surfaced-" + risk.id, category, "숨은 위험 드러남 · " + risk.title, body);
        OwnerActionImpact impact;
        impact.fanTrustDelta = -std::clamp(risk.severity, 1, 3);
        impact.reputationDelta = risk.disclosed ? 0 : -1;
        impact.skepticismDelta = std::clamp(risk.severity, 1, 3);
        recordOwnerAction(s, "hidden_risk_surfaced", "hidden_risk", risk.id, risk.title, risk.title + " · " + severityLabel(risk.severity), impact, kProvenance);
    }

    for (auto& pledge : s.ownerPledges) {
        if (pledge.status != "open") continue;
        const auto verdict = evaluatePledge(s, pledge);
        if (verdict.newDeadline && !verdict.decided) {
            if (*verdict.newDeadline > pledge.deadline) pledge.deadline = *verdict.newDeadline;
            continue;
        }
        if (!verdict.decided) continue;
        pledge.status = verdict.withdrawn ? "withdrawn" : verdict.kept ? "kept" : "broken";
        pledge.evidence = verdict.evidence + " (" + s.currentDate.toIsoString() + ")";
        const std::string label = verdict.withdrawn ? "취임 약속 철회 · " : verdict.kept ? "취임 약속 지킴 · " : "취임 약속 어김 · ";
        pushInbox(s, "pledge-verdict-" + pledge.id, InboxCategory::Board, label + pledge.title,
            pledge.detail + "\n" + verdict.evidence + (verdict.withdrawn ? "" : verdict.kept ? " 팬 신뢰와 구단주 평판이 조금 올랐습니다." : " 팬 신뢰와 구단주 평판이 떨어졌습니다."));
        if (!verdict.withdrawn) {
            OwnerActionImpact impact;
            impact.fanTrustDelta = verdict.kept ? kKeptFanTrust : kBrokenFanTrust;
            impact.reputationDelta = verdict.kept ? kKeptReputation : kBrokenReputation;
            impact.skepticismDelta = verdict.kept ? -1 : 2;
            recordOwnerAction(s, verdict.kept ? "pledge_kept" : "pledge_broken", "owner_pledge", pledge.id, pledge.title, verdict.evidence, impact, kProvenance);
        }
    }

    // Which surfaced risks are actually dealt with by now (obligation settled, debt repaid,
    // player back, case fulfilled). Runs after the operating and debt settlements of the day.
    refreshHiddenRiskResolutions(s);

    // The hundred-day review goes out after the day's verdicts so it can count them.
    for (const auto& briefing : firstDaysBriefings()) {
        if (briefing.id != "review" || !firstDaysBriefingApplies(s, briefing)) continue;
        if (since.addDays(briefing.dayOffset) > s.currentDate || inboxHas(s, firstDaysBriefingKey(briefing.id))) continue;
        pushInbox(s, firstDaysBriefingKey(briefing.id), briefing.category, briefing.title, firstDaysBriefingBody(s, briefing.id));
    }
}

std::optional<GameDate> nextFirstHundredDaysDate(const GameState& s)
{
    if (!acquired(s)) return std::nullopt;
    std::optional<GameDate> next;
    const auto consider = [&](GameDate date) {
        if (date <= s.currentDate) date = s.currentDate.addDays(1); // overdue work runs on the next processed day
        if (!next || date < *next) next = date;
    };
    const GameDate since = acquisitionDate(s);
    for (const auto& briefing : firstDaysBriefings()) {
        if (firstDaysBriefingApplies(s, briefing) && !inboxHas(s, firstDaysBriefingKey(briefing.id))) consider(since.addDays(briefing.dayOffset));
    }
    for (const auto& risk : s.ownerHiddenRisks) if (risk.status == "latent") consider(risk.surfacesOn);
    for (const auto& pledge : s.ownerPledges) if (pledge.status == "open") consider(pledge.deadline);
    return next;
}

FirstHundredDaysView firstHundredDaysView(const GameState& s)
{
    FirstHundredDaysView view;
    if (!acquired(s)) return view;
    view.active = true;
    const GameDate since = acquisitionDate(s);
    view.day = since.daysUntil(s.currentDate);
    if (view.day <= 7) { view.phaseId = "emergency"; view.phaseLabel = "긴급 (취임 첫 주)"; }
    else if (view.day <= 30) { view.phaseId = "first_month"; view.phaseLabel = "첫 30일"; }
    else if (view.day <= 60) { view.phaseId = "first_window"; view.phaseLabel = "첫 이적시장"; }
    else if (view.day <= kFirstHundredDays) { view.phaseId = "first_season"; view.phaseLabel = "첫 시즌 준비"; }
    else { view.phaseId = "complete"; view.phaseLabel = "100일 완료"; }
    for (const auto& briefing : firstDaysBriefings()) {
        FirstHundredDaysView::Briefing row;
        row.id = briefing.id;
        row.title = briefing.title;
        row.date = since.addDays(briefing.dayOffset);
        row.skipped = !firstDaysBriefingApplies(s, briefing);
        row.delivered = !row.skipped && inboxHas(s, firstDaysBriefingKey(briefing.id));
        view.briefings.push_back(std::move(row));
    }
    for (const auto& pledge : s.ownerPledges) {
        if (pledge.status == "open") {
            ++view.pledgesOpen;
            if (!view.nextPledgeDeadline || pledge.deadline < *view.nextPledgeDeadline) view.nextPledgeDeadline = pledge.deadline;
        } else if (pledge.status == "kept") {
            ++view.pledgesKept;
        } else if (pledge.status == "broken") {
            ++view.pledgesBroken;
        }
    }
    for (const auto& risk : s.ownerHiddenRisks) {
        ++view.risksTotal;
        if (risk.disclosed) ++view.risksDisclosed;
        if (risk.status != "latent") ++view.risksSurfaced;
    }
    view.nextDate = nextFirstHundredDaysDate(s);
    return view;
}

} // namespace football_owner
