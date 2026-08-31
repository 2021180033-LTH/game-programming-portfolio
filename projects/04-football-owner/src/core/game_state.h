#pragma once

#include "core/game_date.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace football_owner {

using Money = std::int64_t;

enum class InboxCategory {
    Board,
    Football,
    Finance,
    Match,
    System
};

struct InboxItem {
    int id {};
    std::string eventKey;
    GameDate date;
    InboxCategory category {InboxCategory::System};
    std::string title;
    std::string body;
    bool unread {true};
    bool requiresDecision {false};
};

struct ScheduledEvent {
    int id {};
    std::string key;
    GameDate dueDate;
    InboxCategory category {InboxCategory::System};
    std::string title;
    std::string body;
    bool requiresDecision {false};
    bool processed {false};
};

struct Player {
    Player() = default;

    Player(
        std::string idValue,
        std::string fullNameValue,
        std::optional<GameDate> birthDateValue,
        std::string nationalityValue,
        std::string positionValue,
        std::string squadGroupValue,
        std::string statusValue,
        std::string dataConfidenceValue,
        std::optional<GameDate> joinedDateValue = std::nullopt,
        std::optional<GameDate> departedDateValue = std::nullopt)
        : id(std::move(idValue))
        , fullName(std::move(fullNameValue))
        , birthDate(std::move(birthDateValue))
        , nationality(std::move(nationalityValue))
        , position(std::move(positionValue))
        , squadGroup(std::move(squadGroupValue))
        , status(std::move(statusValue))
        , dataConfidence(std::move(dataConfidenceValue))
        , joinedDate(std::move(joinedDateValue))
        , departedDate(std::move(departedDateValue))
    {
    }

    std::string id;
    std::string fullName;
    std::optional<GameDate> birthDate;
    std::string nationality;
    std::string position;
    std::string squadGroup;
    std::string status;
    std::string dataConfidence;
    std::optional<GameDate> joinedDate;
    std::optional<GameDate> departedDate;
};

enum class TransferStatus {
    Pending,
    Approved,
    Rejected,
    Completed,
    Expired
};

struct TransferFeeTerms {
    Money upfrontFeePence {};
    Money installmentTotalPence {};
    int installmentCount {};
    Money appearanceBonusPence {};
    int appearanceThreshold {};
    int sellOnPercentage {};
};

struct PlayerContractTerms {
    Money weeklyWagePence {};
    int contractYears {};
    Money signingBonusPence {};
    Money loyaltyBonusPence {};
    Money appearanceFeePence {};
    Money goalBonusPence {};
    Money agentFeePence {};
    Money releaseClausePence {};
    std::string squadRole {"First Team"};
};

struct TransferProposal {
    std::string id;
    Player target;
    std::string fromClubId;
    std::string fromClubName;
    Money feePence {};
    Money weeklyWagePence {};
    int contractYears {};
    std::string requestedBy;
    std::string rationale;
    GameDate proposedDate;
    GameDate decisionDeadline;
    std::optional<GameDate> decisionDate;
    std::optional<GameDate> joinDate;
    TransferStatus status {TransferStatus::Pending};
    std::string dataConfidence;
    TransferFeeTerms feeTerms;
    PlayerContractTerms contractTerms;
    std::string medicalStatus {"not_checked"};
    std::string registrationStatus {"not_checked"};
};

enum class TransferPursuitStage {
    AwaitingClubResponse,
    ClubCountered,
    AwaitingPlayerTerms,
    AwaitingPlayerResponse,
    PlayerCountered,
    AwaitingMedical,
    AwaitingRegistration,
    AwaitingOwnerApproval,
    AwaitingCompletion,
    ClubRejected,
    PlayerRejected,
    MedicalFailed,
    RegistrationFailed,
    OwnerRejected,
    Withdrawn,
    Completed,
    Expired
};

