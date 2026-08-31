#include "core/simulation_engine.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace football_owner {
namespace {

Money percentageOf(const Money value, const int percentage)
{
    return value * static_cast<Money>(percentage) / 100;
}

Money clubNegotiationValue(
    const TransferFeeTerms& terms,
    const Money marketValuePence)
{
    return guaranteedTransferFee(terms)
        + terms.appearanceBonusPence / 2
        + percentageOf(
            marketValuePence,
            terms.sellOnPercentage) / 4;
}

TransferFeeTerms recommendedOpeningTerms(const TransferCandidate& candidate)
{
    return {
        candidate.openingOfferPence,
        0,
        0,
        0,
        0,
        0
    };
}

TransferFeeTerms sellingClubCounterTerms(
    const TransferCandidate& candidate,
    const TransferPursuit& pursuit)
{
    const Money target = std::max(
        candidate.askingPricePence,
        pursuit.rivalBidActive ? pursuit.rivalBidPence : Money {});
    const Money upfront = percentageOf(target, 70);
    return {
        upfront,
        target - upfront,
        2,
        percentageOf(target, 10),
        30,
        10
    };
}

PlayerContractTerms recommendedPlayerTerms(const TransferCandidate& candidate)
{
    const Money wage = candidate.expectedWeeklyWagePence;
    std::string role = candidate.managerPriority >= 90
        ? "Key Player"
        : (candidate.managerPriority >= 75 ? "First Team" : "Rotation");
    return {
        wage,
        candidate.expectedContractYears,
        wage * 8,
        wage * 4,
        wage / 10,
        wage / 8,
        wage * 6,
        0,
        std::move(role)
    };
}

Money contractOfferWeeklyValue(const PlayerContractTerms& terms)
{
    const int years = std::max(1, terms.contractYears);
    const Money fixedBonuses = terms.signingBonusPence + terms.loyaltyBonusPence;
    const Money variableEstimate = terms.appearanceFeePence * 20
        + terms.goalBonusPence * 8;
    return terms.weeklyWagePence
        + (fixedBonuses + variableEstimate)
            / static_cast<Money>(years * 52);
}

bool playerAcceptsTerms(
    const TransferCandidate& candidate,
    const PlayerContractTerms& offer)
{
    const PlayerContractTerms expected = recommendedPlayerTerms(candidate);
    const int interestPremium = candidate.playerInterest < 55 ? 110 : 100;
    const Money requiredWeeklyValue =
        percentageOf(contractOfferWeeklyValue(expected), interestPremium);
    const bool durationAcceptable =
        offer.contractYears >= std::max(1, candidate.expectedContractYears - 1);
    const bool agentSatisfied =
        offer.agentFeePence >= expected.agentFeePence * 3 / 4;
    return durationAcceptable
        && agentSatisfied
        && contractOfferWeeklyValue(offer) >= requiredWeeklyValue;
}

std::uint32_t stablePercent(const std::string_view value)
{
    std::uint32_t hash = 2'166'136'261U;
    for (const unsigned char character : value) {
        hash ^= character;
        hash *= 16'777'619U;
    }
    return hash % 100U;
}

bool isClosedPursuitStage(const TransferPursuitStage stage)
{
    return stage == TransferPursuitStage::ClubRejected
        || stage == TransferPursuitStage::PlayerRejected
        || stage == TransferPursuitStage::MedicalFailed
        || stage == TransferPursuitStage::RegistrationFailed
        || stage == TransferPursuitStage::OwnerRejected
        || stage == TransferPursuitStage::Withdrawn
        || stage == TransferPursuitStage::Completed
        || stage == TransferPursuitStage::Expired;
}

bool isClosedLoanStage(const LoanStage stage)
{
    return stage == LoanStage::Recalled
        || stage == LoanStage::Completed
        || stage == LoanStage::Rejected
        || stage == LoanStage::Withdrawn
        || stage == LoanStage::Expired;
}

bool isClosedRenewalStage(const ContractRenewalStage stage)
{
    return stage == ContractRenewalStage::Completed
        || stage == ContractRenewalStage::Rejected
        || stage == ContractRenewalStage::Withdrawn
        || stage == ContractRenewalStage::Expired;
}

Money loanWeeklyWageShare(
    const Money weeklyWagePence,
    const LoanTerms& terms)
{
    return percentageOf(
        weeklyWagePence,
        terms.borrowingClubWagePercentage);
}

bool domesticRegistrationOpen(
    const GameState& state,
    const GameDate& date)
{
    if (state.eraRules.transferWindowsActive) {
        return true;
    }
    const int cutoffYear = date.month >= 7
        ? date.year + 1
        : date.year;
    const GameDate cutoff {
        cutoffYear,
        state.eraRules.registrationCutoffMonth,
        state.eraRules.registrationCutoffDay
    };
    return cutoff.isValid() && date <= cutoff;
}

bool aiDealRequiresNewRegistration(const AiMarketDealType type)
{
    return type == AiMarketDealType::Transfer
        || type == AiMarketDealType::Loan
        || type == AiMarketDealType::Buyout;
}

Money homeClubWageShare(
    const GameState& state,
    const std::string& playerId,
    const Money weeklyWagePence)
{
    const auto outgoingLoan = std::find_if(
        state.loanDeals.begin(),
        state.loanDeals.end(),
        [&playerId](const LoanDeal& loan) {
            return loan.player.id == playerId
                && loan.direction == LoanDirection::Outgoing
                && (loan.stage == LoanStage::Active
                    || loan.stage == LoanStage::RecallPending);
        });
    if (outgoingLoan == state.loanDeals.end()) {
        return weeklyWagePence;
    }
    return weeklyWagePence
        - loanWeeklyWageShare(weeklyWagePence, outgoingLoan->terms);
}

LoanTerms recommendedIncomingLoanTerms(
    const TransferCandidate& candidate,
    const GameDate& currentDate)
{
    const GameDate seasonEnd = currentDate.month >= 6
        ? GameDate {currentDate.year + 1, 5, 31}
        : GameDate {currentDate.year, 5, 31};
    return {
        std::max<Money>(100'000, candidate.marketValuePence / 50),
        100,
        candidate.managerPriority >= 80
            ? "Regular Starter"
            : "Squad Player",
        seasonEnd,
        true,
        7,
        "option",
        candidate.askingPricePence,
        true,
        0,
        "loan_club"
    };
}

Money loanProposalValue(
    const LoanTerms& terms,
    const TransferCandidate& candidate)
{
    Money value = terms.loanFeePence;
    value += percentageOf(
        candidate.expectedWeeklyWagePence * 52,
        terms.borrowingClubWagePercentage);
    if (terms.futureFeeType == "mandatory") {
        value += terms.futureFeePence;
    } else if (terms.futureFeeType == "option") {
        value += terms.futureFeePence / 5;
    }
    return value;
}

bool renewalOfferAccepted(
    const ContractRenewalCase& renewal,
    const PlayerContractTerms& terms)
{
    const Money required = percentageOf(
        contractOfferWeeklyValue(renewal.recommendedTerms),
        renewal.playerInterest >= 70 ? 95 : 100);
    return terms.contractYears
            >= std::max(1, renewal.recommendedTerms.contractYears - 1)
        && terms.agentFeePence
            >= renewal.recommendedTerms.agentFeePence * 3 / 4
        && contractOfferWeeklyValue(terms) >= required;
}

std::optional<std::string> transferCapacityError(
    const GameState& state,
    const TransferFeeTerms& feeTerms,
    const PlayerContractTerms* contractTerms,
    const std::string& excludedPursuitId = {},
    const std::string& excludedProposalId = {})
{
    const Money commitment = contractTerms != nullptr
        ? transferBudgetCommitment(feeTerms, *contractTerms)
        : maximumTransferFee(feeTerms);
    if (availableTransferCash(
            state,
            excludedPursuitId,
            excludedProposalId) < commitment) {
        return "기존 미정산 이적 지출을 제외한 가용 현금보다 큰 금액은 약정할 수 없습니다.";
    }
    if (availableTransferBudget(
            state,
            excludedPursuitId,
            excludedProposalId) < commitment) {
        return "현재 이적 예산보다 큰 금액은 약정할 수 없습니다.";
    }
    return std::nullopt;
}

bool validScoutingFocus(const std::string& focus)
{
    return focus == "overall"
        || focus == "current_ability"
        || focus == "potential"
        || focus == "availability";
}

std::string lowerAscii(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

bool containsAsciiInsensitive(
    const std::string& haystack,
    const std::string& needle)
{
    return needle.empty()
        || lowerAscii(haystack).find(lowerAscii(needle)) != std::string::npos;
}

bool playerLooksHomegrown(const Player& player)
{
    return containsAsciiInsensitive(player.nationality, "England")
        || containsAsciiInsensitive(player.nationality, "Wales")
        || containsAsciiInsensitive(player.nationality, "Scotland")
        || containsAsciiInsensitive(player.nationality, "Ireland");
}

bool playerIsYoungForFanNarrative(
    const Player& player,
    const GameDate& date)
{
    const auto age = ageOnDate(player, date);
    return age.has_value() && *age <= 23;
}

OwnerActionImpact ownerImpactForApprovedTransfer(
    const GameState& state,
    const TransferProposal& proposal,
    const Money committedBudget)
{
    OwnerActionImpact impact;
    impact.reputationDelta = 1;
    impact.fanTrustDelta = 1;
    impact.ambitionDelta = 2;
    impact.prudenceDelta = -1;
    if (committedBudget >= 300'000'000) {
        impact.starPreferenceDelta = 1;
        impact.skepticismDelta = 1;
    }
    if (playerLooksHomegrown(proposal.target)) {
        impact.homegrownPreferenceDelta = 1;
    }
    if (playerIsYoungForFanNarrative(proposal.target, state.currentDate)) {
        impact.youthPreferenceDelta = 1;
    }
    return impact;
}

OwnerActionImpact ownerImpactForRejectedTransfer()
{
    OwnerActionImpact impact;
    impact.fanTrustDelta = -1;
    impact.ambitionDelta = -1;
    impact.prudenceDelta = 1;
    impact.skepticismDelta = 1;
    return impact;
}

OwnerActionImpact ownerImpactForTargetScoutingDirective(
    const GameState& state,
    const TransferCandidate& candidate)
{
    OwnerActionImpact impact;
    impact.reputationDelta = 1;
    impact.fanTrustDelta = 1;
    impact.ambitionDelta = 1;
    if (playerLooksHomegrown(candidate.player)) {
        impact.homegrownPreferenceDelta = 1;
    }
    if (playerIsYoungForFanNarrative(candidate.player, state.currentDate)) {
        impact.youthPreferenceDelta = 1;
    }
    return impact;
}

OwnerActionImpact ownerImpactForRangeScoutingDirective(
    const bool homegrownOnly,
    const int maxAge,
    const Money maxBudgetPence)
{
    OwnerActionImpact impact;
    impact.reputationDelta = 1;
    impact.fanTrustDelta = 1;
    impact.ambitionDelta = 1;
    if (homegrownOnly) {
        impact.homegrownPreferenceDelta = 1;
    }
    if (maxAge > 0 && maxAge <= 25) {
        impact.youthPreferenceDelta = 1;
    }
    if (maxBudgetPence > 0 && maxBudgetPence <= 1'000'000'000) {
        impact.prudenceDelta = 1;
    }
    return impact;
}

bool scoutingRegionMatches(
    const TransferCandidate& candidate,
    const std::string& region)
{
    if (region.empty() || region == "any") {
        return true;
    }
    const std::string filter = lowerAscii(region);
    const std::string country = lowerAscii(candidate.player.nationality);
    if (filter.find("united kingdom") != std::string::npos
        || filter == "uk"
        || filter.find("ireland") != std::string::npos) {
        return country == "england"
            || country == "scotland"
            || country == "wales"
            || country == "republic of ireland"
            || country == "northern ireland";
    }
    if (filter.find("england") != std::string::npos) {
        return country == "england";
    }
    if (filter.find("france") != std::string::npos) {
        return country == "france";
    }
    return containsAsciiInsensitive(candidate.player.nationality, region);
}

bool scoutingPositionMatches(
    const TransferCandidate& candidate,
    const std::string& position)
{
    if (position.empty() || position == "any") {
        return true;
    }
    return containsAsciiInsensitive(candidate.player.position, position)
        || scoutingPositionGroup(candidate.player.position) == position;
}

bool scoutingStyleMatches(
    const TransferCandidate& candidate,
    const std::string& style)
{
    if (style.empty() || style == "any") {
        return true;
    }
    const std::string key = lowerAscii(style);
    const std::string text = candidate.roleFit + " "
        + candidate.scoutSummary + " " + candidate.player.position;
    if (key == "potential") {
        return candidate.potentialHigh >= 88
            || text.find("성장") != std::string::npos
            || text.find("장기") != std::string::npos;
    }
    if (key == "leadership") {
        return text.find("리더십") != std::string::npos;
    }
    if (key == "physical") {
        return text.find("신체") != std::string::npos
            || text.find("피지컬") != std::string::npos
            || text.find("제공권") != std::string::npos;
    }
    if (key == "creative") {
        return text.find("창조") != std::string::npos
            || text.find("연계") != std::string::npos;
    }
    if (key == "versatile") {
        return candidate.player.position.find('/') != std::string::npos
            || text.find("다목적") != std::string::npos;
    }
    return containsAsciiInsensitive(text, style);
}

bool scoutingHomegrownProxyMatches(const TransferCandidate& candidate)
{
    const std::string country = lowerAscii(candidate.player.nationality);
    return country == "england"
        || country == "scotland"
        || country == "wales"
        || country == "northern ireland";
}

std::string scoutingRegionKeyForCandidate(const TransferCandidate& candidate)
{
    const std::string country = lowerAscii(candidate.player.nationality);
    if (country == "england") {
        return "England";
    }
    if (country == "scotland"
        || country == "wales"
        || country == "republic of ireland"
        || country == "northern ireland") {
        return "United Kingdom & Ireland";
    }
    if (country == "france") {
        return "France";
    }
    return candidate.player.nationality;
}

int scoutingCandidateFamiliarityScore(
    const GameState& state,
    const ScoutProfile& scout,
    const TransferCandidate& candidate)
{
    const int country = scoutFamiliarityLevel(
        state,
        scout.id,
        "country",
        candidate.player.nationality);
    const int region = scoutFamiliarityLevel(
        state,
        scout.id,
        "region",
        scoutingRegionKeyForCandidate(candidate));
    const int position = scoutFamiliarityLevel(
        state,
        scout.id,
        "position_group",
        scoutingPositionGroup(candidate.player.position));
    const int ageBand = scoutFamiliarityLevel(
        state,
        scout.id,
        "age_band",
        scoutingAgeBand(candidate.player, state.currentDate));
    const int market = scoutFamiliarityLevel(
        state,
        scout.id,
        "market",
        "domestic");

    return std::clamp(
        (country * 3 + region * 2 + position * 2 + ageBand + market) / 9,
        0,
        100);
}

int scoutingDurationDaysWithFamiliarity(
    const GameState& state,
    const ScoutProfile& scout,
    const TransferCandidate& candidate,
    const ScoutingDepth depth)
{
    const int baseDuration = scoutingDurationDays(depth);
    const int familiarity = scoutingCandidateFamiliarityScore(
        state,
        scout,
        candidate);
    const int maxReduction = depth == ScoutingDepth::Quick
        ? 1
        : (depth == ScoutingDepth::Standard ? 2 : 3);
    const int reduction = std::min(maxReduction, familiarity / 35);
    return std::max(1, baseDuration - reduction);
}

int scoutingKnowledgeGainWithFamiliarity(
    const GameState& state,
    const ScoutProfile& scout,
    const TransferCandidate& candidate,
    const ScoutingDepth depth)
{
    const int baseGain = scoutingKnowledgeGain(depth);
    const int familiarity = scoutingCandidateFamiliarityScore(
        state,
        scout,
        candidate);
    return baseGain + std::min(8, familiarity / 14);
}

int scoutingAccuracyModifierWithFamiliarity(
    const GameState& state,
    const ScoutProfile& scout,
    const TransferCandidate& candidate)
{
    const int familiarity = scoutingCandidateFamiliarityScore(
        state,
        scout,
        candidate);
    return std::min(8, familiarity / 14);
}

int scoutingDirectiveCandidateScore(
    const GameState& state,
    const ScoutProfile& scout,
    const TransferCandidate& candidate,
    const Money maxBudgetPence)
{
    int score = 40;
    score += candidate.managerPriority / 3;
    score += candidate.playerInterest / 5;
    score += std::max(0, 24 - candidate.recommendationRank * 3);
    score += scoutFamiliarityLevel(
        state,
        scout.id,
        "country",
        candidate.player.nationality) / 5;
    score += scoutFamiliarityLevel(
        state,
        scout.id,
        "region",
        scoutingRegionKeyForCandidate(candidate)) / 7;
    score += scoutFamiliarityLevel(
        state,
        scout.id,
        "position_group",
        scoutingPositionGroup(candidate.player.position)) / 6;
    score += scoutFamiliarityLevel(
        state,
        scout.id,
        "age_band",
        scoutingAgeBand(candidate.player, state.currentDate)) / 7;
    score += scoutFamiliarityLevel(state, scout.id, "market", "domestic") / 8;
    if (maxBudgetPence > 0 && candidate.askingPricePence <= maxBudgetPence) {
        score += 8;
    }
    return std::clamp(score, 0, 100);
}

std::string joinPlayerIds(const std::vector<const TransferCandidate*>& candidates)
{
    std::string result;
    for (const auto* candidate : candidates) {
        if (candidate == nullptr) {
            continue;
        }
        if (!result.empty()) {
            result += ";";
        }
        result += candidate->player.id;
    }
    return result;
}

} // namespace

SimulationEngine::SimulationEngine(GameState initialState)
    : state_(std::move(initialState))
{
    if (!state_.currentDate.isValid()) {
        throw std::invalid_argument("Simulation requires a valid current date");
    }
    ensureDefaultAiMarketState(state_);
    ensureDefaultScoutFamiliarities(state_);
    recalculateLeagueStandings(state_);
}

const GameState& SimulationEngine::state() const noexcept
{
    return state_;
}

GameState& SimulationEngine::mutableState() noexcept
{
    return state_;
}

void SimulationEngine::advanceDays(const int dayCount)
{
    if (dayCount < 0) {
        throw std::invalid_argument("Cannot advance simulation by a negative day count");
    }

    for (int day = 0; day < dayCount; ++day) {
        state_.currentDate = state_.currentDate.addDays(1);
        processCurrentDate();
    }
}

void SimulationEngine::advanceToNextEvent()
{
    const auto nextDate = nextPendingDate();
    if (!nextDate.has_value()) {
        advanceDays(1);
        return;
    }

    const int distance = state_.currentDate.daysUntil(*nextDate);
    if (distance <= 0) {
        processCurrentDate();
        return;
    }
    advanceDays(distance);
}

void SimulationEngine::processAiTransferMarket()
{
    ensureDefaultAiMarketState(state_);

    for (auto& deal : state_.aiMarketDeals) {
        if (deal.stage == AiMarketDealStage::Proposed
            && deal.nextActionDate.has_value()
            && *deal.nextActionDate <= state_.currentDate) {
            deal.stage = AiMarketDealStage::AwaitingCompletion;
            deal.completionDate = state_.currentDate.addDays(3);
            addFootballInbox(
                "ai-market-agreed-" + deal.id,
                deal.player.fullName + " AI 거래가 합의됐습니다",
                deal.buyingClubName + "과 " + deal.sellingClubName
                    + "이 " + formatMoney(guaranteedTransferFee(deal.feeTerms))
                    + " 수준의 조건에 합의했습니다. 완료일 전까지 선수단과 재정은 바뀌지 않습니다.");
            continue;
        }

        if (deal.stage == AiMarketDealStage::AwaitingCompletion
            && deal.completionDate.has_value()
            && *deal.completionDate <= state_.currentDate) {
            (void)completeAiMarketDeal(deal);
        }
    }

    for (auto& club : state_.aiClubMarketStates) {
        if (club.clubId == state_.club.id
            || club.nextEvaluationDate > state_.currentDate) {
            continue;
        }

        const bool hasOpenDeal = std::any_of(
            state_.aiMarketDeals.begin(),
            state_.aiMarketDeals.end(),
            [&club](const AiMarketDeal& deal) {
                return !deal.completionApplied
                    && (deal.stage == AiMarketDealStage::Proposed
                        || deal.stage == AiMarketDealStage::AwaitingCompletion)
                    && (deal.buyingClubId == club.clubId
                        || deal.sellingClubId == club.clubId);
            });
        club.lastEvaluationDate = state_.currentDate;
        club.nextEvaluationDate = state_.currentDate.addDays(7);
        if (hasOpenDeal) {
            continue;
        }

        if (createAiBuyoutOffer(club)) {
            continue;
        }
        if (createAiCandidateDeal(club)) {
            continue;
        }
        if (createAiLoanDeal(club)) {
            continue;
        }
        if (createAiRenewalDeal(club)) {
            continue;
        }
        (void)createAiPlayerClubOffer(club);
    }
}

void SimulationEngine::processLoanFutureFeeConversions()
{
    for (auto& loan : state_.loanDeals) {
        if (loan.stage != LoanStage::Active
            || loan.terms.futureFeeType == "none"
            || state_.currentDate < loan.terms.endDate) {
            continue;
        }

        const bool mandatory = loan.terms.futureFeeType == "mandatory";
        const bool optionExercised = mandatory
            || (loan.starts > 0
                && stablePercent(
                       loan.id + state_.currentDate.toIsoString()
                       + std::to_string(state_.rngSeed))
                    < static_cast<std::uint32_t>(
                        loan.starts >= 3 ? 75 : 45));
        if (!optionExercised) {
            addFootballInbox(
                "loan-option-declined-" + loan.id,
                loan.player.fullName + " 완전 영입 선택권을 행사하지 않았습니다",
                "AI 판단 결과 임대 구단이 완전 영입 선택권을 포기했습니다. 임대는 일반 종료 절차로 돌아갑니다.");
            completeLoanEnd(loan, false);
            continue;
        }

        const std::string conversionId = "ai-loan-future-" + loan.id;
        const auto duplicate = std::find_if(
            state_.aiMarketDeals.begin(),
            state_.aiMarketDeals.end(),
            [&conversionId](const AiMarketDeal& deal) {
                return deal.id == conversionId;
            });
        if (duplicate != state_.aiMarketDeals.end()
            && duplicate->completionApplied) {
            continue;
        }

        Money weeklyWagePence = 0;
        auto contract = std::find_if(
            state_.playerContracts.begin(),
            state_.playerContracts.end(),
            [&loan](const PlayerContract& item) {
                return item.playerId == loan.player.id;
            });
        if (contract != state_.playerContracts.end()) {
            weeklyWagePence = contract->weeklyWagePence;
        } else if (const TransferCandidate* candidate =
                findTransferCandidate(loan.player.id);
            candidate != nullptr) {
            weeklyWagePence = candidate->expectedWeeklyWagePence;
        }
        const Money wageShare =
            loanWeeklyWageShare(weeklyWagePence, loan.terms);

        const bool incoming = loan.direction == LoanDirection::Incoming;
        const std::string buyerId =
            incoming ? state_.club.id : loan.loanClubId;
        const std::string buyerName =
            incoming ? state_.club.name : loan.loanClubName;
        const std::string sellerId =
            incoming ? loan.parentClubId : state_.club.id;
        const std::string sellerName =
            incoming ? loan.parentClubName : state_.club.name;
        const AiMarketDealType dealType = mandatory
            ? AiMarketDealType::LoanObligation
            : AiMarketDealType::LoanOption;

        TransferFeeTerms feeTerms {
            loan.terms.futureFeePence,
            0,
            0,
            0,
            0,
            0
        };
        PlayerContractTerms contractTerms {
            weeklyWagePence,
            3,
            weeklyWagePence * 6,
            weeklyWagePence * 3,
            weeklyWagePence / 10,
            weeklyWagePence / 8,
            weeklyWagePence * 4,
            0,
            loan.terms.promisedRole == "Regular Starter"
                ? "First Team"
                : "Squad Player"
        };

        if (incoming) {
            const Money budgetCharge = mandatory
                ? Money {}
                : loan.terms.futureFeePence;
            if (availableTransferCash(state_, {}, {}, loan.id)
                    < loan.terms.futureFeePence
                || state_.club.finance.transferBudgetPence < budgetCharge
                || state_.club.finance.weeklyWageCommitmentPence
                        - wageShare + contractTerms.weeklyWagePence
                    > state_.club.finance.weeklyWageBudgetPence) {
                loan.stage = LoanStage::Rejected;
                loan.nextActionDate.reset();
                loan.responseNote =
                    "완전 영입 전환일에 현금·예산 또는 주급 한도를 충족하지 못했습니다.";
                addFootballInbox(
                    "loan-future-failed-" + loan.id,
                    loan.player.fullName + " 완전 영입 전환이 무산되었습니다",
                    "완료일 기준 재정 또는 등록 한도를 통과하지 못해 전환하지 않았습니다. 비용은 중복 반영되지 않았습니다.");
                continue;
            }
            state_.club.finance.cashPence -= loan.terms.futureFeePence;
            state_.club.finance.transferBudgetPence -= budgetCharge;
            state_.club.finance.weeklyWageCommitmentPence =
                state_.club.finance.weeklyWageCommitmentPence
                - wageShare + contractTerms.weeklyWagePence;
            auto player = std::find_if(
                state_.players.begin(),
                state_.players.end(),
                [&loan](const Player& item) {
                    return item.id == loan.player.id;
                });
            if (player != state_.players.end()) {
                player->status = "Available";
                player->squadGroup = contractTerms.squadRole == "First Team"
                    ? "First Team"
                    : "Reserve";
                player->joinedDate = state_.currentDate;
                player->departedDate.reset();
            }
            if (contract == state_.playerContracts.end()) {
                state_.playerContracts.push_back({
                    loan.player.id,
                    state_.club.id,
                    state_.currentDate,
                    state_.currentDate.addMonths(
                        contractTerms.contractYears * 12),
                    contractTerms.weeklyWagePence,
                    contractTerms.squadRole,
                    conversionId,
                    loan.dataConfidence,
                    contractTerms
                });
            }
            addSettledTransferPayment(
                "payment-" + conversionId + "-future-fee",
                conversionId,
                loan.parentClubId,
                loan.parentClubName,
                TransferPaymentDirection::Payable,
                "loan_future_fee",
                loan.terms.futureFeePence,
                loan.player.fullName + " 임대 완전 영입 전환료");
        } else {
            auto buyingClub = std::find_if(
                state_.aiClubMarketStates.begin(),
                state_.aiClubMarketStates.end(),
                [&loan](const AiClubMarketState& club) {
                    return club.clubId == loan.loanClubId;
                });
            if (buyingClub == state_.aiClubMarketStates.end()
                || aiAvailableTransferCash(state_, *buyingClub, conversionId)
                    < loan.terms.futureFeePence
                || aiAvailableTransferBudget(state_, *buyingClub, conversionId)
                    < loan.terms.futureFeePence
                || buyingClub->finance.weeklyWageCommitmentPence
                        + weeklyWagePence
                    > buyingClub->finance.weeklyWageBudgetPence) {
                loan.stage = LoanStage::Rejected;
                loan.nextActionDate.reset();
                loan.responseNote =
                    "임대 구단이 완전 영입 전환일 재정 한도를 충족하지 못했습니다.";
                addFootballInbox(
                    "loan-future-failed-" + loan.id,
                    loan.player.fullName + " 완전 영입 전환이 무산되었습니다",
                    "차입 구단의 현금·예산 또는 주급 한도 부족으로 전환하지 않았습니다.");
                continue;
            }

            buyingClub->finance.cashPence -= loan.terms.futureFeePence;
            buyingClub->finance.transferBudgetPence -=
                loan.terms.futureFeePence;
            buyingClub->finance.weeklyWageCommitmentPence += weeklyWagePence;
            state_.club.finance.cashPence += loan.terms.futureFeePence;
            state_.club.finance.transferBudgetPence +=
                loan.terms.futureFeePence;
            state_.club.finance.weeklyWageCommitmentPence =
                std::max<Money>(
                    0,
                    state_.club.finance.weeklyWageCommitmentPence
                        - (weeklyWagePence - wageShare));
            state_.players.erase(
                std::remove_if(
                    state_.players.begin(),
                    state_.players.end(),
                    [&loan](const Player& player) {
                        return player.id == loan.player.id;
                    }),
                state_.players.end());
            if (contract != state_.playerContracts.end()) {
                state_.playerContracts.erase(contract);
            }
            addSettledTransferPayment(
                "payment-" + conversionId + "-future-fee",
                conversionId,
                loan.loanClubId,
                loan.loanClubName,
                TransferPaymentDirection::Receivable,
                "loan_future_fee",
                loan.terms.futureFeePence,
                loan.player.fullName + " 임대 완전 영입 전환료");
        }

        loan.stage = LoanStage::Completed;
        loan.actualEndDate = state_.currentDate;
        loan.nextActionDate.reset();
        loan.responseNote =
            "임대의 완전 영입 조항이 실제 영구 이적으로 전환되었습니다.";

        AiMarketDeal deal {
            conversionId,
            dealType,
            AiMarketDealStage::Completed,
            loan.player,
            sellerId,
            sellerName,
            buyerId,
            buyerName,
            state_.currentDate,
            state_.currentDate,
            std::nullopt,
            state_.currentDate,
            state_.currentDate,
            feeTerms,
            contractTerms,
            loan.terms,
            loan.id,
            {},
            mandatory
                ? "임대 의무 영입 조건 충족"
                : "AI가 임대 선택 영입권을 행사",
            true,
            true,
            true,
            loan.dataConfidence
        };
        if (duplicate == state_.aiMarketDeals.end()) {
            state_.aiMarketDeals.push_back(std::move(deal));
        }

        addFootballInbox(
            "loan-future-completed-" + loan.id,
            loan.player.fullName + " 임대가 완전 영입으로 전환되었습니다",
            buyerName + "이 " + sellerName + "에 "
                + formatMoney(loan.terms.futureFeePence)
                + "를 지급하고 선수 등록을 영구 전환했습니다. 전환은 완료일에 한 번만 반영됐습니다.");
    }
}

bool SimulationEngine::completeAiMarketDeal(AiMarketDeal& deal)
{
    if (deal.completionApplied) {
        return true;
    }

    auto buyingClub = std::find_if(
        state_.aiClubMarketStates.begin(),
        state_.aiClubMarketStates.end(),
        [&deal](const AiClubMarketState& club) {
            return club.clubId == deal.buyingClubId;
        });
    if (buyingClub == state_.aiClubMarketStates.end()) {
        deal.stage = AiMarketDealStage::Rejected;
        deal.nextActionDate.reset();
        deal.completedDate = state_.currentDate;
        addFootballInbox(
            "ai-market-failed-" + deal.id,
            deal.player.fullName + " AI 거래가 무산되었습니다",
            "구매 구단의 시장 상태를 찾지 못해 완료일에 거래를 반영하지 않았습니다.");
        return false;
    }

    const bool renewal = deal.type == AiMarketDealType::Renewal;
    const bool loan = deal.type == AiMarketDealType::Loan;

    if (aiDealRequiresNewRegistration(deal.type)
        && !domesticRegistrationOpen(state_, state_.currentDate)) {
        deal.stage = AiMarketDealStage::Rejected;
        deal.nextActionDate.reset();
        deal.completedDate = state_.currentDate;
        addFootballInbox(
            "ai-market-registration-failed-" + deal.id,
            deal.player.fullName + " AI 거래 등록이 취소되었습니다",
            "3월 31일 등록 마감 뒤에는 새 영입 또는 임대를 등록할 수 없습니다. 완료 전 단계였으므로 거래 전 재정과 선수 소속은 그대로 유지됩니다.");
        return false;
    }

    const Money budgetCommitment = renewal
        ? contractRenewalCommitment(deal.contractTerms)
        : (loan
              ? loanFinancialCommitment(deal.loanTerms)
              : transferBudgetCommitment(deal.feeTerms, deal.contractTerms));
    const Money immediateCash = renewal
        ? deal.contractTerms.signingBonusPence
            + deal.contractTerms.agentFeePence
        : (loan
              ? deal.loanTerms.loanFeePence
              : immediateTransferCashCost(
                    deal.feeTerms,
                    deal.contractTerms));
    const Money weeklyWage = loan
        ? loanWeeklyWageShare(
              deal.contractTerms.weeklyWagePence,
              deal.loanTerms)
        : deal.contractTerms.weeklyWagePence;

    if (aiAvailableTransferCash(state_, *buyingClub, deal.id)
            < budgetCommitment
        || (!renewal
            && aiAvailableTransferBudget(state_, *buyingClub, deal.id)
                < budgetCommitment)
        || buyingClub->finance.weeklyWageCommitmentPence + weeklyWage
            > buyingClub->finance.weeklyWageBudgetPence) {
        deal.stage = AiMarketDealStage::Rejected;
        deal.nextActionDate.reset();
        deal.completedDate = state_.currentDate;
        addFootballInbox(
            "ai-market-failed-" + deal.id,
            deal.player.fullName + " AI 거래가 무산되었습니다",
            deal.buyingClubName
                + "이 완료일 기준 현금·이적 예산 또는 주급 한도를 충족하지 못했습니다. 거래 전 재정은 그대로 유지됩니다.");
        return false;
    }

    buyingClub->finance.cashPence -= immediateCash;
    if (!renewal) {
        buyingClub->finance.transferBudgetPence =
            std::max<Money>(
                0,
                buyingClub->finance.transferBudgetPence
                    - budgetCommitment);
    }
    buyingClub->finance.weeklyWageCommitmentPence += weeklyWage;

    if (!renewal) {
        auto sellingClub = std::find_if(
            state_.aiClubMarketStates.begin(),
            state_.aiClubMarketStates.end(),
            [&deal](const AiClubMarketState& club) {
                return club.clubId == deal.sellingClubId;
            });
        if (sellingClub != state_.aiClubMarketStates.end()) {
            const Money sellerCash = loan
                ? deal.loanTerms.loanFeePence
                : deal.feeTerms.upfrontFeePence;
            const Money sellerBudget = loan
                ? deal.loanTerms.loanFeePence
                : guaranteedTransferFee(deal.feeTerms);
            sellingClub->finance.cashPence += sellerCash;
            sellingClub->finance.transferBudgetPence += sellerBudget;
            if (!loan) {
                sellingClub->finance.weeklyWageCommitmentPence =
                    std::max<Money>(
                        0,
                        sellingClub->finance.weeklyWageCommitmentPence
                            - deal.contractTerms.weeklyWagePence);
            }
        }

        if (!loan) {
            auto candidate = std::find_if(
                state_.transferCandidates.begin(),
                state_.transferCandidates.end(),
                [&deal](const TransferCandidate& item) {
                    return item.player.id == deal.player.id;
                });
            if (candidate != state_.transferCandidates.end()) {
                candidate->currentClubId = deal.buyingClubId;
                candidate->currentClubName = deal.buyingClubName;
                candidate->rivalClubId.clear();
                candidate->rivalClubName.clear();
                candidate->rivalBidPence = 0;
            }
        }
    }

    deal.stage = AiMarketDealStage::Completed;
    deal.completedDate = state_.currentDate;
    deal.completionApplied = true;
    deal.nextActionDate.reset();

    addFootballInbox(
        "ai-market-completed-" + deal.id,
        deal.player.fullName + " AI 거래가 완료되었습니다",
        deal.buyingClubName + "이 " + deal.sellingClubName
            + "과의 거래를 완료했습니다. 재정 반영은 완료일에 한 번만 수행됐습니다.");
    return true;
}

bool SimulationEngine::createAiPlayerClubOffer(const AiClubMarketState& club)
{
    const auto alreadyOpenForPlayer = [this](const std::string& playerId) {
        return std::any_of(
            state_.outgoingTransferOffers.begin(),
            state_.outgoingTransferOffers.end(),
            [&playerId](const OutgoingTransferOffer& offer) {
                return offer.playerId == playerId
                    && (offer.status == OutgoingTransferStatus::Pending
                        || offer.status == OutgoingTransferStatus::Accepted);
            });
    };

    for (const auto& player : state_.players) {
        if (player.status != "Available"
            || player.squadGroup == "Loan"
            || alreadyOpenForPlayer(player.id)) {
            continue;
        }

        const auto contract = std::find_if(
            state_.playerContracts.begin(),
            state_.playerContracts.end(),
            [&player, this](const PlayerContract& item) {
                return item.playerId == player.id
                    && item.clubId == state_.club.id
                    && item.lifecycleStatus == "active";
            });
        if (contract == state_.playerContracts.end()) {
            continue;
        }

        const Money deterministicPremium =
            static_cast<Money>(
                stablePercent(
                    club.clubId + player.id
                    + std::to_string(state_.rngSeed)))
            * 1'250'000;
        const Money baseFee =
            std::max<Money>(
                60'000'000,
                contract->weeklyWagePence * 260);
        const Money fee = baseFee + deterministicPremium;
        const PlayerContractTerms playerTerms {
            std::max<Money>(
                contract->weeklyWagePence * 6 / 5,
                contract->weeklyWagePence + 250'000),
            std::max(2, contract->terms.contractYears),
            contract->weeklyWagePence * 8,
            contract->weeklyWagePence * 4,
            contract->weeklyWagePence / 10,
            contract->weeklyWagePence / 8,
            contract->weeklyWagePence * 6,
            0,
            contract->squadRole.empty()
                ? "First Team"
                : contract->squadRole
        };
        const TransferFeeTerms feeTerms {
            fee,
            0,
            0,
            0,
            0,
            0
        };
        const Money commitment =
            transferBudgetCommitment(feeTerms, playerTerms);
        if (aiAvailableTransferCash(state_, club) < commitment
            || aiAvailableTransferBudget(state_, club) < commitment
            || club.finance.weeklyWageCommitmentPence
                    + playerTerms.weeklyWagePence
                > club.finance.weeklyWageBudgetPence) {
            continue;
        }

        const std::string offerId =
            "ai-offer-" + club.clubId + "-" + player.id
            + "-" + state_.currentDate.toIsoString();
        state_.outgoingTransferOffers.push_back({
            offerId,
            player.id,
            player.fullName,
            club.clubId,
            club.clubName,
            fee,
            contract->weeklyWagePence,
            state_.currentDate,
            state_.currentDate.addDays(5),
            std::nullopt,
            std::nullopt,
            OutgoingTransferStatus::Pending,
            playerTerms.squadRole,
            club.transferNeed
                + " 보강 판단에 따른 AI 구단의 영입 제안",
            "gameplay_estimate_current_dataset",
            feeTerms,
            feeTerms,
            OutgoingNegotiationStage::DecisionPending,
            0,
            std::nullopt,
            commitment + percentageOf(commitment, 20)
        });
        addFootballInbox(
            "outgoing-offer-" + offerId,
            club.clubName + "이 " + player.fullName + " 영입을 제안했습니다",
            "AI 구단도 현금·이적 예산·주급 한도를 통과한 제안만 제출합니다. 구단주는 수락, 거절, 역제안을 선택할 수 있습니다.",
            true);
        return true;
    }

    return false;
}

bool SimulationEngine::createAiCandidateDeal(AiClubMarketState& club)
{
    TransferCandidate* bestCandidate = nullptr;
    int bestScore = -1;

    for (auto& candidate : state_.transferCandidates) {
        if (candidate.currentClubId.empty()
            || candidate.currentClubId == state_.club.id
            || candidate.currentClubId == club.clubId) {
            continue;
        }

        const bool alreadyTracked = std::any_of(
            state_.aiMarketDeals.begin(),
            state_.aiMarketDeals.end(),
            [&candidate](const AiMarketDeal& deal) {
                return deal.player.id == candidate.player.id
                    && deal.stage != AiMarketDealStage::Rejected
                    && deal.stage != AiMarketDealStage::Expired;
            });
        if (alreadyTracked) {
            continue;
        }

        int score = candidate.managerPriority;
        score += candidate.rivalClubId == club.clubId ? 100 : 0;
        score += static_cast<int>(
            stablePercent(
                club.clubId + candidate.player.id
                + state_.currentDate.toIsoString()));
        if (score > bestScore) {
            bestScore = score;
            bestCandidate = &candidate;
        }
    }

    if (bestCandidate == nullptr) {
        return false;
    }

    const Money baseFee = std::max(
        bestCandidate->askingPricePence,
        std::max(
            bestCandidate->sellerMinimumPence,
            bestCandidate->rivalBidPence));
    const Money targetFee =
        baseFee + percentageOf(
            bestCandidate->marketValuePence,
            5 + static_cast<int>(club.marketSeed % 6U));
    const Money upfront = percentageOf(targetFee, 75);
    const TransferFeeTerms feeTerms {
        upfront,
        targetFee - upfront,
        targetFee > 120'000'000 ? 2 : 1,
        percentageOf(targetFee, 5),
        25,
        club.riskTolerance > 60 ? 10 : 5
    };
    PlayerContractTerms contractTerms =
        recommendedPlayerTerms(*bestCandidate);
    if (club.riskTolerance > 60) {
        contractTerms.weeklyWagePence =
            percentageOf(contractTerms.weeklyWagePence, 110);
        contractTerms.signingBonusPence =
            percentageOf(contractTerms.signingBonusPence, 110);
    }

    const Money commitment =
        transferBudgetCommitment(feeTerms, contractTerms);
    if (aiAvailableTransferCash(state_, club) < commitment
        || aiAvailableTransferBudget(state_, club) < commitment
        || club.finance.weeklyWageCommitmentPence
                + contractTerms.weeklyWagePence
            > club.finance.weeklyWageBudgetPence) {
        return false;
    }

    const std::string dealId =
        "ai-transfer-" + club.clubId + "-"
        + bestCandidate->player.id + "-"
        + state_.currentDate.toIsoString();
    AiMarketDeal deal;
    deal.id = dealId;
    deal.type = AiMarketDealType::Transfer;
    deal.stage = AiMarketDealStage::Proposed;
    deal.player = bestCandidate->player;
    deal.sellingClubId = bestCandidate->currentClubId;
    deal.sellingClubName = bestCandidate->currentClubName;
    deal.buyingClubId = club.clubId;
    deal.buyingClubName = club.clubName;
    deal.proposedDate = state_.currentDate;
    deal.decisionDeadline = state_.currentDate.addDays(5);
    deal.nextActionDate = state_.currentDate.addDays(1);
    deal.feeTerms = feeTerms;
    deal.contractTerms = contractTerms;
    deal.rationale =
        club.transferNeed + " 보강을 위한 AI 독립 영입";
    deal.budgetReserved = true;
    deal.dataConfidence = bestCandidate->dataConfidence;
    state_.aiMarketDeals.push_back(std::move(deal));

    for (auto& pursuit : state_.transferPursuits) {
        if (pursuit.candidatePlayerId == bestCandidate->player.id
            && !isClosedPursuitStage(pursuit.stage)) {
            pursuit.rivalBidActive = true;
            pursuit.rivalClubId = club.clubId;
            pursuit.rivalClubName = club.clubName;
            pursuit.rivalBidPence = targetFee;
            pursuit.responseNote =
                club.clubName
                + "의 경쟁 제안이 확인되어 협상 압력이 커졌습니다.";
        }
    }

    addFootballInbox(
        "ai-market-proposed-" + dealId,
        club.clubName + "이 " + bestCandidate->player.fullName
            + " 영입 협상에 착수했습니다",
        "AI 구단이 보유 현금·이적 예산·주급 한도를 예약한 뒤 독립 협상을 시작했습니다.");
    return true;
}

bool SimulationEngine::createAiLoanDeal(AiClubMarketState& club)
{
    for (const auto& candidate : state_.transferCandidates) {
        if (candidate.currentClubId.empty()
            || candidate.currentClubId == state_.club.id
            || candidate.currentClubId == club.clubId) {
            continue;
        }

        const bool alreadyTracked = std::any_of(
            state_.aiMarketDeals.begin(),
            state_.aiMarketDeals.end(),
            [&candidate](const AiMarketDeal& deal) {
                return deal.player.id == candidate.player.id
                    && deal.stage != AiMarketDealStage::Rejected
                    && deal.stage != AiMarketDealStage::Expired;
            });
        if (alreadyTracked) {
            continue;
        }

        PlayerContractTerms contractTerms =
            recommendedPlayerTerms(candidate);
        const GameDate seasonEnd = state_.currentDate.month >= 6
            ? GameDate {state_.currentDate.year + 1, 5, 31}
            : GameDate {state_.currentDate.year, 5, 31};
        LoanTerms loanTerms {
            std::max<Money>(50'000, candidate.marketValuePence / 80),
            club.riskTolerance > 55 ? 100 : 75,
            candidate.managerPriority >= 80
                ? "Regular Starter"
                : "Squad Player",
            seasonEnd,
            true,
            7,
            stablePercent(club.clubId + candidate.player.id) < 25
                ? "mandatory"
                : "option",
            candidate.askingPricePence,
            true,
            0,
            "loan_club"
        };
        if (!validLoanTerms(loanTerms, state_.currentDate)) {
            continue;
        }

        const Money commitment = loanFinancialCommitment(loanTerms);
        const Money wageShare =
            loanWeeklyWageShare(
                contractTerms.weeklyWagePence,
                loanTerms);
        if (aiAvailableTransferCash(state_, club) < commitment
            || aiAvailableTransferBudget(state_, club) < commitment
            || club.finance.weeklyWageCommitmentPence + wageShare
                > club.finance.weeklyWageBudgetPence) {
            continue;
        }

        const std::string dealId =
            "ai-loan-" + club.clubId + "-"
            + candidate.player.id + "-"
            + state_.currentDate.toIsoString();
        AiMarketDeal deal;
        deal.id = dealId;
        deal.type = AiMarketDealType::Loan;
        deal.stage = AiMarketDealStage::Proposed;
        deal.player = candidate.player;
        deal.sellingClubId = candidate.currentClubId;
        deal.sellingClubName = candidate.currentClubName;
        deal.buyingClubId = club.clubId;
        deal.buyingClubName = club.clubName;
        deal.proposedDate = state_.currentDate;
        deal.decisionDeadline = state_.currentDate.addDays(5);
        deal.nextActionDate = state_.currentDate.addDays(1);
        deal.contractTerms = contractTerms;
        deal.loanTerms = loanTerms;
        deal.rationale =
            club.transferNeed + " 보강을 위한 AI 임대 협상";
        deal.budgetReserved = true;
        deal.dataConfidence = candidate.dataConfidence;
        state_.aiMarketDeals.push_back(std::move(deal));

        addFootballInbox(
            "ai-market-proposed-" + dealId,
            club.clubName + "이 " + candidate.player.fullName
                + " 임대 협상에 착수했습니다",
            "AI 구단이 임대료, 주급 분담, 선택·의무 영입 조항을 예산 예약에 포함했습니다.");
        return true;
    }

    return false;
}

bool SimulationEngine::createAiRenewalDeal(AiClubMarketState& club)
{
    for (const auto& candidate : state_.transferCandidates) {
        if (candidate.currentClubId != club.clubId) {
            continue;
        }

        const bool alreadyTracked = std::any_of(
            state_.aiMarketDeals.begin(),
            state_.aiMarketDeals.end(),
            [&candidate, &club](const AiMarketDeal& deal) {
                return deal.player.id == candidate.player.id
                    && deal.buyingClubId == club.clubId
                    && deal.type == AiMarketDealType::Renewal
                    && deal.stage != AiMarketDealStage::Rejected
                    && deal.stage != AiMarketDealStage::Expired;
            });
        if (alreadyTracked) {
            continue;
        }

        PlayerContractTerms terms = recommendedPlayerTerms(candidate);
        terms.contractYears = std::max(2, terms.contractYears);
        terms.weeklyWagePence =
            percentageOf(terms.weeklyWagePence, 105);
        const Money commitment = contractRenewalCommitment(terms);
        if (aiAvailableTransferCash(state_, club) < commitment
            || club.finance.weeklyWageCommitmentPence
                    + terms.weeklyWagePence
                > club.finance.weeklyWageBudgetPence) {
            continue;
        }

        const std::string dealId =
            "ai-renewal-" + club.clubId + "-"
            + candidate.player.id + "-"
            + state_.currentDate.toIsoString();
        AiMarketDeal deal;
        deal.id = dealId;
        deal.type = AiMarketDealType::Renewal;
        deal.stage = AiMarketDealStage::Proposed;
        deal.player = candidate.player;
        deal.sellingClubId = club.clubId;
        deal.sellingClubName = club.clubName;
        deal.buyingClubId = club.clubId;
        deal.buyingClubName = club.clubName;
        deal.proposedDate = state_.currentDate;
        deal.decisionDeadline = state_.currentDate.addDays(5);
        deal.nextActionDate = state_.currentDate.addDays(1);
        deal.contractTerms = terms;
        deal.sourceContractId = "ai-current-contract-"
            + candidate.player.id;
        deal.rationale =
            "AI 구단이 이적시장 경쟁을 막기 위해 재계약을 추진";
        deal.budgetReserved = true;
        deal.dataConfidence = candidate.dataConfidence;
        state_.aiMarketDeals.push_back(std::move(deal));

        addFootballInbox(
            "ai-market-proposed-" + dealId,
            club.clubName + "이 " + candidate.player.fullName
                + " 재계약을 추진합니다",
            "AI 구단 재계약도 계약금·에이전트 수수료와 주급 한도를 같은 방식으로 검사합니다.");
        return true;
    }

    return false;
}

bool SimulationEngine::createAiBuyoutOffer(const AiClubMarketState& club)
{
    for (const auto& contract : state_.playerContracts) {
        if (contract.clubId != state_.club.id
            || contract.lifecycleStatus != "active"
            || contract.terms.releaseClausePence <= 0) {
            continue;
        }

        const auto player = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&contract](const Player& item) {
                return item.id == contract.playerId
                    && item.status == "Available";
            });
        if (player == state_.players.end()) {
            continue;
        }

        const bool alreadyOpen = std::any_of(
            state_.outgoingTransferOffers.begin(),
            state_.outgoingTransferOffers.end(),
            [&contract](const OutgoingTransferOffer& offer) {
                return offer.playerId == contract.playerId
                    && (offer.status == OutgoingTransferStatus::Pending
                        || offer.status == OutgoingTransferStatus::Accepted);
            });
        if (alreadyOpen) {
            continue;
        }

        const PlayerContractTerms buyerTerms {
            std::max<Money>(
                percentageOf(contract.weeklyWagePence, 125),
                contract.weeklyWagePence + 300'000),
            std::max(2, contract.terms.contractYears),
            contract.weeklyWagePence * 10,
            contract.weeklyWagePence * 5,
            contract.weeklyWagePence / 10,
            contract.weeklyWagePence / 8,
            contract.weeklyWagePence * 7,
            0,
            contract.squadRole.empty()
                ? "First Team"
                : contract.squadRole
        };
        const TransferFeeTerms feeTerms {
            contract.terms.releaseClausePence,
            0,
            0,
            0,
            0,
            0
        };
        const Money commitment =
            transferBudgetCommitment(feeTerms, buyerTerms);
        if (aiAvailableTransferCash(state_, club) < commitment
            || aiAvailableTransferBudget(state_, club) < commitment
            || club.finance.weeklyWageCommitmentPence
                    + buyerTerms.weeklyWagePence
                > club.finance.weeklyWageBudgetPence) {
            continue;
        }

        const std::string offerId =
            "ai-buyout-" + club.clubId + "-"
            + contract.playerId + "-"
            + state_.currentDate.toIsoString();
        state_.outgoingTransferOffers.push_back({
            offerId,
            contract.playerId,
            player->fullName,
            club.clubId,
            club.clubName,
            contract.terms.releaseClausePence,
            contract.weeklyWagePence,
            state_.currentDate,
            state_.currentDate.addDays(3),
            std::nullopt,
            std::nullopt,
            OutgoingTransferStatus::Pending,
            buyerTerms.squadRole,
            "AI 구단이 방출 조항 충족 가능성을 판단해 바이아웃을 발동했습니다.",
            "gameplay_estimate_current_dataset",
            feeTerms,
            feeTerms,
            OutgoingNegotiationStage::DecisionPending,
            0,
            std::nullopt,
            commitment
        });
        addFootballInbox(
            "outgoing-offer-" + offerId,
            club.clubName + "이 " + player->fullName
                + " 바이아웃을 발동했습니다",
            "제안액은 계약의 방출 조항과 일치합니다. 수락하면 기존 outgoing transfer 완료 흐름으로 실제 판매가 반영됩니다.",
            true);
        return true;
    }

    return false;
}

void SimulationEngine::markInboxRead(const int inboxId)
{
    const auto item = std::find_if(state_.inbox.begin(), state_.inbox.end(), [inboxId](const InboxItem& candidate) {
        return candidate.id == inboxId;
    });
    if (item != state_.inbox.end()) {
        item->unread = false;
    }
}

void SimulationEngine::markAllInboxRead()
{
    for (auto& item : state_.inbox) {
        item.unread = false;
    }
}

TransferActionResult SimulationEngine::approveTransfer(const std::string& proposalId)
{
    const auto proposal = std::find_if(
        state_.transferProposals.begin(),
        state_.transferProposals.end(),
        [&proposalId](const TransferProposal& candidate) {
            return candidate.id == proposalId;
        });
    if (proposal == state_.transferProposals.end()) {
        return {false, "영입 안건을 찾을 수 없습니다."};
    }
    if (proposal->status != TransferStatus::Pending) {
        return {false, "이미 처리된 영입 안건입니다."};
    }
    if (proposal->decisionDeadline < state_.currentDate) {
        return {false, "승인 기한이 지난 영입 안건입니다."};
    }

    const TransferFeeTerms feeTerms =
        effectiveFeeTerms(proposal->feePence, proposal->feeTerms);
    const PlayerContractTerms contractTerms = effectiveContractTerms(
        proposal->weeklyWagePence,
        proposal->contractYears,
        proposal->contractTerms);
    if (!validTransferFeeTerms(feeTerms)
        || !validPlayerContractTerms(contractTerms)) {
        return {false, "합의된 이적 또는 선수 계약 조건이 올바르지 않습니다."};
    }

    const Money committedBudget =
        transferBudgetCommitment(feeTerms, contractTerms);
    const Money immediateCash =
        immediateTransferCashCost(feeTerms, contractTerms);
    auto& finance = state_.club.finance;
    if (const auto capacityError = transferCapacityError(
            state_,
            feeTerms,
            &contractTerms,
            {},
            proposal->id);
        capacityError.has_value()) {
        return {false, *capacityError};
    }
    if (finance.weeklyWageCommitmentPence + contractTerms.weeklyWagePence
        > finance.weeklyWageBudgetPence) {
        return {false, "주급 예산이 부족합니다."};
    }

    proposal->feeTerms = feeTerms;
    proposal->contractTerms = contractTerms;
    proposal->feePence = guaranteedTransferFee(feeTerms);
    proposal->weeklyWagePence = contractTerms.weeklyWagePence;
    proposal->contractYears = contractTerms.contractYears;

    finance.transferBudgetPence -= committedBudget;
    finance.cashPence -= immediateCash;
    finance.weeklyWageCommitmentPence += contractTerms.weeklyWagePence;

    proposal->status = TransferStatus::Approved;
    proposal->decisionDate = state_.currentDate;
    proposal->joinDate = state_.currentDate.addDays(3);

    addSettledTransferPayment(
        "payment-" + proposal->id + "-upfront",
        proposal->id,
        proposal->fromClubId,
        proposal->fromClubName,
        TransferPaymentDirection::Payable,
        "upfront_fee",
        feeTerms.upfrontFeePence,
        proposal->target.fullName + " 영입 즉시 이적료");
    addSettledTransferPayment(
        "payment-" + proposal->id + "-signing",
        proposal->id,
        proposal->target.id,
        proposal->target.fullName,
        TransferPaymentDirection::Payable,
        "signing_bonus",
        contractTerms.signingBonusPence,
        proposal->target.fullName + " 계약금");
    addSettledTransferPayment(
        "payment-" + proposal->id + "-agent",
        proposal->id,
        proposal->target.id,
        proposal->target.fullName + " 측 에이전트",
        TransferPaymentDirection::Payable,
        "agent_fee",
        contractTerms.agentFeePence,
        proposal->target.fullName + " 영입 에이전트 수수료");
    scheduleTransferInstallments(
        "payment-" + proposal->id + "-installment",
        proposal->id,
        proposal->fromClubId,
        proposal->fromClubName,
        TransferPaymentDirection::Payable,
        feeTerms.installmentTotalPence,
        feeTerms.installmentCount,
        state_.currentDate);
    if (contractTerms.loyaltyBonusPence > 0) {
        const std::string paymentId = "payment-" + proposal->id + "-loyalty";
        const auto duplicatePayment = std::find_if(
            state_.transferPayments.begin(),
            state_.transferPayments.end(),
            [&paymentId](const TransferPayment& payment) {
                return payment.id == paymentId;
            });
        if (duplicatePayment == state_.transferPayments.end()) {
            state_.transferPayments.push_back({
                paymentId,
                proposal->id,
                proposal->target.id,
                proposal->target.fullName,
                TransferPaymentDirection::Payable,
                "loyalty_bonus",
                state_.currentDate.addMonths(12),
                contractTerms.loyaltyBonusPence,
                TransferPaymentStatus::Scheduled,
                std::nullopt,
                proposal->target.fullName + " 1년차 로열티 보너스"
            });
        }
    }

    resolveTransferRequestInbox(*proposal);
    synchronizePursuitWithProposal(*proposal);

    std::ostringstream body;
    body << proposal->target.fullName
         << " 영입을 승인했습니다. 즉시 "
         << formatMoney(immediateCash)
         << "가 지급됐고 최대 약정 "
         << formatMoney(committedBudget)
         << "와 주급이 예산에 반영됐습니다. "
         << proposal->joinDate->toIsoString()
         << "에 선수단 합류가 예정되어 있습니다.";
    addTransferInbox(
        *proposal,
        "transfer-approved-",
        proposal->target.fullName + " 영입을 승인했습니다",
        body.str());
    recordOwnerAction(
        state_,
        "approve_transfer",
        "transfer_market",
        proposal->target.id,
        proposal->target.fullName,
        proposal->target.fullName + " 영입 최종 승인",
        ownerImpactForApprovedTransfer(state_, *proposal, committedBudget),
        "player_owner_transfer_approval");
    return {true, "영입을 승인했습니다."};
}

TransferActionResult SimulationEngine::rejectTransfer(const std::string& proposalId)
{
    const auto proposal = std::find_if(
        state_.transferProposals.begin(),
        state_.transferProposals.end(),
        [&proposalId](const TransferProposal& candidate) {
            return candidate.id == proposalId;
        });
    if (proposal == state_.transferProposals.end()) {
        return {false, "영입 안건을 찾을 수 없습니다."};
    }
    if (proposal->status != TransferStatus::Pending) {
        return {false, "이미 처리된 영입 안건입니다."};
    }
    if (proposal->decisionDeadline < state_.currentDate) {
        return {false, "결정 기한이 지난 영입 안건입니다."};
    }

    proposal->status = TransferStatus::Rejected;
    proposal->decisionDate = state_.currentDate;
    resolveTransferRequestInbox(*proposal);
    synchronizePursuitWithProposal(*proposal);

    addTransferInbox(
        *proposal,
        "transfer-rejected-",
        proposal->target.fullName + " 영입을 거절했습니다",
        "예산은 사용되지 않았습니다. 개별 후보를 놓친 것만으로 감독 신뢰는 즉시 하락하지 않으며 전체 보강 계획의 달성도에 따라 평가됩니다.");
    recordOwnerAction(
        state_,
        "reject_transfer",
        "transfer_market",
        proposal->target.id,
        proposal->target.fullName,
        proposal->target.fullName + " 영입 최종 거절",
        ownerImpactForRejectedTransfer(),
        "player_owner_transfer_rejection");
    return {true, "영입을 거절했습니다. 감독 신뢰는 전체 보강 결과에서 평가됩니다."};
}

TransferActionResult SimulationEngine::startTransferPursuit(const std::string& playerId)
{
    const TransferCandidate* candidate = findTransferCandidate(playerId);
    if (candidate == nullptr) {
        return {false, "영입 후보를 찾을 수 없습니다."};
    }
    return startTransferPursuit(playerId, recommendedOpeningTerms(*candidate));
}

TransferActionResult SimulationEngine::startTransferPursuit(
    const std::string& playerId,
    const TransferFeeTerms& offerTerms)
{
    const TransferCandidate* candidate = findTransferCandidate(playerId);
    if (candidate == nullptr) {
        return {false, "영입 후보를 찾을 수 없습니다."};
    }
    if (!validTransferFeeTerms(offerTerms)) {
        return {false, "첫 구단 제안의 금액 또는 조항이 올바르지 않습니다."};
    }
    if (const auto capacityError =
            transferCapacityError(state_, offerTerms, nullptr);
        capacityError.has_value()) {
        return {false, *capacityError};
    }

    const auto alreadyRegistered = std::find_if(
        state_.players.begin(),
        state_.players.end(),
        [&playerId](const Player& player) {
            return player.id == playerId;
        });
    if (alreadyRegistered != state_.players.end()) {
        return {false, "이미 우리 선수단에 등록된 선수입니다."};
    }

    auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&playerId](const TransferPursuit& item) {
            return item.candidatePlayerId == playerId;
        });
    if (pursuit != state_.transferPursuits.end()
        && !isClosedPursuitStage(pursuit->stage)) {
        return {false, "이미 진행 중이거나 완료된 영입 협상입니다."};
    }

    int attemptNumber = 1;
    for (const auto& proposal : state_.transferProposals) {
        if (proposal.target.id == playerId) {
            ++attemptNumber;
        }
    }

    TransferPursuit fresh {
        "pursuit-" + playerId + "-" + state_.currentDate.toIsoString()
            + "-" + std::to_string(attemptNumber),
        playerId,
        TransferPursuitStage::AwaitingClubResponse,
        guaranteedTransferFee(offerTerms),
        0,
        1,
        state_.currentDate,
        state_.currentDate.addDays(1),
        state_.currentDate.addDays(14),
        state_.currentDate,
        candidate->currentClubName + "에 첫 구단 제안을 전달했습니다.",
        !candidate->rivalClubId.empty(),
        candidate->rivalClubId,
        candidate->rivalClubName,
        candidate->rivalBidPence,
        {},
        {},
        {},
        {},
        {},
        0,
        false,
        0,
        "not_started",
        "not_checked"
    };
    fresh.offerTerms = offerTerms;
    fresh.playerOfferTerms = recommendedPlayerTerms(*candidate);
    fresh.medicalRiskPercent = 0;

    if (pursuit == state_.transferPursuits.end()) {
        state_.transferPursuits.push_back(std::move(fresh));
        pursuit = std::prev(state_.transferPursuits.end());
    } else {
        *pursuit = std::move(fresh);
    }

    std::ostringstream body;
    body << candidate->currentClubName << "에 "
         << formatMoney(guaranteedTransferFee(pursuit->offerTerms))
         << " 보장액(즉시 "
         << formatMoney(pursuit->offerTerms.upfrontFeePence)
         << ")의 첫 제안을 보냈습니다. 답변은 "
         << pursuit->nextActionDate.toIsoString()
         << "까지 도착할 예정이며 아직 예산은 차감되지 않았습니다.";
    addFootballInbox(
        "transfer-pursuit-started-" + pursuit->id,
        candidate->player.fullName + " 영입 협상을 시작했습니다",
        body.str());
    return {true, "구단 간 영입 협상을 시작했습니다."};
}

TransferActionResult SimulationEngine::acceptClubCounter(const std::string& pursuitId)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&pursuitId](const TransferPursuit& item) {
            return item.id == pursuitId;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return {false, "영입 협상을 찾을 수 없습니다."};
    }
    if (pursuit->stage != TransferPursuitStage::ClubCountered) {
        return {false, "현재는 상대 구단의 역제안을 수락할 수 있는 단계가 아닙니다."};
    }

    pursuit->clubCounterTerms = effectiveFeeTerms(
        pursuit->clubCounterPence,
        pursuit->clubCounterTerms);
    if (!validTransferFeeTerms(pursuit->clubCounterTerms)) {
        return {false, "상대 구단 역제안의 세부 조건이 올바르지 않습니다."};
    }
    if (const auto capacityError = transferCapacityError(
            state_,
            pursuit->clubCounterTerms,
            nullptr,
            pursuit->id);
        capacityError.has_value()) {
        return {false, *capacityError};
    }
    pursuit->offerTerms = pursuit->clubCounterTerms;
    pursuit->currentOfferPence = guaranteedTransferFee(pursuit->offerTerms);
    pursuit->stage = TransferPursuitStage::AwaitingPlayerTerms;
    pursuit->nextActionDate = state_.currentDate;
    pursuit->lastUpdateDate = state_.currentDate;
    pursuit->playerOfferSubmitted = false;
    pursuit->responseNote = "상대 구단의 역제안을 수락했습니다. 선수에게 제시할 계약 조건을 작성해야 합니다.";
    resolveInboxEvent("transfer-pursuit-counter-" + pursuit->id);

    const TransferCandidate* candidate = findTransferCandidate(pursuit->candidatePlayerId);
    const std::string playerName = candidate != nullptr ? candidate->player.fullName : pursuit->candidatePlayerId;
    addFootballInbox(
        "transfer-pursuit-counter-accepted-" + pursuit->id,
        playerName + " 이적료에 합의했습니다",
        "상대 구단 역제안을 수락했습니다. 이제 주급·계약금·보너스·에이전트 수수료를 작성해 선수 측에 제안해야 합니다. 최종 승인 전까지 예산은 차감되지 않습니다.",
        true);
    return {true, "구단 역제안을 수락했습니다. 선수 계약 조건을 작성해 주세요."};
}

