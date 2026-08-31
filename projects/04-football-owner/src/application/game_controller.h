#pragma once

#include "core/simulation_engine.h"
#include "infrastructure/save_game_manager.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace football_owner {

class GameController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool careerLoaded READ careerLoaded NOTIFY careerChanged)
    Q_PROPERTY(bool canContinue READ canContinue NOTIFY savesChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString currentSaveId READ currentSaveId NOTIFY careerChanged)
    Q_PROPERTY(QString playTime READ playTime NOTIFY stateChanged)
    Q_PROPERTY(QVariantList saveEntries READ saveEntries NOTIFY savesChanged)
    Q_PROPERTY(QString currentDate READ currentDate NOTIFY stateChanged)
    Q_PROPERTY(QString saveName READ saveName NOTIFY stateChanged)
    Q_PROPERTY(QString clubName READ clubName NOTIFY stateChanged)
    Q_PROPERTY(QString clubShortName READ clubShortName NOTIFY stateChanged)
    Q_PROPERTY(QString leagueName READ leagueName NOTIFY stateChanged)
    Q_PROPERTY(QString ownerName READ ownerName NOTIFY stateChanged)
    Q_PROPERTY(int ownerReputationScore READ ownerReputationScore NOTIFY stateChanged)
    Q_PROPERTY(QString ownerReputationLabel READ ownerReputationLabel NOTIFY stateChanged)
    Q_PROPERTY(int fanTrustScore READ fanTrustScore NOTIFY stateChanged)
    Q_PROPERTY(QString fanTrustLabel READ fanTrustLabel NOTIFY stateChanged)
    Q_PROPERTY(QString fanMoodLabel READ fanMoodLabel NOTIFY stateChanged)
    Q_PROPERTY(QString dominantFanTendency READ dominantFanTendency NOTIFY stateChanged)
    Q_PROPERTY(QString managerName READ managerName NOTIFY stateChanged)
    Q_PROPERTY(int managerConfidence READ managerConfidence NOTIFY stateChanged)
    Q_PROPERTY(QString managerAssessment READ managerAssessment NOTIFY stateChanged)
    Q_PROPERTY(QString managerIdentity READ managerIdentity NOTIFY stateChanged)
    Q_PROPERTY(QString operatingMeetingCadence READ operatingMeetingCadence NOTIFY stateChanged)
    Q_PROPERTY(QString operatingMeetingCadenceLabel READ operatingMeetingCadenceLabel NOTIFY stateChanged)
    Q_PROPERTY(QString nextOperatingMeetingDate READ nextOperatingMeetingDate NOTIFY stateChanged)
    Q_PROPERTY(bool operatingMeetingAutoAssign READ operatingMeetingAutoAssign NOTIFY stateChanged)
    Q_PROPERTY(bool operatingMeetingIncludeManager READ operatingMeetingIncludeManager NOTIFY stateChanged)
    Q_PROPERTY(bool operatingMeetingIncludeDirector READ operatingMeetingIncludeDirector NOTIFY stateChanged)
    Q_PROPERTY(bool operatingMeetingIncludeFinance READ operatingMeetingIncludeFinance NOTIFY stateChanged)
    Q_PROPERTY(bool operatingMeetingIncludeScouting READ operatingMeetingIncludeScouting NOTIFY stateChanged)
    Q_PROPERTY(bool operatingMeetingIncludeMedical READ operatingMeetingIncludeMedical NOTIFY stateChanged)
    Q_PROPERTY(QString cash READ cash NOTIFY stateChanged)
    Q_PROPERTY(QString availableTransferCash READ availableTransferCash NOTIFY stateChanged)
    Q_PROPERTY(QString transferBudget READ transferBudget NOTIFY stateChanged)
    Q_PROPERTY(QString weeklyWageBudget READ weeklyWageBudget NOTIFY stateChanged)
    Q_PROPERTY(QString weeklyWageCommitment READ weeklyWageCommitment NOTIFY stateChanged)
    Q_PROPERTY(int wageUsagePercent READ wageUsagePercent NOTIFY stateChanged)
    Q_PROPERTY(QString debt READ debt NOTIFY stateChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY stateChanged)
    Q_PROPERTY(int decisionCount READ decisionCount NOTIFY stateChanged)
    Q_PROPERTY(int pendingTransferCount READ pendingTransferCount NOTIFY stateChanged)
    Q_PROPERTY(int activeNegotiationCount READ activeNegotiationCount NOTIFY stateChanged)
    Q_PROPERTY(int pendingOutgoingOfferCount READ pendingOutgoingOfferCount NOTIFY stateChanged)
    Q_PROPERTY(int activeScoutingAssignmentCount READ activeScoutingAssignmentCount NOTIFY stateChanged)
    Q_PROPERTY(int scoutingCapacity READ scoutingCapacity NOTIFY stateChanged)
    Q_PROPERTY(int completedScoutingReportCount READ completedScoutingReportCount NOTIFY stateChanged)
    Q_PROPERTY(int pendingLoanDecisionCount READ pendingLoanDecisionCount NOTIFY stateChanged)
    Q_PROPERTY(int activeLoanCount READ activeLoanCount NOTIFY stateChanged)
    Q_PROPERTY(int activeRenewalCount READ activeRenewalCount NOTIFY stateChanged)
    Q_PROPERTY(int expiringContractCount READ expiringContractCount NOTIFY stateChanged)
    Q_PROPERTY(int firstTeamCount READ firstTeamCount NOTIFY stateChanged)
    Q_PROPERTY(int reserveCount READ reserveCount NOTIFY stateChanged)
    Q_PROPERTY(int youthCount READ youthCount NOTIFY stateChanged)
    Q_PROPERTY(QString dataNotice READ dataNotice NOTIFY stateChanged)
    Q_PROPERTY(QString nextFixtureTitle READ nextFixtureTitle NOTIFY stateChanged)
    Q_PROPERTY(QString nextFixtureDate READ nextFixtureDate NOTIFY stateChanged)
    Q_PROPERTY(QString nextFixtureStatus READ nextFixtureStatus NOTIFY stateChanged)
    Q_PROPERTY(QString recentResult READ recentResult NOTIFY stateChanged)
    Q_PROPERTY(QVariantList inbox READ inbox NOTIFY stateChanged)
    Q_PROPERTY(QVariantList squad READ squad NOTIFY stateChanged)
    Q_PROPERTY(QVariantList squadDiagnostics READ squadDiagnostics NOTIFY stateChanged)
    Q_PROPERTY(QVariantList squadPlanningOpinions READ squadPlanningOpinions NOTIFY stateChanged)
    Q_PROPERTY(QVariantList operatingMeetings READ operatingMeetings NOTIFY stateChanged)
    Q_PROPERTY(QVariantList ownerActionLog READ ownerActionLog NOTIFY stateChanged)
    Q_PROPERTY(QVariantList transferProposals READ transferProposals NOTIFY stateChanged)
    Q_PROPERTY(QVariantList transferCandidates READ transferCandidates NOTIFY stateChanged)
    Q_PROPERTY(QVariantList scouts READ scouts NOTIFY stateChanged)
    Q_PROPERTY(QVariantList scoutingAssignments READ scoutingAssignments NOTIFY stateChanged)
    Q_PROPERTY(QVariantList scoutingDirectives READ scoutingDirectives NOTIFY stateChanged)
    Q_PROPERTY(QVariantList recruitmentPlans READ recruitmentPlans NOTIFY stateChanged)
    Q_PROPERTY(QVariantList transferPursuits READ transferPursuits NOTIFY stateChanged)
    Q_PROPERTY(QVariantList outgoingTransferOffers READ outgoingTransferOffers NOTIFY stateChanged)
    Q_PROPERTY(QVariantList aiMarketDeals READ aiMarketDeals NOTIFY stateChanged)
    Q_PROPERTY(QVariantList transferPayments READ transferPayments NOTIFY stateChanged)
    Q_PROPERTY(QVariantList playerContracts READ playerContracts NOTIFY stateChanged)
    Q_PROPERTY(QVariantList loanDeals READ loanDeals NOTIFY stateChanged)
    Q_PROPERTY(QVariantList contractRenewals READ contractRenewals NOTIFY stateChanged)
    Q_PROPERTY(QVariantList fixtures READ fixtures NOTIFY stateChanged)
    Q_PROPERTY(QVariantList leagueStandings READ leagueStandings NOTIFY stateChanged)