struct TransferCandidate {
    Player player;
    std::string currentClubId;
    std::string currentClubName;
    int recommendationRank {};
    int currentAbilityLow {};
    int currentAbilityHigh {};
    int potentialLow {};
    int potentialHigh {};
    int managerPriority {};
    int playerInterest {};
    Money marketValuePence {};
    Money openingOfferPence {};
    Money sellerMinimumPence {};
    Money askingPricePence {};
    Money expectedWeeklyWagePence {};
    int expectedContractYears {};
    std::string roleFit;
    std::string scoutSummary;
    std::string availability;
    std::string rivalClubId;
    std::string rivalClubName;
    Money rivalBidPence {};
    std::string dataConfidence;
};

enum class ScoutingAssignmentStatus {
    Active,
    Completed,
    Cancelled
};

enum class ScoutingDepth {
    Quick,
    Standard,
    Comprehensive
};

enum class ScoutingDirectiveType {
    Range,
    Target
};

enum class ScoutingDirectiveStatus {
    Active,
    Completed,
    Cancelled
};

struct ScoutProfile {
    std::string id;
    std::string fullName;
    std::string role;
    std::string regionSpecialism;
    int judgingCurrentAbility {};
    int judgingPotential {};
    int adaptability {};
    int assignmentCapacity {1};
    std::string dataConfidence;
};

struct ScoutingReport {
    std::string playerId;
    int knowledgeLevel {};
    int currentAbilityLow {};
    int currentAbilityHigh {};
    int potentialLow {};
    int potentialHigh {};
    Money marketValueLowPence {};
    Money marketValueHighPence {};
    Money askingPriceLowPence {};
    Money askingPriceHighPence {};
    Money expectedWeeklyWageLowPence {};
    Money expectedWeeklyWageHighPence {};
    std::string roleFit;
    std::string summary;
    std::string availability;
    GameDate lastObservedDate;
    GameDate validUntil;
    std::string scoutId;
    std::string focus {"overall"};
    int reportVersion {1};
    bool staleNotified {false};
};

struct ScoutingAssignment {
    std::string id;
    std::string playerId;
    std::string scoutId;
    ScoutingDepth depth {ScoutingDepth::Standard};
    std::string focus {"overall"};
    GameDate startedDate;
    GameDate nextUpdateDate;
    GameDate dueDate;
    ScoutingAssignmentStatus status {ScoutingAssignmentStatus::Active};
    int startingKnowledge {};
    int targetKnowledge {};
    int lastMilestone {};
    std::optional<GameDate> completedDate;
};

struct ScoutFamiliarity {
    std::string scoutId;
    std::string category;
    std::string key;
    int level {};
    GameDate updatedDate;
    std::string dataConfidence;
};

struct ScoutingDirective {
    std::string id;
    ScoutingDirectiveType type {ScoutingDirectiveType::Range};
    ScoutingDirectiveStatus status {ScoutingDirectiveStatus::Active};
    std::string scoutId;
    ScoutingDepth depth {ScoutingDepth::Standard};
    std::string focus {"overall"};
    std::string targetPlayerId;
    std::string generatedAssignmentId;
    std::string region;
    std::string country;
    std::string league;
    std::string position;
    std::string style;
    int minAge {};
    int maxAge {};
    bool homegrownOnly {false};
    Money maxBudgetPence {};
    int matchedCandidateCount {};
    std::string matchedPlayerIds;
    int recommendationScore {};
    int estimatedDurationDays {};
    int accuracyModifier {};
    GameDate createdDate;
    GameDate dueDate;
    std::optional<GameDate> completedDate;
    std::string summary;
    std::string dataConfidence;
};

struct TransferPursuit {
    std::string id;
    std::string candidatePlayerId;
    TransferPursuitStage stage {TransferPursuitStage::AwaitingClubResponse};
    Money currentOfferPence {};
    Money clubCounterPence {};
    int negotiationRound {};
    GameDate startedDate;
    GameDate nextActionDate;
    GameDate decisionDeadline;
    std::optional<GameDate> lastUpdateDate;
    std::string responseNote;
    bool rivalBidActive {false};
    std::string rivalClubId;
    std::string rivalClubName;
    Money rivalBidPence {};
    std::string linkedProposalId;
    TransferFeeTerms offerTerms;
    TransferFeeTerms clubCounterTerms;
    PlayerContractTerms playerOfferTerms;
    PlayerContractTerms playerCounterTerms;
    int playerNegotiationRound {};
    bool playerOfferSubmitted {false};
    int medicalRiskPercent {};
    std::string medicalStatus {"not_started"};
    std::string registrationStatus {"not_checked"};
};