TransferActionResult SimulationEngine::improveClubOffer(const std::string& pursuitId)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&pursuitId](const TransferPursuit& item) {
            return item.id == pursuitId;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return {false, "영입 협상을 찾을 수 없습니다."};
    }
    if (pursuit->stage != TransferPursuitStage::ClubCountered) {
        return {false, "현재는 이적료를 개선해 다시 제안할 수 있는 단계가 아닙니다."};
    }

    const Money gap = std::max<Money>(0, pursuit->clubCounterPence - pursuit->currentOfferPence);
    const Money increase = std::max<Money>(10'000'000, (gap + 1) / 2);
    const TransferCandidate* candidate = findTransferCandidate(pursuit->candidatePlayerId);
    const Money viableOffer = candidate != nullptr
        ? std::max(
            candidate->sellerMinimumPence,
            pursuit->rivalBidActive ? pursuit->rivalBidPence : Money {})
        : pursuit->currentOfferPence + increase;
    const Money improvedOffer = std::min(
        pursuit->clubCounterPence,
        std::max(pursuit->currentOfferPence + increase, viableOffer));
    const TransferFeeTerms improvedTerms {
        improvedOffer,
        0,
        0,
        0,
        0,
        0
    };
    if (const auto capacityError = transferCapacityError(
            state_,
            improvedTerms,
            nullptr,
            pursuit->id);
        capacityError.has_value()) {
        return {false, *capacityError};
    }
    pursuit->currentOfferPence = improvedOffer;
    pursuit->offerTerms = improvedTerms;
    ++pursuit->negotiationRound;
    pursuit->stage = TransferPursuitStage::AwaitingClubResponse;
    pursuit->nextActionDate = state_.currentDate.addDays(1);
    pursuit->lastUpdateDate = state_.currentDate;
    pursuit->responseNote = "이적료를 개선해 상대 구단에 다시 제안했습니다.";
    resolveInboxEvent("transfer-pursuit-counter-" + pursuit->id);

    const std::string playerName = candidate != nullptr ? candidate->player.fullName : pursuit->candidatePlayerId;
    addFootballInbox(
        "transfer-pursuit-improved-" + pursuit->id + "-" + std::to_string(pursuit->negotiationRound),
        playerName + " 영입 조건을 개선해 재제안했습니다",
        "새 구단 제안은 " + formatMoney(pursuit->currentOfferPence)
            + "입니다. 상대 구단은 다음 날 답변할 예정입니다.");
    return {true, "개선된 이적료를 다시 제안했습니다."};
}