public:
    explicit GameController(QString dataDirectory, QObject* parent = nullptr);

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool careerLoaded() const noexcept;
    [[nodiscard]] bool canContinue() const;
    [[nodiscard]] QString errorMessage() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString currentSaveId() const;
    [[nodiscard]] QString playTime() const;
    [[nodiscard]] QVariantList saveEntries() const;

    [[nodiscard]] QString currentDate() const;
    [[nodiscard]] QString saveName() const;
    [[nodiscard]] QString clubName() const;
    [[nodiscard]] QString clubShortName() const;
    [[nodiscard]] QString leagueName() const;
    [[nodiscard]] QString ownerName() const;
    [[nodiscard]] int ownerReputationScore() const;
    [[nodiscard]] QString ownerReputationLabel() const;
    [[nodiscard]] int fanTrustScore() const;
    [[nodiscard]] QString fanTrustLabel() const;
    [[nodiscard]] QString fanMoodLabel() const;
    [[nodiscard]] QString dominantFanTendency() const;
    [[nodiscard]] QString managerName() const;
    [[nodiscard]] int managerConfidence() const;
    [[nodiscard]] QString managerAssessment() const;
    [[nodiscard]] QString managerIdentity() const;
    [[nodiscard]] QString operatingMeetingCadence() const;
    [[nodiscard]] QString operatingMeetingCadenceLabel() const;
    [[nodiscard]] QString nextOperatingMeetingDate() const;
    [[nodiscard]] bool operatingMeetingAutoAssign() const;
    [[nodiscard]] bool operatingMeetingIncludeManager() const;
    [[nodiscard]] bool operatingMeetingIncludeDirector() const;
    [[nodiscard]] bool operatingMeetingIncludeFinance() const;
    [[nodiscard]] bool operatingMeetingIncludeScouting() const;
    [[nodiscard]] bool operatingMeetingIncludeMedical() const;
    [[nodiscard]] QString cash() const;
    [[nodiscard]] QString availableTransferCash() const;
    [[nodiscard]] QString transferBudget() const;
    [[nodiscard]] QString weeklyWageBudget() const;
    [[nodiscard]] QString weeklyWageCommitment() const;
    [[nodiscard]] int wageUsagePercent() const;
    [[nodiscard]] QString debt() const;
    [[nodiscard]] int unreadCount() const;
    [[nodiscard]] int decisionCount() const;
    [[nodiscard]] int pendingTransferCount() const;
    [[nodiscard]] int activeNegotiationCount() const;
    [[nodiscard]] int pendingOutgoingOfferCount() const;
    [[nodiscard]] int activeScoutingAssignmentCount() const;
    [[nodiscard]] int scoutingCapacity() const;
    [[nodiscard]] int completedScoutingReportCount() const;
    [[nodiscard]] int pendingLoanDecisionCount() const;
    [[nodiscard]] int activeLoanCount() const;
    [[nodiscard]] int activeRenewalCount() const;
    [[nodiscard]] int expiringContractCount() const;
    [[nodiscard]] int firstTeamCount() const;
    [[nodiscard]] int reserveCount() const;
    [[nodiscard]] int youthCount() const;
    [[nodiscard]] QString dataNotice() const;
    [[nodiscard]] QString nextFixtureTitle() const;
    [[nodiscard]] QString nextFixtureDate() const;
    [[nodiscard]] QString nextFixtureStatus() const;
    [[nodiscard]] QString recentResult() const;
    [[nodiscard]] QVariantList inbox() const;
    [[nodiscard]] QVariantList squad() const;
    [[nodiscard]] QVariantList squadDiagnostics() const;
    [[nodiscard]] QVariantList squadPlanningOpinions() const;
    [[nodiscard]] QVariantList operatingMeetings() const;
    [[nodiscard]] QVariantList ownerActionLog() const;
    [[nodiscard]] QVariantList transferProposals() const;
    [[nodiscard]] QVariantList transferCandidates() const;
    [[nodiscard]] QVariantList scouts() const;
    [[nodiscard]] QVariantList scoutingAssignments() const;
    [[nodiscard]] QVariantList scoutingDirectives() const;
    [[nodiscard]] QVariantList recruitmentPlans() const;
    [[nodiscard]] QVariantList transferPursuits() const;
    [[nodiscard]] QVariantList outgoingTransferOffers() const;
    [[nodiscard]] QVariantList aiMarketDeals() const;
    [[nodiscard]] QVariantList transferPayments() const;
    [[nodiscard]] QVariantList playerContracts() const;
    [[nodiscard]] QVariantList loanDeals() const;
    [[nodiscard]] QVariantList contractRenewals() const;
    [[nodiscard]] QVariantList fixtures() const;
    [[nodiscard]] QVariantList leagueStandings() const;

    Q_INVOKABLE bool createNewCareer(const QString& saveName, const QString& ownerName);
    Q_INVOKABLE bool continueCareer();
    Q_INVOKABLE bool loadSave(const QString& saveId);
    Q_INVOKABLE bool saveNameExists(const QString& saveName) const;
    Q_INVOKABLE bool saveGame();
    Q_INVOKABLE bool saveGameAs(const QString& saveName, bool overwrite);
    Q_INVOKABLE bool renameSave(const QString& saveId, const QString& saveName);
    Q_INVOKABLE bool deleteSave(const QString& saveId);
    Q_INVOKABLE bool returnToMainMenu();
    Q_INVOKABLE void exitGame();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void advanceDays(int dayCount);
    Q_INVOKABLE void advanceToNextEvent();
    Q_INVOKABLE void markInboxRead(int inboxId);
    Q_INVOKABLE void markAllInboxRead();
    Q_INVOKABLE void setOwnerSquadOpinion(
        const QString& playerId,
        const QString& opinionType,
        int priority,
        const QString& reason);
    Q_INVOKABLE void clearOwnerSquadOpinion(const QString& playerId);
    Q_INVOKABLE void setOperatingMeetingCadence(const QString& cadence);
    Q_INVOKABLE void callOperatingMeeting(
        const QString& agendaType,
        bool autoAssign,
        bool includeManager,
        bool includeDirector,
        bool includeFinance,
        bool includeScouting,
        bool includeMedical);
    Q_INVOKABLE void approveTransfer(const QString& proposalId);
    Q_INVOKABLE void rejectTransfer(const QString& proposalId);
    Q_INVOKABLE void startTransferPursuit(const QString& playerId);
    Q_INVOKABLE void startDetailedTransferPursuit(
        const QString& playerId,
        qint64 upfrontPounds,
        qint64 installmentPounds,
        int installmentCount,
        qint64 appearanceBonusPounds,
        int appearanceThreshold,
        int sellOnPercentage);
    Q_INVOKABLE void acceptClubCounter(const QString& pursuitId);
    Q_INVOKABLE void improveClubOffer(const QString& pursuitId);
    Q_INVOKABLE void submitDetailedClubOffer(
        const QString& pursuitId,
        qint64 upfrontPounds,
        qint64 installmentPounds,
        int installmentCount,
        qint64 appearanceBonusPounds,
        int appearanceThreshold,
        int sellOnPercentage);
    Q_INVOKABLE void submitPlayerContractOffer(
        const QString& pursuitId,
        qint64 weeklyWagePounds,
        int contractYears,
        qint64 signingBonusPounds,
        qint64 loyaltyBonusPounds,
        qint64 appearanceFeePounds,
        qint64 goalBonusPounds,
        qint64 agentFeePounds,
        qint64 releaseClausePounds,
        const QString& squadRole);
    Q_INVOKABLE void acceptPlayerCounter(const QString& pursuitId);
    Q_INVOKABLE void withdrawTransferPursuit(const QString& pursuitId);
    Q_INVOKABLE void acceptOutgoingTransfer(const QString& offerId);
    Q_INVOKABLE void rejectOutgoingTransfer(const QString& offerId);
    Q_INVOKABLE void submitOutgoingTransferCounter(
        const QString& offerId,
        qint64 upfrontPounds,
        qint64 installmentPounds,
        int installmentCount,
        qint64 appearanceBonusPounds,
        int appearanceThreshold,
        int sellOnPercentage);
    Q_INVOKABLE void acceptOutgoingTransferCounter(const QString& offerId);
    Q_INVOKABLE void startScoutingAssignment(
        const QString& playerId,
        const QString& scoutId,
        const QString& depth,
        const QString& focus);
    Q_INVOKABLE void startTargetScoutingDirective(
        const QString& playerId,
        const QString& scoutId,
        const QString& depth,
        const QString& focus);
    Q_INVOKABLE void startRangeScoutingDirective(
        const QString& scoutId,
        const QString& depth,
        const QString& focus,
        const QString& region,
        const QString& country,
        const QString& league,
        const QString& position,
        const QString& style,
        int minAge,
        int maxAge,
        bool homegrownOnly,
        qint64 maxBudgetPounds);
    Q_INVOKABLE void cancelScoutingAssignment(const QString& assignmentId);
    Q_INVOKABLE void cancelScoutingDirective(const QString& directiveId);
    Q_INVOKABLE void startIncomingLoan(
        const QString& playerId,
        qint64 loanFeePounds,
        int borrowingWagePercentage,
        const QString& promisedRole,
        const QString& endDate,
        bool canRecall,
        int recallNoticeDays,
        const QString& futureFeeType,
        qint64 futureFeePounds);
    Q_INVOKABLE void acceptLoanOffer(const QString& loanId);
    Q_INVOKABLE void rejectLoanOffer(const QString& loanId);
    Q_INVOKABLE void submitLoanCounter(
        const QString& loanId,
        qint64 loanFeePounds,
        int borrowingWagePercentage,
        const QString& promisedRole,
        const QString& endDate,
        bool canRecall,
        int recallNoticeDays,
        const QString& futureFeeType,
        qint64 futureFeePounds);
    Q_INVOKABLE void acceptLoanCounter(const QString& loanId);
    Q_INVOKABLE void recallLoan(const QString& loanId);
    Q_INVOKABLE void submitContractRenewal(
        const QString& playerId,
        qint64 weeklyWagePounds,
        int contractYears,
        qint64 signingBonusPounds,
        qint64 loyaltyBonusPounds,
        qint64 appearanceFeePounds,
        qint64 goalBonusPounds,
        qint64 agentFeePounds,
        qint64 releaseClausePounds,
        const QString& squadRole);
    Q_INVOKABLE void acceptContractRenewalCounter(const QString& renewalId);
    Q_INVOKABLE void confirmContractRenewal(const QString& renewalId);
    Q_INVOKABLE void withdrawContractRenewal(const QString& renewalId);

signals:
    void readyChanged();
    void careerChanged();
    void savesChanged();
    void errorMessageChanged();
    void statusMessageChanged();
    void stateChanged();
    void quitRequested();

private:
    std::unique_ptr<SaveGameManager> saveManager_;
    std::unique_ptr<SimulationEngine> engine_;
    QString activeCareerId_;
    qint64 committedPlayTimeSeconds_ {};
    QElapsedTimer playTimer_;
    bool ready_ {false};
    QString errorMessage_;
    QString statusMessage_;

    [[nodiscard]] const GameState* state() const noexcept;
    [[nodiscard]] qint64 currentPlayTimeSeconds() const noexcept;
    [[nodiscard]] bool activateCareer(
        std::optional<GameState> state,
        const QString& careerId,
        qint64 playTimeSeconds,
        const QString& successMessage);
    [[nodiscard]] bool persist(
        const QString& successMessage,
        const GameState* rollbackState = nullptr,
        bool createAutosave = false);
    void setError(const QString& message);
    void setStatus(const QString& message);
};

} // namespace football_owner