enum class OutgoingTransferStatus {
    Pending,
    Accepted,
    Rejected,
    Completed,
    Expired
};

enum class OutgoingNegotiationStage {
    DecisionPending,
    AwaitingBuyerResponse,
    BuyerCountered,
    AgreementReached,
    Closed
};

struct OutgoingTransferOffer {
    std::string id;
    std::string playerId;
    std::string playerName;
    std::string buyerClubId;
    std::string buyerClubName;
    Money feePence {};
    Money weeklyWageReliefPence {};
    GameDate proposedDate;
    GameDate decisionDeadline;
    std::optional<GameDate> decisionDate;
    std::optional<GameDate> completionDate;
    OutgoingTransferStatus status {OutgoingTransferStatus::Pending};
    std::string requestedRole;
    std::string rationale;
    std::string dataConfidence;
    TransferFeeTerms feeTerms;
    TransferFeeTerms buyerCounterTerms;
    OutgoingNegotiationStage negotiationStage {OutgoingNegotiationStage::DecisionPending};
    int negotiationRound {};
    std::optional<GameDate> nextActionDate;
    Money buyerMaximumValuePence {};
};

struct PlayerContract {
    std::string playerId;
    std::string clubId;
    GameDate startDate;
    GameDate endDate;
    Money weeklyWagePence {};
    std::string squadRole;
    std::string sourceProposalId;
    std::string dataConfidence;
    PlayerContractTerms terms;
    std::string lifecycleStatus {"active"};
    bool expiryNotified {false};
};

enum class LoanDirection {
    Incoming,
    Outgoing
};

enum class LoanStage {
    DecisionPending,
    AwaitingClubResponse,
    ClubCountered,
    AwaitingStart,
    Active,
    RecallPending,
    Recalled,
    Completed,
    Rejected,
    Withdrawn,
    Expired
};

struct LoanTerms {
    Money loanFeePence {};
    int borrowingClubWagePercentage {100};
    std::string promisedRole {"Regular Starter"};
    GameDate endDate;
    bool canRecall {false};
    int recallNoticeDays {7};
    std::string futureFeeType {"none"};
    Money futureFeePence {};
    bool parentClubMatchRestricted {true};
    Money unusedPlayerFeePence {};
    std::string treatmentResponsibility {"loan_club"};
};

struct LoanDeal {
    std::string id;
    Player player;
    LoanDirection direction {LoanDirection::Outgoing};
    std::string parentClubId;
    std::string parentClubName;
    std::string loanClubId;
    std::string loanClubName;
    std::string purpose;
    GameDate proposedDate;
    GameDate decisionDeadline;
    GameDate plannedStartDate;
    LoanStage stage {LoanStage::DecisionPending};
    LoanTerms terms;
    LoanTerms counterTerms;
    std::optional<GameDate> nextActionDate;
    std::optional<GameDate> actualStartDate;
    std::optional<GameDate> actualEndDate;
    std::optional<GameDate> lastReportDate;
    std::string parentSquadGroup;
    int negotiationRound {};
    int appearances {};
    int starts {};
    std::string responseNote;
    std::string dataConfidence;
};

enum class ContractRenewalStage {
    Recommended,
    AwaitingPlayerResponse,
    PlayerCountered,
    AgreementReached,
    Completed,
    Rejected,
    Withdrawn,
    Expired
};

struct ContractRenewalCase {
    std::string id;
    std::string playerId;
    std::string playerName;
    std::string requestedBy;
    std::string rationale;
    GameDate currentContractEndDate;
    GameDate decisionDeadline;
    ContractRenewalStage stage {ContractRenewalStage::Recommended};
    PlayerContractTerms recommendedTerms;
    PlayerContractTerms offerTerms;
    PlayerContractTerms counterTerms;
    std::optional<GameDate> nextActionDate;
    std::optional<GameDate> lastUpdateDate;
    std::optional<GameDate> signedDate;
    int playerInterest {60};
    int agentPatience {3};
    int negotiationRound {};
    std::string responseNote;
    std::string dataConfidence;
};