TransferActionResult SimulationEngine::submitClubOffer(
    const std::string& pursuitId,
    const TransferFeeTerms& offerTerms)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&pursuitId](const TransferPursuit& item) {
            return item.id == pursuitId;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return {false, "영입 협상을 찾을 수 없습니다."};
    }
    if (pursuit->stage != TransferPursuitStage::ClubCountered) {
        return {false, "현재는 구단 조건을 수정해 재제안할 수 있는 단계가 아닙니다."};
    }
    if (!validTransferFeeTerms(offerTerms)) {
        return {false, "재제안 금액 또는 세부 조항이 올바르지 않습니다."};
    }
    if (const auto capacityError = transferCapacityError(
            state_,
            offerTerms,
            nullptr,
            pursuit->id);
        capacityError.has_value()) {
        return {false, *capacityError};
    }

    pursuit->offerTerms = offerTerms;
    pursuit->currentOfferPence = guaranteedTransferFee(offerTerms);
    ++pursuit->negotiationRound;
    pursuit->stage = TransferPursuitStage::AwaitingClubResponse;
    pursuit->nextActionDate = state_.currentDate.addDays(1);
    pursuit->lastUpdateDate = state_.currentDate;
    pursuit->responseNote = "즉시 지급·할부·조건부 보너스·재판매 조항을 수정해 다시 제안했습니다.";
    resolveInboxEvent("transfer-pursuit-counter-" + pursuit->id);

    const TransferCandidate* candidate =
        findTransferCandidate(pursuit->candidatePlayerId);
    const std::string playerName = candidate != nullptr
        ? candidate->player.fullName
        : pursuit->candidatePlayerId;
    std::ostringstream body;
    body << "보장액 " << formatMoney(guaranteedTransferFee(offerTerms))
         << "(즉시 " << formatMoney(offerTerms.upfrontFeePence)
         << ", 할부 " << formatMoney(offerTerms.installmentTotalPence)
         << "), 조건부 " << formatMoney(offerTerms.appearanceBonusPence)
         << ", 다음 이적 수익 " << offerTerms.sellOnPercentage
         << "% 조건을 보냈습니다. 상대 구단 답변은 다음 날 도착합니다.";
    addFootballInbox(
        "transfer-pursuit-detailed-offer-" + pursuit->id
            + "-" + std::to_string(pursuit->negotiationRound),
        playerName + " 세부 이적 조건을 재제안했습니다",
        body.str());
    return {true, "세부 이적 조건을 상대 구단에 다시 제안했습니다."};
}

TransferActionResult SimulationEngine::submitPlayerContractOffer(
    const std::string& pursuitId,
    const PlayerContractTerms& contractTerms)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&pursuitId](const TransferPursuit& item) {
            return item.id == pursuitId;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return {false, "영입 협상을 찾을 수 없습니다."};
    }
    if (pursuit->stage != TransferPursuitStage::AwaitingPlayerTerms
        && pursuit->stage != TransferPursuitStage::PlayerCountered) {
        return {false, "현재는 선수 계약 조건을 제안할 수 있는 단계가 아닙니다."};
    }
    if (!validPlayerContractTerms(contractTerms)) {
        return {false, "선수 계약의 금액·기간·선수단 지위가 올바르지 않습니다."};
    }
    const TransferFeeTerms feeTerms = effectiveFeeTerms(
        pursuit->currentOfferPence,
        pursuit->offerTerms);
    if (const auto capacityError = transferCapacityError(
            state_,
            feeTerms,
            &contractTerms,
            pursuit->id);
        capacityError.has_value()) {
        return {false, *capacityError};
    }

    if (pursuit->stage == TransferPursuitStage::PlayerCountered) {
        resolveInboxEvent("transfer-player-counter-" + pursuit->id);
    } else {
        resolveInboxEvent("transfer-pursuit-counter-accepted-" + pursuit->id);
        resolveInboxEvent("transfer-pursuit-club-accepted-" + pursuit->id);
    }
    pursuit->playerOfferTerms = contractTerms;
    pursuit->playerOfferSubmitted = true;
    ++pursuit->playerNegotiationRound;
    pursuit->stage = TransferPursuitStage::AwaitingPlayerResponse;
    pursuit->nextActionDate = state_.currentDate.addDays(1);
    pursuit->lastUpdateDate = state_.currentDate;
    pursuit->responseNote = "선수와 에이전트에게 세부 계약 조건을 전달했습니다.";

    const TransferCandidate* candidate =
        findTransferCandidate(pursuit->candidatePlayerId);
    const std::string playerName = candidate != nullptr
        ? candidate->player.fullName
        : pursuit->candidatePlayerId;
    std::ostringstream body;
    body << "주급 " << formatMoney(contractTerms.weeklyWagePence)
         << ", " << contractTerms.contractYears
         << "년, 계약금 " << formatMoney(contractTerms.signingBonusPence)
         << ", 에이전트 수수료 " << formatMoney(contractTerms.agentFeePence)
         << ", 선수단 지위 " << contractTerms.squadRole
         << " 조건을 제안했습니다. 답변은 다음 날 도착합니다.";
    addFootballInbox(
        "transfer-player-offer-" + pursuit->id
            + "-" + std::to_string(pursuit->playerNegotiationRound),
        playerName + " 측에 계약 조건을 제안했습니다",
        body.str());
    return {true, "선수와 에이전트에게 계약 조건을 제안했습니다."};
}

