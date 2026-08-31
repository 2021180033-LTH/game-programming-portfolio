#pragma once

#include "core/game_state.h"

#include <optional>
#include <string>

namespace football_owner {

struct TransferActionResult {
    bool success {false};
    std::string message;
};

class SimulationEngine {
public:
    explicit SimulationEngine(GameState initialState);

    [[nodiscard]] const GameState& state() const noexcept;
    [[nodiscard]] GameState& mutableState() noexcept;

    void advanceDays(int dayCount);
    void advanceToNextEvent();
    void markInboxRead(int inboxId);
    void markAllInboxRead();
    [[nodiscard]] TransferActionResult approveTransfer(const std::string& proposalId);
    [[nodiscard]] TransferActionResult rejectTransfer(const std::string& proposalId);
    [[nodiscard]] TransferActionResult startTransferPursuit(const std::string& playerId);
    [[nodiscard]] TransferActionResult startTransferPursuit(
        const std::string& playerId,
        const TransferFeeTerms& offerTerms);
    [[nodiscard]] TransferActionResult acceptClubCounter(const std::string& pursuitId);
    [[nodiscard]] TransferActionResult improveClubOffer(const std::string& pursuitId);
    [[nodiscard]] TransferActionResult submitClubOffer(
        const std::string& pursuitId,
        const TransferFeeTerms& offerTerms);
    [[nodiscard]] TransferActionResult submitPlayerContractOffer(
        const std::string& pursuitId,
        const PlayerContractTerms& contractTerms);
    [[nodiscard]] TransferActionResult acceptPlayerCounter(const std::string& pursuitId);
    [[nodiscard]] TransferActionResult withdrawTransferPursuit(const std::string& pursuitId);
    [[nodiscard]] TransferActionResult acceptOutgoingTransfer(const std::string& offerId);
    [[nodiscard]] TransferActionResult rejectOutgoingTransfer(const std::string& offerId);
    [[nodiscard]] TransferActionResult submitOutgoingTransferCounter(
        const std::string& offerId,
        const TransferFeeTerms& feeTerms);
    [[nodiscard]] TransferActionResult acceptOutgoingTransferCounter(
        const std::string& offerId);
    [[nodiscard]] TransferActionResult startScoutingAssignment(
        const std::string& playerId,
        const std::string& scoutId,
        ScoutingDepth depth,
        const std::string& focus);
    [[nodiscard]] TransferActionResult startTargetScoutingDirective(
        const std::string& playerId,
        const std::string& scoutId,
        ScoutingDepth depth,
        const std::string& focus);
    [[nodiscard]] TransferActionResult startRangeScoutingDirective(
        const std::string& scoutId,
        ScoutingDepth depth,
        const std::string& focus,
        const std::string& region,
        const std::string& country,
        const std::string& league,
        const std::string& position,
        const std::string& style,
        int minAge,
        int maxAge,
        bool homegrownOnly,
        Money maxBudgetPence);
    [[nodiscard]] TransferActionResult cancelScoutingAssignment(
        const std::string& assignmentId);
    [[nodiscard]] TransferActionResult cancelScoutingDirective(
        const std::string& directiveId);
    [[nodiscard]] TransferActionResult startIncomingLoan(
        const std::string& playerId,
        const LoanTerms& terms);
    [[nodiscard]] TransferActionResult acceptLoanOffer(
        const std::string& loanId);
    [[nodiscard]] TransferActionResult rejectLoanOffer(
        const std::string& loanId);
    [[nodiscard]] TransferActionResult submitLoanCounter(
        const std::string& loanId,
        const LoanTerms& terms);
    [[nodiscard]] TransferActionResult acceptLoanCounter(
        const std::string& loanId);
    [[nodiscard]] TransferActionResult recallLoan(
        const std::string& loanId);
    [[nodiscard]] TransferActionResult submitContractRenewal(
        const std::string& playerId,
        const PlayerContractTerms& terms);
    [[nodiscard]] TransferActionResult acceptContractRenewalCounter(
        const std::string& renewalId);
    [[nodiscard]] TransferActionResult confirmContractRenewal(
        const std::string& renewalId);
    [[nodiscard]] TransferActionResult withdrawContractRenewal(
        const std::string& renewalId);

    [[nodiscard]] std::optional<GameDate> nextPendingDate() const;

private:
    GameState state_;

    void processCurrentDate();
    void processAiTransferMarket();
    void processLoanFutureFeeConversions();
    void processTransferProposals();
    void processTransferPursuits();
    void processOutgoingTransferOffers();
    void processTransferPayments();
    void processScoutingAssignments();
    void processScoutingReportStaleness();
    void processRecruitmentPlans();
    void processLoanDeals();
    void processContractRenewals();
    void processContractExpiries();
    void processScheduledEvents();
    void processFixtures();
    void simulateFixture(Fixture& fixture);
    void addInboxFromEvent(const ScheduledEvent& event);
    void addMatchInbox(const Fixture& fixture);
    void addTransferInbox(
        const TransferProposal& proposal,
        const std::string& eventPrefix,
        const std::string& title,
        const std::string& body);
    void addFootballInbox(
        const std::string& eventKey,
        const std::string& title,
        const std::string& body,
        bool requiresDecision = false);
    void resolveTransferRequestInbox(const TransferProposal& proposal);
    void resolveInboxEvent(const std::string& eventKey);
    void synchronizePursuitWithProposal(const TransferProposal& proposal);
    void createFinalTransferProposal(
        TransferPursuit& pursuit,
        const TransferCandidate& candidate);
    void addSettledTransferPayment(
        const std::string& id,
        const std::string& sourceId,
        const std::string& counterpartyClubId,
        const std::string& counterpartyName,
        TransferPaymentDirection direction,
        const std::string& category,
        Money amountPence,
        const std::string& description);
    void scheduleTransferInstallments(
        const std::string& idPrefix,
        const std::string& sourceId,
        const std::string& counterpartyClubId,
        const std::string& counterpartyName,
        TransferPaymentDirection direction,
        Money installmentTotalPence,
        int installmentCount,
        const GameDate& startDate);
    [[nodiscard]] bool completeAiMarketDeal(AiMarketDeal& deal);
    [[nodiscard]] bool createAiPlayerClubOffer(const AiClubMarketState& club);
    [[nodiscard]] bool createAiCandidateDeal(AiClubMarketState& club);
    [[nodiscard]] bool createAiLoanDeal(AiClubMarketState& club);
    [[nodiscard]] bool createAiRenewalDeal(AiClubMarketState& club);
    [[nodiscard]] bool createAiBuyoutOffer(const AiClubMarketState& club);
    void completeLoanStart(LoanDeal& loan);
    void completeLoanEnd(LoanDeal& loan, bool recalled);
    void resolveLoanInbox(const LoanDeal& loan);
    void resolveContractRenewalInbox(const ContractRenewalCase& renewal);
    [[nodiscard]] const TransferCandidate* findTransferCandidate(const std::string& playerId) const;
    [[nodiscard]] int nextInboxId() const;
};

} // namespace football_owner