enum class TransferPaymentDirection {
    Payable,
    Receivable
};

enum class TransferPaymentStatus {
    Scheduled,
    Settled,
    Overdue,
    Cancelled
};

struct TransferPayment {
    std::string id;
    std::string sourceId;
    std::string counterpartyClubId;
    std::string counterpartyName;
    TransferPaymentDirection direction {TransferPaymentDirection::Payable};
    std::string category;
    GameDate dueDate;
    Money amountPence {};
    TransferPaymentStatus status {TransferPaymentStatus::Scheduled};
    std::optional<GameDate> settledDate;
    std::string description;
};

struct Fixture {
    std::string id;
    GameDate date;
    std::string competition;
    std::string homeClub;
    std::string awayClub;
    int homeStrength {50};
    int awayStrength {50};
    bool played {false};
    int homeGoals {};
    int awayGoals {};
};

struct CompetitionSeasonTeam {
    std::string seasonId;
    std::string competition;
    std::string clubId;
    std::string clubName;
    std::string sourceLockedClubId;
    int seedOrder {};
};

struct LeagueStandingRow {
    std::string seasonId;
    std::string competition;
    std::string clubId;
    std::string clubName;
    int position {};
    int played {};
    int won {};
    int drawn {};
    int lost {};
    int goalsFor {};
    int goalsAgainst {};
    int goalDifference {};
    int points {};
};

struct ManagerState {
    std::string name;
    std::string role;
    int boardConfidence {50};
    std::string tacticalIdentity;
    std::string currentAssessment;
};

struct FinanceState {
    Money cashPence {};
    Money transferBudgetPence {};
    Money weeklyWageBudgetPence {};
    Money weeklyWageCommitmentPence {};
    Money debtPence {};
    std::string confidence;
};

struct ClubState {
    std::string id;
    std::string name;
    std::string shortName;
    std::string leagueName;
    int reputation {50};
    int squadStrength {50};
    FinanceState finance;
    ManagerState manager;
};

enum class AiMarketDealType {
    Transfer,
    Loan,
    Renewal,
    Buyout,
    LoanOption,
    LoanObligation
};

enum class AiMarketDealStage {
    Proposed,
    AwaitingCompletion,
    Completed,
    Rejected,
    Expired
};

enum class SquadPlanningOpinionSource {
    Owner,
    Manager,
    Director
};

enum class SquadPlanningOpinionType {
    Sale,
    Release,
    Reinforce,
    Loan,
    Keep,
    Monitor
};

enum class SquadPlanningOpinionStatus {
    Active,
    Resolved,
    Dismissed,
    Superseded
};

enum class OperatingMeetingCadence {
    Off,
    UrgentOnly,
    Monthly,
    Quarterly,
    TransferWindows,
    SeasonMilestones,
    Custom
};

enum class OperatingMeetingAgendaType {
    General,
    SquadPlanning,
    TransferMarket,
    Finance,
    Staff,
    Scouting,
    Crisis
};

enum class OperatingMeetingAttendeeMode {
    Auto,
    Manual
};

enum class OperatingMeetingStatus {
    Scheduled,
    Held,
    Cancelled
};

struct AiClubMarketState {
    std::string clubId;
    std::string clubName;
    FinanceState finance;
    int riskTolerance {50};
    std::string transferNeed {"balanced"};
    std::optional<GameDate> lastEvaluationDate;
    GameDate nextEvaluationDate;
    std::uint32_t marketSeed {};
    std::string dataConfidence {"gameplay_estimate_current_dataset"};
};

struct AiMarketDeal {
    std::string id;
    AiMarketDealType type {AiMarketDealType::Transfer};
    AiMarketDealStage stage {AiMarketDealStage::Proposed};
    Player player;
    std::string sellingClubId;
    std::string sellingClubName;
    std::string buyingClubId;
    std::string buyingClubName;
    GameDate proposedDate;
    GameDate decisionDeadline;
    std::optional<GameDate> nextActionDate;
    std::optional<GameDate> completionDate;
    std::optional<GameDate> completedDate;
    TransferFeeTerms feeTerms;
    PlayerContractTerms contractTerms;
    LoanTerms loanTerms;
    std::string sourceLoanId;
    std::string sourceContractId;
    std::string rationale;
    bool playerClubInvolved {false};
    bool budgetReserved {false};
    bool completionApplied {false};
    std::string dataConfidence {"gameplay_estimate_current_dataset"};
};