TransferActionResult SimulationEngine::acceptPlayerCounter(
    const std::string& pursuitId)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&pursuitId](const TransferPursuit& item) {
            return item.id == pursuitId;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return {false, "영입 협상을 찾을 수 없습니다."};
    }
    if (pursuit->stage != TransferPursuitStage::PlayerCountered) {
        return {false, "현재는 선수 측 역제안을 수락할 수 있는 단계가 아닙니다."};
    }
    if (!validPlayerContractTerms(pursuit->playerCounterTerms)) {
        return {false, "선수 측 역제안 조건이 올바르지 않습니다."};
    }
    const TransferFeeTerms feeTerms = effectiveFeeTerms(
        pursuit->currentOfferPence,
        pursuit->offerTerms);
    if (const auto capacityError = transferCapacityError(
            state_,
            feeTerms,
            &pursuit->playerCounterTerms,
            pursuit->id);
        capacityError.has_value()) {
        return {false, *capacityError};
    }

    pursuit->playerOfferTerms = pursuit->playerCounterTerms;
    pursuit->playerOfferSubmitted = true;
    pursuit->stage = TransferPursuitStage::AwaitingMedical;
    pursuit->nextActionDate = state_.currentDate.addDays(1);
    pursuit->lastUpdateDate = state_.currentDate;
    pursuit->medicalStatus = "scheduled";
    pursuit->responseNote = "선수 측 역제안을 수락했습니다. 메디컬이 예약되었습니다.";
    resolveInboxEvent("transfer-player-counter-" + pursuit->id);

    const TransferCandidate* candidate =
        findTransferCandidate(pursuit->candidatePlayerId);
    const std::string playerName = candidate != nullptr
        ? candidate->player.fullName
        : pursuit->candidatePlayerId;
    addFootballInbox(
        "transfer-player-counter-accepted-" + pursuit->id,
        playerName + " 측 계약 역제안을 수락했습니다",
        "선수 계약 조건에 합의했으며 다음 날 메디컬을 진행합니다. 아직 구단주 최종 승인 전이므로 비용은 지출되지 않았습니다.");
    return {true, "선수 측 역제안을 수락하고 메디컬을 예약했습니다."};
}

TransferActionResult SimulationEngine::withdrawTransferPursuit(const std::string& pursuitId)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&pursuitId](const TransferPursuit& item) {
            return item.id == pursuitId;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return {false, "영입 협상을 찾을 수 없습니다."};
    }
    if (pursuit->stage == TransferPursuitStage::AwaitingOwnerApproval
        || pursuit->stage == TransferPursuitStage::AwaitingCompletion) {
        return {false, "최종 승인 단계에서는 해당 영입 안건을 거절해야 합니다."};
    }
    if (pursuit->stage == TransferPursuitStage::Completed
        || pursuit->stage == TransferPursuitStage::ClubRejected
        || pursuit->stage == TransferPursuitStage::PlayerRejected
        || pursuit->stage == TransferPursuitStage::MedicalFailed
        || pursuit->stage == TransferPursuitStage::RegistrationFailed
        || pursuit->stage == TransferPursuitStage::OwnerRejected
        || pursuit->stage == TransferPursuitStage::Withdrawn
        || pursuit->stage == TransferPursuitStage::Expired) {
        return {false, "이미 종료된 영입 협상입니다."};
    }

    pursuit->stage = TransferPursuitStage::Withdrawn;
    pursuit->lastUpdateDate = state_.currentDate;
    pursuit->responseNote = "구단주 지시로 협상을 철회했습니다.";
    resolveInboxEvent("transfer-pursuit-counter-" + pursuit->id);
    resolveInboxEvent("transfer-player-counter-" + pursuit->id);
    resolveInboxEvent("transfer-pursuit-counter-accepted-" + pursuit->id);

    const TransferCandidate* candidate = findTransferCandidate(pursuit->candidatePlayerId);
    const std::string playerName = candidate != nullptr ? candidate->player.fullName : pursuit->candidatePlayerId;
    addFootballInbox(
        "transfer-pursuit-withdrawn-" + pursuit->id,
        playerName + " 영입 협상을 철회했습니다",
        "계약이나 이적료 약정이 확정되기 전 철회했으므로 예산 변화는 없습니다.");
    return {true, "영입 협상을 철회했습니다."};
}

TransferActionResult SimulationEngine::acceptOutgoingTransfer(const std::string& offerId)
{
    const auto offer = std::find_if(
        state_.outgoingTransferOffers.begin(),
        state_.outgoingTransferOffers.end(),
        [&offerId](const OutgoingTransferOffer& item) {
            return item.id == offerId;
        });
    if (offer == state_.outgoingTransferOffers.end()) {
        return {false, "선수 판매 제안을 찾을 수 없습니다."};
    }
    if (offer->status != OutgoingTransferStatus::Pending) {
        return {false, "이미 처리된 선수 판매 제안입니다."};
    }
    if (offer->negotiationStage == OutgoingNegotiationStage::AwaitingBuyerResponse) {
        return {false, "구매 구단의 재협상 답변을 기다리고 있습니다."};
    }
    if (offer->negotiationStage == OutgoingNegotiationStage::BuyerCountered) {
        return {false, "구매 구단의 역제안을 먼저 수락하거나 다시 협상해야 합니다."};
    }
    if (offer->decisionDeadline < state_.currentDate) {
        return {false, "판매 제안 기한이 지났습니다."};
    }
    const auto player = std::find_if(
        state_.players.begin(),
        state_.players.end(),
        [&offer](const Player& item) {
            return item.id == offer->playerId;
        });
    if (player == state_.players.end()) {
        return {false, "판매 대상 선수가 현재 선수단에 없습니다."};
    }

    offer->feeTerms = effectiveFeeTerms(offer->feePence, offer->feeTerms);
    if (!validTransferFeeTerms(offer->feeTerms)) {
        return {false, "판매 합의의 금액 또는 세부 조항이 올바르지 않습니다."};
    }
    offer->feePence = guaranteedTransferFee(offer->feeTerms);
    offer->status = OutgoingTransferStatus::Accepted;
    offer->negotiationStage = OutgoingNegotiationStage::AgreementReached;
    offer->decisionDate = state_.currentDate;
    offer->completionDate = state_.currentDate.addDays(3);
    resolveInboxEvent("outgoing-offer-" + offer->id);

    addFootballInbox(
        "outgoing-offer-accepted-" + offer->id,
        offer->playerName + " 판매 제안을 수락했습니다",
        offer->completionDate->toIsoString()
            + "에 이적이 완료됩니다. 보장 이적료 "
            + formatMoney(guaranteedTransferFee(offer->feeTerms))
            + "는 이적 예산에 반영되고, 즉시 수령액 "
            + formatMoney(offer->feeTerms.upfrontFeePence)
            + "와 향후 할부는 각 지급일에 현금으로 들어옵니다.");
    return {true, "선수 판매 제안을 수락했습니다."};
}

TransferActionResult SimulationEngine::rejectOutgoingTransfer(const std::string& offerId)
{
    const auto offer = std::find_if(
        state_.outgoingTransferOffers.begin(),
        state_.outgoingTransferOffers.end(),
        [&offerId](const OutgoingTransferOffer& item) {
            return item.id == offerId;
        });
    if (offer == state_.outgoingTransferOffers.end()) {
        return {false, "선수 판매 제안을 찾을 수 없습니다."};
    }
    if (offer->status != OutgoingTransferStatus::Pending) {
        return {false, "이미 처리된 선수 판매 제안입니다."};
    }
    if (offer->decisionDeadline < state_.currentDate) {
        return {false, "판매 제안 기한이 지났습니다."};
    }

    offer->status = OutgoingTransferStatus::Rejected;
    offer->negotiationStage = OutgoingNegotiationStage::Closed;
    offer->decisionDate = state_.currentDate;
    resolveInboxEvent("outgoing-offer-" + offer->id);
    addFootballInbox(
        "outgoing-offer-rejected-" + offer->id,
        offer->playerName + " 판매 제안을 거절했습니다",
        "선수는 현재 선수단에 남으며 재정 변화는 없습니다.");
    return {true, "선수 판매 제안을 거절했습니다."};
}

TransferActionResult SimulationEngine::submitOutgoingTransferCounter(
    const std::string& offerId,
    const TransferFeeTerms& feeTerms)
{
    const auto offer = std::find_if(
        state_.outgoingTransferOffers.begin(),
        state_.outgoingTransferOffers.end(),
        [&offerId](const OutgoingTransferOffer& item) {
            return item.id == offerId;
        });
    if (offer == state_.outgoingTransferOffers.end()) {
        return {false, "선수 판매 제안을 찾을 수 없습니다."};
    }
    if (offer->status != OutgoingTransferStatus::Pending
        || offer->negotiationStage == OutgoingNegotiationStage::AwaitingBuyerResponse) {
        return {false, "현재는 판매 조건을 다시 제안할 수 없습니다."};
    }
    if (offer->decisionDeadline < state_.currentDate) {
        return {false, "판매 제안 기한이 지났습니다."};
    }
    if (!validTransferFeeTerms(feeTerms)) {
        return {false, "판매 역제안의 금액 또는 세부 조항이 올바르지 않습니다."};
    }

    offer->feeTerms = feeTerms;
    offer->feePence = guaranteedTransferFee(feeTerms);
    offer->negotiationStage = OutgoingNegotiationStage::AwaitingBuyerResponse;
    ++offer->negotiationRound;
    offer->nextActionDate = state_.currentDate.addDays(1);
    resolveInboxEvent("outgoing-offer-" + offer->id);
    resolveInboxEvent("outgoing-buyer-counter-" + offer->id);

    std::ostringstream body;
    body << "보장액 " << formatMoney(guaranteedTransferFee(feeTerms))
         << "(즉시 " << formatMoney(feeTerms.upfrontFeePence)
         << ", 할부 " << formatMoney(feeTerms.installmentTotalPence)
         << "), 조건부 " << formatMoney(feeTerms.appearanceBonusPence)
         << ", 다음 이적 수익 " << feeTerms.sellOnPercentage
         << "%를 요구했습니다. 구매 구단은 다음 날 답변합니다.";
    addFootballInbox(
        "outgoing-counter-submitted-" + offer->id
            + "-" + std::to_string(offer->negotiationRound),
        offer->playerName + " 판매 조건을 역제안했습니다",
        body.str());
    return {true, "구매 구단에 세부 판매 조건을 역제안했습니다."};
}

TransferActionResult SimulationEngine::acceptOutgoingTransferCounter(
    const std::string& offerId)
{
    const auto offer = std::find_if(
        state_.outgoingTransferOffers.begin(),
        state_.outgoingTransferOffers.end(),
        [&offerId](const OutgoingTransferOffer& item) {
            return item.id == offerId;
        });
    if (offer == state_.outgoingTransferOffers.end()) {
        return {false, "선수 판매 제안을 찾을 수 없습니다."};
    }
    if (offer->status != OutgoingTransferStatus::Pending
        || offer->negotiationStage != OutgoingNegotiationStage::BuyerCountered) {
        return {false, "현재는 구매 구단 역제안을 수락할 수 없습니다."};
    }
    if (!validTransferFeeTerms(offer->buyerCounterTerms)) {
        return {false, "구매 구단 역제안 조건이 올바르지 않습니다."};
    }

    offer->feeTerms = offer->buyerCounterTerms;
    offer->feePence = guaranteedTransferFee(offer->feeTerms);
    offer->negotiationStage = OutgoingNegotiationStage::AgreementReached;
    offer->nextActionDate.reset();
    resolveInboxEvent("outgoing-buyer-counter-" + offer->id);
    addFootballInbox(
        "outgoing-buyer-counter-accepted-" + offer->id,
        offer->buyerClubName + "의 " + offer->playerName + " 제안을 수락했습니다",
        "구매 구단과 세부 이적료 조건에 합의했습니다. 이제 판매 승인 또는 거절을 최종 결정할 수 있습니다.",
        true);
    return {true, "구매 구단 역제안을 수락했습니다. 판매 최종 결정을 내려 주세요."};
}

TransferActionResult SimulationEngine::startScoutingAssignment(
    const std::string& playerId,
    const std::string& scoutId,
    const ScoutingDepth depth,
    const std::string& focus)
{
    const TransferCandidate* candidate = findTransferCandidate(playerId);
    if (candidate == nullptr) {
        return {false, "스카우팅할 영입 후보를 찾을 수 없습니다."};
    }
    const auto scout = std::find_if(
        state_.scouts.begin(),
        state_.scouts.end(),
        [&scoutId](const ScoutProfile& item) {
            return item.id == scoutId;
        });
    if (scout == state_.scouts.end()) {
        return {false, "배정할 스카우트를 찾을 수 없습니다."};
    }
    if (!validScoutingFocus(focus)) {
        return {false, "스카우팅 중점 항목이 올바르지 않습니다."};
    }
    const auto activeForPlayer = std::find_if(
        state_.scoutingAssignments.begin(),
        state_.scoutingAssignments.end(),
        [&playerId](const ScoutingAssignment& assignment) {
            return assignment.playerId == playerId
                && assignment.status == ScoutingAssignmentStatus::Active;
        });
    if (activeForPlayer != state_.scoutingAssignments.end()) {
        return {false, "이 선수는 이미 관찰 중입니다."};
    }
    const int scoutWorkload = static_cast<int>(std::count_if(
        state_.scoutingAssignments.begin(),
        state_.scoutingAssignments.end(),
        [&scoutId](const ScoutingAssignment& assignment) {
            return assignment.scoutId == scoutId
                && assignment.status == ScoutingAssignmentStatus::Active;
        }));
    if (scoutWorkload >= scout->assignmentCapacity) {
        return {false, "이 스카우트는 현재 배정 한도에 도달했습니다."};
    }

    const auto existingReport = std::find_if(
        state_.scoutingReports.begin(),
        state_.scoutingReports.end(),
        [&playerId](const ScoutingReport& report) {
            return report.playerId == playerId;
        });
    const int startingKnowledge = existingReport != state_.scoutingReports.end()
        ? existingReport->knowledgeLevel
        : 0;
    if (existingReport != state_.scoutingReports.end()
        && startingKnowledge >= 100
        && state_.currentDate <= existingReport->validUntil) {
        return {false, "이미 최신 종합 보고서가 있어 다시 배정할 필요가 없습니다."};
    }

    int assignmentNumber = 1;
    for (const auto& assignment : state_.scoutingAssignments) {
        if (assignment.playerId == playerId) {
            ++assignmentNumber;
        }
    }
    const int duration = scoutingDurationDaysWithFamiliarity(
        state_,
        *scout,
        *candidate,
        depth);
    const int targetKnowledge = std::min(
        100,
        startingKnowledge
            + scoutingKnowledgeGainWithFamiliarity(
                state_,
                *scout,
                *candidate,
                depth));
    state_.scoutingAssignments.push_back({
        "scouting-" + playerId + "-" + state_.currentDate.toIsoString()
            + "-" + std::to_string(assignmentNumber),
        playerId,
        scoutId,
        depth,
        focus,
        state_.currentDate,
        state_.currentDate.addDays(std::max(1, duration / 2)),
        state_.currentDate.addDays(duration),
        ScoutingAssignmentStatus::Active,
        startingKnowledge,
        targetKnowledge,
        0,
        std::nullopt
    });

    addFootballInbox(
        "scouting-assigned-" + state_.scoutingAssignments.back().id,
        candidate->player.fullName + " 스카우팅을 배정했습니다",
        scout->fullName + "이 " + std::to_string(duration)
            + "일 동안 관찰합니다. 담당자의 지역·국가·포지션 친숙도에 따라 보고 속도와 정보 정확도가 보정됩니다.");
    return {true, "스카우팅을 배정했습니다."};
}

TransferActionResult SimulationEngine::startTargetScoutingDirective(
    const std::string& playerId,
    const std::string& scoutId,
    const ScoutingDepth depth,
    const std::string& focus)
{
    const TransferCandidate* candidate = findTransferCandidate(playerId);
    if (candidate == nullptr) {
        return {false, "특정 선수 지시의 대상을 찾을 수 없습니다."};
    }
    const auto scout = std::find_if(
        state_.scouts.begin(),
        state_.scouts.end(),
        [&scoutId](const ScoutProfile& item) {
            return item.id == scoutId;
        });
    if (scout == state_.scouts.end()) {
        return {false, "지시할 스카우트를 찾을 수 없습니다."};
    }

    const auto assignmentResult =
        startScoutingAssignment(playerId, scoutId, depth, focus);
    if (!assignmentResult.success) {
        return assignmentResult;
    }

    const std::string assignmentId = state_.scoutingAssignments.empty()
        ? std::string {}
        : state_.scoutingAssignments.back().id;
    const ScoutingAssignment* generatedAssignment =
        state_.scoutingAssignments.empty()
        ? nullptr
        : &state_.scoutingAssignments.back();
    const int duration = generatedAssignment != nullptr
        ? state_.currentDate.daysUntil(generatedAssignment->dueDate)
        : scoutingDurationDays(depth);
    const int accuracyModifier = scoutingAccuracyModifierWithFamiliarity(
        state_,
        *scout,
        *candidate);
    const int score = scoutingDirectiveCandidateScore(
        state_,
        *scout,
        *candidate,
        candidate->askingPricePence);
    state_.scoutingDirectives.push_back({
        "directive-target-" + playerId + "-"
            + state_.currentDate.toIsoString() + "-"
            + std::to_string(state_.scoutingDirectives.size() + 1),
        ScoutingDirectiveType::Target,
        ScoutingDirectiveStatus::Active,
        scoutId,
        depth,
        focus,
        playerId,
        assignmentId,
        {},
        candidate->player.nationality,
        {},
        scoutingPositionGroup(candidate->player.position),
        {},
        0,
        0,
        false,
        candidate->askingPricePence,
        1,
        playerId,
        score,
        duration,
        accuracyModifier,
        state_.currentDate,
        state_.currentDate.addDays(duration),
        std::nullopt,
        candidate->player.fullName + " 특정 관찰 지시를 생성했습니다.",
        "player_created_target_scouting_directive"
    });

    addFootballInbox(
        "scouting-directive-created-" + state_.scoutingDirectives.back().id,
        candidate->player.fullName + " 특정 스카우팅 지시",
        scout->fullName + "에게 특정 선수 관찰 지시를 내렸습니다. 기존 후보 관찰 배정과 연결되어 중간·최종 보고가 도착합니다.");
    recordOwnerAction(
        state_,
        "target_scouting_directive",
        "scouting",
        candidate->player.id,
        candidate->player.fullName,
        candidate->player.fullName + " 특정 스카우팅 지시",
        ownerImpactForTargetScoutingDirective(state_, *candidate),
        "player_owner_scouting_directive");
    return {true, "특정 선수 스카우팅 지시를 생성했습니다."};
}

TransferActionResult SimulationEngine::startRangeScoutingDirective(
    const std::string& scoutId,
    const ScoutingDepth depth,
    const std::string& focus,
    const std::string& region,
    const std::string& country,
    const std::string& league,
    const std::string& position,
    const std::string& style,
    const int minAge,
    const int maxAge,
    const bool homegrownOnly,
    const Money maxBudgetPence)
{
    const auto scout = std::find_if(
        state_.scouts.begin(),
        state_.scouts.end(),
        [&scoutId](const ScoutProfile& item) {
            return item.id == scoutId;
        });
    if (scout == state_.scouts.end()) {
        return {false, "지시할 스카우트를 찾을 수 없습니다."};
    }
    if (!validScoutingFocus(focus)) {
        return {false, "스카우팅 중점 항목이 올바르지 않습니다."};
    }
    if (minAge < 0 || maxAge < 0 || (maxAge > 0 && minAge > maxAge)) {
        return {false, "범위형 지시의 나이 조건이 올바르지 않습니다."};
    }
    if (maxBudgetPence < 0) {
        return {false, "범위형 지시의 예산 조건이 올바르지 않습니다."};
    }

    std::vector<const TransferCandidate*> matches;
    for (const auto& candidate : state_.transferCandidates) {
        const bool alreadyScouting = std::any_of(
            state_.scoutingAssignments.begin(),
            state_.scoutingAssignments.end(),
            [&candidate](const ScoutingAssignment& assignment) {
                return assignment.playerId == candidate.player.id
                    && assignment.status == ScoutingAssignmentStatus::Active;
            });
        if (alreadyScouting) {
            continue;
        }
        if (!scoutingRegionMatches(candidate, region)) {
            continue;
        }
        if (!country.empty()
            && country != "any"
            && !containsAsciiInsensitive(candidate.player.nationality, country)) {
            continue;
        }
        if (!league.empty()
            && league != "any"
            && !containsAsciiInsensitive(candidate.currentClubName, league)
            && !containsAsciiInsensitive(candidate.currentClubId, league)) {
            continue;
        }
        if (!scoutingPositionMatches(candidate, position)) {
            continue;
        }
        if (!scoutingStyleMatches(candidate, style)) {
            continue;
        }
        const auto age = ageOnDate(candidate.player, state_.currentDate);
        if (age.has_value()
            && ((minAge > 0 && *age < minAge)
                || (maxAge > 0 && *age > maxAge))) {
            continue;
        }
        if (homegrownOnly && !scoutingHomegrownProxyMatches(candidate)) {
            continue;
        }
        if (maxBudgetPence > 0
            && candidate.askingPricePence > maxBudgetPence) {
            continue;
        }
        matches.push_back(&candidate);
    }

    if (matches.empty()) {
        return {false, "현재 후보 풀에서 조건에 맞는 선수를 찾지 못했습니다."};
    }

    const auto best = std::max_element(
        matches.begin(),
        matches.end(),
        [this, &scout, maxBudgetPence](
            const TransferCandidate* left,
            const TransferCandidate* right) {
            return scoutingDirectiveCandidateScore(
                state_,
                *scout,
                *left,
                maxBudgetPence)
                < scoutingDirectiveCandidateScore(
                    state_,
                    *scout,
                    *right,
                    maxBudgetPence);
        });
    if (best == matches.end() || *best == nullptr) {
        return {false, "범위형 지시에 배정할 후보를 결정하지 못했습니다."};
    }

    const TransferCandidate& selected = **best;
    const auto assignmentResult =
        startScoutingAssignment(selected.player.id, scoutId, depth, focus);
    if (!assignmentResult.success) {
        return assignmentResult;
    }

    const std::string assignmentId = state_.scoutingAssignments.empty()
        ? std::string {}
        : state_.scoutingAssignments.back().id;
    const ScoutingAssignment* generatedAssignment =
        state_.scoutingAssignments.empty()
        ? nullptr
        : &state_.scoutingAssignments.back();
    const int duration = generatedAssignment != nullptr
        ? state_.currentDate.daysUntil(generatedAssignment->dueDate)
        : scoutingDurationDays(depth);
    const int accuracyModifier = scoutingAccuracyModifierWithFamiliarity(
        state_,
        *scout,
        selected);
    const int score = scoutingDirectiveCandidateScore(
        state_,
        *scout,
        selected,
        maxBudgetPence);
    state_.scoutingDirectives.push_back({
        "directive-range-" + state_.currentDate.toIsoString() + "-"
            + std::to_string(state_.scoutingDirectives.size() + 1),
        ScoutingDirectiveType::Range,
        ScoutingDirectiveStatus::Active,
        scoutId,
        depth,
        focus,
        {},
        assignmentId,
        region,
        country,
        league,
        position,
        style,
        minAge,
        maxAge,
        homegrownOnly,
        maxBudgetPence,
        static_cast<int>(matches.size()),
        joinPlayerIds(matches),
        score,
        duration,
        accuracyModifier,
        state_.currentDate,
        state_.currentDate.addDays(duration),
        std::nullopt,
        "범위 조건에 맞는 후보 "
            + std::to_string(matches.size())
            + "명 중 "
            + selected.player.fullName
            + "을 우선 관찰 대상으로 배정했습니다.",
        "player_created_range_scouting_directive_current_candidate_pool"
    });

    addFootballInbox(
        "scouting-directive-created-" + state_.scoutingDirectives.back().id,
        "범위형 스카우팅 지시를 생성했습니다",
        scout->fullName + "에게 범위형 지시를 내렸습니다. 현재 후보 풀에서 "
            + selected.player.fullName
            + "이 우선 관찰 대상으로 연결됐습니다.");
    recordOwnerAction(
        state_,
        "range_scouting_directive",
        "scouting",
        state_.scoutingDirectives.back().id,
        "범위형 지시",
        state_.scoutingDirectives.back().summary,
        ownerImpactForRangeScoutingDirective(
            homegrownOnly,
            maxAge,
            maxBudgetPence),
        "player_owner_scouting_directive");
    return {true, "범위형 스카우팅 지시를 생성했습니다."};
}