struct SquadPlanningOpinion {
    std::string id;
    std::string seasonId;
    std::string clubId;
    std::optional<std::string> playerId;
    std::optional<std::string> positionGroup;
    SquadPlanningOpinionSource source {SquadPlanningOpinionSource::Owner};
    SquadPlanningOpinionType type {SquadPlanningOpinionType::Monitor};
    int priority {3};
    std::string reason;
    std::string targetWindow {"current"};
    SquadPlanningOpinionStatus status {SquadPlanningOpinionStatus::Active};
    GameDate createdDate;
    GameDate updatedDate;
    std::string dataConfidence {"player_entered"};
};

struct OperatingMeetingSettings {
    OperatingMeetingCadence cadence {OperatingMeetingCadence::Off};
    OperatingMeetingAttendeeMode attendeeMode {OperatingMeetingAttendeeMode::Auto};
    bool includeManager {true};
    bool includeDirector {true};
    bool includeFinance {true};
    bool includeScouting {true};
    bool includeMedical {false};
    std::optional<GameDate> nextMeetingDate;
    GameDate updatedDate;
    std::string dataConfidence {"gameplay_owner_ops_default"};
};

struct OperatingMeetingAttendee {
    std::string role;
    std::string displayName;
    bool autoAssigned {true};
    std::string notes;
};

struct OperatingMeeting {
    std::string id;
    GameDate meetingDate;
    OperatingMeetingAgendaType agenda {OperatingMeetingAgendaType::General};
    OperatingMeetingStatus status {OperatingMeetingStatus::Held};
    OperatingMeetingAttendeeMode attendeeMode {OperatingMeetingAttendeeMode::Auto};
    std::vector<OperatingMeetingAttendee> attendees;
    std::string summary;
    std::string outcomeInboxEventKey;
    GameDate createdDate;
    std::string dataConfidence {"gameplay_owner_ops_meeting"};
};

struct OwnerReputationState {
    int overallScore {50};
    int sportingAmbition {50};
    int financialPrudence {50};
    int squadLoyalty {50};
    int autonomyRespect {50};
    std::string headline {"평가 형성 중"};
    GameDate updatedDate;
    std::string dataConfidence {"gameplay_owner_reputation_v1"};
};

struct FanTendencyState {
    int trustScore {50};
    int patienceScore {50};
    int skepticismScore {50};
    int starSigningPreference {55};
    int homegrownPreference {60};
    int youthDevelopmentPreference {50};
    int balancedSquadPreference {50};
    std::string mood {"cautiously_optimistic"};
    GameDate updatedDate;
    std::string dataConfidence {"gameplay_fan_tendency_v1"};
};

struct OwnerActionLogEntry {
    std::string id;
    GameDate actionDate;
    std::string actionType;
    std::string category;
    std::string subjectId;
    std::string subjectName;
    std::string summary;
    int reputationDelta {};
    int fanTrustDelta {};
    int ambitionDelta {};
    int prudenceDelta {};
    int loyaltyDelta {};
    int autonomyDelta {};
    int starPreferenceDelta {};
    int homegrownPreferenceDelta {};
    int youthPreferenceDelta {};
    int balancedPreferenceDelta {};
    int skepticismDelta {};
    std::string dataConfidence {"gameplay_owner_action_log_v1"};
};

struct OwnerActionImpact {
    int reputationDelta {};
    int fanTrustDelta {};
    int ambitionDelta {};
    int prudenceDelta {};
    int loyaltyDelta {};
    int autonomyDelta {};
    int starPreferenceDelta {};
    int homegrownPreferenceDelta {};
    int youthPreferenceDelta {};
    int balancedPreferenceDelta {};
    int skepticismDelta {};
};

enum class RecruitmentPlanStatus {
    Active,
    Achieved,
    Acceptable,
    UnderDelivered,
    Failed
};