TransferActionResult SimulationEngine::cancelScoutingAssignment(
    const std::string& assignmentId)
{
    const auto assignment = std::find_if(
        state_.scoutingAssignments.begin(),
        state_.scoutingAssignments.end(),
        [&assignmentId](const ScoutingAssignment& item) {
            return item.id == assignmentId;
        });
    if (assignment == state_.scoutingAssignments.end()) {
        return {false, "스카우팅 배정을 찾을 수 없습니다."};
    }
    if (assignment->status != ScoutingAssignmentStatus::Active) {
        return {false, "이미 종료된 스카우팅 배정입니다."};
    }
    assignment->status = ScoutingAssignmentStatus::Cancelled;
    assignment->completedDate = state_.currentDate;
    for (auto& directive : state_.scoutingDirectives) {
        if (directive.generatedAssignmentId == assignmentId
            && directive.status == ScoutingDirectiveStatus::Active) {
            directive.status = ScoutingDirectiveStatus::Cancelled;
            directive.completedDate = state_.currentDate;
            directive.summary += " 연결된 관찰 배정이 중단되어 지시도 취소됐습니다.";
        }
    }
    const TransferCandidate* candidate =
        findTransferCandidate(assignment->playerId);
    const std::string playerName = candidate != nullptr
        ? candidate->player.fullName
        : assignment->playerId;
    addFootballInbox(
        "scouting-cancelled-" + assignment->id,
        playerName + " 스카우팅을 중단했습니다",
        "이미 도착한 중간 보고 내용은 유지되지만 남은 관찰 진척은 반영되지 않습니다.");
    return {true, "스카우팅 배정을 중단했습니다."};
}

TransferActionResult SimulationEngine::cancelScoutingDirective(
    const std::string& directiveId)
{
    const auto directive = std::find_if(
        state_.scoutingDirectives.begin(),
        state_.scoutingDirectives.end(),
        [&directiveId](const ScoutingDirective& item) {
            return item.id == directiveId;
        });
    if (directive == state_.scoutingDirectives.end()) {
        return {false, "스카우팅 지시를 찾을 수 없습니다."};
    }
    if (directive->status != ScoutingDirectiveStatus::Active) {
        return {false, "이미 종료된 스카우팅 지시입니다."};
    }

    if (!directive->generatedAssignmentId.empty()) {
        const auto assignment = std::find_if(
            state_.scoutingAssignments.begin(),
            state_.scoutingAssignments.end(),
            [&directive](const ScoutingAssignment& item) {
                return item.id == directive->generatedAssignmentId;
            });
        if (assignment != state_.scoutingAssignments.end()
            && assignment->status == ScoutingAssignmentStatus::Active) {
            assignment->status = ScoutingAssignmentStatus::Cancelled;
            assignment->completedDate = state_.currentDate;
        }
    }

    directive->status = ScoutingDirectiveStatus::Cancelled;
    directive->completedDate = state_.currentDate;
    directive->summary += " 구단주가 지시를 취소했습니다.";
    addFootballInbox(
        "scouting-directive-cancelled-" + directive->id,
        "스카우팅 지시를 취소했습니다",
        "연결된 관찰 배정이 진행 중이었다면 함께 중단했습니다.");
    return {true, "스카우팅 지시를 취소했습니다."};
}

TransferActionResult SimulationEngine::startIncomingLoan(
    const std::string& playerId,
    const LoanTerms& terms)
{
    if (!domesticRegistrationOpen(state_, state_.currentDate)) {
        return {
            false,
            "1992-93 잉글랜드 등록 마감일인 3월 31일이 지나 새 임대를 등록할 수 없습니다."};
    }
    const TransferCandidate* candidate = findTransferCandidate(playerId);
    if (candidate == nullptr) {
        return {false, "임대를 제안할 영입 후보를 찾을 수 없습니다."};
    }
    if (!validLoanTerms(terms, state_.currentDate)) {
        return {false, "임대 기간 또는 세부 조건이 올바르지 않습니다."};
    }
    if (std::any_of(
            state_.players.begin(),
            state_.players.end(),
            [&playerId](const Player& player) {
                return player.id == playerId;
            })) {
        return {false, "이미 우리 선수단에 등록된 선수입니다."};
    }
    if (std::any_of(
            state_.loanDeals.begin(),
            state_.loanDeals.end(),
            [&playerId](const LoanDeal& loan) {
                return loan.player.id == playerId
                    && !isClosedLoanStage(loan.stage);
            })) {
        return {false, "이 선수와 이미 진행 중인 임대 안건이 있습니다."};
    }

    const Money weeklyShare = loanWeeklyWageShare(
        candidate->expectedWeeklyWagePence,
        terms);
    const Money financialCommitment =
        loanFinancialCommitment(terms);
    if (availableTransferCash(state_) < financialCommitment) {
        return {false, "가용 현금보다 큰 임대료·의무 영입 약정은 제안할 수 없습니다."};
    }
    if (availableTransferBudget(state_) < financialCommitment) {
        return {false, "현재 이적 예산보다 큰 임대료·의무 영입 약정은 제안할 수 없습니다."};
    }
    if (state_.club.finance.weeklyWageCommitmentPence + weeklyShare
        > state_.club.finance.weeklyWageBudgetPence) {
        return {false, "임대 선수의 주급 부담을 감당할 수 없습니다."};
    }

    int attempt = 1;
    for (const auto& loan : state_.loanDeals) {
        if (loan.player.id == playerId) {
            ++attempt;
        }
    }
    LoanDeal loan {
        "incoming-loan-" + playerId + "-" + state_.currentDate.toIsoString()
            + "-" + std::to_string(attempt),
        candidate->player,
        LoanDirection::Incoming,
        candidate->currentClubId,
        candidate->currentClubName,
        state_.club.id,
        state_.club.name,
        "완전 영입 전 평가와 즉시 전력 보강",
        state_.currentDate,
        state_.currentDate.addDays(14),
        state_.currentDate.addDays(3),
        LoanStage::AwaitingClubResponse,
        terms,
        recommendedIncomingLoanTerms(*candidate, state_.currentDate),
        state_.currentDate.addDays(1),
        std::nullopt,
        std::nullopt,
        std::nullopt,
        {},
        1,
        0,
        0,
        candidate->currentClubName + "에 임대 조건을 전달했습니다.",
        "gameplay_loan_negotiation"
    };
    state_.loanDeals.push_back(std::move(loan));
    addFootballInbox(
        "loan-started-" + state_.loanDeals.back().id,
        candidate->player.fullName + " 임대 협상을 시작했습니다",
        "임대료 " + formatMoney(terms.loanFeePence)
            + ", 주급 부담 " + std::to_string(terms.borrowingClubWagePercentage)
            + "% 조건을 제안했습니다. 상대 구단은 다음 날 답변합니다.");
    return {true, "들어오는 임대 협상을 시작했습니다."};
}

TransferActionResult SimulationEngine::acceptLoanOffer(
    const std::string& loanId)
{
    const auto loan = std::find_if(
        state_.loanDeals.begin(),
        state_.loanDeals.end(),
        [&loanId](const LoanDeal& item) {
            return item.id == loanId;
        });
    if (loan == state_.loanDeals.end()) {
        return {false, "임대 안건을 찾을 수 없습니다."};
    }
    if (loan->stage != LoanStage::DecisionPending) {
        return {false, "현재는 이 임대 제안을 수락할 수 없습니다."};
    }
    if (loan->decisionDeadline < state_.currentDate
        || !validLoanTerms(loan->terms, state_.currentDate)) {
        return {false, "임대 제안 기한이 지났거나 조건이 올바르지 않습니다."};
    }
    if (!domesticRegistrationOpen(state_, state_.currentDate)) {
        return {
            false,
            "1992-93 잉글랜드 등록 마감일인 3월 31일이 지나 임대를 승인할 수 없습니다."};
    }

    if (loan->direction == LoanDirection::Incoming) {
        const TransferCandidate* candidate =
            findTransferCandidate(loan->player.id);
        const Money weeklyWage = candidate != nullptr
            ? candidate->expectedWeeklyWagePence
            : Money {};
        const Money financialCommitment =
            loanFinancialCommitment(loan->terms);
        if (availableTransferCash(state_, {}, {}, loan->id)
                < financialCommitment
            || availableTransferBudget(state_) < financialCommitment) {
            return {false, "임대료를 감당할 가용 현금 또는 이적 예산이 부족합니다."};
        }
        if (state_.club.finance.weeklyWageCommitmentPence
                + loanWeeklyWageShare(weeklyWage, loan->terms)
            > state_.club.finance.weeklyWageBudgetPence) {
            return {false, "임대 선수의 주급 부담을 감당할 수 없습니다."};
        }
    }

    loan->stage = LoanStage::AwaitingStart;
    loan->plannedStartDate = state_.currentDate.addDays(3);
    loan->nextActionDate = loan->plannedStartDate;
    resolveLoanInbox(*loan);
    addFootballInbox(
        "loan-accepted-" + loan->id,
        loan->player.fullName + " 임대에 합의했습니다",
        loan->plannedStartDate.toIsoString()
            + "에 임대가 시작됩니다. 임대료와 주급 분담은 실제 합류일에 반영됩니다.");
    return {true, "임대 제안을 수락했습니다."};
}

TransferActionResult SimulationEngine::rejectLoanOffer(
    const std::string& loanId)
{
    const auto loan = std::find_if(
        state_.loanDeals.begin(),
        state_.loanDeals.end(),
        [&loanId](const LoanDeal& item) {
            return item.id == loanId;
        });
    if (loan == state_.loanDeals.end()) {
        return {false, "임대 안건을 찾을 수 없습니다."};
    }
    if (loan->stage != LoanStage::DecisionPending
        && loan->stage != LoanStage::ClubCountered) {
        return {false, "현재는 이 임대 안건을 거절할 수 없습니다."};
    }
    loan->stage = LoanStage::Rejected;
    loan->nextActionDate.reset();
    resolveLoanInbox(*loan);
    addFootballInbox(
        "loan-rejected-" + loan->id,
        loan->player.fullName + " 임대 안건을 거절했습니다",
        "선수 등록과 구단 재정에는 변화가 없습니다.");
    return {true, "임대 안건을 거절했습니다."};
}

TransferActionResult SimulationEngine::submitLoanCounter(
    const std::string& loanId,
    const LoanTerms& terms)
{
    const auto loan = std::find_if(
        state_.loanDeals.begin(),
        state_.loanDeals.end(),
        [&loanId](const LoanDeal& item) {
            return item.id == loanId;
        });
    if (loan == state_.loanDeals.end()) {
        return {false, "임대 안건을 찾을 수 없습니다."};
    }
    if (loan->stage != LoanStage::DecisionPending
        && loan->stage != LoanStage::ClubCountered) {
        return {false, "현재는 임대 조건을 다시 제안할 수 없습니다."};
    }
    if (!validLoanTerms(terms, state_.currentDate)) {
        return {false, "임대 역제안 조건이 올바르지 않습니다."};
    }
    if (!domesticRegistrationOpen(state_, state_.currentDate)) {
        return {
            false,
            "1992-93 잉글랜드 등록 마감일인 3월 31일이 지나 임대 조건을 제안할 수 없습니다."};
    }
    if (loan->direction == LoanDirection::Incoming
        && (availableTransferCash(state_, {}, {}, loan->id)
                < loanFinancialCommitment(terms)
            || availableTransferBudget(state_)
                < loanFinancialCommitment(terms))) {
        return {false, "가용 현금 또는 이적 예산보다 큰 임대료·의무 영입 약정은 제안할 수 없습니다."};
    }

    loan->terms = terms;
    loan->stage = LoanStage::AwaitingClubResponse;
    ++loan->negotiationRound;
    loan->nextActionDate = state_.currentDate.addDays(1);
    loan->responseNote = "세부 임대 조건을 역제안했습니다.";
    resolveLoanInbox(*loan);
    addFootballInbox(
        "loan-counter-submitted-" + loan->id
            + "-" + std::to_string(loan->negotiationRound),
        loan->player.fullName + " 임대 조건을 역제안했습니다",
        "임대료 " + formatMoney(terms.loanFeePence)
            + ", 주급 부담 " + std::to_string(terms.borrowingClubWagePercentage)
            + "% 조건에 대해 다음 날 상대 구단 답변이 도착합니다.");
    return {true, "임대 조건을 역제안했습니다."};
}

TransferActionResult SimulationEngine::acceptLoanCounter(
    const std::string& loanId)
{
    const auto loan = std::find_if(
        state_.loanDeals.begin(),
        state_.loanDeals.end(),
        [&loanId](const LoanDeal& item) {
            return item.id == loanId;
        });
    if (loan == state_.loanDeals.end()) {
        return {false, "임대 안건을 찾을 수 없습니다."};
    }
    if (loan->stage != LoanStage::ClubCountered
        || !validLoanTerms(loan->counterTerms, state_.currentDate)) {
        return {false, "현재 수락할 수 있는 상대 구단 역제안이 없습니다."};
    }
    if (!domesticRegistrationOpen(state_, state_.currentDate)) {
        return {
            false,
            "1992-93 잉글랜드 등록 마감일인 3월 31일이 지나 임대 역제안을 수락할 수 없습니다."};
    }

    loan->terms = loan->counterTerms;
    if (loan->direction == LoanDirection::Incoming) {
        const TransferCandidate* candidate =
            findTransferCandidate(loan->player.id);
        const Money weeklyWage = candidate != nullptr
            ? candidate->expectedWeeklyWagePence
            : Money {};
        const Money financialCommitment =
            loanFinancialCommitment(loan->terms);
        if (availableTransferCash(state_, {}, {}, loan->id)
                < financialCommitment
            || availableTransferBudget(state_) < financialCommitment) {
            return {false, "상대 구단 역제안의 임대료를 감당할 수 없습니다."};
        }
        if (state_.club.finance.weeklyWageCommitmentPence
                + loanWeeklyWageShare(weeklyWage, loan->terms)
            > state_.club.finance.weeklyWageBudgetPence) {
            return {false, "상대 구단 역제안의 주급 부담을 감당할 수 없습니다."};
        }
    }

    loan->stage = LoanStage::AwaitingStart;
    loan->plannedStartDate = state_.currentDate.addDays(3);
    loan->nextActionDate = loan->plannedStartDate;
    resolveLoanInbox(*loan);
    addFootballInbox(
        "loan-counter-accepted-" + loan->id,
        loan->player.fullName + " 임대 역제안을 수락했습니다",
        loan->plannedStartDate.toIsoString()
            + "에 임대가 시작되며 실제 합류일에 재정과 선수단이 갱신됩니다.");
    return {true, "상대 구단의 임대 역제안을 수락했습니다."};
}

TransferActionResult SimulationEngine::recallLoan(
    const std::string& loanId)
{
    const auto loan = std::find_if(
        state_.loanDeals.begin(),
        state_.loanDeals.end(),
        [&loanId](const LoanDeal& item) {
            return item.id == loanId;
        });
    if (loan == state_.loanDeals.end()) {
        return {false, "임대 안건을 찾을 수 없습니다."};
    }
    if (loan->direction != LoanDirection::Outgoing
        || loan->stage != LoanStage::Active) {
        return {false, "현재 우리 선수를 조기 복귀시킬 수 있는 임대가 아닙니다."};
    }
    if (!loan->terms.canRecall) {
        return {false, "이 임대 계약에는 리콜 조항이 없습니다."};
    }

    loan->stage = LoanStage::RecallPending;
    loan->nextActionDate =
        state_.currentDate.addDays(loan->terms.recallNoticeDays);
    addFootballInbox(
        "loan-recall-requested-" + loan->id,
        loan->player.fullName + " 조기 복귀를 요청했습니다",
        std::to_string(loan->terms.recallNoticeDays)
            + "일의 리콜 통지 기간 뒤 선수단에 복귀합니다.");
    return {true, "임대 선수의 조기 복귀를 요청했습니다."};
}

TransferActionResult SimulationEngine::submitContractRenewal(
    const std::string& playerId,
    const PlayerContractTerms& terms)
{
    if (!validPlayerContractTerms(terms)) {
        return {false, "재계약 제안 조건이 올바르지 않습니다."};
    }
    const auto contract = std::find_if(
        state_.playerContracts.begin(),
        state_.playerContracts.end(),
        [&playerId, this](const PlayerContract& item) {
            return item.playerId == playerId
                && item.clubId == state_.club.id;
        });
    if (contract == state_.playerContracts.end()) {
        return {false, "재계약할 기존 선수 계약을 찾을 수 없습니다."};
    }
    const auto player = std::find_if(
        state_.players.begin(),
        state_.players.end(),
        [&playerId](const Player& item) {
            return item.id == playerId;
        });
    if (player == state_.players.end()) {
        return {false, "재계약할 선수가 현재 선수단에 없습니다."};
    }

    auto renewal = std::find_if(
        state_.contractRenewals.begin(),
        state_.contractRenewals.end(),
        [&playerId](const ContractRenewalCase& item) {
            return item.playerId == playerId
                && !isClosedRenewalStage(item.stage);
        });
    std::string renewalId;
    if (renewal != state_.contractRenewals.end()) {
        renewalId = renewal->id;
    }
    if (availableTransferCash(
            state_,
            {},
            {},
            {},
            renewalId) < contractRenewalCommitment(terms)) {
        return {false, "미정산 지출을 제외한 가용 현금보다 큰 재계약 보너스는 약정할 수 없습니다."};
    }
    const Money oldHomeWage = homeClubWageShare(
        state_,
        playerId,
        contract->weeklyWagePence);
    const Money newHomeWage = homeClubWageShare(
        state_,
        playerId,
        terms.weeklyWagePence);
    const Money nextWageCommitment =
        state_.club.finance.weeklyWageCommitmentPence
        - oldHomeWage
        + newHomeWage;
    if (nextWageCommitment > state_.club.finance.weeklyWageBudgetPence) {
        return {false, "재계약 후 주급 예산을 초과합니다."};
    }

    if (renewal == state_.contractRenewals.end()) {
        PlayerContractTerms recommended = terms;
        recommended.weeklyWagePence =
            std::max(terms.weeklyWagePence, contract->weeklyWagePence * 110 / 100);
        state_.contractRenewals.push_back({
            "renewal-" + playerId + "-" + state_.currentDate.toIsoString(),
            playerId,
            player->fullName,
            state_.club.manager.name,
            "계약 만료 전 선수단 계획과 임금 구조를 확인해야 합니다.",
            contract->endDate,
            state_.currentDate.addDays(14),
            ContractRenewalStage::Recommended,
            recommended,
            {},
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
            60,
            3,
            0,
            {},
            "gameplay_contract_estimate"
        });
        renewal = std::prev(state_.contractRenewals.end());
    }

    renewal->offerTerms = terms;
    renewal->stage = ContractRenewalStage::AwaitingPlayerResponse;
    renewal->nextActionDate = state_.currentDate.addDays(1);
    renewal->lastUpdateDate = state_.currentDate;
    renewal->decisionDeadline = state_.currentDate.addDays(14);
    ++renewal->negotiationRound;
    renewal->responseNote = "선수와 에이전트에게 재계약 조건을 전달했습니다.";
    resolveContractRenewalInbox(*renewal);
    addFootballInbox(
        "contract-renewal-submitted-" + renewal->id
            + "-" + std::to_string(renewal->negotiationRound),
        renewal->playerName + "에게 재계약을 제안했습니다",
        formatMoney(terms.weeklyWagePence) + " 주급, "
            + std::to_string(terms.contractYears)
            + "년 계약 조건입니다. 선수 측은 다음 날 답변합니다.");
    return {true, "선수에게 재계약 조건을 제안했습니다."};
}

TransferActionResult SimulationEngine::acceptContractRenewalCounter(
    const std::string& renewalId)
{
    const auto renewal = std::find_if(
        state_.contractRenewals.begin(),
        state_.contractRenewals.end(),
        [&renewalId](const ContractRenewalCase& item) {
            return item.id == renewalId;
        });
    if (renewal == state_.contractRenewals.end()
        || renewal->stage != ContractRenewalStage::PlayerCountered
        || !validPlayerContractTerms(renewal->counterTerms)) {
        return {false, "수락할 수 있는 선수 측 재계약 역제안이 없습니다."};
    }

    const auto contract = std::find_if(
        state_.playerContracts.begin(),
        state_.playerContracts.end(),
        [&renewal, this](const PlayerContract& item) {
            return item.playerId == renewal->playerId
                && item.clubId == state_.club.id;
        });
    if (contract == state_.playerContracts.end()) {
        return {false, "기존 선수 계약을 찾을 수 없습니다."};
    }
    if (availableTransferCash(
            state_,
            {},
            {},
            {},
            renewal->id)
        < contractRenewalCommitment(renewal->counterTerms)) {
        return {false, "선수 측 역제안의 보너스를 감당할 가용 현금이 부족합니다."};
    }
    if (state_.club.finance.weeklyWageCommitmentPence
            - homeClubWageShare(
                state_,
                renewal->playerId,
                contract->weeklyWagePence)
            + homeClubWageShare(
                state_,
                renewal->playerId,
                renewal->counterTerms.weeklyWagePence)
        > state_.club.finance.weeklyWageBudgetPence) {
        return {false, "선수 측 역제안을 수락하면 주급 예산을 초과합니다."};
    }

    renewal->offerTerms = renewal->counterTerms;
    renewal->stage = ContractRenewalStage::AgreementReached;
    renewal->nextActionDate.reset();
    renewal->lastUpdateDate = state_.currentDate;
    renewal->responseNote = "선수 측 역제안을 수락해 최종 서명만 남았습니다.";
    resolveContractRenewalInbox(*renewal);
    addFootballInbox(
        "contract-renewal-agreed-" + renewal->id,
        renewal->playerName + " 재계약 조건에 합의했습니다",
        "계약서 요약을 확인한 뒤 구단주 최종 서명을 진행할 수 있습니다.",
        true);
    return {true, "선수 측 재계약 역제안을 수락했습니다."};
}

TransferActionResult SimulationEngine::confirmContractRenewal(
    const std::string& renewalId)
{
    const auto renewal = std::find_if(
        state_.contractRenewals.begin(),
        state_.contractRenewals.end(),
        [&renewalId](const ContractRenewalCase& item) {
            return item.id == renewalId;
        });
    if (renewal == state_.contractRenewals.end()
        || renewal->stage != ContractRenewalStage::AgreementReached
        || !validPlayerContractTerms(renewal->offerTerms)) {
        return {false, "최종 서명할 재계약 합의안을 찾을 수 없습니다."};
    }
    const auto contract = std::find_if(
        state_.playerContracts.begin(),
        state_.playerContracts.end(),
        [&renewal, this](const PlayerContract& item) {
            return item.playerId == renewal->playerId
                && item.clubId == state_.club.id;
        });
    if (contract == state_.playerContracts.end()) {
        return {false, "갱신할 기존 선수 계약을 찾을 수 없습니다."};
    }

    const PlayerContractTerms terms = renewal->offerTerms;
    if (availableTransferCash(
            state_,
            {},
            {},
            {},
            renewal->id) < contractRenewalCommitment(terms)) {
        return {false, "재계약 보너스를 지급할 가용 현금이 부족합니다."};
    }
    const Money nextWageCommitment =
        state_.club.finance.weeklyWageCommitmentPence
        - homeClubWageShare(
            state_,
            renewal->playerId,
            contract->weeklyWagePence)
        + homeClubWageShare(
            state_,
            renewal->playerId,
            terms.weeklyWagePence);
    if (nextWageCommitment > state_.club.finance.weeklyWageBudgetPence) {
        return {false, "재계약 후 주급 예산을 초과합니다."};
    }

    const Money immediateCost =
        terms.signingBonusPence + terms.agentFeePence;
    state_.club.finance.cashPence -= immediateCost;
    state_.club.finance.weeklyWageCommitmentPence =
        nextWageCommitment;

    contract->startDate = state_.currentDate;
    contract->endDate =
        state_.currentDate.addMonths(terms.contractYears * 12);
    contract->weeklyWagePence = terms.weeklyWagePence;
    contract->squadRole = terms.squadRole;
    contract->sourceProposalId = renewal->id;
    contract->terms = terms;
    contract->lifecycleStatus = "active";
    contract->expiryNotified = false;

    addSettledTransferPayment(
        "payment-" + renewal->id + "-signing",
        renewal->id,
        renewal->playerId,
        renewal->playerName,
        TransferPaymentDirection::Payable,
        "renewal_signing_bonus",
        terms.signingBonusPence,
        renewal->playerName + " 재계약 계약금");
    addSettledTransferPayment(
        "payment-" + renewal->id + "-agent",
        renewal->id,
        renewal->playerId,
        renewal->playerName + " 측 에이전트",
        TransferPaymentDirection::Payable,
        "renewal_agent_fee",
        terms.agentFeePence,
        renewal->playerName + " 재계약 에이전트 수수료");
    if (terms.loyaltyBonusPence > 0) {
        state_.transferPayments.push_back({
            "payment-" + renewal->id + "-loyalty",
            renewal->id,
            renewal->playerId,
            renewal->playerName,
            TransferPaymentDirection::Payable,
            "renewal_loyalty_bonus",
            state_.currentDate.addMonths(12),
            terms.loyaltyBonusPence,
            TransferPaymentStatus::Scheduled,
            std::nullopt,
            renewal->playerName + " 재계약 1년차 로열티 보너스"
        });
    }

    renewal->stage = ContractRenewalStage::Completed;
    renewal->signedDate = state_.currentDate;
    renewal->lastUpdateDate = state_.currentDate;
    renewal->nextActionDate.reset();
    renewal->responseNote = "재계약 서명을 완료했습니다.";
    resolveContractRenewalInbox(*renewal);
    addFootballInbox(
        "contract-renewal-completed-" + renewal->id,
        renewal->playerName + " 재계약을 완료했습니다",
        contract->endDate.toIsoString() + "까지 계약을 연장했습니다. 즉시 "
            + formatMoney(immediateCost)
            + "가 지급됐고 새 주급이 임금 예산에 반영됐습니다.");
    return {true, "재계약 최종 서명을 완료했습니다."};
}