struct RecruitmentPlan {
    std::string id;
    std::string title;
    std::string requestedBy;
    std::string description;
    GameDate startDate;
    GameDate evaluationDate;
    int targetSignings {};
    int minimumAcceptableSignings {};
    RecruitmentPlanStatus status {RecruitmentPlanStatus::Active};
    int confidenceDelta {};
    std::optional<GameDate> evaluatedDate;
};

struct EraRuleProfile {
    std::string id {"england-1992-93"};
    bool transferWindowsActive {false};
    int registrationCutoffMonth {3};
    int registrationCutoffDay {31};
    bool freeMovementAfterContract {false};
    std::string domesticLoanModel {"period_contract_provisional"};
    std::string dataConfidence {"partly_sourced_exact_loan_limits_provisional"};
};

struct GameState {
    int saveId {};
    std::string saveName;
    std::uint32_t rngSeed {19920701U};
    GameDate currentDate;
    std::string ownerName;
    EraRuleProfile eraRules;
    ClubState club;
    std::vector<Player> players;
    std::vector<TransferProposal> transferProposals;
    std::vector<TransferCandidate> transferCandidates;
    std::vector<ScoutProfile> scouts;
    std::vector<ScoutingReport> scoutingReports;
    std::vector<ScoutingAssignment> scoutingAssignments;
    std::vector<ScoutFamiliarity> scoutFamiliarities;
    std::vector<ScoutingDirective> scoutingDirectives;
    std::vector<TransferPursuit> transferPursuits;
    std::vector<OutgoingTransferOffer> outgoingTransferOffers;
    std::vector<PlayerContract> playerContracts;
    std::vector<LoanDeal> loanDeals;
    std::vector<ContractRenewalCase> contractRenewals;
    std::vector<TransferPayment> transferPayments;
    std::vector<AiClubMarketState> aiClubMarketStates;
    std::vector<AiMarketDeal> aiMarketDeals;
    std::vector<SquadPlanningOpinion> squadPlanningOpinions;
    OperatingMeetingSettings operatingMeetingSettings;
    std::vector<OperatingMeeting> operatingMeetings;
    OwnerReputationState ownerReputation;
    FanTendencyState fanTendency;
    std::vector<OwnerActionLogEntry> ownerActionLog;
    std::vector<Fixture> fixtures;
    std::vector<CompetitionSeasonTeam> competitionSeasonTeams;
    std::vector<LeagueStandingRow> leagueStandings;
    std::vector<InboxItem> inbox;
    std::vector<ScheduledEvent> eventQueue;
    std::vector<RecruitmentPlan> recruitmentPlans;
    std::string dataNotice;
};

[[nodiscard]] std::optional<int> ageOnDate(const Player& player, const GameDate& date);
[[nodiscard]] std::string formatMoney(Money pence);
[[nodiscard]] std::string inboxCategoryKey(InboxCategory category);
[[nodiscard]] InboxCategory inboxCategoryFromKey(const std::string& value);
[[nodiscard]] std::string transferStatusKey(TransferStatus status);
[[nodiscard]] TransferStatus transferStatusFromKey(const std::string& value);
[[nodiscard]] std::string transferPursuitStageKey(TransferPursuitStage stage);
[[nodiscard]] TransferPursuitStage transferPursuitStageFromKey(const std::string& value);
[[nodiscard]] std::string outgoingTransferStatusKey(OutgoingTransferStatus status);
[[nodiscard]] OutgoingTransferStatus outgoingTransferStatusFromKey(const std::string& value);
[[nodiscard]] std::string outgoingNegotiationStageKey(OutgoingNegotiationStage stage);
[[nodiscard]] OutgoingNegotiationStage outgoingNegotiationStageFromKey(const std::string& value);
[[nodiscard]] std::string loanDirectionKey(LoanDirection direction);
[[nodiscard]] LoanDirection loanDirectionFromKey(const std::string& value);
[[nodiscard]] std::string loanStageKey(LoanStage stage);
[[nodiscard]] LoanStage loanStageFromKey(const std::string& value);
[[nodiscard]] std::string contractRenewalStageKey(ContractRenewalStage stage);
[[nodiscard]] ContractRenewalStage contractRenewalStageFromKey(const std::string& value);
[[nodiscard]] std::string transferPaymentDirectionKey(TransferPaymentDirection direction);
[[nodiscard]] TransferPaymentDirection transferPaymentDirectionFromKey(const std::string& value);
[[nodiscard]] std::string transferPaymentStatusKey(TransferPaymentStatus status);
[[nodiscard]] TransferPaymentStatus transferPaymentStatusFromKey(const std::string& value);
[[nodiscard]] std::string aiMarketDealTypeKey(AiMarketDealType type);
[[nodiscard]] AiMarketDealType aiMarketDealTypeFromKey(const std::string& value);
[[nodiscard]] std::string aiMarketDealStageKey(AiMarketDealStage stage);
[[nodiscard]] AiMarketDealStage aiMarketDealStageFromKey(const std::string& value);
[[nodiscard]] std::string squadPlanningOpinionSourceKey(SquadPlanningOpinionSource source);
[[nodiscard]] SquadPlanningOpinionSource squadPlanningOpinionSourceFromKey(const std::string& value);
[[nodiscard]] std::string squadPlanningOpinionTypeKey(SquadPlanningOpinionType type);
[[nodiscard]] SquadPlanningOpinionType squadPlanningOpinionTypeFromKey(const std::string& value);
[[nodiscard]] std::string squadPlanningOpinionStatusKey(SquadPlanningOpinionStatus status);
[[nodiscard]] SquadPlanningOpinionStatus squadPlanningOpinionStatusFromKey(const std::string& value);
[[nodiscard]] std::string operatingMeetingCadenceKey(OperatingMeetingCadence cadence);
[[nodiscard]] OperatingMeetingCadence operatingMeetingCadenceFromKey(const std::string& value);
[[nodiscard]] std::string operatingMeetingAgendaTypeKey(OperatingMeetingAgendaType agenda);
[[nodiscard]] OperatingMeetingAgendaType operatingMeetingAgendaTypeFromKey(const std::string& value);
[[nodiscard]] std::string operatingMeetingAttendeeModeKey(OperatingMeetingAttendeeMode mode);
[[nodiscard]] OperatingMeetingAttendeeMode operatingMeetingAttendeeModeFromKey(const std::string& value);
[[nodiscard]] std::string operatingMeetingStatusKey(OperatingMeetingStatus status);
[[nodiscard]] OperatingMeetingStatus operatingMeetingStatusFromKey(const std::string& value);
[[nodiscard]] std::string scoutingAssignmentStatusKey(ScoutingAssignmentStatus status);
[[nodiscard]] ScoutingAssignmentStatus scoutingAssignmentStatusFromKey(const std::string& value);
[[nodiscard]] std::string scoutingDepthKey(ScoutingDepth depth);
[[nodiscard]] ScoutingDepth scoutingDepthFromKey(const std::string& value);
[[nodiscard]] std::string scoutingDirectiveTypeKey(ScoutingDirectiveType type);
[[nodiscard]] ScoutingDirectiveType scoutingDirectiveTypeFromKey(const std::string& value);
[[nodiscard]] std::string scoutingDirectiveStatusKey(ScoutingDirectiveStatus status);
[[nodiscard]] ScoutingDirectiveStatus scoutingDirectiveStatusFromKey(const std::string& value);
[[nodiscard]] int scoutingDurationDays(ScoutingDepth depth);
[[nodiscard]] int scoutingKnowledgeGain(ScoutingDepth depth);
[[nodiscard]] std::string scoutingPositionGroup(const std::string& position);
[[nodiscard]] std::string scoutingAgeBand(const Player& player, const GameDate& date);
[[nodiscard]] int scoutFamiliarityLevel(
    const GameState& state,
    const std::string& scoutId,
    const std::string& category,
    const std::string& key);
void recordScoutFamiliarity(
    GameState& state,
    const std::string& scoutId,
    const std::string& category,
    const std::string& key,
    int delta,
    const GameDate& updatedDate,
    const std::string& dataConfidence);