TransferActionResult SimulationEngine::withdrawContractRenewal(
    const std::string& renewalId)
{
    const auto renewal = std::find_if(
        state_.contractRenewals.begin(),
        state_.contractRenewals.end(),
        [&renewalId](const ContractRenewalCase& item) {
            return item.id == renewalId;
        });
    if (renewal == state_.contractRenewals.end()
        || isClosedRenewalStage(renewal->stage)) {
        return {false, "철회할 수 있는 재계약 협상을 찾을 수 없습니다."};
    }
    renewal->stage = ContractRenewalStage::Withdrawn;
    renewal->nextActionDate.reset();
    renewal->lastUpdateDate = state_.currentDate;
    renewal->responseNote = "구단이 재계약 협상을 철회했습니다.";
    resolveContractRenewalInbox(*renewal);
    addFootballInbox(
        "contract-renewal-withdrawn-" + renewal->id,
        renewal->playerName + " 재계약 협상을 철회했습니다",
        "기존 계약은 그대로 유지되며 계약 만료 전 다시 협상할 수 있습니다.");
    return {true, "재계약 협상을 철회했습니다."};
}

std::optional<GameDate> SimulationEngine::nextPendingDate() const
{
    std::optional<GameDate> result;

    const auto consider = [&](const GameDate& date) {
        if (date < state_.currentDate) {
            return;
        }
        if (!result.has_value() || date < *result) {
            result = date;
        }
    };

    for (const auto& event : state_.eventQueue) {
        if (!event.processed) {
            consider(event.dueDate);
        }
    }

    for (const auto& fixture : state_.fixtures) {
        if (!fixture.played) {
            consider(fixture.date);
        }
    }

    for (const auto& club : state_.aiClubMarketStates) {
        if (club.clubId != state_.club.id) {
            consider(club.nextEvaluationDate);
        }
    }

    for (const auto& deal : state_.aiMarketDeals) {
        if (deal.stage == AiMarketDealStage::Proposed
            && deal.nextActionDate.has_value()) {
            consider(*deal.nextActionDate);
        } else if (deal.stage == AiMarketDealStage::AwaitingCompletion
            && deal.completionDate.has_value()) {
            consider(*deal.completionDate);
        }
    }

    for (const auto& proposal : state_.transferProposals) {
        if (proposal.status == TransferStatus::Pending) {
            consider(proposal.decisionDeadline.addDays(1));
        } else if (proposal.status == TransferStatus::Approved && proposal.joinDate.has_value()) {
            consider(*proposal.joinDate);
        }
    }

    for (const auto& pursuit : state_.transferPursuits) {
        if (pursuit.stage == TransferPursuitStage::AwaitingClubResponse
            || pursuit.stage == TransferPursuitStage::AwaitingPlayerResponse
            || pursuit.stage == TransferPursuitStage::AwaitingMedical
            || pursuit.stage == TransferPursuitStage::AwaitingRegistration) {
            consider(pursuit.nextActionDate);
        }
        if (pursuit.stage == TransferPursuitStage::AwaitingClubResponse
            || pursuit.stage == TransferPursuitStage::ClubCountered
            || pursuit.stage == TransferPursuitStage::AwaitingPlayerTerms
            || pursuit.stage == TransferPursuitStage::AwaitingPlayerResponse
            || pursuit.stage == TransferPursuitStage::PlayerCountered
            || pursuit.stage == TransferPursuitStage::AwaitingMedical
            || pursuit.stage == TransferPursuitStage::AwaitingRegistration) {
            consider(pursuit.decisionDeadline.addDays(1));
        }
    }

    for (const auto& offer : state_.outgoingTransferOffers) {
        if (offer.status == OutgoingTransferStatus::Pending) {
            consider(offer.decisionDeadline.addDays(1));
        } else if (offer.status == OutgoingTransferStatus::Accepted
            && offer.completionDate.has_value()) {
            consider(*offer.completionDate);
        }
        if (offer.status == OutgoingTransferStatus::Pending
            && offer.negotiationStage == OutgoingNegotiationStage::AwaitingBuyerResponse
            && offer.nextActionDate.has_value()) {
            consider(*offer.nextActionDate);
        }
    }

    for (const auto& payment : state_.transferPayments) {
        if (payment.status == TransferPaymentStatus::Scheduled
            || payment.status == TransferPaymentStatus::Overdue) {
            consider(payment.dueDate);
        }
    }

    for (const auto& assignment : state_.scoutingAssignments) {
        if (assignment.status == ScoutingAssignmentStatus::Active) {
            consider(assignment.nextUpdateDate);
            consider(assignment.dueDate);
        }
    }

    for (const auto& report : state_.scoutingReports) {
        if (!report.staleNotified) {
            consider(report.validUntil);
        }
    }

    for (const auto& plan : state_.recruitmentPlans) {
        if (plan.status == RecruitmentPlanStatus::Active) {
            consider(plan.evaluationDate);
        }
    }

    for (const auto& loan : state_.loanDeals) {
        if (loan.nextActionDate.has_value()
            && (loan.stage == LoanStage::AwaitingClubResponse
                || loan.stage == LoanStage::AwaitingStart
                || loan.stage == LoanStage::RecallPending)) {
            consider(*loan.nextActionDate);
        }
        if (loan.stage == LoanStage::DecisionPending
            || loan.stage == LoanStage::AwaitingClubResponse
            || loan.stage == LoanStage::ClubCountered) {
            consider(loan.decisionDeadline.addDays(1));
        }
        if (loan.stage == LoanStage::Active) {
            consider(loan.terms.endDate);
            consider(
                loan.lastReportDate.value_or(
                    loan.actualStartDate.value_or(state_.currentDate))
                    .addDays(30));
        }
    }

    for (const auto& renewal : state_.contractRenewals) {
        if (renewal.stage == ContractRenewalStage::AwaitingPlayerResponse
            && renewal.nextActionDate.has_value()) {
            consider(*renewal.nextActionDate);
        }
        if (!isClosedRenewalStage(renewal.stage)) {
            consider(renewal.decisionDeadline.addDays(1));
        }
    }

    for (const auto& contract : state_.playerContracts) {
        if (contract.lifecycleStatus == "active"
            && !contract.expiryNotified) {
            consider(contract.endDate.addDays(1));
        }
    }

    return result;
}

void SimulationEngine::processCurrentDate()
{
    processAiTransferMarket();
    processLoanFutureFeeConversions();
    processTransferProposals();
    processTransferPursuits();
    processOutgoingTransferOffers();
    processLoanDeals();
    processContractRenewals();
    processContractExpiries();
    processTransferPayments();
    processScoutingAssignments();
    processScoutingReportStaleness();
    processRecruitmentPlans();
    processScheduledEvents();
    processFixtures();
}

void SimulationEngine::processTransferProposals()
{
    for (auto& proposal : state_.transferProposals) {
        if (proposal.status == TransferStatus::Pending
            && proposal.decisionDeadline < state_.currentDate) {
            proposal.status = TransferStatus::Expired;
            proposal.decisionDate = state_.currentDate;
            resolveTransferRequestInbox(proposal);
            synchronizePursuitWithProposal(proposal);
            addTransferInbox(
                proposal,
                "transfer-expired-",
                proposal.target.fullName + " 영입 승인 기한이 만료되었습니다",
                "기한 안에 결정을 내리지 않아 거래가 종료되었습니다. 예산은 사용되지 않았으며 감독 신뢰는 전체 보강 계획의 달성도에 따라 평가됩니다.");
            continue;
        }

        if (proposal.status != TransferStatus::Approved
            || !proposal.joinDate.has_value()
            || state_.currentDate < *proposal.joinDate) {
            continue;
        }

        const auto duplicate = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&proposal](const Player& player) {
                return player.id == proposal.target.id;
            });
        if (duplicate == state_.players.end()) {
            Player recruitedPlayer = proposal.target;
            recruitedPlayer.squadGroup = "First Team";
            recruitedPlayer.status = "Available";
            recruitedPlayer.joinedDate = proposal.joinDate;
            recruitedPlayer.departedDate.reset();
            state_.players.push_back(std::move(recruitedPlayer));
        }

        const auto contract = std::find_if(
            state_.playerContracts.begin(),
            state_.playerContracts.end(),
            [this, &proposal](const PlayerContract& item) {
                return item.playerId == proposal.target.id
                    && item.clubId == state_.club.id;
            });
        if (contract == state_.playerContracts.end()) {
            const GameDate contractStart = *proposal.joinDate;
            const PlayerContractTerms contractTerms = effectiveContractTerms(
                proposal.weeklyWagePence,
                proposal.contractYears,
                proposal.contractTerms);
            state_.playerContracts.push_back({
                proposal.target.id,
                state_.club.id,
                contractStart,
                contractStart.addMonths(contractTerms.contractYears * 12),
                contractTerms.weeklyWagePence,
                contractTerms.squadRole,
                proposal.id,
                proposal.dataConfidence,
                contractTerms
            });
        }

        proposal.status = TransferStatus::Completed;
        synchronizePursuitWithProposal(proposal);
        addTransferInbox(
            proposal,
            "transfer-completed-",
            proposal.target.fullName + " 선수가 선수단에 합류했습니다",
            "메디컬과 등록을 마치고 " + proposal.contractTerms.squadRole
                + " 계약으로 선수단에 추가되었습니다. 선수단 화면에서 새 영입을 확인할 수 있습니다.");
    }
}

void SimulationEngine::processTransferPursuits()
{
    for (auto& pursuit : state_.transferPursuits) {
        if ((pursuit.stage == TransferPursuitStage::AwaitingClubResponse
                || pursuit.stage == TransferPursuitStage::ClubCountered
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerTerms
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerResponse
                || pursuit.stage == TransferPursuitStage::PlayerCountered
                || pursuit.stage == TransferPursuitStage::AwaitingMedical
                || pursuit.stage == TransferPursuitStage::AwaitingRegistration)
            && pursuit.decisionDeadline < state_.currentDate) {
            pursuit.stage = TransferPursuitStage::Expired;
            pursuit.lastUpdateDate = state_.currentDate;
            pursuit.responseNote = "협상 기한 안에 합의하지 못해 거래가 종료되었습니다.";
            resolveInboxEvent("transfer-pursuit-counter-" + pursuit.id);
            resolveInboxEvent("transfer-player-counter-" + pursuit.id);
            resolveInboxEvent("transfer-pursuit-counter-accepted-" + pursuit.id);
            const TransferCandidate* candidate = findTransferCandidate(pursuit.candidatePlayerId);
            const std::string playerName = candidate != nullptr
                ? candidate->player.fullName
                : pursuit.candidatePlayerId;
            addFootballInbox(
                "transfer-pursuit-expired-" + pursuit.id,
                playerName + " 영입 협상이 만료되었습니다",
                "구단 또는 선수 측과 기한 안에 합의하지 못했습니다. 예산은 차감되지 않았습니다.");
            continue;
        }

        if (pursuit.stage == TransferPursuitStage::AwaitingClubResponse
            && pursuit.nextActionDate <= state_.currentDate) {
            const TransferCandidate* candidate = findTransferCandidate(pursuit.candidatePlayerId);
            if (candidate == nullptr) {
                pursuit.stage = TransferPursuitStage::ClubRejected;
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = "후보 데이터가 없어 협상을 종료했습니다.";
                continue;
            }

            pursuit.offerTerms = effectiveFeeTerms(
                pursuit.currentOfferPence,
                pursuit.offerTerms);
            const Money requiredOffer = std::max(
                candidate->sellerMinimumPence,
                pursuit.rivalBidActive ? pursuit.rivalBidPence : Money {});
            const Money offeredValue = clubNegotiationValue(
                pursuit.offerTerms,
                candidate->marketValuePence);
            if (offeredValue >= requiredOffer) {
                pursuit.stage = TransferPursuitStage::AwaitingPlayerTerms;
                pursuit.nextActionDate = state_.currentDate;
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.playerOfferTerms = recommendedPlayerTerms(*candidate);
                pursuit.playerOfferSubmitted = false;
                pursuit.responseNote = candidate->currentClubName
                    + "이 세부 이적 조건을 수락했습니다. 선수 계약 제안을 작성해야 합니다.";
                std::ostringstream acceptedBody;
                acceptedBody << candidate->currentClubName << "이 보장액 "
                    << formatMoney(guaranteedTransferFee(pursuit.offerTerms))
                    << ", 조건부 "
                    << formatMoney(pursuit.offerTerms.appearanceBonusPence)
                    << ", 다음 이적 수익 "
                    << pursuit.offerTerms.sellOnPercentage
                    << "% 조건을 수락했습니다. 선수 계약 조건을 작성해 제안하십시오.";
                addFootballInbox(
                    "transfer-pursuit-club-accepted-" + pursuit.id,
                    candidate->player.fullName + " 이적료에 잠정 합의했습니다",
                    acceptedBody.str(),
                    true);
                continue;
            }

            if (pursuit.negotiationRound >= 3) {
                pursuit.stage = TransferPursuitStage::ClubRejected;
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = candidate->currentClubName
                    + "이 최종 제안이 요구 수준에 미치지 못한다며 협상을 종료했습니다.";
                addFootballInbox(
                    "transfer-pursuit-club-rejected-" + pursuit.id,
                    candidate->currentClubName + "이 영입 제안을 거절했습니다",
                    "개선 제안도 판매 기준을 충족하지 못해 협상이 결렬됐습니다. 이적료는 지출되지 않았습니다.");
                continue;
            }

            pursuit.clubCounterTerms = sellingClubCounterTerms(*candidate, pursuit);
            pursuit.clubCounterPence =
                guaranteedTransferFee(pursuit.clubCounterTerms);
            pursuit.stage = TransferPursuitStage::ClubCountered;
            pursuit.lastUpdateDate = state_.currentDate;
            pursuit.responseNote = candidate->currentClubName + "이 "
                + formatMoney(pursuit.clubCounterPence) + "를 역제안했습니다.";

            std::ostringstream bodyStream;
            bodyStream << candidate->currentClubName << "이 보장액 "
                << formatMoney(guaranteedTransferFee(pursuit.clubCounterTerms))
                << "(즉시 "
                << formatMoney(pursuit.clubCounterTerms.upfrontFeePence)
                << ", " << pursuit.clubCounterTerms.installmentCount
                << "회 할부 "
                << formatMoney(pursuit.clubCounterTerms.installmentTotalPence)
                << "), 출전 보너스 "
                << formatMoney(pursuit.clubCounterTerms.appearanceBonusPence)
                << ", 다음 이적 수익 "
                << pursuit.clubCounterTerms.sellOnPercentage
                << "%를 요구했습니다. 수락하거나 세부 조건을 수정해 재제안할 수 있습니다.";
            std::string body = bodyStream.str();
            if (pursuit.rivalBidActive && !pursuit.rivalClubName.empty()) {
                body += " " + pursuit.rivalClubName + "도 "
                    + formatMoney(pursuit.rivalBidPence)
                    + " 수준의 거래를 추진하고 있어 시간이 지날수록 협상력이 낮아집니다.";
            }
            addFootballInbox(
                "transfer-pursuit-counter-" + pursuit.id,
                candidate->player.fullName + " 영입에 역제안이 도착했습니다",
                body,
                true);
            continue;
        }

        if (pursuit.stage == TransferPursuitStage::AwaitingPlayerResponse
            && state_.currentDate >= pursuit.nextActionDate) {
            const TransferCandidate* candidate =
                findTransferCandidate(pursuit.candidatePlayerId);
            if (candidate == nullptr) {
                pursuit.stage = TransferPursuitStage::PlayerRejected;
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = "후보 데이터가 없어 선수 계약 협상을 종료했습니다.";
                continue;
            }

            if (playerAcceptsTerms(*candidate, pursuit.playerOfferTerms)) {
                pursuit.stage = TransferPursuitStage::AwaitingMedical;
                pursuit.nextActionDate = state_.currentDate.addDays(1);
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.medicalStatus = "scheduled";
                pursuit.responseNote = "선수와 에이전트가 계약 조건을 수락했습니다. 메디컬이 예약되었습니다.";
                addFootballInbox(
                    "transfer-player-accepted-" + pursuit.id,
                    candidate->player.fullName + " 측과 계약 조건에 합의했습니다",
                    "주급·계약 기간·계약금·보너스·에이전트 수수료 합의가 끝났습니다. 다음 날 메디컬을 진행하며 아직 비용은 지출되지 않았습니다.");
                continue;
            }

            if (pursuit.playerNegotiationRound >= 3) {
                pursuit.stage = TransferPursuitStage::PlayerRejected;
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = "선수 측이 최종 계약 제안도 받아들이지 않아 협상이 결렬됐습니다.";
                addFootballInbox(
                    "transfer-pursuit-player-rejected-" + pursuit.id,
                    candidate->player.fullName + " 측과 계약 합의에 실패했습니다",
                    "선수 또는 에이전트가 최종 역할·계약 조건을 받아들이지 않았습니다. 이적료와 주급 예산은 사용되지 않았습니다.");
                continue;
            }

            pursuit.playerCounterTerms = recommendedPlayerTerms(*candidate);
            if (candidate->playerInterest < 55) {
                pursuit.playerCounterTerms.weeklyWagePence =
                    percentageOf(pursuit.playerCounterTerms.weeklyWagePence, 110);
                pursuit.playerCounterTerms.signingBonusPence =
                    percentageOf(pursuit.playerCounterTerms.signingBonusPence, 110);
                pursuit.playerCounterTerms.agentFeePence =
                    percentageOf(pursuit.playerCounterTerms.agentFeePence, 110);
            }
            pursuit.stage = TransferPursuitStage::PlayerCountered;
            pursuit.lastUpdateDate = state_.currentDate;
            pursuit.responseNote = "선수와 에이전트가 계약 조건을 역제안했습니다.";

            std::ostringstream counterBody;
            counterBody << "선수 측은 주급 "
                << formatMoney(pursuit.playerCounterTerms.weeklyWagePence)
                << ", " << pursuit.playerCounterTerms.contractYears
                << "년, 계약금 "
                << formatMoney(pursuit.playerCounterTerms.signingBonusPence)
                << ", 로열티 보너스 "
                << formatMoney(pursuit.playerCounterTerms.loyaltyBonusPence)
                << ", 에이전트 수수료 "
                << formatMoney(pursuit.playerCounterTerms.agentFeePence)
                << "를 요구했습니다. 수락하거나 계약 조건을 수정해 다시 제안할 수 있습니다.";
            addFootballInbox(
                "transfer-player-counter-" + pursuit.id,
                candidate->player.fullName + " 측의 계약 역제안이 도착했습니다",
                counterBody.str(),
                true);
            continue;
        }

        if (pursuit.stage == TransferPursuitStage::AwaitingMedical
            && state_.currentDate >= pursuit.nextActionDate) {
            const TransferCandidate* candidate =
                findTransferCandidate(pursuit.candidatePlayerId);
            if (candidate == nullptr) {
                pursuit.stage = TransferPursuitStage::MedicalFailed;
                pursuit.medicalStatus = "failed_missing_profile";
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = "의료 검사를 진행할 선수 데이터를 찾을 수 없어 거래를 중단했습니다.";
                continue;
            }

            const bool failed = pursuit.medicalRiskPercent > 0
                && stablePercent(pursuit.id + state_.currentDate.toIsoString())
                    < static_cast<std::uint32_t>(pursuit.medicalRiskPercent);
            if (failed) {
                pursuit.stage = TransferPursuitStage::MedicalFailed;
                pursuit.medicalStatus = "failed";
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = "메디컬에서 계약 진행이 어려운 중대한 위험이 확인됐습니다.";
                addFootballInbox(
                    "transfer-medical-failed-" + pursuit.id,
                    candidate->player.fullName + " 메디컬을 통과하지 못했습니다",
                    "의료진이 중대한 계약 위험을 보고해 거래가 종료됐습니다. 최종 승인 전이므로 이적료·계약금·주급은 지출되지 않았습니다.");
                continue;
            }

            pursuit.medicalStatus = "passed";
            pursuit.stage = TransferPursuitStage::AwaitingRegistration;
            pursuit.nextActionDate = state_.currentDate.addDays(1);
            pursuit.lastUpdateDate = state_.currentDate;
            pursuit.responseNote = "메디컬을 통과했습니다. 1992년 기준 등록 가능 여부를 확인 중입니다.";
            addFootballInbox(
                "transfer-medical-passed-" + pursuit.id,
                candidate->player.fullName + " 선수가 메디컬을 통과했습니다",
                "의료진이 계약 진행 가능 판정을 내렸습니다. 다음 날 당시 규정에 따른 등록 가능 여부와 재정 조건을 확인합니다.");
            continue;
        }

        if (pursuit.stage == TransferPursuitStage::AwaitingRegistration
            && state_.currentDate >= pursuit.nextActionDate) {
            const TransferCandidate* candidate =
                findTransferCandidate(pursuit.candidatePlayerId);
            const bool registrationOpen =
                domesticRegistrationOpen(state_, state_.currentDate);
            const bool eligible = registrationOpen
                && candidate != nullptr
                && !candidate->currentClubId.empty()
                && !candidate->player.nationality.empty();
            if (!eligible) {
                pursuit.stage = TransferPursuitStage::RegistrationFailed;
                pursuit.registrationStatus = registrationOpen
                    ? "failed"
                    : "closed_1992_cutoff";
                pursuit.lastUpdateDate = state_.currentDate;
                pursuit.responseNote = registrationOpen
                    ? "당시 등록에 필요한 소속·신원 자료를 확인하지 못해 거래가 중단됐습니다."
                    : "1992-93 잉글랜드 등록 마감일인 3월 31일이 지나 거래가 중단됐습니다.";
                addFootballInbox(
                    "transfer-registration-failed-" + pursuit.id,
                    pursuit.candidatePlayerId + " 등록 확인에 실패했습니다",
                    registrationOpen
                        ? "1992년 당시 규정에 필요한 등록 자료가 부족해 거래를 완료할 수 없습니다. 비용은 지출되지 않았습니다."
                        : "3월 31일 등록 마감 뒤에는 새 선수를 등록할 수 없습니다. 최종 승인 전이므로 비용은 지출되지 않았습니다.");
                continue;
            }

            pursuit.registrationStatus = "eligible_1992_domestic";
            pursuit.lastUpdateDate = state_.currentDate;
            createFinalTransferProposal(pursuit, *candidate);
            continue;
        }
    }
}

void SimulationEngine::processOutgoingTransferOffers()
{
    for (auto& offer : state_.outgoingTransferOffers) {
        if (offer.status == OutgoingTransferStatus::Pending
            && offer.decisionDeadline < state_.currentDate) {
            offer.status = OutgoingTransferStatus::Expired;
            offer.negotiationStage = OutgoingNegotiationStage::Closed;
            offer.decisionDate = state_.currentDate;
            resolveInboxEvent("outgoing-offer-" + offer.id);
            addFootballInbox(
                "outgoing-offer-expired-" + offer.id,
                offer.playerName + " 판매 제안이 만료되었습니다",
                offer.buyerClubName + "이 제안을 철회했습니다. 선수단과 재정에는 변화가 없습니다.");
            continue;
        }

        if (offer.status == OutgoingTransferStatus::Pending
            && offer.negotiationStage == OutgoingNegotiationStage::AwaitingBuyerResponse
            && offer.nextActionDate.has_value()
            && *offer.nextActionDate <= state_.currentDate) {
            offer.feeTerms = effectiveFeeTerms(offer.feePence, offer.feeTerms);
            const Money buyerMaximum = offer.buyerMaximumValuePence > 0
                ? offer.buyerMaximumValuePence
                : percentageOf(offer.feePence, 125);
            const Money requestedValue =
                clubNegotiationValue(offer.feeTerms, buyerMaximum);
            if (requestedValue <= buyerMaximum) {
                offer.negotiationStage = OutgoingNegotiationStage::AgreementReached;
                offer.nextActionDate.reset();
                addFootballInbox(
                    "outgoing-counter-agreed-" + offer.id,
                    offer.buyerClubName + "이 " + offer.playerName + " 판매 조건을 수락했습니다",
                    "구매 구단이 보장액 " + formatMoney(guaranteedTransferFee(offer.feeTerms))
                        + "와 세부 조항을 수락했습니다. 판매 승인 또는 거절을 최종 결정하십시오.",
                    true);
                continue;
            }

            if (offer.negotiationRound >= 3) {
                offer.status = OutgoingTransferStatus::Rejected;
                offer.negotiationStage = OutgoingNegotiationStage::Closed;
                offer.decisionDate = state_.currentDate;
                offer.nextActionDate.reset();
                addFootballInbox(
                    "outgoing-counter-rejected-" + offer.id,
                    offer.buyerClubName + "이 " + offer.playerName + " 협상에서 철수했습니다",
                    "세 차례 제안에도 조건 차이를 좁히지 못했습니다. 선수단과 재정에는 변화가 없습니다.");
                continue;
            }

            const Money upfront = percentageOf(buyerMaximum, 75);
            offer.buyerCounterTerms = {
                upfront,
                buyerMaximum - upfront,
                1,
                percentageOf(buyerMaximum, 5),
                25,
                5
            };
            offer.negotiationStage = OutgoingNegotiationStage::BuyerCountered;
            offer.nextActionDate.reset();

            std::ostringstream counterBody;
            counterBody << offer.buyerClubName << "은 보장액 "
                << formatMoney(guaranteedTransferFee(offer.buyerCounterTerms))
                << "(즉시 "
                << formatMoney(offer.buyerCounterTerms.upfrontFeePence)
                << ", 할부 "
                << formatMoney(offer.buyerCounterTerms.installmentTotalPence)
                << "), 출전 보너스 "
                << formatMoney(offer.buyerCounterTerms.appearanceBonusPence)
                << ", 다음 이적 수익 "
                << offer.buyerCounterTerms.sellOnPercentage
                << "%를 제안했습니다.";
            addFootballInbox(
                "outgoing-buyer-counter-" + offer.id,
                offer.buyerClubName + "의 판매 역제안이 도착했습니다",
                counterBody.str(),
                true);
            continue;
        }

        if (offer.status != OutgoingTransferStatus::Accepted
            || !offer.completionDate.has_value()
            || state_.currentDate < *offer.completionDate) {
            continue;
        }

        const auto player = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&offer](const Player& item) {
                return item.id == offer.playerId;
            });
        if (player != state_.players.end()) {
            player->departedDate = offer.completionDate;
            state_.players.erase(player);
        }

        const auto contract = std::find_if(
            state_.playerContracts.begin(),
            state_.playerContracts.end(),
            [&offer](const PlayerContract& item) {
                return item.playerId == offer.playerId;
            });
        std::string sourceProposalId;
        if (contract != state_.playerContracts.end()) {
            sourceProposalId = contract->sourceProposalId;
            state_.club.finance.weeklyWageCommitmentPence = std::max<Money>(
                0,
                state_.club.finance.weeklyWageCommitmentPence - contract->weeklyWagePence);
            state_.playerContracts.erase(contract);
        } else if (offer.weeklyWageReliefPence > 0) {
            state_.club.finance.weeklyWageCommitmentPence = std::max<Money>(
                0,
                state_.club.finance.weeklyWageCommitmentPence - offer.weeklyWageReliefPence);
        }

        offer.feeTerms = effectiveFeeTerms(offer.feePence, offer.feeTerms);
        const Money guaranteedFee = guaranteedTransferFee(offer.feeTerms);
        Money sellOnObligation = 0;
        if (!sourceProposalId.empty()) {
            const auto originalProposal = std::find_if(
                state_.transferProposals.begin(),
                state_.transferProposals.end(),
                [&sourceProposalId](const TransferProposal& proposal) {
                    return proposal.id == sourceProposalId;
                });
            if (originalProposal != state_.transferProposals.end()) {
                const TransferFeeTerms originalTerms = effectiveFeeTerms(
                    originalProposal->feePence,
                    originalProposal->feeTerms);
                sellOnObligation = percentageOf(
                    guaranteedFee,
                    originalTerms.sellOnPercentage);
                if (sellOnObligation > 0) {
                    const std::string paymentId =
                        "payment-" + offer.id + "-sell-on";
                    const auto duplicatePayment = std::find_if(
                        state_.transferPayments.begin(),
                        state_.transferPayments.end(),
                        [&paymentId](const TransferPayment& payment) {
                            return payment.id == paymentId;
                        });
                    if (duplicatePayment == state_.transferPayments.end()) {
                        state_.transferPayments.push_back({
                            paymentId,
                            offer.id,
                            originalProposal->fromClubId,
                            originalProposal->fromClubName,
                            TransferPaymentDirection::Payable,
                            "sell_on",
                            state_.currentDate,
                            sellOnObligation,
                            TransferPaymentStatus::Scheduled,
                            std::nullopt,
                            offer.playerName + " 다음 이적 수익 조항"
                        });
                    }
                }
            }
        }
        state_.club.finance.cashPence += offer.feeTerms.upfrontFeePence;
        state_.club.finance.transferBudgetPence +=
            std::max<Money>(0, guaranteedFee - sellOnObligation);
        addSettledTransferPayment(
            "payment-" + offer.id + "-upfront-receipt",
            offer.id,
            offer.buyerClubId,
            offer.buyerClubName,
            TransferPaymentDirection::Receivable,
            "upfront_fee",
            offer.feeTerms.upfrontFeePence,
            offer.playerName + " 판매 즉시 수령액");
        scheduleTransferInstallments(
            "payment-" + offer.id + "-installment-receipt",
            offer.id,
            offer.buyerClubId,
            offer.buyerClubName,
            TransferPaymentDirection::Receivable,
            offer.feeTerms.installmentTotalPence,
            offer.feeTerms.installmentCount,
            *offer.completionDate);
        offer.status = OutgoingTransferStatus::Completed;
        offer.negotiationStage = OutgoingNegotiationStage::Closed;

        addFootballInbox(
            "outgoing-offer-completed-" + offer.id,
            offer.playerName + " 선수가 " + offer.buyerClubName + "에 합류했습니다",
            "완전 이적이 완료되어 선수단에서 제외됐습니다. 이적 예산에는 보장액 "
                + formatMoney(guaranteedFee - sellOnObligation)
                + ", 현금에는 즉시 수령액 "
                + formatMoney(offer.feeTerms.upfrontFeePence)
                + "가 반영됐으며 남은 할부는 각 지급일에 정산됩니다.");
    }
}

void SimulationEngine::processTransferPayments()
{
    for (auto& payment : state_.transferPayments) {
        if ((payment.status != TransferPaymentStatus::Scheduled
                && payment.status != TransferPaymentStatus::Overdue)
            || payment.dueDate > state_.currentDate) {
            continue;
        }

        if (payment.direction == TransferPaymentDirection::Receivable) {
            state_.club.finance.cashPence += payment.amountPence;
            payment.status = TransferPaymentStatus::Settled;
            payment.settledDate = state_.currentDate;
            addFootballInbox(
                "transfer-payment-received-" + payment.id,
                payment.counterpartyName + "에서 이적료 할부가 입금됐습니다",
                payment.description + " " + formatMoney(payment.amountPence)
                    + "가 구단 현금에 반영됐습니다.");
            continue;
        }

        if (state_.club.finance.cashPence < payment.amountPence) {
            if (payment.status != TransferPaymentStatus::Overdue) {
                payment.status = TransferPaymentStatus::Overdue;
                addFootballInbox(
                    "transfer-payment-overdue-" + payment.id,
                    payment.counterpartyName + " 이적료 지급이 연체됐습니다",
                    payment.description + " " + formatMoney(payment.amountPence)
                        + "를 지급할 현금이 부족합니다. 현금이 확보되면 우선 정산됩니다.");
            }
            continue;
        }

        state_.club.finance.cashPence -= payment.amountPence;
        payment.status = TransferPaymentStatus::Settled;
        payment.settledDate = state_.currentDate;
        addFootballInbox(
            "transfer-payment-paid-" + payment.id,
            payment.counterpartyName + "에 이적료 할부를 지급했습니다",
            payment.description + " " + formatMoney(payment.amountPence)
                + "가 구단 현금에서 지급됐습니다.");
    }
}

void SimulationEngine::processScoutingAssignments()
{
    for (auto& assignment : state_.scoutingAssignments) {
        if (assignment.status != ScoutingAssignmentStatus::Active) {
            continue;
        }
        const TransferCandidate* candidate =
            findTransferCandidate(assignment.playerId);
        const auto scout = std::find_if(
            state_.scouts.begin(),
            state_.scouts.end(),
            [&assignment](const ScoutProfile& item) {
                return item.id == assignment.scoutId;
            });
        if (candidate == nullptr || scout == state_.scouts.end()) {
            assignment.status = ScoutingAssignmentStatus::Cancelled;
            assignment.completedDate = state_.currentDate;
            for (auto& directive : state_.scoutingDirectives) {
                if (directive.generatedAssignmentId == assignment.id
                    && directive.status == ScoutingDirectiveStatus::Active) {
                    directive.status = ScoutingDirectiveStatus::Cancelled;
                    directive.completedDate = state_.currentDate;
                    directive.summary += " 관찰 대상 또는 담당자를 찾지 못해 지시가 취소됐습니다.";
                }
            }
            continue;
        }

        const auto updateReport = [&](const int knowledge) {
            auto report = std::find_if(
                state_.scoutingReports.begin(),
                state_.scoutingReports.end(),
                [&assignment](const ScoutingReport& item) {
                    return item.playerId == assignment.playerId;
                });
            const int nextVersion = report != state_.scoutingReports.end()
                ? report->reportVersion + 1
                : 1;
            ScoutingReport updated = makeScoutingReport(
                *candidate,
                *scout,
                knowledge,
                assignment.focus,
                state_.currentDate,
                nextVersion);
            if (report == state_.scoutingReports.end()) {
                state_.scoutingReports.push_back(std::move(updated));
            } else {
                *report = std::move(updated);
            }
        };

        if (state_.currentDate >= assignment.dueDate) {
            updateReport(assignment.targetKnowledge);
            assignment.lastMilestone = 100;
            assignment.status = ScoutingAssignmentStatus::Completed;
            assignment.completedDate = state_.currentDate;
            const int familiarityGain = assignment.depth == ScoutingDepth::Quick
                ? 2
                : (assignment.depth == ScoutingDepth::Standard ? 4 : 6);
            recordScoutFamiliarity(
                state_,
                scout->id,
                "country",
                candidate->player.nationality,
                familiarityGain,
                state_.currentDate,
                "scouting_assignment_completed");
            recordScoutFamiliarity(
                state_,
                scout->id,
                "region",
                scoutingRegionKeyForCandidate(*candidate),
                std::max(1, familiarityGain - 1),
                state_.currentDate,
                "scouting_assignment_completed");
            recordScoutFamiliarity(
                state_,
                scout->id,
                "position_group",
                scoutingPositionGroup(candidate->player.position),
                familiarityGain,
                state_.currentDate,
                "scouting_assignment_completed");
            recordScoutFamiliarity(
                state_,
                scout->id,
                "age_band",
                scoutingAgeBand(candidate->player, state_.currentDate),
                std::max(1, familiarityGain - 1),
                state_.currentDate,
                "scouting_assignment_completed");
            recordScoutFamiliarity(
                state_,
                scout->id,
                "market",
                "domestic",
                std::max(1, familiarityGain - 1),
                state_.currentDate,
                "scouting_assignment_completed");
            for (auto& directive : state_.scoutingDirectives) {
                if (directive.generatedAssignmentId != assignment.id
                    || directive.status != ScoutingDirectiveStatus::Active) {
                    continue;
                }
                directive.status = ScoutingDirectiveStatus::Completed;
                directive.completedDate = state_.currentDate;
                directive.summary += " 최종 보고서가 도착해 지시를 완료 처리했습니다.";
                if (!directive.style.empty() && directive.style != "any") {
                    recordScoutFamiliarity(
                        state_,
                        scout->id,
                        "style",
                        directive.style,
                        std::max(1, familiarityGain - 2),
                        state_.currentDate,
                        "scouting_directive_completed");
                }
            }
            addFootballInbox(
                "scouting-final-" + assignment.id,
                candidate->player.fullName + " 최종 스카우팅 보고서",
                scout->fullName + "의 관찰이 완료됐습니다. 지식 수준 "
                    + std::to_string(assignment.targetKnowledge)
                    + "/100으로 능력·잠재력·가치·계약 추정 범위가 갱신됐습니다.");
            continue;
        }

        if (assignment.lastMilestone == 0
            && state_.currentDate >= assignment.nextUpdateDate) {
            const int intermediateKnowledge =
                assignment.startingKnowledge
                + (assignment.targetKnowledge - assignment.startingKnowledge) / 2;
            updateReport(intermediateKnowledge);
            assignment.lastMilestone = 50;
            assignment.nextUpdateDate = assignment.dueDate;
            addFootballInbox(
                "scouting-interim-" + assignment.id,
                candidate->player.fullName + " 중간 스카우팅 보고서",
                scout->fullName + "이 중간 관찰 결과를 제출했습니다. 현재 지식 수준은 "
                    + std::to_string(intermediateKnowledge)
                    + "/100이며 최종 보고 전까지 일부 평가는 넓은 범위로 표시됩니다.");
        }
    }
}

void SimulationEngine::processScoutingReportStaleness()
{
    for (auto& report : state_.scoutingReports) {
        if (report.staleNotified || state_.currentDate < report.validUntil) {
            continue;
        }
        report.staleNotified = true;
        const TransferCandidate* candidate =
            findTransferCandidate(report.playerId);
        const std::string playerName = candidate != nullptr
            ? candidate->player.fullName
            : report.playerId;
        addFootballInbox(
            "scouting-stale-" + report.playerId
                + "-" + std::to_string(report.reportVersion),
            playerName + " 스카우팅 보고서가 오래됐습니다",
            "마지막 관찰 후 30일이 지나 시장 가치·요구 조건의 신뢰도가 낮아졌습니다. 새 배정을 통해 보고서를 갱신할 수 있습니다.");
    }
}

void SimulationEngine::processRecruitmentPlans()
{
    for (auto& plan : state_.recruitmentPlans) {
        if (plan.status != RecruitmentPlanStatus::Active
            || state_.currentDate < plan.evaluationDate) {
            continue;
        }

        const int completed = recruitmentPlanCompletedSignings(state_, plan);
        const int priorityScore = recruitmentPlanPriorityScore(state_, plan);
        if (completed >= plan.targetSignings) {
            plan.status = RecruitmentPlanStatus::Achieved;
            plan.confidenceDelta = 2;
        } else if (completed >= plan.minimumAcceptableSignings) {
            plan.status = RecruitmentPlanStatus::Acceptable;
            plan.confidenceDelta = 0;
        } else if (completed > 0) {
            plan.status = RecruitmentPlanStatus::UnderDelivered;
            plan.confidenceDelta = priorityScore >= 75 ? -1 : -2;
        } else {
            plan.status = RecruitmentPlanStatus::Failed;
            plan.confidenceDelta = -4;
        }
        plan.evaluatedDate = state_.currentDate;
        state_.club.manager.boardConfidence = std::clamp(
            state_.club.manager.boardConfidence + plan.confidenceDelta,
            0,
            100);

        std::ostringstream body;
        body << "목표 " << plan.targetSignings
             << "명 중 " << completed
             << "명을 영입했고 감독 우선순위 점수는 "
             << priorityScore << "점입니다. ";
        if (plan.status == RecruitmentPlanStatus::Achieved) {
            body << "보강 목표를 모두 달성해 감독 신뢰가 2 상승했습니다.";
        } else if (plan.status == RecruitmentPlanStatus::Acceptable) {
            body << "일부 후보를 놓쳤지만 최소 기대치를 충족해 감독 신뢰는 하락하지 않습니다.";
        } else if (plan.status == RecruitmentPlanStatus::UnderDelivered) {
            body << "일부 보강은 인정됐지만 최소 기대치에는 미치지 못해 감독 신뢰가 "
                 << -plan.confidenceDelta << " 하락했습니다.";
        } else {
            body << "보강이 이뤄지지 않아 감독 신뢰가 4 하락했습니다.";
        }
        addFootballInbox(
            "recruitment-plan-evaluated-" + plan.id,
            plan.title + " 평가가 완료됐습니다",
            body.str());
    }
}

void SimulationEngine::processLoanDeals()
{
    for (auto& loan : state_.loanDeals) {
        if ((loan.stage == LoanStage::DecisionPending
                || loan.stage == LoanStage::AwaitingClubResponse
                || loan.stage == LoanStage::ClubCountered)
            && loan.decisionDeadline < state_.currentDate) {
            loan.stage = LoanStage::Expired;
            loan.nextActionDate.reset();
            resolveLoanInbox(loan);
            addFootballInbox(
                "loan-expired-" + loan.id,
                loan.player.fullName + " 임대 협상 기한이 만료되었습니다",
                "선수 등록과 구단 재정에는 변화가 없습니다.");
            continue;
        }

        if (loan.stage == LoanStage::AwaitingClubResponse
            && loan.nextActionDate.has_value()
            && state_.currentDate >= *loan.nextActionDate) {
            bool accepted = false;
            bool rejected = false;
            if (loan.direction == LoanDirection::Incoming) {
                const TransferCandidate* candidate =
                    findTransferCandidate(loan.player.id);
                if (candidate == nullptr) {
                    rejected = true;
                } else {
                    const LoanTerms recommended =
                        recommendedIncomingLoanTerms(
                            *candidate,
                            state_.currentDate);
                    const Money offeredValue =
                        loanProposalValue(loan.terms, *candidate);
                    const Money expectedValue =
                        loanProposalValue(recommended, *candidate);
                    accepted = offeredValue >= expectedValue
                        && loan.terms.borrowingClubWagePercentage >= 75;
                    rejected = !accepted
                        && (loan.negotiationRound >= 3
                            || offeredValue < expectedValue * 45 / 100);
                    loan.counterTerms = recommended;
                }
            } else {
                const LoanTerms baseline = loan.counterTerms;
                accepted =
                    loan.terms.loanFeePence <= baseline.loanFeePence * 3 / 2
                    && loan.terms.borrowingClubWagePercentage
                        >= std::max(
                            0,
                            baseline.borrowingClubWagePercentage - 10);
                rejected = !accepted && loan.negotiationRound >= 3;
                loan.counterTerms = baseline;
            }

            if (accepted) {
                loan.stage = LoanStage::AwaitingStart;
                loan.plannedStartDate = state_.currentDate.addDays(3);
                loan.nextActionDate = loan.plannedStartDate;
                loan.responseNote =
                    "상대 구단이 세부 임대 조건을 수락했습니다.";
                addFootballInbox(
                    "loan-club-accepted-" + loan.id,
                    loan.player.fullName + " 임대 조건에 합의했습니다",
                    loan.plannedStartDate.toIsoString()
                        + "에 임대가 시작되고 그날 재정과 선수단이 갱신됩니다.");
            } else if (rejected) {
                loan.stage = LoanStage::Rejected;
                loan.nextActionDate.reset();
                loan.responseNote = "상대 구단이 임대 협상을 종료했습니다.";
                addFootballInbox(
                    "loan-club-rejected-" + loan.id,
                    loan.player.fullName + " 임대 협상이 결렬되었습니다",
                    "조건 차이가 커 상대 구단이 협상을 종료했습니다. 재정 변화는 없습니다.");
            } else {
                loan.stage = LoanStage::ClubCountered;
                loan.nextActionDate.reset();
                loan.responseNote =
                    "상대 구단이 임대료와 주급 부담 조건을 역제안했습니다.";
                addFootballInbox(
                    "loan-counter-" + loan.id,
                    loan.player.fullName + " 임대 역제안이 도착했습니다",
                    "임대료 "
                        + formatMoney(loan.counterTerms.loanFeePence)
                        + ", 차입 구단 주급 부담 "
                        + std::to_string(
                            loan.counterTerms.borrowingClubWagePercentage)
                        + "% 조건입니다. 수락·재협상·거절할 수 있습니다.",
                    true);
            }
            continue;
        }

        if (loan.stage == LoanStage::AwaitingStart
            && loan.nextActionDate.has_value()
            && state_.currentDate >= *loan.nextActionDate) {
            completeLoanStart(loan);
            continue;
        }

        if (loan.stage == LoanStage::RecallPending
            && loan.nextActionDate.has_value()
            && state_.currentDate >= *loan.nextActionDate) {
            completeLoanEnd(loan, true);
            continue;
        }

        if (loan.stage != LoanStage::Active) {
            continue;
        }
        if (state_.currentDate >= loan.terms.endDate) {
            completeLoanEnd(loan, false);
            continue;
        }

        const GameDate reportBase = loan.lastReportDate.value_or(
            loan.actualStartDate.value_or(state_.currentDate));
        if (state_.currentDate < reportBase.addDays(30)) {
            continue;
        }

        int appearances = 2;
        int starts = 0;
        if (loan.terms.promisedRole == "Regular Starter") {
            appearances = 5;
            starts = 4;
        } else if (loan.terms.promisedRole == "Squad Player") {
            appearances = 3;
            starts = 1;
        }
        loan.appearances += appearances;
        loan.starts += starts;
        loan.lastReportDate = state_.currentDate;

        const bool promiseConcern =
            loan.terms.promisedRole == "Regular Starter"
            && starts < 3;
        addFootballInbox(
            "loan-report-" + loan.id + "-"
                + state_.currentDate.toIsoString(),
            loan.player.fullName + " 월간 임대 보고서",
            "이번 기간 " + std::to_string(appearances)
                + "경기 출전, " + std::to_string(starts)
                + "경기 선발입니다. 누적 "
                + std::to_string(loan.appearances)
                + "경기이며 "
                + (promiseConcern
                    ? "약속한 역할보다 출전이 부족해 임대 구단과 면담이 필요합니다."
                    : "현재까지 약속한 출전 경로를 대체로 지키고 있습니다."));
    }
}

void SimulationEngine::processContractRenewals()
{
    for (auto& renewal : state_.contractRenewals) {
        if (!isClosedRenewalStage(renewal.stage)
            && renewal.decisionDeadline < state_.currentDate) {
            renewal.stage = ContractRenewalStage::Expired;
            renewal.nextActionDate.reset();
            renewal.lastUpdateDate = state_.currentDate;
            renewal.responseNote = "재계약 협상 기한이 만료되었습니다.";
            resolveContractRenewalInbox(renewal);
            addFootballInbox(
                "contract-renewal-expired-" + renewal.id,
                renewal.playerName + " 재계약 협상 기한이 만료되었습니다",
                "기존 계약은 유지되지만 만료가 가까워질수록 선수 측 요구와 이탈 위험이 커질 수 있습니다.");
            continue;
        }

        if (renewal.stage != ContractRenewalStage::AwaitingPlayerResponse
            || !renewal.nextActionDate.has_value()
            || state_.currentDate < *renewal.nextActionDate) {
            continue;
        }

        renewal.lastUpdateDate = state_.currentDate;
        renewal.nextActionDate.reset();
        if (renewalOfferAccepted(renewal, renewal.offerTerms)) {
            renewal.stage = ContractRenewalStage::AgreementReached;
            renewal.responseNote =
                "선수 측이 제안을 수락해 최종 서명만 남았습니다.";
            addFootballInbox(
                "contract-renewal-agreed-" + renewal.id,
                renewal.playerName + " 측이 재계약 제안을 수락했습니다",
                "계약서 요약을 확인한 뒤 구단주 최종 서명을 진행할 수 있습니다.",
                true);
            continue;
        }

        const Money offerValue =
            contractOfferWeeklyValue(renewal.offerTerms);
        const Money expectedValue =
            contractOfferWeeklyValue(renewal.recommendedTerms);
        if (renewal.negotiationRound >= renewal.agentPatience
            || offerValue < expectedValue * 60 / 100) {
            renewal.stage = ContractRenewalStage::Rejected;
            renewal.responseNote =
                "조건 차이가 커 선수 측이 재계약 협상을 종료했습니다.";
            addFootballInbox(
                "contract-renewal-rejected-" + renewal.id,
                renewal.playerName + " 측이 재계약 협상을 종료했습니다",
                "기존 계약은 유지됩니다. 일정 기간 뒤 더 나은 조건으로 다시 접근할 수 있습니다.");
            continue;
        }

        renewal.counterTerms = renewal.recommendedTerms;
        if (renewal.negotiationRound >= 2) {
            renewal.counterTerms.weeklyWagePence =
                renewal.counterTerms.weeklyWagePence * 105 / 100;
            renewal.counterTerms.agentFeePence =
                renewal.counterTerms.agentFeePence * 110 / 100;
        }
        renewal.stage = ContractRenewalStage::PlayerCountered;
        renewal.responseNote =
            "선수 측이 임금·보너스·선수단 지위를 역제안했습니다.";
        addFootballInbox(
            "contract-renewal-counter-" + renewal.id,
            renewal.playerName + " 재계약 역제안이 도착했습니다",
            "주급 "
                + formatMoney(renewal.counterTerms.weeklyWagePence)
                + ", " + std::to_string(
                    renewal.counterTerms.contractYears)
                + "년 조건입니다. 수락하거나 조건을 고쳐 다시 제안할 수 있습니다.",
            true);
    }
}