void ensureDefaultScoutFamiliarities(GameState& state);
void ensureDefaultOwnerReputation(GameState& state);
[[nodiscard]] std::string ownerReputationHeadline(int score);
[[nodiscard]] std::string fanMoodKey(const FanTendencyState& fanState);
[[nodiscard]] std::string fanTrustLabel(int score);
void recordOwnerAction(
    GameState& state,
    const std::string& actionType,
    const std::string& category,
    const std::string& subjectId,
    const std::string& subjectName,
    const std::string& summary,
    const OwnerActionImpact& impact,
    const std::string& dataConfidence = "gameplay_owner_action_log_v1");
[[nodiscard]] std::string recruitmentPlanStatusKey(RecruitmentPlanStatus status);
[[nodiscard]] RecruitmentPlanStatus recruitmentPlanStatusFromKey(const std::string& value);
[[nodiscard]] TransferFeeTerms effectiveFeeTerms(
    Money legacyFeePence,
    const TransferFeeTerms& terms);
[[nodiscard]] PlayerContractTerms effectiveContractTerms(
    Money legacyWeeklyWagePence,
    int legacyContractYears,
    const PlayerContractTerms& terms);
[[nodiscard]] Money guaranteedTransferFee(const TransferFeeTerms& terms);
[[nodiscard]] Money maximumTransferFee(const TransferFeeTerms& terms);
[[nodiscard]] Money immediateTransferCashCost(
    const TransferFeeTerms& feeTerms,
    const PlayerContractTerms& contractTerms);
[[nodiscard]] Money transferBudgetCommitment(
    const TransferFeeTerms& feeTerms,
    const PlayerContractTerms& contractTerms);
[[nodiscard]] bool validTransferFeeTerms(const TransferFeeTerms& terms);
[[nodiscard]] bool validPlayerContractTerms(const PlayerContractTerms& terms);
[[nodiscard]] bool validLoanTerms(
    const LoanTerms& terms,
    const GameDate& currentDate);
[[nodiscard]] Money loanFinancialCommitment(const LoanTerms& terms);
[[nodiscard]] Money contractRenewalCommitment(
    const PlayerContractTerms& terms);
[[nodiscard]] ScoutingReport makeScoutingReport(
    const TransferCandidate& candidate,
    const ScoutProfile& scout,
    int knowledgeLevel,
    const std::string& focus,
    const GameDate& observedDate,
    int reportVersion = 1);
[[nodiscard]] Money outstandingTransferPayables(const GameState& state);
[[nodiscard]] Money outstandingConditionalTransferCommitments(
    const GameState& state);
[[nodiscard]] Money reservedTransferNegotiationCommitments(
    const GameState& state,
    const std::string& excludedPursuitId = {},
    const std::string& excludedProposalId = {},
    const std::string& excludedLoanId = {},
    const std::string& excludedRenewalId = {});
[[nodiscard]] Money availableTransferCash(
    const GameState& state,
    const std::string& excludedPursuitId = {},
    const std::string& excludedProposalId = {},
    const std::string& excludedLoanId = {},
    const std::string& excludedRenewalId = {});
[[nodiscard]] Money availableTransferBudget(
    const GameState& state,
    const std::string& excludedPursuitId = {},
    const std::string& excludedProposalId = {});
[[nodiscard]] Money aiReservedMarketCommitments(
    const GameState& state,
    const std::string& clubId,
    const std::string& excludedDealId = {});
[[nodiscard]] Money aiAvailableTransferCash(
    const GameState& state,
    const AiClubMarketState& club,
    const std::string& excludedDealId = {});
[[nodiscard]] Money aiAvailableTransferBudget(
    const GameState& state,
    const AiClubMarketState& club,
    const std::string& excludedDealId = {});
void ensureDefaultAiMarketState(GameState& state);
void recalculateLeagueStandings(GameState& state);
[[nodiscard]] int recruitmentPlanCompletedSignings(
    const GameState& state,
    const RecruitmentPlan& plan);
[[nodiscard]] int recruitmentPlanPriorityScore(
    const GameState& state,
    const RecruitmentPlan& plan);
[[nodiscard]] GameState makeManchesterUnitedPrototype();

} // namespace football_owner