void SimulationEngine::processContractExpiries()
{
    for (auto& contract : state_.playerContracts) {
        if (contract.lifecycleStatus != "active"
            || contract.expiryNotified
            || state_.currentDate <= contract.endDate) {
            continue;
        }

        const bool preBosman =
            !state_.eraRules.freeMovementAfterContract;
        contract.lifecycleStatus = preBosman
            ? "expired_retained"
            : "expired_free_agent";
        contract.expiryNotified = true;

        const auto player = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&contract](const Player& item) {
                return item.id == contract.playerId;
            });
        const std::string playerName = player != state_.players.end()
            ? player->fullName
            : contract.playerId;
        if (player != state_.players.end()) {
            player->status = preBosman
                ? "Contract Expired · Registration Retained"
                : "Contract Expired";
        }

        addFootballInbox(
            "contract-expired-" + contract.playerId,
            playerName + " 계약이 만료되었습니다",
            preBosman
                ? "1992년의 보스만 판결 이전 규칙 경로가 적용되어 선수가 즉시 자유계약으로 이탈하지 않고 구단 등록 권리가 유지됩니다. 재계약 또는 이적 보상 협상이 필요합니다."
                : "보스만 판결 이후 규칙 경로에 따라 자유계약 이탈 절차가 시작됩니다. 자유계약 등록 전환 처리가 연결되기 전까지 상태만 보존됩니다.");

        const auto renewal = std::find_if(
            state_.contractRenewals.begin(),
            state_.contractRenewals.end(),
            [&contract](const ContractRenewalCase& item) {
                return item.playerId == contract.playerId
                    && !isClosedRenewalStage(item.stage);
            });
        if (renewal != state_.contractRenewals.end()) {
            renewal->stage = ContractRenewalStage::Expired;
            renewal->nextActionDate.reset();
            renewal->lastUpdateDate = state_.currentDate;
            resolveContractRenewalInbox(*renewal);
        }
    }
}

void SimulationEngine::processScheduledEvents()
{
    for (auto& event : state_.eventQueue) {
        if (!event.processed && event.dueDate <= state_.currentDate) {
            addInboxFromEvent(event);
            event.processed = true;
        }
    }
}

void SimulationEngine::processFixtures()
{
    bool playedFixture = false;
    for (auto& fixture : state_.fixtures) {
        if (!fixture.played && fixture.date <= state_.currentDate) {
            simulateFixture(fixture);
            addMatchInbox(fixture);
            playedFixture = true;
        }
    }
    if (playedFixture) {
        recalculateLeagueStandings(state_);
    }
}

void SimulationEngine::simulateFixture(Fixture& fixture)
{
    const double strengthDifference = static_cast<double>(fixture.homeStrength - fixture.awayStrength);
    const double homeExpectedGoals = std::clamp(1.25 + strengthDifference * 0.025, 0.25, 3.25);
    const double awayExpectedGoals = std::clamp(1.05 - strengthDifference * 0.025, 0.20, 3.00);

    std::seed_seq seed {
        state_.rngSeed,
        static_cast<std::uint32_t>(fixture.date.year),
        static_cast<std::uint32_t>(fixture.date.month),
        static_cast<std::uint32_t>(fixture.date.day),
        static_cast<std::uint32_t>(fixture.homeStrength),
        static_cast<std::uint32_t>(fixture.awayStrength)
    };
    std::mt19937 random(seed);
    std::poisson_distribution<int> homeGoals(homeExpectedGoals);
    std::poisson_distribution<int> awayGoals(awayExpectedGoals);

    fixture.homeGoals = std::min(homeGoals(random), 7);
    fixture.awayGoals = std::min(awayGoals(random), 7);
    fixture.played = true;

    state_.rngSeed = random();
}

void SimulationEngine::addInboxFromEvent(const ScheduledEvent& event)
{
    const auto duplicate = std::find_if(state_.inbox.begin(), state_.inbox.end(), [&event](const InboxItem& item) {
        return item.eventKey == event.key;
    });
    if (duplicate != state_.inbox.end()) {
        return;
    }

    state_.inbox.push_back({
        nextInboxId(),
        event.key,
        state_.currentDate,
        event.category,
        event.title,
        event.body,
        true,
        event.requiresDecision
    });
}

void SimulationEngine::addMatchInbox(const Fixture& fixture)
{
    const std::string eventKey = "fixture-result-" + fixture.id;
    const auto duplicate = std::find_if(state_.inbox.begin(), state_.inbox.end(), [&eventKey](const InboxItem& item) {
        return item.eventKey == eventKey;
    });
    if (duplicate != state_.inbox.end()) {
        return;
    }

    std::ostringstream title;
    title << fixture.homeClub << ' ' << fixture.homeGoals << "–" << fixture.awayGoals << ' ' << fixture.awayClub;

    std::string body = "동일한 결과 엔진에서 생성된 경기 결과입니다. ";
    if (fixture.homeClub == state_.club.name || fixture.awayClub == state_.club.name) {
        const bool playerClubAtHome = fixture.homeClub == state_.club.name;
        const int playerGoals = playerClubAtHome ? fixture.homeGoals : fixture.awayGoals;
        const int opponentGoals = playerClubAtHome ? fixture.awayGoals : fixture.homeGoals;
        body += playerGoals > opponentGoals ? "승리" : (playerGoals == opponentGoals ? "무승부" : "패배");
        body += "로 기록되었으며, 다음 단계에서 슈팅·점유·선수 기여 사건을 확장합니다.";
    } else {
        body += "리그 전체 일정의 다른 구단 경기로 기록되었으며, 순위표 계산에 반영됩니다.";
    }

    state_.inbox.push_back({
        nextInboxId(),
        eventKey,
        state_.currentDate,
        InboxCategory::Match,
        title.str(),
        body,
        true,
        false
    });
}

void SimulationEngine::addTransferInbox(
    const TransferProposal& proposal,
    const std::string& eventPrefix,
    const std::string& title,
    const std::string& body)
{
    const std::string eventKey = eventPrefix + proposal.id;
    const auto duplicate = std::find_if(
        state_.inbox.begin(),
        state_.inbox.end(),
        [&eventKey](const InboxItem& item) {
            return item.eventKey == eventKey;
        });
    if (duplicate != state_.inbox.end()) {
        return;
    }

    state_.inbox.push_back({
        nextInboxId(),
        eventKey,
        state_.currentDate,
        InboxCategory::Football,
        title,
        body,
        true,
        false
    });
}

void SimulationEngine::addFootballInbox(
    const std::string& eventKey,
    const std::string& title,
    const std::string& body,
    const bool requiresDecision)
{
    const auto duplicate = std::find_if(
        state_.inbox.begin(),
        state_.inbox.end(),
        [&eventKey](const InboxItem& item) {
            return item.eventKey == eventKey;
        });
    if (duplicate != state_.inbox.end()) {
        duplicate->date = state_.currentDate;
        duplicate->title = title;
        duplicate->body = body;
        duplicate->unread = true;
        duplicate->requiresDecision = requiresDecision;
        return;
    }

    state_.inbox.push_back({
        nextInboxId(),
        eventKey,
        state_.currentDate,
        InboxCategory::Football,
        title,
        body,
        true,
        requiresDecision
    });
}

void SimulationEngine::resolveTransferRequestInbox(const TransferProposal& proposal)
{
    resolveInboxEvent("transfer-proposal-" + proposal.id);
}

void SimulationEngine::resolveInboxEvent(const std::string& eventKey)
{
    const auto request = std::find_if(
        state_.inbox.begin(),
        state_.inbox.end(),
        [&eventKey](const InboxItem& item) {
            return item.eventKey == eventKey;
        });
    if (request == state_.inbox.end()) {
        return;
    }
    request->requiresDecision = false;
    request->unread = false;
}

void SimulationEngine::createFinalTransferProposal(
    TransferPursuit& pursuit,
    const TransferCandidate& candidate)
{
    const std::string proposalId = "negotiated-" + pursuit.id;
    const TransferFeeTerms feeTerms = effectiveFeeTerms(
        pursuit.currentOfferPence,
        pursuit.offerTerms);
    const PlayerContractTerms contractTerms = validPlayerContractTerms(
        pursuit.playerOfferTerms)
        ? pursuit.playerOfferTerms
        : recommendedPlayerTerms(candidate);

    auto proposal = std::find_if(
        state_.transferProposals.begin(),
        state_.transferProposals.end(),
        [&proposalId](const TransferProposal& item) {
            return item.id == proposalId;
        });
    if (proposal == state_.transferProposals.end()) {
        state_.transferProposals.push_back({
            proposalId,
            candidate.player,
            candidate.currentClubId,
            candidate.currentClubName,
            guaranteedTransferFee(feeTerms),
            contractTerms.weeklyWagePence,
            contractTerms.contractYears,
            "Alex Ferguson 및 구단 실무진",
            candidate.roleFit + " · " + candidate.scoutSummary,
            state_.currentDate,
            state_.currentDate.addDays(pursuit.rivalBidActive ? 3 : 5),
            std::nullopt,
            std::nullopt,
            TransferStatus::Pending,
            candidate.dataConfidence,
            feeTerms,
            contractTerms,
            pursuit.medicalStatus,
            pursuit.registrationStatus
        });
    } else {
        proposal->feePence = guaranteedTransferFee(feeTerms);
        proposal->weeklyWagePence = contractTerms.weeklyWagePence;
        proposal->contractYears = contractTerms.contractYears;
        proposal->feeTerms = feeTerms;
        proposal->contractTerms = contractTerms;
        proposal->medicalStatus = pursuit.medicalStatus;
        proposal->registrationStatus = pursuit.registrationStatus;
    }

    pursuit.stage = TransferPursuitStage::AwaitingOwnerApproval;
    pursuit.lastUpdateDate = state_.currentDate;
    pursuit.linkedProposalId = proposalId;
    pursuit.responseNote = "구단·선수 합의, 메디컬과 등록 확인이 끝나 구단주 최종 승인을 기다립니다.";

    const Money immediateCash =
        immediateTransferCashCost(feeTerms, contractTerms);
    const Money maximumCommitment =
        transferBudgetCommitment(feeTerms, contractTerms);
    std::ostringstream body;
    body << candidate.currentClubName << "과 보장액 "
         << formatMoney(guaranteedTransferFee(feeTerms))
         << "(즉시 " << formatMoney(feeTerms.upfrontFeePence)
         << ", 할부 " << formatMoney(feeTerms.installmentTotalPence)
         << "), 최대 이적료 " << formatMoney(maximumTransferFee(feeTerms))
         << "에 합의했습니다. 선수 측 조건은 주급 "
         << formatMoney(contractTerms.weeklyWagePence)
         << ", " << contractTerms.contractYears
         << "년, 계약금 " << formatMoney(contractTerms.signingBonusPence)
         << ", 에이전트 수수료 " << formatMoney(contractTerms.agentFeePence)
         << "입니다. 승인 즉시 현금 "
         << formatMoney(immediateCash)
         << ", 이적 예산 최대 "
         << formatMoney(maximumCommitment)
         << "가 반영됩니다.";
    addFootballInbox(
        "transfer-proposal-" + proposalId,
        candidate.player.fullName + " 영입의 최종 승인이 필요합니다",
        body.str(),
        true);
}

void SimulationEngine::addSettledTransferPayment(
    const std::string& id,
    const std::string& sourceId,
    const std::string& counterpartyClubId,
    const std::string& counterpartyName,
    const TransferPaymentDirection direction,
    const std::string& category,
    const Money amountPence,
    const std::string& description)
{
    if (amountPence <= 0) {
        return;
    }
    const auto duplicate = std::find_if(
        state_.transferPayments.begin(),
        state_.transferPayments.end(),
        [&id](const TransferPayment& payment) {
            return payment.id == id;
        });
    if (duplicate != state_.transferPayments.end()) {
        return;
    }
    state_.transferPayments.push_back({
        id,
        sourceId,
        counterpartyClubId,
        counterpartyName,
        direction,
        category,
        state_.currentDate,
        amountPence,
        TransferPaymentStatus::Settled,
        state_.currentDate,
        description
    });
}

void SimulationEngine::scheduleTransferInstallments(
    const std::string& idPrefix,
    const std::string& sourceId,
    const std::string& counterpartyClubId,
    const std::string& counterpartyName,
    const TransferPaymentDirection direction,
    const Money installmentTotalPence,
    const int installmentCount,
    const GameDate& startDate)
{
    if (installmentTotalPence <= 0 || installmentCount <= 0) {
        return;
    }

    Money scheduled = 0;
    for (int index = 1; index <= installmentCount; ++index) {
        const std::string id = idPrefix + "-" + std::to_string(index);
        const auto duplicate = std::find_if(
            state_.transferPayments.begin(),
            state_.transferPayments.end(),
            [&id](const TransferPayment& payment) {
                return payment.id == id;
            });
        if (duplicate != state_.transferPayments.end()) {
            continue;
        }

        const Money amount = index == installmentCount
            ? installmentTotalPence - scheduled
            : installmentTotalPence / installmentCount;
        scheduled += amount;
        state_.transferPayments.push_back({
            id,
            sourceId,
            counterpartyClubId,
            counterpartyName,
            direction,
            "installment",
            startDate.addMonths(index * 6),
            amount,
            TransferPaymentStatus::Scheduled,
            std::nullopt,
            std::to_string(index) + "/" + std::to_string(installmentCount)
                + "회 이적료 할부"
        });
    }
}

void SimulationEngine::completeLoanStart(LoanDeal& loan)
{
    Money weeklyWagePence = 0;
    const auto contract = std::find_if(
        state_.playerContracts.begin(),
        state_.playerContracts.end(),
        [&loan](const PlayerContract& item) {
            return item.playerId == loan.player.id;
        });
    if (contract != state_.playerContracts.end()) {
        weeklyWagePence = contract->weeklyWagePence;
    } else if (const TransferCandidate* candidate =
            findTransferCandidate(loan.player.id);
        candidate != nullptr) {
        weeklyWagePence = candidate->expectedWeeklyWagePence;
    }
    const Money wageShare =
        loanWeeklyWageShare(weeklyWagePence, loan.terms);

    if (!domesticRegistrationOpen(state_, state_.currentDate)) {
        loan.stage = LoanStage::Rejected;
        loan.nextActionDate.reset();
        loan.responseNote =
            "1992-93 잉글랜드 등록 마감일인 3월 31일이 지나 임대 등록이 취소되었습니다.";
        addFootballInbox(
            "loan-start-failed-" + loan.id,
            loan.player.fullName + " 임대 등록이 취소되었습니다",
            "3월 31일 등록 마감 뒤에는 새 임대를 등록할 수 없습니다. 임대료는 지급되거나 수령되지 않았습니다.");
        return;
    }

    if (loan.direction == LoanDirection::Incoming
        && (availableTransferCash(state_, {}, {}, loan.id)
                < loanFinancialCommitment(loan.terms)
            || availableTransferBudget(state_)
                < loanFinancialCommitment(loan.terms)
            || state_.club.finance.weeklyWageCommitmentPence + wageShare
                > state_.club.finance.weeklyWageBudgetPence)) {
        loan.stage = LoanStage::Rejected;
        loan.nextActionDate.reset();
        loan.responseNote =
            "합류일에 가용 현금·이적 예산 또는 주급 예산이 부족해 등록이 취소되었습니다.";
        addFootballInbox(
            "loan-start-failed-" + loan.id,
            loan.player.fullName + " 임대 등록이 취소되었습니다",
            "합의 뒤 다른 약정이 반영되어 합류일에 재정 한도를 충족하지 못했습니다. 임대료는 지급되지 않았습니다.");
        return;
    }

    if (loan.direction == LoanDirection::Outgoing) {
        const auto registeredPlayer = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&loan](const Player& item) {
                return item.id == loan.player.id;
            });
        if (registeredPlayer == state_.players.end()) {
            loan.stage = LoanStage::Rejected;
            loan.nextActionDate.reset();
            loan.responseNote =
                "선수가 더 이상 우리 선수단에 없어 임대 등록이 취소되었습니다.";
            addFootballInbox(
                "loan-start-failed-" + loan.id,
                loan.player.fullName + " 임대 등록이 취소되었습니다",
                "합류일 전에 선수 등록 상태가 바뀌어 임대를 진행할 수 없습니다.");
            return;
        }
        state_.club.finance.cashPence += loan.terms.loanFeePence;
        state_.club.finance.transferBudgetPence +=
            loan.terms.loanFeePence;
        state_.club.finance.weeklyWageCommitmentPence =
            std::max<Money>(
                0,
                state_.club.finance.weeklyWageCommitmentPence
                    - wageShare);

        const auto player = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&loan](const Player& item) {
                return item.id == loan.player.id;
            });
        if (player != state_.players.end()) {
            if (loan.parentSquadGroup.empty()) {
                loan.parentSquadGroup = player->squadGroup;
            }
            player->squadGroup = "Loan";
            player->status = "On Loan";
        }
        addSettledTransferPayment(
            "payment-" + loan.id + "-loan-fee",
            loan.id,
            loan.loanClubId,
            loan.loanClubName,
            TransferPaymentDirection::Receivable,
            "loan_fee",
            loan.terms.loanFeePence,
            loan.player.fullName + " 임대료 수입");
    } else {
        state_.club.finance.cashPence -= loan.terms.loanFeePence;
        state_.club.finance.transferBudgetPence -=
            loanFinancialCommitment(loan.terms);
        state_.club.finance.weeklyWageCommitmentPence += wageShare;

        const auto duplicate = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&loan](const Player& item) {
                return item.id == loan.player.id;
            });
        if (duplicate == state_.players.end()) {
            Player incoming = loan.player;
            incoming.squadGroup =
                loan.terms.promisedRole == "Regular Starter"
                ? "First Team"
                : "Reserve";
            incoming.status = "Loaned In";
            incoming.joinedDate = state_.currentDate;
            incoming.departedDate.reset();
            state_.players.push_back(std::move(incoming));
        }
        addSettledTransferPayment(
            "payment-" + loan.id + "-loan-fee",
            loan.id,
            loan.parentClubId,
            loan.parentClubName,
            TransferPaymentDirection::Payable,
            "loan_fee",
            loan.terms.loanFeePence,
            loan.player.fullName + " 임대료 지출");
    }

    loan.stage = LoanStage::Active;
    loan.actualStartDate = state_.currentDate;
    loan.lastReportDate = state_.currentDate;
    loan.nextActionDate.reset();
    loan.responseNote = "임대 등록과 선수 이동이 완료되었습니다.";

    addFootballInbox(
        "loan-active-" + loan.id,
        loan.player.fullName + " 임대가 시작되었습니다",
        loan.terms.endDate.toIsoString()
            + "까지 임대됩니다. 임대료 "
            + formatMoney(loan.terms.loanFeePence)
            + "와 주급 분담이 재정에 반영됐습니다.");
}

void SimulationEngine::completeLoanEnd(
    LoanDeal& loan,
    const bool recalled)
{
    Money weeklyWagePence = 0;
    const auto contract = std::find_if(
        state_.playerContracts.begin(),
        state_.playerContracts.end(),
        [&loan](const PlayerContract& item) {
            return item.playerId == loan.player.id;
        });
    if (contract != state_.playerContracts.end()) {
        weeklyWagePence = contract->weeklyWagePence;
    } else if (const TransferCandidate* candidate =
            findTransferCandidate(loan.player.id);
        candidate != nullptr) {
        weeklyWagePence = candidate->expectedWeeklyWagePence;
    }
    const Money wageShare =
        loanWeeklyWageShare(weeklyWagePence, loan.terms);

    if (loan.direction == LoanDirection::Outgoing) {
        state_.club.finance.weeklyWageCommitmentPence += wageShare;
        const auto player = std::find_if(
            state_.players.begin(),
            state_.players.end(),
            [&loan](const Player& item) {
                return item.id == loan.player.id;
            });
        if (player != state_.players.end()) {
            player->squadGroup = loan.parentSquadGroup.empty()
                ? "Reserve"
                : loan.parentSquadGroup;
            player->status = "Available";
        }
    } else {
        state_.club.finance.weeklyWageCommitmentPence =
            std::max<Money>(
                0,
                state_.club.finance.weeklyWageCommitmentPence
                    - wageShare);
        state_.players.erase(
            std::remove_if(
                state_.players.begin(),
                state_.players.end(),
                [&loan](const Player& player) {
                    return player.id == loan.player.id;
                }),
            state_.players.end());
    }

    loan.stage = recalled ? LoanStage::Recalled : LoanStage::Completed;
    loan.actualEndDate = state_.currentDate;
    loan.nextActionDate.reset();
    loan.responseNote = recalled
        ? "리콜 통지 기간이 끝나 조기 복귀했습니다."
        : "합의된 임대 기간이 끝났습니다.";

    std::string followUp =
        "누적 " + std::to_string(loan.appearances)
        + "경기, 선발 " + std::to_string(loan.starts)
        + "경기로 임대를 마쳤습니다.";
    if (loan.terms.futureFeeType != "none") {
        followUp += " 완전 영입 "
            + std::string(
                loan.terms.futureFeeType == "mandatory"
                ? "의무"
                : "선택")
            + " 조항은 종료일에 이미 검토됐고, 미행사 또는 재정 한도 미충족으로 일반 종료 처리됐습니다.";
    }
    addFootballInbox(
        "loan-ended-" + loan.id,
        loan.player.fullName
            + (recalled ? " 조기 복귀가 완료되었습니다" : " 임대가 종료되었습니다"),
        followUp);
}

void SimulationEngine::resolveLoanInbox(const LoanDeal& loan)
{
    resolveInboxEvent("loan-offer-" + loan.id);
    resolveInboxEvent("loan-counter-" + loan.id);
    resolveInboxEvent("loan-counter-agreed-" + loan.id);
}

void SimulationEngine::resolveContractRenewalInbox(
    const ContractRenewalCase& renewal)
{
    resolveInboxEvent(
        "contract-renewal-recommendation-" + renewal.id);
    resolveInboxEvent("contract-renewal-counter-" + renewal.id);
    resolveInboxEvent("contract-renewal-agreed-" + renewal.id);
}

void SimulationEngine::synchronizePursuitWithProposal(const TransferProposal& proposal)
{
    const auto pursuit = std::find_if(
        state_.transferPursuits.begin(),
        state_.transferPursuits.end(),
        [&proposal](const TransferPursuit& item) {
            return item.linkedProposalId == proposal.id;
        });
    if (pursuit == state_.transferPursuits.end()) {
        return;
    }

    pursuit->lastUpdateDate = state_.currentDate;
    switch (proposal.status) {
    case TransferStatus::Pending:
        pursuit->stage = TransferPursuitStage::AwaitingOwnerApproval;
        pursuit->responseNote = "구단주 최종 승인을 기다립니다.";
        break;
    case TransferStatus::Approved:
        pursuit->stage = TransferPursuitStage::AwaitingCompletion;
        pursuit->responseNote = "구단주 승인이 완료되어 선수 합류를 기다립니다.";
        break;
    case TransferStatus::Rejected:
        pursuit->stage = TransferPursuitStage::OwnerRejected;
        pursuit->responseNote = "구단주가 최종 영입 안건을 거절했습니다.";
        break;
    case TransferStatus::Completed:
        pursuit->stage = TransferPursuitStage::Completed;
        pursuit->responseNote = "선수 등록과 합류가 완료되었습니다.";
        break;
    case TransferStatus::Expired:
        pursuit->stage = TransferPursuitStage::Expired;
        pursuit->responseNote = "최종 승인 기한이 지나 거래가 종료되었습니다.";
        break;
    }
}

const TransferCandidate* SimulationEngine::findTransferCandidate(const std::string& playerId) const
{
    const auto candidate = std::find_if(
        state_.transferCandidates.begin(),
        state_.transferCandidates.end(),
        [&playerId](const TransferCandidate& item) {
            return item.player.id == playerId;
        });
    return candidate == state_.transferCandidates.end() ? nullptr : &*candidate;
}

int SimulationEngine::nextInboxId() const
{
    int result = 0;
    for (const auto& item : state_.inbox) {
        result = std::max(result, item.id);
    }
    return result + 1;
}

} // namespace football_owner
