#include "core/simulation_engine.h"
#include "infrastructure/save_game_manager.h"
#include "infrastructure/sqlite_game_repository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <algorithm>
#include <iostream>
#include <string>

namespace {

bool require(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

const football_owner::TransferProposal* findProposal(
    const football_owner::GameState& state,
    const std::string& proposalId)
{
    const auto proposal = std::find_if(
        state.transferProposals.begin(),
        state.transferProposals.end(),
        [&proposalId](const football_owner::TransferProposal& candidate) {
            return candidate.id == proposalId;
        });
    return proposal == state.transferProposals.end() ? nullptr : &*proposal;
}

const football_owner::TransferPursuit* findPursuit(
    const football_owner::GameState& state,
    const std::string& playerId)
{
    const auto pursuit = std::find_if(
        state.transferPursuits.begin(),
        state.transferPursuits.end(),
        [&playerId](const football_owner::TransferPursuit& candidate) {
            return candidate.candidatePlayerId == playerId;
        });
    return pursuit == state.transferPursuits.end() ? nullptr : &*pursuit;
}

const football_owner::OutgoingTransferOffer* findOutgoingOffer(
    const football_owner::GameState& state,
    const std::string& offerId)
{
    const auto offer = std::find_if(
        state.outgoingTransferOffers.begin(),
        state.outgoingTransferOffers.end(),
        [&offerId](const football_owner::OutgoingTransferOffer& candidate) {
            return candidate.id == offerId;
        });
    return offer == state.outgoingTransferOffers.end() ? nullptr : &*offer;
}

const football_owner::ScoutingReport* findScoutingReport(
    const football_owner::GameState& state,
    const std::string& playerId)
{
    const auto report = std::find_if(
        state.scoutingReports.begin(),
        state.scoutingReports.end(),
        [&playerId](const football_owner::ScoutingReport& candidate) {
            return candidate.playerId == playerId;
        });
    return report == state.scoutingReports.end() ? nullptr : &*report;
}

const football_owner::LoanDeal* findLoan(
    const football_owner::GameState& state,
    const std::string& loanId)
{
    const auto loan = std::find_if(
        state.loanDeals.begin(),
        state.loanDeals.end(),
        [&loanId](const football_owner::LoanDeal& candidate) {
            return candidate.id == loanId;
        });
    return loan == state.loanDeals.end() ? nullptr : &*loan;
}

const football_owner::ContractRenewalCase* findRenewal(
    const football_owner::GameState& state,
    const std::string& playerId)
{
    const auto renewal = std::find_if(
        state.contractRenewals.begin(),
        state.contractRenewals.end(),
        [&playerId](const football_owner::ContractRenewalCase& candidate) {
            return candidate.playerId == playerId;
        });
    return renewal == state.contractRenewals.end() ? nullptr : &*renewal;
}

const football_owner::PlayerContract* findContract(
    const football_owner::GameState& state,
    const std::string& playerId)
{
    const auto contract = std::find_if(
        state.playerContracts.begin(),
        state.playerContracts.end(),
        [&playerId](const football_owner::PlayerContract& candidate) {
            return candidate.playerId == playerId;
        });
    return contract == state.playerContracts.end() ? nullptr : &*contract;
}

bool hasPlayer(const football_owner::GameState& state, const std::string& playerId)
{
    return std::any_of(
        state.players.begin(),
        state.players.end(),
        [&playerId](const football_owner::Player& player) {
            return player.id == playerId;
        });
}

std::size_t descriptorCount(
    const football_owner::SaveGameManager& manager,
    const QString& kind,
    QString parentId = {})
{
    return static_cast<std::size_t>(std::count_if(
        manager.descriptors().begin(),
        manager.descriptors().end(),
        [&kind, &parentId](const football_owner::SaveGameDescriptor& descriptor) {
            return descriptor.kind == kind
                && (parentId.isEmpty() || descriptor.parentId == parentId);
        }));
}

const football_owner::SaveGameDescriptor* findByName(
    const football_owner::SaveGameManager& manager,
    const QString& displayName)
{
    const auto found = std::find_if(
        manager.descriptors().begin(),
        manager.descriptors().end(),
        [&displayName](const football_owner::SaveGameDescriptor& descriptor) {
            return !descriptor.isAutosave() && descriptor.displayName == displayName;
        });
    return found == manager.descriptors().end() ? nullptr : &*found;
}

bool testNamedSaveBranches()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the named-save test directory")) {
        return false;
    }

    QString error;
    QString approvalCareerId;
    QString rejectionCareerId;
    GameState openingState;

    {
        SaveGameManager manager(directory.path());
        if (!require(manager.initialize(&error), "Save manager initialization failed: " + error.toStdString())) {
            return false;
        }
        if (!require(!manager.canContinue(), "An empty profile unexpectedly has a continue target")) {
            return false;
        }

        const auto created = manager.createCareer(
            QStringLiteral("시어러 승인 루트"),
            QStringLiteral("테스트 구단주"),
            &approvalCareerId,
            &error);
        if (!require(created.has_value(), "Named career creation failed: " + error.toStdString())) {
            return false;
        }
        openingState = *created;
        if (!require(openingState.ownerName == "테스트 구단주", "Owner name was not stored")) {
            return false;
        }

        if (!require(
                manager.saveAs(
                    approvalCareerId,
                    openingState,
                    QStringLiteral("시어러 거절 루트"),
                    {},
                    12,
                    &rejectionCareerId,
                    &error),
                "Save As failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                approvalCareerId != rejectionCareerId,
                "Save As reused the source career id")) {
            return false;
        }
        if (!require(
                descriptorCount(manager, QStringLiteral("career")) == 2,
                "Save As did not create two independent careers")) {
            return false;
        }

        SimulationEngine approvalEngine(openingState);
        const auto approval = approvalEngine.approveTransfer("alan-shearer-1992-opening");
        if (!require(approval.success, "Approval branch could not approve the transfer")) {
            return false;
        }
        approvalEngine.advanceDays(3);
        if (!require(hasPlayer(approvalEngine.state(), "alan-shearer"), "Approved player did not join")) {
            return false;
        }
        if (!require(
                manager.saveCareer(approvalCareerId, approvalEngine.state(), 45, &error),
                "Approval branch save failed: " + error.toStdString())) {
            return false;
        }

        for (int index = 0; index < 4; ++index) {
            approvalEngine.advanceDays(1);
            if (!require(
                    manager.saveCareer(
                        approvalCareerId,
                        approvalEngine.state(),
                        46 + index,
                        &error),
                    "Autosave source save failed: " + error.toStdString())) {
                return false;
            }
            int slot = 0;
            if (!require(
                    manager.createAutosave(
                        approvalCareerId,
                        approvalEngine.state(),
                        46 + index,
                        &slot,
                        &error),
                    "Autosave creation failed: " + error.toStdString())) {
                return false;
            }
            if (!require(slot >= 1 && slot <= 3, "Autosave slot is outside the 1-3 range")) {
                return false;
            }
        }
        if (!require(
                descriptorCount(
                    manager,
                    QStringLiteral("autosave"),
                    approvalCareerId) == 3,
                "Autosave rotation did not retain exactly three snapshots")) {
            return false;
        }

        QString loadedCareerId;
        qint64 loadedPlayTime = 0;
        bool restoredFromAutosave = false;
        const auto rejectionState = manager.loadCareer(
            rejectionCareerId,
            &loadedCareerId,
            &loadedPlayTime,
            &restoredFromAutosave,
            &error);
        if (!require(rejectionState.has_value(), "Rejection branch load failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                loadedCareerId == rejectionCareerId && !restoredFromAutosave,
                "Manual career load was treated as an autosave restore")) {
            return false;
        }

        SimulationEngine rejectionEngine(*rejectionState);
        const auto rejection = rejectionEngine.rejectTransfer("alan-shearer-1992-opening");
        if (!require(rejection.success, "Rejection branch could not reject the transfer")) {
            return false;
        }
        if (!require(
                manager.saveCareer(rejectionCareerId, rejectionEngine.state(), 30, &error),
                "Rejection branch save failed: " + error.toStdString())) {
            return false;
        }

        const auto approvedReload = manager.loadCareer(
            approvalCareerId,
            nullptr,
            nullptr,
            nullptr,
            &error);
        const auto rejectedReload = manager.loadCareer(
            rejectionCareerId,
            nullptr,
            nullptr,
            nullptr,
            &error);
        if (!require(
                approvedReload.has_value() && rejectedReload.has_value(),
                "Branch reload failed: " + error.toStdString())) {
            return false;
        }

        const auto* approvedProposal = findProposal(
            *approvedReload,
            "alan-shearer-1992-opening");
        const auto* rejectedProposal = findProposal(
            *rejectedReload,
            "alan-shearer-1992-opening");
        if (!require(
                approvedProposal != nullptr
                    && approvedProposal->status == TransferStatus::Completed
                    && hasPlayer(*approvedReload, "alan-shearer"),
                "Approval branch state was not isolated")) {
            return false;
        }
        if (!require(
                rejectedProposal != nullptr
                    && rejectedProposal->status == TransferStatus::Rejected
                    && !hasPlayer(*rejectedReload, "alan-shearer"),
                "Rejection branch state was not isolated")) {
            return false;
        }

        if (!require(
                manager.renameCareer(
                    rejectionCareerId,
                    QStringLiteral("시어러 거절 보존본"),
                    &error),
                "Career rename failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                manager.nameExists(QStringLiteral("시어러 거절 보존본")),
                "Renamed career was not indexed")) {
            return false;
        }
    }

    {
        SaveGameManager reopened(directory.path());
        if (!require(reopened.initialize(&error), "Catalog reload failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                descriptorCount(reopened, QStringLiteral("career")) == 2,
                "Career catalog did not survive manager restart")) {
            return false;
        }
        if (!require(
                descriptorCount(
                    reopened,
                    QStringLiteral("autosave"),
                    approvalCareerId) == 3,
                "Autosave catalog did not survive manager restart")) {
            return false;
        }
        if (!require(
                findByName(reopened, QStringLiteral("시어러 거절 보존본")) != nullptr,
                "Renamed career did not survive manager restart")) {
            return false;
        }

        const auto autosave = std::find_if(
            reopened.descriptors().begin(),
            reopened.descriptors().end(),
            [&approvalCareerId](const SaveGameDescriptor& descriptor) {
                return descriptor.isAutosave() && descriptor.parentId == approvalCareerId;
            });
        if (!require(autosave != reopened.descriptors().end(), "No autosave available for restore")) {
            return false;
        }

        QString restoredCareerId;
        bool restoredFromAutosave = false;
        const auto restored = reopened.loadCareer(
            autosave->id,
            &restoredCareerId,
            nullptr,
            &restoredFromAutosave,
            &error);
        if (!require(restored.has_value(), "Autosave restore failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                restoredFromAutosave && restoredCareerId == approvalCareerId,
                "Autosave did not resolve back to its parent career")) {
            return false;
        }

        if (!require(
                reopened.deleteCareer(rejectionCareerId, &error),
                "Career deletion failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                descriptorCount(reopened, QStringLiteral("career")) == 1,
                "Deleted career remained in the catalog")) {
            return false;
        }
    }

    return true;
}

bool testLegacyImportPreservesSource()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the legacy-import test directory")) {
        return false;
    }

    const QString legacyPath = QDir(directory.path()).filePath(
        QStringLiteral("football_owner_mvp.db"));
    QString error;
    {
        SqliteGameRepository legacyRepository(legacyPath);
        const auto legacyState = legacyRepository.loadOrCreateCareer(&error);
        if (!require(legacyState.has_value(), "Legacy database creation failed: " + error.toStdString())) {
            return false;
        }

        SimulationEngine engine(*legacyState);
        const auto approval = engine.approveTransfer("alan-shearer-1992-opening");
        if (!require(approval.success, "Legacy approval setup failed")) {
            return false;
        }
        if (!require(
                legacyRepository.save(engine.state(), &error),
                "Legacy progress save failed: " + error.toStdString())) {
            return false;
        }
    }

    const qint64 originalSize = QFileInfo(legacyPath).size();
    QString importedId;
    {
        SaveGameManager manager(directory.path());
        if (!require(manager.initialize(&error), "Legacy import failed: " + error.toStdString())) {
            return false;
        }
        if (!require(manager.legacyImportedThisRun(), "Legacy import was not reported")) {
            return false;
        }
        if (!require(
                descriptorCount(manager, QStringLiteral("career")) == 1,
                "Legacy import created an unexpected number of careers")) {
            return false;
        }

        const auto imported = std::find_if(
            manager.descriptors().begin(),
            manager.descriptors().end(),
            [](const SaveGameDescriptor& descriptor) {
                return !descriptor.isAutosave()
                    && descriptor.displayName.startsWith(
                        QStringLiteral("기존 세이브 - Manchester United"));
            });
        if (!require(imported != manager.descriptors().end(), "Legacy career name was not registered")) {
            return false;
        }
        importedId = imported->id;

        const auto importedState = manager.loadCareer(
            importedId,
            nullptr,
            nullptr,
            nullptr,
            &error);
        const auto* proposal = importedState.has_value()
            ? findProposal(*importedState, "alan-shearer-1992-opening")
            : nullptr;
        if (!require(
                proposal != nullptr && proposal->status == TransferStatus::Approved,
                "Legacy transfer decision was not preserved")) {
            return false;
        }
    }

    if (!require(QFile::exists(legacyPath), "Legacy source database was removed")) {
        return false;
    }
    if (!require(QFileInfo(legacyPath).size() == originalSize, "Legacy source database was modified")) {
        return false;
    }

    {
        SaveGameManager reopened(directory.path());
        if (!require(reopened.initialize(&error), "Imported catalog reload failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                descriptorCount(reopened, QStringLiteral("career")) == 1,
                "Legacy source was imported more than once")) {
            return false;
        }
        if (!require(
                reopened.descriptor(importedId) != nullptr,
                "Imported career id changed after restart")) {
            return false;
        }
    }

    return true;
}

bool testLegacyTransferConfidenceMigration()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(
            directory.isValid(),
            "Could not create the legacy-confidence test directory")) {
        return false;
    }

    const QString databasePath =
        QDir(directory.path()).filePath(QStringLiteral("legacy-confidence.db"));
    QString error;
    {
        SqliteGameRepository repository(databasePath);
        const auto loaded = repository.loadOrCreateCareer(&error);
        if (!require(
                loaded.has_value(),
                "Could not create legacy-confidence fixture: "
                    + error.toStdString())) {
            return false;
        }
        SimulationEngine engine(*loaded);
        if (!require(
                engine.rejectTransfer("alan-shearer-1992-opening").success,
                "Could not prepare legacy rejected transfer")) {
            return false;
        }
        if (!require(
                repository.save(engine.state(), &error),
                "Could not save legacy-confidence fixture: "
                    + error.toStdString())) {
            return false;
        }
    }

    const QString connectionName =
        QStringLiteral("legacy-confidence-migration-fixture");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(
                database.open(),
                "Could not open legacy-confidence fixture for mutation")) {
            return false;
        }
        QSqlQuery query(database);
        if (!require(
                query.exec(QStringLiteral("DELETE FROM save_recruitment_plans")),
                "Could not remove recruitment plan from legacy fixture")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral(
                    "UPDATE save_club_deltas "
                    "SET manager_board_confidence = 80")),
                "Could not apply legacy rejection confidence delta")) {
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    {
        SqliteGameRepository repository(databasePath);
        const auto migrated = repository.loadOrCreateCareer(&error);
        if (!require(
                migrated.has_value(),
                "Could not load legacy-confidence fixture: "
                    + error.toStdString())) {
            return false;
        }
        if (!require(
                migrated->club.manager.boardConfidence == 82,
                "Legacy per-transfer confidence delta was not neutralized")) {
            return false;
        }
        if (!require(
                migrated->recruitmentPlans.size() == 1
                    && migrated->recruitmentPlans.front().status
                        == RecruitmentPlanStatus::Active,
                "Recruitment plan was not created during legacy migration")) {
            return false;
        }
    }

    return true;
}

bool testTransferMarketRoundTrip()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the transfer-market test directory")) {
        return false;
    }

    QString error;
    QString careerId;
    SaveGameManager manager(directory.path());
    if (!require(manager.initialize(&error), "Transfer-market manager initialization failed: " + error.toStdString())) {
        return false;
    }

    auto created = manager.createCareer(
        QStringLiteral("0.7.0 계약·임대 왕복"),
        QStringLiteral("테스트 구단주"),
        &careerId,
        &error);
    if (!require(created.has_value(), "Transfer-market career creation failed: " + error.toStdString())) {
        return false;
    }
    if (!require(created->transferCandidates.size() == 8, "Expanded transfer candidate data did not load")) {
        return false;
    }
    if (!require(
            created->scouts.size() == 2
                && created->scoutingReports.size() == 8
                && created->recruitmentPlans.size() == 1,
            "Scouting staff, reports, or recruitment plan did not load")) {
        return false;
    }
    if (!require(
            !created->eraRules.transferWindowsActive
                && !created->eraRules.freeMovementAfterContract
                && created->eraRules.registrationCutoffMonth == 3
                && created->loanDeals.size() == 1
                && created->contractRenewals.size() == 2
                && created->playerContracts.size() >= 5,
            "1992 rule profile, loan offer, renewals, or contracts did not load")) {
        return false;
    }

    SimulationEngine engine(*created);
    const auto* openingRenewal =
        findRenewal(engine.state(), "bryan-robson");
    if (!require(
            openingRenewal != nullptr,
            "Opening Bryan Robson renewal is missing")) {
        return false;
    }
    if (!require(
            engine.acceptLoanOffer(
                "cambridge-kieran-toal-1992-opening").success,
            "Could not accept the opening Kieran Toal loan")) {
        return false;
    }
    if (!require(
            engine.submitContractRenewal(
                "bryan-robson",
                openingRenewal->recommendedTerms).success,
            "Could not submit the opening Bryan Robson renewal")) {
        return false;
    }
    const TransferFeeTerms detailedFeeTerms {
        60'000'000,
        45'000'000,
        2,
        10'000'000,
        30,
        5
    };
    if (!require(
            engine.startTransferPursuit(
                "dion-dublin",
                detailedFeeTerms).success,
            "Could not start detailed Dublin pursuit")) {
        return false;
    }
    engine.advanceDays(1);
    const auto* agreedRenewal =
        findRenewal(engine.state(), "bryan-robson");
    if (!require(
            agreedRenewal != nullptr
                && agreedRenewal->stage
                    == ContractRenewalStage::AgreementReached
                && engine.confirmContractRenewal(
                    agreedRenewal->id).success,
            "Bryan Robson renewal did not reach final signature")) {
        return false;
    }
    const auto* playerTermsStage = findPursuit(engine.state(), "dion-dublin");
    if (!require(
            playerTermsStage != nullptr
                && playerTermsStage->stage
                    == TransferPursuitStage::AwaitingPlayerTerms,
            "Detailed club terms did not reach the player contract stage")) {
        return false;
    }
    const std::string pursuitId = playerTermsStage->id;
    const PlayerContractTerms detailedContractTerms {
        500'000,
        4,
        4'000'000,
        2'000'000,
        50'000,
        62'500,
        3'000'000,
        0,
        "First Team"
    };
    if (!require(
            engine.submitPlayerContractOffer(
                pursuitId,
                detailedContractTerms).success,
            "Could not submit detailed Dublin contract")) {
        return false;
    }
    engine.advanceDays(3);
    const auto* awaitingOwner = findPursuit(engine.state(), "dion-dublin");
    if (!require(
            awaitingOwner != nullptr
                && awaitingOwner->stage == TransferPursuitStage::AwaitingOwnerApproval
                && !awaitingOwner->linkedProposalId.empty(),
            "Player terms did not create an owner proposal")) {
        return false;
    }
    if (!require(
            engine.approveTransfer(awaitingOwner->linkedProposalId).success,
            "Could not approve negotiated Dublin transfer")) {
        return false;
    }
    const TransferFeeTerms saleCounter {
        90'000'000,
        20'000'000,
        1,
        10'000'000,
        25,
        10
    };
    if (!require(
            engine.submitOutgoingTransferCounter(
                "norwich-mark-robins-1992-opening",
                saleCounter).success,
            "Could not counter Mark Robins sale")) {
        return false;
    }
    engine.advanceDays(1);
    if (!require(
            engine.acceptOutgoingTransferCounter(
                "norwich-mark-robins-1992-opening").success,
            "Could not accept Mark Robins buyer counter")) {
        return false;
    }
    if (!require(
            engine.acceptOutgoingTransfer(
                "norwich-mark-robins-1992-opening").success,
            "Could not approve Mark Robins sale")) {
        return false;
    }
    engine.advanceDays(3);

    if (!require(hasPlayer(engine.state(), "dion-dublin"), "Dublin did not join before save")) {
        return false;
    }
    if (!require(!hasPlayer(engine.state(), "mark-robins"), "Mark Robins did not leave before save")) {
        return false;
    }
    if (!require(
            engine.startScoutingAssignment(
                "teddy-sheringham",
                "les-kershaw",
                ScoutingDepth::Standard,
                "potential").success,
            "Could not start scouting round-trip assignment")) {
        return false;
    }
    engine.advanceDays(3);
    const auto* interimReport =
        findScoutingReport(engine.state(), "teddy-sheringham");
    if (!require(
            interimReport != nullptr
                && interimReport->knowledgeLevel > 45
                && engine.state().scoutingAssignments.size() == 1,
            "Intermediate scouting report was not created before save")) {
        return false;
    }
    if (!require(
            manager.saveCareer(careerId, engine.state(), 120, &error),
            "Expanded transfer state save failed: " + error.toStdString())) {
        return false;
    }

    const auto loaded = manager.loadCareer(
        careerId,
        nullptr,
        nullptr,
        nullptr,
        &error);
    if (!require(loaded.has_value(), "Expanded transfer state reload failed: " + error.toStdString())) {
        return false;
    }
    const auto* loadedPursuit = findPursuit(*loaded, "dion-dublin");
    const auto* loadedSale = findOutgoingOffer(
        *loaded,
        "norwich-mark-robins-1992-opening");
    if (!require(
            loaded->transferCandidates.size() == 8
                && loadedPursuit != nullptr
                && loadedPursuit->stage == TransferPursuitStage::Completed,
            "Transfer candidate or pursuit state was not restored")) {
        return false;
    }
    if (!require(
            loadedSale != nullptr
                && loadedSale->status == OutgoingTransferStatus::Completed,
            "Outgoing transfer state was not restored")) {
        return false;
    }
    const auto* loadedProposal = loadedPursuit != nullptr
        ? findProposal(*loaded, loadedPursuit->linkedProposalId)
        : nullptr;
    if (!require(
            loadedProposal != nullptr
                && loadedProposal->feeTerms.installmentCount == 2
                && loadedProposal->feeTerms.installmentTotalPence
                    == 45'000'000
                && loadedProposal->contractTerms.signingBonusPence
                    == 4'000'000
                && loadedProposal->medicalStatus == "passed"
                && loadedProposal->registrationStatus
                    == "eligible_1992_domestic",
            "Detailed incoming transfer terms were not restored")) {
        return false;
    }
    if (!require(
            loadedSale != nullptr
                && loadedSale->feeTerms.installmentCount == 1
                && loadedSale->negotiationStage
                    == OutgoingNegotiationStage::Closed,
            "Detailed outgoing sale terms were not restored")) {
        return false;
    }
    if (!require(
            hasPlayer(*loaded, "dion-dublin") && !hasPlayer(*loaded, "mark-robins"),
            "Incoming or outgoing player registration did not survive reload")) {
        return false;
    }
    if (!require(
            std::any_of(
                loaded->playerContracts.begin(),
                loaded->playerContracts.end(),
                [](const PlayerContract& contract) {
                    return contract.playerId == "dion-dublin";
                }),
            "Negotiated player contract did not survive reload")) {
        return false;
    }
    if (!require(
            loaded->transferPayments.size() >= 8,
            "Transfer payment schedule did not survive reload")) {
        return false;
    }
    const auto* loadedReport =
        findScoutingReport(*loaded, "teddy-sheringham");
    if (!require(
            loaded->scouts.size() == 2
                && loaded->scoutingReports.size() == 8
                && loadedReport != nullptr
                && loadedReport->knowledgeLevel == interimReport->knowledgeLevel
                && loaded->scoutingAssignments.size() == 1
                && loaded->scoutingAssignments.front().lastMilestone == 50
                && loaded->scoutingAssignments.front().status
                    == ScoutingAssignmentStatus::Active,
            "Scouting staff, report, or assignment did not survive reload")) {
        return false;
    }
    if (!require(
            loaded->recruitmentPlans.size() == 1
                && loaded->recruitmentPlans.front().targetSignings == 3
                && loaded->recruitmentPlans.front().minimumAcceptableSignings == 2
                && loaded->recruitmentPlans.front().status
                    == RecruitmentPlanStatus::Active,
            "Recruitment-plan progress did not survive reload")) {
        return false;
    }
    const auto* loadedLoan = findLoan(
        *loaded,
        "cambridge-kieran-toal-1992-opening");
    if (!require(
            loadedLoan != nullptr
                && loadedLoan->direction == LoanDirection::Outgoing
                && loadedLoan->stage == LoanStage::Active
                && loadedLoan->terms.loanFeePence == 500'000
                && loadedLoan->terms.borrowingClubWagePercentage == 60
                && loadedLoan->terms.canRecall
                && loadedLoan->actualStartDate.has_value()
                && loadedLoan->parentSquadGroup == "Reserve",
            "Active outgoing loan terms or progress did not survive reload")) {
        return false;
    }
    const auto* loadedRenewal =
        findRenewal(*loaded, "bryan-robson");
    const auto* loadedContract =
        findContract(*loaded, "bryan-robson");
    if (!require(
            loadedRenewal != nullptr
                && loadedRenewal->stage
                    == ContractRenewalStage::Completed
                && loadedRenewal->signedDate.has_value()
                && loadedContract != nullptr
                && loadedContract->sourceProposalId
                    == loadedRenewal->id
                && loadedContract->weeklyWagePence
                    == loadedRenewal->offerTerms.weeklyWagePence
                && loadedContract->lifecycleStatus == "active"
                && !loadedContract->expiryNotified,
            "Completed renewal or contract lifecycle did not survive reload")) {
        return false;
    }
    if (!require(
            !loaded->eraRules.transferWindowsActive
                && !loaded->eraRules.freeMovementAfterContract
                && loaded->eraRules.registrationCutoffDay == 31,
            "1992 transfer rule profile did not survive reload")) {
        return false;
    }
    if (!require(
            std::any_of(
                loaded->transferPayments.begin(),
                loaded->transferPayments.end(),
                [](const TransferPayment& payment) {
                    return payment.category == "loan_fee"
                        && payment.sourceId
                            == "cambridge-kieran-toal-1992-opening"
                        && payment.status
                            == TransferPaymentStatus::Settled;
                })
                && std::any_of(
                    loaded->transferPayments.begin(),
                    loaded->transferPayments.end(),
                    [](const TransferPayment& payment) {
                        return payment.category
                                == "renewal_loyalty_bonus"
                            && payment.status
                                == TransferPaymentStatus::Scheduled;
                    }),
            "Loan fee or renewal loyalty payment did not survive reload")) {
        return false;
    }

    return true;
}

bool testAiTransferMarketRoundTrip()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(
            directory.isValid(),
            "Could not create the AI transfer market test directory")) {
        return false;
    }

    QString error;
    QString careerId;
    SaveGameManager manager(directory.path());
    if (!require(
            manager.initialize(&error),
            "Save manager initialization failed: " + error.toStdString())) {
        return false;
    }
    auto created = manager.createCareer(
        QStringLiteral("AI 이적시장 검증"),
        QStringLiteral("테스트 구단주"),
        &careerId,
        &error);
    if (!require(
            created.has_value(),
            "AI market career creation failed: " + error.toStdString())) {
        return false;
    }

    auto state = *created;
    if (!require(
            !state.aiClubMarketStates.empty(),
            "Created save did not include AI club market states")) {
        return false;
    }
    for (auto& club : state.aiClubMarketStates) {
        club.nextEvaluationDate = state.currentDate.addDays(90);
    }
    state.aiClubMarketStates.front().nextEvaluationDate =
        state.currentDate.addDays(1);

    SimulationEngine engine(std::move(state));
    engine.advanceDays(5);
    if (!require(
            !engine.state().aiMarketDeals.empty(),
            "AI market did not create a deal before save")) {
        return false;
    }

    const auto completedDeal = std::find_if(
        engine.state().aiMarketDeals.begin(),
        engine.state().aiMarketDeals.end(),
        [](const AiMarketDeal& deal) {
            return deal.stage == AiMarketDealStage::Completed
                && deal.completionApplied;
        });
    if (!require(
            completedDeal != engine.state().aiMarketDeals.end(),
            "AI market did not complete a deal before save")) {
        return false;
    }
    const std::string dealId = completedDeal->id;
    const std::string buyerId = completedDeal->buyingClubId;
    const Money buyerCashAfterCompletion = std::find_if(
        engine.state().aiClubMarketStates.begin(),
        engine.state().aiClubMarketStates.end(),
        [&buyerId](const AiClubMarketState& club) {
            return club.clubId == buyerId;
        })->finance.cashPence;

    if (!require(
            manager.saveCareer(careerId, engine.state(), 80, &error),
            "AI market save failed: " + error.toStdString())) {
        return false;
    }

    const auto loaded = manager.loadCareer(
        careerId,
        nullptr,
        nullptr,
        nullptr,
        &error);
    if (!require(
            loaded.has_value(),
            "AI market reload failed: " + error.toStdString())) {
        return false;
    }
    if (!require(
            loaded->aiClubMarketStates.size()
                == engine.state().aiClubMarketStates.size()
                && loaded->aiMarketDeals.size()
                    == engine.state().aiMarketDeals.size(),
            "AI market state or deal count did not survive reload")) {
        return false;
    }
    const auto loadedDeal = std::find_if(
        loaded->aiMarketDeals.begin(),
        loaded->aiMarketDeals.end(),
        [&dealId](const AiMarketDeal& deal) {
            return deal.id == dealId;
        });
    if (!require(
            loadedDeal != loaded->aiMarketDeals.end()
                && loadedDeal->stage == AiMarketDealStage::Completed
                && loadedDeal->completionApplied,
            "Completed AI deal status did not survive reload")) {
        return false;
    }

    SimulationEngine reloadedEngine(*loaded);
    reloadedEngine.advanceDays(1);
    const auto reloadedBuyer = std::find_if(
        reloadedEngine.state().aiClubMarketStates.begin(),
        reloadedEngine.state().aiClubMarketStates.end(),
        [&buyerId](const AiClubMarketState& club) {
            return club.clubId == buyerId;
        });
    if (!require(
            reloadedBuyer
                != reloadedEngine.state().aiClubMarketStates.end()
                && reloadedBuyer->finance.cashPence
                    == buyerCashAfterCompletion,
            "Reloaded completed AI deal applied cash twice")) {
        return false;
    }

    const auto* descriptor = manager.descriptor(careerId);
    if (!require(descriptor != nullptr, "AI market descriptor is missing")) {
        return false;
    }
    const QString databasePath = QDir(
        QDir(directory.path()).filePath(QStringLiteral("saves")))
        .filePath(descriptor->fileName);
    const QString connectionName =
        QStringLiteral("ai-market-schema-roundtrip");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open AI market save DB")) {
            return false;
        }
        QSqlQuery query(database);
        if (!require(
                query.exec(QStringLiteral(
                    "SELECT MAX(version) FROM schema_migrations")),
                "Could not query schema migration version")) {
            return false;
        }
        if (!require(
                query.next() && query.value(0).toInt() == 18,
                "Schema migration version 18 was not recorded")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral("PRAGMA foreign_key_check"))
                    && !query.next(),
                "Schema 18 foreign key check reported violations")) {
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QString loadError;
    {
        SqliteGameRepository repository(databasePath);
        const auto bridgeLoaded = repository.loadOrCreateCareer(&loadError);
        if (!require(
                bridgeLoaded.has_value(),
                "LOCKED v11 bridge career load failed: " + loadError.toStdString())) {
            return false;
        }
        if (!require(
                bridgeLoaded->competitionSeasonTeams.size() == 22,
                "Premier League season teams did not load into GameState")) {
            return false;
        }
        if (!require(
                bridgeLoaded->fixtures.size() == 462,
                "Premier League v5.4.5 result-free schedule did not load into GameState")) {
            return false;
        }
        if (!require(
                bridgeLoaded->leagueStandings.size() == 22,
                "Premier League standings did not derive into GameState")) {
            return false;
        }
        const auto manUtd = std::find_if(
            bridgeLoaded->leagueStandings.begin(),
            bridgeLoaded->leagueStandings.end(),
            [](const LeagueStandingRow& row) {
                return row.clubId == "man-utd" && row.clubName == "Manchester United";
            });
        if (!require(
                manUtd != bridgeLoaded->leagueStandings.end(),
                "Manchester United is missing from loaded standings")) {
            return false;
        }
    }

    return true;
}

bool testSquadPlanningOpinionRoundTrip()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(
            directory.isValid(),
            "Could not create the squad-planning opinion test directory")) {
        return false;
    }

    QString error;
    QString careerId;
    SaveGameManager manager(directory.path());
    if (!require(
            manager.initialize(&error),
            "Save manager initialization failed: " + error.toStdString())) {
        return false;
    }
    auto created = manager.createCareer(
        QStringLiteral("선수단 의견 검증"),
        QStringLiteral("테스트 구단주"),
        &careerId,
        &error);
    if (!require(
            created.has_value(),
            "Squad-planning career creation failed: " + error.toStdString())) {
        return false;
    }

    auto state = *created;
    const auto player = std::find_if(
        state.players.begin(),
        state.players.end(),
        [](const Player& item) {
            return item.squadGroup == "First Team";
        });
    if (!require(
            player != state.players.end(),
            "Could not find a first-team player for squad-planning opinion")) {
        return false;
    }

    SquadPlanningOpinion ownerOpinion;
    ownerOpinion.id = "owner-player-" + player->id;
    ownerOpinion.seasonId = "eng-premier-league-1992-93";
    ownerOpinion.clubId = state.club.id;
    ownerOpinion.playerId = player->id;
    ownerOpinion.positionGroup = "MID";
    ownerOpinion.source = SquadPlanningOpinionSource::Owner;
    ownerOpinion.type = SquadPlanningOpinionType::Sale;
    ownerOpinion.priority = 4;
    ownerOpinion.reason = "구단주 매각 후보 검증";
    ownerOpinion.targetWindow = "current_season";
    ownerOpinion.status = SquadPlanningOpinionStatus::Active;
    ownerOpinion.createdDate = state.currentDate;
    ownerOpinion.updatedDate = state.currentDate;
    ownerOpinion.dataConfidence = "test_owner_opinion";
    state.squadPlanningOpinions.push_back(ownerOpinion);

    SquadPlanningOpinion managerOpinion;
    managerOpinion.id = "manager-player-" + player->id;
    managerOpinion.seasonId = ownerOpinion.seasonId;
    managerOpinion.clubId = state.club.id;
    managerOpinion.playerId = player->id;
    managerOpinion.positionGroup = "MID";
    managerOpinion.source = SquadPlanningOpinionSource::Manager;
    managerOpinion.type = SquadPlanningOpinionType::Keep;
    managerOpinion.priority = 5;
    managerOpinion.reason = "감독은 유지 의견";
    managerOpinion.targetWindow = "current_season";
    managerOpinion.status = SquadPlanningOpinionStatus::Active;
    managerOpinion.createdDate = state.currentDate;
    managerOpinion.updatedDate = state.currentDate;
    managerOpinion.dataConfidence = "test_manager_opinion";
    state.squadPlanningOpinions.push_back(managerOpinion);

    SquadPlanningOpinion positionOpinion;
    positionOpinion.id = "director-position-def";
    positionOpinion.seasonId = ownerOpinion.seasonId;
    positionOpinion.clubId = state.club.id;
    positionOpinion.positionGroup = "DEF";
    positionOpinion.source = SquadPlanningOpinionSource::Director;
    positionOpinion.type = SquadPlanningOpinionType::Reinforce;
    positionOpinion.priority = 3;
    positionOpinion.reason = "단장 수비 보강 의견";
    positionOpinion.targetWindow = "next_season";
    positionOpinion.status = SquadPlanningOpinionStatus::Active;
    positionOpinion.createdDate = state.currentDate;
    positionOpinion.updatedDate = state.currentDate;
    positionOpinion.dataConfidence = "test_director_opinion";
    state.squadPlanningOpinions.push_back(positionOpinion);

    if (!require(
            manager.saveCareer(careerId, state, 30, &error),
            "Squad-planning opinion save failed: " + error.toStdString())) {
        return false;
    }

    const auto loaded = manager.loadCareer(
        careerId,
        nullptr,
        nullptr,
        nullptr,
        &error);
    if (!require(
            loaded.has_value(),
            "Squad-planning opinion reload failed: " + error.toStdString())) {
        return false;
    }
    if (!require(
            loaded->squadPlanningOpinions.size() == 3,
            "Squad-planning opinion count did not survive reload")) {
        return false;
    }
    const auto loadedOwner = std::find_if(
        loaded->squadPlanningOpinions.begin(),
        loaded->squadPlanningOpinions.end(),
        [&player](const SquadPlanningOpinion& item) {
            return item.source == SquadPlanningOpinionSource::Owner
                && item.playerId.has_value()
                && *item.playerId == player->id;
        });
    if (!require(
            loadedOwner != loaded->squadPlanningOpinions.end()
                && loadedOwner->type == SquadPlanningOpinionType::Sale
                && loadedOwner->priority == 4
                && loadedOwner->reason == "구단주 매각 후보 검증"
                && loadedOwner->status == SquadPlanningOpinionStatus::Active,
            "Owner player-level squad-planning opinion was not restored")) {
        return false;
    }
    const auto loadedDirector = std::find_if(
        loaded->squadPlanningOpinions.begin(),
        loaded->squadPlanningOpinions.end(),
        [](const SquadPlanningOpinion& item) {
            return item.source == SquadPlanningOpinionSource::Director
                && !item.playerId.has_value()
                && item.positionGroup.has_value()
                && *item.positionGroup == "DEF";
        });
    if (!require(
            loadedDirector != loaded->squadPlanningOpinions.end()
                && loadedDirector->type == SquadPlanningOpinionType::Reinforce
                && loadedDirector->targetWindow == "next_season",
            "Director position-level squad-planning opinion was not restored")) {
        return false;
    }

    const auto* descriptor = manager.descriptor(careerId);
    if (!require(descriptor != nullptr, "Squad-planning descriptor is missing")) {
        return false;
    }
    const QString databasePath = QDir(
        QDir(directory.path()).filePath(QStringLiteral("saves")))
        .filePath(descriptor->fileName);
    const QString connectionName =
        QStringLiteral("squad-planning-schema-roundtrip");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open squad-planning save DB")) {
            return false;
        }
        QSqlQuery query(database);
        if (!require(
                query.exec(QStringLiteral(
                    "SELECT MAX(version) FROM schema_migrations")),
                "Could not query squad-planning schema migration version")) {
            return false;
        }
        if (!require(
                query.next() && query.value(0).toInt() == 18,
                "Schema migration version 18 was not recorded for squad planning")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral("PRAGMA foreign_key_check"))
                    && !query.next(),
                "Squad-planning schema foreign key check reported violations")) {
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testOperatingMeetingRoundTrip()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(
            directory.isValid(),
            "Could not create the operating-meeting test directory")) {
        return false;
    }

    QString error;
    QString careerId;
    SaveGameManager manager(directory.path());
    if (!require(
            manager.initialize(&error),
            "Save manager initialization failed: " + error.toStdString())) {
        return false;
    }
    auto created = manager.createCareer(
        QStringLiteral("운영 회의 검증"),
        QStringLiteral("테스트 구단주"),
        &careerId,
        &error);
    if (!require(
            created.has_value(),
            "Operating-meeting career creation failed: " + error.toStdString())) {
        return false;
    }

    auto state = *created;
    state.operatingMeetingSettings.cadence = OperatingMeetingCadence::Monthly;
    state.operatingMeetingSettings.attendeeMode =
        OperatingMeetingAttendeeMode::Manual;
    state.operatingMeetingSettings.includeManager = true;
    state.operatingMeetingSettings.includeDirector = true;
    state.operatingMeetingSettings.includeFinance = true;
    state.operatingMeetingSettings.includeScouting = false;
    state.operatingMeetingSettings.includeMedical = true;
    state.operatingMeetingSettings.nextMeetingDate = state.currentDate.addMonths(1);
    state.operatingMeetingSettings.updatedDate = state.currentDate;
    state.operatingMeetingSettings.dataConfidence = "test_meeting_settings";

    OperatingMeeting meeting;
    meeting.id = "operating-meeting-test-19920701";
    meeting.meetingDate = state.currentDate;
    meeting.agenda = OperatingMeetingAgendaType::SquadPlanning;
    meeting.status = OperatingMeetingStatus::Held;
    meeting.attendeeMode = OperatingMeetingAttendeeMode::Manual;
    meeting.summary = "선수단 계획 회의 테스트";
    meeting.outcomeInboxEventKey = "operating-meeting-test-report";
    meeting.createdDate = state.currentDate;
    meeting.dataConfidence = "test_meeting";
    meeting.attendees = {
        {"owner", state.ownerName, false, "회의 소집자"},
        {"manager", state.club.manager.name, false, "경기 책임자"},
        {"director", "구단 운영 책임자", false, "운영 책임"}
    };
    state.operatingMeetings.push_back(meeting);
    state.inbox.push_back({
        99,
        meeting.outcomeInboxEventKey,
        state.currentDate,
        InboxCategory::Board,
        "운영 회의 결과",
        meeting.summary,
        true,
        false
    });

    if (!require(
            manager.saveCareer(careerId, state, 45, &error),
            "Operating-meeting save failed: " + error.toStdString())) {
        return false;
    }

    const auto loaded = manager.loadCareer(
        careerId,
        nullptr,
        nullptr,
        nullptr,
        &error);
    if (!require(
            loaded.has_value(),
            "Operating-meeting reload failed: " + error.toStdString())) {
        return false;
    }
    if (!require(
            loaded->operatingMeetingSettings.cadence
                    == OperatingMeetingCadence::Monthly
                && loaded->operatingMeetingSettings.attendeeMode
                    == OperatingMeetingAttendeeMode::Manual
                && loaded->operatingMeetingSettings.nextMeetingDate.has_value()
                && loaded->operatingMeetingSettings.nextMeetingDate->toIsoString()
                    == state.currentDate.addMonths(1).toIsoString()
                && loaded->operatingMeetingSettings.includeMedical,
            "Operating-meeting settings did not survive reload")) {
        return false;
    }
    if (!require(
            loaded->operatingMeetings.size() == 1
                && loaded->operatingMeetings.front().agenda
                    == OperatingMeetingAgendaType::SquadPlanning
                && loaded->operatingMeetings.front().attendees.size() == 3
                && loaded->operatingMeetings.front().outcomeInboxEventKey
                    == meeting.outcomeInboxEventKey,
            "Operating meeting or attendees did not survive reload")) {
        return false;
    }
    if (!require(
            std::any_of(
                loaded->inbox.begin(),
                loaded->inbox.end(),
                [&meeting](const InboxItem& item) {
                    return item.eventKey == meeting.outcomeInboxEventKey
                        && item.category == InboxCategory::Board;
                }),
            "Operating meeting inbox report did not survive reload")) {
        return false;
    }

    const auto* descriptor = manager.descriptor(careerId);
    if (!require(descriptor != nullptr, "Operating-meeting descriptor is missing")) {
        return false;
    }
    const QString databasePath = QDir(
        QDir(directory.path()).filePath(QStringLiteral("saves")))
        .filePath(descriptor->fileName);
    const QString connectionName =
        QStringLiteral("operating-meeting-schema-roundtrip");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open operating-meeting save DB")) {
            return false;
        }
        QSqlQuery query(database);
        if (!require(
                query.exec(QStringLiteral(
                    "SELECT MAX(version) FROM schema_migrations")),
                "Could not query operating-meeting schema migration version")) {
            return false;
        }
        if (!require(
                query.next() && query.value(0).toInt() == 18,
                "Schema migration version 18 was not recorded for operating meetings")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral("PRAGMA foreign_key_check"))
                    && !query.next(),
                "Operating-meeting schema foreign key check reported violations")) {
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testOwnerReputationAndFanTrustRoundTrip()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(
            directory.isValid(),
            "Could not create the owner-reputation test directory")) {
        return false;
    }

    QString error;
    QString careerId;
    SaveGameManager manager(directory.path());
    if (!require(
            manager.initialize(&error),
            "Save manager initialization failed: " + error.toStdString())) {
        return false;
    }
    auto created = manager.createCareer(
        QStringLiteral("구단주 평판 검증"),
        QStringLiteral("테스트 구단주"),
        &careerId,
        &error);
    if (!require(
            created.has_value(),
            "Owner-reputation career creation failed: " + error.toStdString())) {
        return false;
    }
    if (!require(
            created->ownerReputation.overallScore == 50
                && created->fanTendency.trustScore == 50
                && created->ownerActionLog.empty(),
            "Owner reputation defaults were not initialized")) {
        return false;
    }

    SimulationEngine engine(*created);
    if (!require(
            engine.approveTransfer("alan-shearer-1992-opening").success,
            "Could not approve opening transfer for owner reputation")) {
        return false;
    }
    if (!require(
            engine.startTargetScoutingDirective(
                "dion-dublin",
                "les-kershaw",
                ScoutingDepth::Standard,
                "overall").success,
            "Could not create target scouting directive for owner reputation")) {
        return false;
    }
    if (!require(
            engine.state().ownerActionLog.size() == 2,
            "Owner actions were not recorded by gameplay actions")) {
        return false;
    }
    if (!require(
            engine.state().ownerReputation.overallScore == 52
                && engine.state().fanTendency.trustScore == 52,
            "Owner reputation or fan trust did not accumulate expected deltas")) {
        return false;
    }
    if (!require(
            engine.state().fanTendency.homegrownPreference > 60
                && engine.state().fanTendency.youthDevelopmentPreference > 50,
            "Fan tendencies did not move from owner actions")) {
        return false;
    }

    if (!require(
            manager.saveCareer(careerId, engine.state(), 63, &error),
            "Owner-reputation save failed: " + error.toStdString())) {
        return false;
    }

    const auto loaded = manager.loadCareer(
        careerId,
        nullptr,
        nullptr,
        nullptr,
        &error);
    if (!require(
            loaded.has_value(),
            "Owner-reputation reload failed: " + error.toStdString())) {
        return false;
    }
    if (!require(
            loaded->ownerReputation.overallScore
                    == engine.state().ownerReputation.overallScore
                && loaded->ownerReputation.sportingAmbition
                    == engine.state().ownerReputation.sportingAmbition
                && loaded->fanTendency.trustScore
                    == engine.state().fanTendency.trustScore
                && loaded->fanTendency.mood
                    == fanMoodKey(engine.state().fanTendency),
            "Owner reputation or fan tendency state did not survive reload")) {
        return false;
    }
    if (!require(
            loaded->ownerActionLog.size() == 2
                && loaded->ownerActionLog.front().actionType == "approve_transfer"
                && loaded->ownerActionLog.back().actionType
                    == "target_scouting_directive",
            "Owner action log did not survive reload in chronological order")) {
        return false;
    }

    const auto* descriptor = manager.descriptor(careerId);
    if (!require(descriptor != nullptr, "Owner-reputation descriptor is missing")) {
        return false;
    }
    const QString databasePath = QDir(
        QDir(directory.path()).filePath(QStringLiteral("saves")))
        .filePath(descriptor->fileName);
    const QString connectionName =
        QStringLiteral("owner-reputation-schema-roundtrip");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open owner-reputation save DB")) {
            return false;
        }
        QSqlQuery query(database);
        if (!require(
                query.exec(QStringLiteral(
                    "SELECT MAX(version) FROM schema_migrations")),
                "Could not query owner-reputation schema migration version")) {
            return false;
        }
        if (!require(
                query.next() && query.value(0).toInt() == 18,
                "Schema migration version 18 was not recorded for owner reputation")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral(
                    "SELECT COUNT(*) FROM save_owner_action_log"))
                    && query.next()
                    && query.value(0).toInt() == 2,
                "Owner action log row count was not saved")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral("PRAGMA foreign_key_check"))
                    && !query.next(),
                "Owner-reputation schema foreign key check reported violations")) {
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testScoutingDirectiveRoundTrip()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(
            directory.isValid(),
            "Could not create the scouting-directive test directory")) {
        return false;
    }

    QString error;
    QString careerId;
    SaveGameManager manager(directory.path());
    if (!require(
            manager.initialize(&error),
            "Save manager initialization failed: " + error.toStdString())) {
        return false;
    }
    auto created = manager.createCareer(
        QStringLiteral("스카우팅 지시 검증"),
        QStringLiteral("테스트 구단주"),
        &careerId,
        &error);
    if (!require(
            created.has_value(),
            "Scouting-directive career creation failed: " + error.toStdString())) {
        return false;
    }

    SimulationEngine engine(*created);
    const int openingEngland =
        scoutFamiliarityLevel(
            engine.state(),
            "les-kershaw",
            "country",
            "England");
    const int openingEnglandRegion =
        scoutFamiliarityLevel(
            engine.state(),
            "les-kershaw",
            "region",
            "England");
    const auto directive = engine.startRangeScoutingDirective(
        "les-kershaw",
        ScoutingDepth::Standard,
        "potential",
        "England",
        "",
        "",
        "ATT",
        "potential",
        0,
        25,
        true,
        400'000'000);
    if (!require(
            directive.success,
            "Could not create range scouting directive")) {
        return false;
    }
    engine.advanceDays(7);
    if (!require(
            engine.state().scoutingDirectives.size() == 1
                && engine.state().scoutingDirectives.front().status
                    == ScoutingDirectiveStatus::Completed
                && !engine.state().scoutingDirectives.front()
                    .generatedAssignmentId.empty()
                && engine.state().scoutingDirectives.front()
                    .matchedCandidateCount >= 1
                && engine.state().scoutingDirectives.front()
                    .estimatedDurationDays
                    < scoutingDurationDays(ScoutingDepth::Standard)
                && engine.state().scoutingDirectives.front()
                    .accuracyModifier > 0,
            "Scouting directive did not complete before save")) {
        return false;
    }
    if (!require(
            scoutFamiliarityLevel(
                engine.state(),
                "les-kershaw",
                "country",
                "England")
                > openingEngland,
            "Scouting directive did not increase familiarity before save")) {
        return false;
    }
    if (!require(
            scoutFamiliarityLevel(
                engine.state(),
                "les-kershaw",
                "region",
                "England")
                > openingEnglandRegion,
            "Scouting directive did not increase region familiarity before save")) {
        return false;
    }

    if (!require(
            manager.saveCareer(careerId, engine.state(), 30, &error),
            "Scouting-directive save failed: " + error.toStdString())) {
        return false;
    }

    const auto loaded = manager.loadCareer(
        careerId,
        nullptr,
        nullptr,
        nullptr,
        &error);
    if (!require(
            loaded.has_value(),
            "Scouting-directive reload failed: " + error.toStdString())) {
        return false;
    }
    if (!require(
            loaded->scoutingDirectives.size() == 1
                && loaded->scoutingDirectives.front().type
                    == ScoutingDirectiveType::Range
                && loaded->scoutingDirectives.front().status
                    == ScoutingDirectiveStatus::Completed
                && loaded->scoutingDirectives.front().matchedCandidateCount >= 1
                && loaded->scoutingDirectives.front().estimatedDurationDays
                    < scoutingDurationDays(ScoutingDepth::Standard)
                && loaded->scoutingDirectives.front().accuracyModifier > 0
                && !loaded->scoutingDirectives.front().matchedPlayerIds.empty(),
            "Scouting directive did not survive reload")) {
        return false;
    }
    if (!require(
            !loaded->scoutFamiliarities.empty()
                && scoutFamiliarityLevel(
                    *loaded,
                    "les-kershaw",
                    "country",
                    "England")
                    > openingEngland,
            "Scout familiarity did not survive reload")) {
        return false;
    }
    if (!require(
            scoutFamiliarityLevel(
                *loaded,
                "les-kershaw",
                "region",
                "England")
                > openingEnglandRegion,
            "Scout region familiarity did not survive reload")) {
        return false;
    }
    if (!require(
            !loaded->scoutingAssignments.empty()
                && loaded->scoutingAssignments.front().status
                    == ScoutingAssignmentStatus::Completed,
            "Directive-linked assignment did not survive reload")) {
        return false;
    }

    const auto* descriptor = manager.descriptor(careerId);
    if (!require(descriptor != nullptr, "Scouting-directive descriptor is missing")) {
        return false;
    }
    const QString databasePath = QDir(
        QDir(directory.path()).filePath(QStringLiteral("saves")))
        .filePath(descriptor->fileName);
    const QString connectionName =
        QStringLiteral("scouting-directive-schema-roundtrip");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open scouting-directive save DB")) {
            return false;
        }
        QSqlQuery query(database);
        if (!require(
                query.exec(QStringLiteral(
                    "SELECT MAX(version) FROM schema_migrations")),
                "Could not query scouting-directive schema migration version")) {
            return false;
        }
        if (!require(
                query.next() && query.value(0).toInt() == 18,
                "Schema migration version 18 was not recorded for scouting directives")) {
            return false;
        }
        if (!require(
                query.exec(QStringLiteral("PRAGMA foreign_key_check"))
                    && !query.next(),
                "Scouting-directive schema foreign key check reported violations")) {
            return false;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testLockedWorldBaselineImport()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the locked-world baseline test directory")) {
        return false;
    }

    const QString databasePath =
        QDir(directory.path()).filePath(QStringLiteral("locked_world_baseline.sqlite"));
    QString error;
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "LOCKED v11 repository initialize failed: " + error.toStdString())) {
            return false;
        }
    }

    const QString connectionName = QStringLiteral("locked-world-baseline-import");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open LOCKED v11 baseline DB")) {
            return false;
        }

        const auto expectScalar = [&](const QString& sql, const qint64 expected, const std::string& message) -> bool {
            QSqlQuery query(database);
            if (!require(
                    query.exec(sql),
                    message + " query failed: " + query.lastError().text().toStdString())) {
                return false;
            }
            if (!require(query.next(), message + " query returned no rows")) {
                return false;
            }
            const qint64 actual = query.value(0).toLongLong();
            query.finish();
            return require(
                actual == expected,
                message + " expected " + std::to_string(expected)
                    + ", actual " + std::to_string(actual));
        };

        if (!expectScalar(
                QStringLiteral("SELECT MAX(version) FROM schema_migrations"),
                18,
                "Schema 18 migration version")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM world_baseline_imports "
                    "WHERE dataset_id = 'football_owner_1992_production' "
                    "AND version = 'LOCKED_v11' "
                    "AND status = 'imported' "
                    "AND LOWER(logical_digest) = '8d47b24488b11270124d3bb7186d0170268ce07b0e8cbfb1e2e8ecee020aa918'"),
                1,
                "LOCKED v11 import metadata row")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM sqlite_master "
                    "WHERE type = 'table' AND name LIKE 'fo92_locked_%'"),
                21,
                "LOCKED v11 mirrored raw table count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_clubs"),
                60,
                "LOCKED v11 clubs count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_clubs WHERE playable_status = 'playable'"),
                46,
                "LOCKED v11 playable clubs count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_people"),
                2076,
                "LOCKED v11 people count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_start_registrations"),
                1486,
                "LOCKED v11 active start registrations count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_contracts_and_academy_terms"),
                1490,
                "LOCKED v11 opening contract states count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_external_registered_players"),
                6,
                "LOCKED v11 external registered players count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_club_staff"),
                345,
                "LOCKED v11 staff records count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_future_transfer_events"),
                54,
                "LOCKED v11 future transfer events count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fo92_locked_data_provenance"),
                5463,
                "LOCKED v11 provenance rows count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fo92_locked_start_registrations AS registration "
                    "JOIN fo92_locked_clubs AS club ON club.club_id = registration.club_id "
                    "WHERE club.playable_status <> 'playable'"),
                0,
                "LOCKED v11 active registrations are playable-only")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fo92_locked_external_registered_players AS external_player "
                    "JOIN fo92_locked_start_registrations AS registration "
                    "ON registration.person_id = external_player.person_id"),
                0,
                "LOCKED v11 external reference players stay inactive")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fo92_locked_people AS person "
                    "JOIN fo92_locked_player_attributes AS attributes "
                    "ON attributes.person_id = person.person_id "
                    "WHERE person.primary_position <> 'GK' "
                    "AND attributes.goalkeeper_attribute_policy <> 'ignore_handling_and_reflexes'"),
                0,
                "LOCKED v11 non-GK goalkeeper attributes are ignored")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM clubs WHERE id = 'man-utd'"),
                1,
                "Existing gameplay club id man-utd remains")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fo92_locked_clubs "
                    "WHERE club_id = 'ENG012' AND club_name = 'Manchester United'"),
                1,
                "LOCKED v11 Manchester United raw id ENG012 exists")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM clubs WHERE id = 'ENG012'"),
                0,
                "LOCKED v11 raw ids do not replace gameplay club ids")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM world_club_id_bridge"),
                60,
                "LOCKED v11 club bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM world_club_id_bridge "
                    "WHERE playable_status = 'playable'"),
                46,
                "LOCKED v11 playable club bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM world_club_id_bridge "
                    "WHERE locked_club_id = 'ENG012' "
                    "AND gameplay_club_id = 'man-utd' "
                    "AND bridge_status = 'mapped_existing'"),
                1,
                "LOCKED v11 Manchester United bridge preserves man-utd")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM clubs AS club "
                    "JOIN world_club_id_bridge AS bridge "
                    "ON bridge.gameplay_club_id = club.id "
                    "WHERE bridge.playable_status = 'playable'"),
                46,
                "All playable LOCKED v11 clubs have gameplay club rows")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM competition_season_teams "
                    "WHERE season_id = 'eng-premier-league-1992-93'"),
                22,
                "Premier League 1992-93 participant count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM competition_season_teams "
                    "WHERE season_id = 'eng-first-division-1992-93'"),
                24,
                "First Division 1992-93 participant count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM competition_season_teams AS team "
                    "JOIN world_club_id_bridge AS bridge "
                    "ON bridge.locked_club_id = team.source_locked_club_id "
                    "WHERE bridge.playable_status <> 'playable'"),
                0,
                "Reference-only clubs are excluded from active season participants")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fixture_generation_profiles"),
                5,
                "Fixture generation profile count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_competition_bridge_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_competitions' "
                    "AND version = 'v5.4.5' "
                    "AND status = 'imported' "
                    "AND competition_definitions = 5 "
                    "AND competition_id_bridges = 5 "
                    "AND competition_seasons = 5 "
                    "AND competition_participants = 48 "
                    "AND fixture_seed_rows = 463 "
                    "AND active_fixture_rows = 463 "
                    "AND premier_league_fixture_rows = 462 "
                    "AND charity_shield_fixture_rows = 1 "
                    "AND result_seeded_rows = 0"),
                1,
                "v5.4.5 competition bridge metadata row")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_competition_id_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_competitions'"),
                5,
                "v5.4.5 competition id bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_fixture_id_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_competitions'"),
                463,
                "v5.4.5 fixture id bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fixtures "
                    "WHERE id IN ("
                    "SELECT gameplay_fixture_id FROM fact_safe_fixture_id_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_competitions')"),
                463,
                "v5.4.5 scheduled fixture rows inserted")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fixtures WHERE competition_id = 'eng-premier-league'"),
                462,
                "v5.4.5 Premier League scheduled fixtures")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM fixtures WHERE competition_id = 'eng-charity-shield'"),
                1,
                "v5.4.5 Charity Shield scheduled fixture")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_fixture_id_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_competitions' "
                    "AND fact_safe_fixture_id = 'PL92-008' "
                    "AND gameplay_fixture_id = 'pl-1992-001' "
                    "AND bridge_status = 'mapped_existing_seed_fixture'"),
                1,
                "v5.4.5 opening Manchester United fixture preserves existing gameplay id")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fixtures "
                    "WHERE id = 'pl-1992-001' "
                    "AND home_club_id = 'sheffield-utd' "
                    "AND away_club_id = 'man-utd' "
                    "AND status = 'scheduled'"),
                1,
                "Existing Manchester United opening fixture remains scheduled")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_fixture_id_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_competitions' "
                    "AND runtime_result_policy <> 'result_not_seeded'"),
                0,
                "v5.4.5 fixture bridge contains no seeded results")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM world_person_id_bridge"),
                2076,
                "LOCKED v11 person bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM world_person_id_bridge "
                    "WHERE active_start_gameplay_club_id IS NOT NULL"),
                1486,
                "LOCKED v11 active person bridge count")) {
            return false;
        }
        QSqlQuery foreignKeyCheck(database);
        if (!require(
                foreignKeyCheck.exec(QStringLiteral("PRAGMA foreign_key_check")),
                "Schema 18 foreign key check query failed: "
                    + foreignKeyCheck.lastError().text().toStdString())) {
            return false;
        }
        if (!require(!foreignKeyCheck.next(), "Schema 18 foreign key check reported violations")) {
            return false;
        }
        foreignKeyCheck.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testFactSafeHandoffV545Import()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the fact-safe handoff test directory")) {
        return false;
    }

    const QString databasePath =
        QDir(directory.path()).filePath(QStringLiteral("fact_safe_handoff_v545.sqlite"));
    QString error;
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "v5.4.5 fact-safe repository initialize failed: " + error.toStdString())) {
            return false;
        }
    }
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "v5.4.5 fact-safe repository reinitialize failed: " + error.toStdString())) {
            return false;
        }
    }

    const QString connectionName = QStringLiteral("fact-safe-handoff-v545-import");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open v5.4.5 fact-safe handoff DB")) {
            return false;
        }

        const auto expectScalar = [&](const QString& sql, const qint64 expected, const std::string& message) -> bool {
            QSqlQuery query(database);
            if (!require(
                    query.exec(sql),
                    message + " query failed: " + query.lastError().text().toStdString())) {
                return false;
            }
            if (!require(query.next(), message + " query returned no rows")) {
                return false;
            }
            const qint64 actual = query.value(0).toLongLong();
            query.finish();
            return require(
                actual == expected,
                message + " expected " + std::to_string(expected)
                    + ", actual " + std::to_string(actual));
        };

        if (!expectScalar(
                QStringLiteral("SELECT MAX(version) FROM schema_migrations"),
                18,
                "Schema 18 migration version")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND version = 'v5.4.5' "
                    "AND status = 'imported' "
                    "AND manifest_file_count = 723 "
                    "AND runtime_file_count = 38 "
                    "AND runtime_row_count = 19577 "
                    "AND competition_runtime_file_count = 6 "
                    "AND competition_runtime_row_count = 578 "
                    "AND clubs = 67 "
                    "AND playable_clubs = 46 "
                    "AND people = 2076 "
                    "AND opening_registrations = 1481 "
                    "AND opening_relationship_records = 1336 "
                    "AND scheduled_future_generations = 415 "
                    "AND future_family_relationships = 4 "
                    "AND premier_league_schedule_fixtures = 462 "
                    "AND charity_shield_schedule_fixtures = 1 "
                    "AND competition_participants = 48"),
                1,
                "v5.4.5 import metadata guardrail row")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_files "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff'"),
                44,
                "v5.4.5 mirrored runtime file count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_files "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND source_area = 'competition_runtime_safe'"),
                6,
                "v5.4.5 competition runtime-safe file count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fo92_fact_safe_rows "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff'"),
                20155,
                "v5.4.5 mirrored raw row count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT row_count FROM fact_safe_handoff_files "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND file_path = 'core/people_master_corrected_candidates.csv'"),
                2076,
                "v5.4.5 people master row count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT row_count FROM fact_safe_handoff_files "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND file_path = 'gameplay_model/contracts_and_academy_terms_model.csv'"),
                1336,
                "v5.4.5 opening relationship row count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT row_count FROM fact_safe_handoff_files "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND file_path = 'competitions/runtime_safe/fixtures_schedule_seed.csv'"),
                463,
                "v5.4.5 runtime-safe fixture seed row count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fo92_fact_safe_rows "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND file_path = 'competitions/runtime_safe/fixtures_schedule_seed.csv' "
                    "AND row_json LIKE '%\"competition_id\":\"ENG-PL-1992\"%'"),
                462,
                "v5.4.5 Premier League fixture seed rows")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fo92_fact_safe_rows "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND file_path = 'competitions/runtime_safe/fixtures_schedule_seed.csv' "
                    "AND row_json LIKE '%\"runtime_result_policy\":\"result_not_seeded\"%'"),
                463,
                "v5.4.5 fixture seeds contain no seeded results")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_files "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND (file_path LIKE 'audit_archive_do_not_import/%' "
                    "OR file_path LIKE 'competitions/historical_reference/%')"),
                0,
                "v5.4.5 non-runtime scopes remain excluded")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_handoff'"),
                1,
                "v5.4.5 reinitialize remains idempotent")) {
            return false;
        }

        QSqlQuery foreignKeyCheck(database);
        if (!require(
                foreignKeyCheck.exec(QStringLiteral("PRAGMA foreign_key_check")),
                "Schema 18 fact-safe foreign key check query failed: "
                    + foreignKeyCheck.lastError().text().toStdString())) {
            return false;
        }
        if (!require(!foreignKeyCheck.next(), "Schema 18 fact-safe foreign key check reported violations")) {
            return false;
        }
        foreignKeyCheck.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testFactSafeRosterActivationBridgeV545Import()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the fact-safe roster bridge test directory")) {
        return false;
    }

    const QString databasePath =
        QDir(directory.path()).filePath(QStringLiteral("fact_safe_roster_bridge_v545.sqlite"));
    QString error;
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "v5.4.5 roster bridge repository initialize failed: " + error.toStdString())) {
            return false;
        }
    }
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "v5.4.5 roster bridge repository reinitialize failed: " + error.toStdString())) {
            return false;
        }
    }

    const QString connectionName = QStringLiteral("fact-safe-roster-bridge-v545-import");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open v5.4.5 roster bridge DB")) {
            return false;
        }

        const auto expectScalar = [&](const QString& sql, const qint64 expected, const std::string& message) -> bool {
            QSqlQuery query(database);
            if (!require(
                    query.exec(sql),
                    message + " query failed: " + query.lastError().text().toStdString())) {
                return false;
            }
            if (!require(query.next(), message + " query returned no rows")) {
                return false;
            }
            const qint64 actual = query.value(0).toLongLong();
            query.finish();
            return require(
                actual == expected,
                message + " expected " + std::to_string(expected)
                    + ", actual " + std::to_string(actual));
        };

        if (!expectScalar(
                QStringLiteral("SELECT MAX(version) FROM schema_migrations"),
                18,
                "Schema 18 migration version")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_bridge_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND version = 'v5.4.5' "
                    "AND source_dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND status = 'imported' "
                    "AND people_source_rows = 2076 "
                    "AND person_bridge_rows = 2076 "
                    "AND people_identity_rows_processed = 2076 "
                    "AND player_profile_rows_processed = 2076 "
                    "AND opening_registration_source_rows = 1481 "
                    "AND registration_bridge_rows = 1481 "
                    "AND active_registration_rows = 1245 "
                    "AND skipped_registration_rows = 236 "
                    "AND contract_source_rows = 1336 "
                    "AND contract_bridge_rows = 1336 "
                    "AND active_contract_rows = 1244 "
                    "AND skipped_contract_rows = 92 "
                    "AND loan_review_rows = 1547"),
                1,
                "v5.4.5 roster bridge metadata row")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_person_activation_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster'"),
                2076,
                "v5.4.5 person activation bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_person_activation_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND fact_safe_person_id = 'FO92-01274' "
                    "AND gameplay_person_id = 'ryan-giggs'"),
                1,
                "v5.4.5 roster bridge preserves existing Ryan Giggs gameplay id")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_registration_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster'"),
                1481,
                "v5.4.5 registration bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_registration_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND activation_status = 'active_opening_registration_insert_or_preserve_existing'"),
                1245,
                "v5.4.5 active opening registration bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fact_safe_roster_registration_bridge AS bridge "
                    "JOIN historical_registrations AS registration "
                    "ON registration.person_id = bridge.gameplay_person_id "
                    "AND registration.club_id = bridge.gameplay_club_id "
                    "AND registration.snapshot_date = bridge.snapshot_date "
                    "WHERE bridge.dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND bridge.activation_status = 'active_opening_registration_insert_or_preserve_existing'"),
                1245,
                "v5.4.5 active registrations are connected to historical_registrations")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_registration_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND fact_safe_club_id = 'ENG012' "
                    "AND gameplay_club_id = 'man-utd' "
                    "AND activation_status = 'active_opening_registration_insert_or_preserve_existing'"),
                45,
                "v5.4.5 Manchester United registration bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM historical_registrations "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'"),
                49,
                "Manchester United roster preserves the existing seed and adds v5.4.5-only rows")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_contract_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster'"),
                1336,
                "v5.4.5 contract bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_contract_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND activation_status = 'active_contract_model_insert_or_preserve_existing'"),
                1244,
                "v5.4.5 active contract bridge count")) {
            return false;
        }
        QSqlQuery connectedContractCount(database);
        if (!require(
                connectedContractCount.exec(QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fact_safe_contract_bridge AS bridge "
                    "JOIN historical_player_contracts AS contract "
                    "ON contract.person_id = bridge.gameplay_person_id "
                    "AND contract.club_id = bridge.gameplay_club_id "
                    "AND contract.snapshot_date = '1992-07-01' "
                    "WHERE bridge.dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND bridge.activation_status = 'active_contract_model_insert_or_preserve_existing' "
                    "AND contract.contract_start_date IS NOT NULL "
                    "AND contract.contract_end_date IS NOT NULL "
                    "AND contract.weekly_wage_pence IS NOT NULL")),
                "v5.4.5 active contract connection count query failed: "
                    + connectedContractCount.lastError().text().toStdString())) {
            return false;
        }
        if (!require(connectedContractCount.next(), "v5.4.5 active contract connection count returned no rows")) {
            return false;
        }
        const qint64 connectedContracts = connectedContractCount.value(0).toLongLong();
        connectedContractCount.finish();
        if (connectedContracts != 1244) {
            QSqlQuery missingContracts(database);
            if (missingContracts.exec(QStringLiteral(
                    "SELECT bridge.fact_safe_contract_id, bridge.display_name, "
                    "bridge.gameplay_person_id, bridge.gameplay_club_id, "
                    "bridge.contract_start_date, bridge.contract_end_date, "
                    "CAST(bridge.weekly_wage_pence AS TEXT), "
                    "CASE WHEN contract.person_id IS NULL THEN 'missing_row' ELSE 'row_exists' END, "
                    "COALESCE(contract.contract_start_date, '<NULL>'), "
                    "COALESCE(contract.contract_end_date, '<NULL>'), "
                    "COALESCE(CAST(contract.weekly_wage_pence AS TEXT), '<NULL>') "
                    "FROM fact_safe_contract_bridge AS bridge "
                    "LEFT JOIN historical_player_contracts AS contract "
                    "ON contract.person_id = bridge.gameplay_person_id "
                    "AND contract.club_id = bridge.gameplay_club_id "
                    "AND contract.snapshot_date = '1992-07-01' "
                    "WHERE bridge.dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND bridge.activation_status = 'active_contract_model_insert_or_preserve_existing' "
                    "AND (contract.person_id IS NULL "
                    "OR contract.contract_start_date IS NULL "
                    "OR contract.contract_end_date IS NULL "
                    "OR contract.weekly_wage_pence IS NULL) "
                    "ORDER BY bridge.display_name "
                    "LIMIT 20"))) {
                while (missingContracts.next()) {
                    std::cerr
                        << "[INFO] missing active contract bridge="
                        << missingContracts.value(0).toString().toStdString()
                        << " name=" << missingContracts.value(1).toString().toStdString()
                        << " person=" << missingContracts.value(2).toString().toStdString()
                        << " club=" << missingContracts.value(3).toString().toStdString()
                        << " bridgeStart=" << missingContracts.value(4).toString().toStdString()
                        << " bridgeEnd=" << missingContracts.value(5).toString().toStdString()
                        << " bridgeWage=" << missingContracts.value(6).toString().toStdString()
                        << " historical=" << missingContracts.value(7).toString().toStdString()
                        << " start=" << missingContracts.value(8).toString().toStdString()
                        << " end=" << missingContracts.value(9).toString().toStdString()
                        << " wage=" << missingContracts.value(10).toString().toStdString()
                        << '\n';
                }
                missingContracts.finish();
            }
            if (!require(
                    false,
                    "v5.4.5 active contracts are connected to historical_player_contracts expected 1244, actual "
                        + std::to_string(connectedContracts))) {
                return false;
            }
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_loan_review_queue "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster' "
                    "AND review_status = 'review_required_not_auto_activated'"),
                1547,
                "v5.4.5 loan-like source rows remain review-only")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral("SELECT COUNT(*) FROM save_loan_deals"),
                0,
                "v5.4.5 roster bridge does not auto-create save loan deals")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_bridge_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_roster'"),
                1,
                "v5.4.5 roster bridge reinitialize remains idempotent")) {
            return false;
        }

        QSqlQuery foreignKeyCheck(database);
        if (!require(
                foreignKeyCheck.exec(QStringLiteral("PRAGMA foreign_key_check")),
                "Schema 18 roster bridge foreign key check query failed: "
                    + foreignKeyCheck.lastError().text().toStdString())) {
            return false;
        }
        if (!require(!foreignKeyCheck.next(), "Schema 18 roster bridge foreign key check reported violations")) {
            return false;
        }
        foreignKeyCheck.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return true;
}

bool testFactSafeClubFinanceModelBridgeV545Import()
{
    using namespace football_owner;

    QTemporaryDir directory;
    if (!require(directory.isValid(), "Could not create the fact-safe club finance bridge test directory")) {
        return false;
    }

    const QString databasePath =
        QDir(directory.path()).filePath(QStringLiteral("fact_safe_club_finance_bridge_v545.sqlite"));
    QString error;
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "v5.4.5 club finance bridge repository initialize failed: " + error.toStdString())) {
            return false;
        }
    }
    {
        SqliteGameRepository repository(databasePath);
        if (!require(
                repository.initialize(&error),
                "v5.4.5 club finance bridge repository reinitialize failed: " + error.toStdString())) {
            return false;
        }
    }

    const QString connectionName = QStringLiteral("fact-safe-club-finance-bridge-v545-import");
    {
        QSqlDatabase database =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        if (!require(database.open(), "Could not open v5.4.5 club finance bridge DB")) {
            return false;
        }

        const auto expectScalar = [&](const QString& sql, const qint64 expected, const std::string& message) -> bool {
            QSqlQuery query(database);
            if (!require(
                    query.exec(sql),
                    message + " query failed: " + query.lastError().text().toStdString())) {
                return false;
            }
            if (!require(query.next(), message + " query returned no rows")) {
                return false;
            }
            const qint64 actual = query.value(0).toLongLong();
            query.finish();
            return require(
                actual == expected,
                message + " expected " + std::to_string(expected)
                    + ", actual " + std::to_string(actual));
        };

        if (!expectScalar(
                QStringLiteral("SELECT MAX(version) FROM schema_migrations"),
                18,
                "Schema 18 migration version")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_club_finance' "
                    "AND version = 'v5.4.5' "
                    "AND source_dataset_id = 'football_owner_1992_fact_safe_handoff' "
                    "AND source_file_path = 'core/clubs_corrected_candidates_and_model_fields.csv' "
                    "AND status = 'imported' "
                    "AND club_model_rows = 67 "
                    "AND playable_model_rows = 46 "
                    "AND active_finance_rows = 46 "
                    "AND historical_snapshot_rows = 46 "
                    "AND active_gameplay_estimate_rows = 46 "
                    "AND historical_fact_rows = 0"),
                1,
                "v5.4.5 club finance bridge metadata row")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_club_finance'"),
                67,
                "v5.4.5 club finance bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_club_finance' "
                    "AND activation_status = 'activated_model_snapshot'"),
                46,
                "v5.4.5 active club finance bridge count")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fact_safe_club_finance_bridge AS bridge "
                    "JOIN historical_club_snapshots AS snapshot "
                    "ON snapshot.club_id = bridge.gameplay_club_id "
                    "AND snapshot.snapshot_date = '1992-07-01' "
                    "WHERE bridge.dataset_id = 'football_owner_1992_fact_safe_club_finance' "
                    "AND bridge.activation_status = 'activated_model_snapshot'"),
                46,
                "v5.4.5 active club finance snapshots connected")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT cash_pence FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'"),
                1600000000,
                "Manchester United v5.4.5 cash model")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT transfer_budget_pence FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'"),
                800000000,
                "Manchester United v5.4.5 transfer budget model")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT debt_pence FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'"),
                0,
                "Manchester United v5.4.5 debt default")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT reputation FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'"),
                88,
                "Manchester United existing reputation preserved")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT squad_strength FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'"),
                78,
                "Manchester United existing squad strength preserved")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' "
                    "AND data_confidence LIKE 'fact_safe_v545_%club_finance_model_not_historical_accounts'"),
                1,
                "Manchester United finance confidence marks model estimate")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge "
                    "WHERE fact_safe_club_id = 'ENG012' "
                    "AND gameplay_club_id = 'man-utd' "
                    "AND cash_gbp_fact_status = 'gameplay_model_no_club_specific_1992_cash_statement_loaded' "
                    "AND finance_model_fallback_policy = 'retain_model_value_until_period_accounts_or_reliable_contemporary_source_is_loaded'"),
                1,
                "Manchester United bridge preserves finance model provenance")) {
            return false;
        }
        if (!expectScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge_imports "
                    "WHERE dataset_id = 'football_owner_1992_fact_safe_club_finance'"),
                1,
                "v5.4.5 club finance bridge reinitialize remains idempotent")) {
            return false;
        }

        QSqlQuery wageBudgetCheck(database);
        if (!require(
                wageBudgetCheck.exec(QStringLiteral(
                    "SELECT weekly_wage_budget_pence, weekly_wage_commitment_pence "
                    "FROM historical_club_snapshots "
                    "WHERE club_id = 'man-utd' AND snapshot_date = '1992-07-01'")),
                "Could not query Manchester United wage model")) {
            return false;
        }
        if (!require(wageBudgetCheck.next(), "Manchester United wage model row missing")) {
            return false;
        }
        const qint64 wageBudget = wageBudgetCheck.value(0).toLongLong();
        const qint64 wageCommitment = wageBudgetCheck.value(1).toLongLong();
        wageBudgetCheck.finish();
        if (!require(
                wageCommitment > 0 && wageBudget >= wageCommitment,
                "Manchester United wage budget did not cover active model commitments")) {
            return false;
        }

        QSqlQuery foreignKeyCheck(database);
        if (!require(
                foreignKeyCheck.exec(QStringLiteral("PRAGMA foreign_key_check")),
                "Schema 18 club finance bridge foreign key check query failed: "
                    + foreignKeyCheck.lastError().text().toStdString())) {
            return false;
        }
        if (!require(!foreignKeyCheck.next(), "Schema 18 club finance bridge foreign key check reported violations")) {
            return false;
        }
        foreignKeyCheck.finish();
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    {
        SqliteGameRepository repository(databasePath);
        const auto loaded = repository.loadOrCreateCareer(&error);
        if (!require(
                loaded.has_value(),
                "v5.4.5 club finance career load failed: " + error.toStdString())) {
            return false;
        }
        if (!require(
                loaded->club.finance.cashPence == 1600000000
                    && loaded->club.finance.transferBudgetPence == 800000000,
                "Player club did not load v5.4.5 finance baseline")) {
            return false;
        }
        const auto blackburn = std::find_if(
            loaded->aiClubMarketStates.begin(),
            loaded->aiClubMarketStates.end(),
            [](const AiClubMarketState& club) {
                return club.clubId == "blackburn" || club.clubName == "Blackburn Rovers";
            });
        if (!require(
                blackburn != loaded->aiClubMarketStates.end(),
                "Blackburn AI market state was missing")) {
            return false;
        }
        if (!require(
                blackburn->finance.cashPence == 2000000000
                    && blackburn->finance.transferBudgetPence == 1000000000,
                "AI club finance did not use v5.4.5 historical snapshot baseline")) {
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    (void)application;

    if (qEnvironmentVariableIsSet("FOOTBALL_OWNER_REPOSITORY_TEST_ROSTER_ONLY")) {
        if (!testFactSafeRosterActivationBridgeV545Import()) {
            return 1;
        }
        std::cout << "[PASS] v5.4.5 fact-safe roster activation bridge and review queues\n";
        return 0;
    }

    if (!testNamedSaveBranches()) {
        return 1;
    }
    std::cout << "[PASS] FM-style named saves, isolated branches, and three rotating autosaves\n";

    if (!testLegacyImportPreservesSource()) {
        return 1;
    }
    std::cout << "[PASS] 0.2.0 legacy save import preserves the source and imports once\n";

    if (!testLegacyTransferConfidenceMigration()) {
        return 1;
    }
    std::cout << "[PASS] legacy per-transfer confidence is migrated to plan-based evaluation\n";

    if (!testTransferMarketRoundTrip()) {
        return 1;
    }
    std::cout << "[PASS] 0.7.0 contracts, loans, scouting, detailed transfers, and payments round-trip\n";

    if (!testAiTransferMarketRoundTrip()) {
        return 1;
    }
    std::cout << "[PASS] 0.8.0 AI transfer market schema and round-trip\n";

    if (!testSquadPlanningOpinionRoundTrip()) {
        return 1;
    }
    std::cout << "[PASS] 0.11.0 squad-planning opinions schema and round-trip\n";

    if (!testOperatingMeetingRoundTrip()) {
        return 1;
    }
    std::cout << "[PASS] 0.12.0 operating meetings schema and round-trip\n";

    if (!testOwnerReputationAndFanTrustRoundTrip()) {
        return 1;
    }
    std::cout << "[PASS] 0.14.0 owner reputation, fan trust, and action log round-trip\n";

    if (!testScoutingDirectiveRoundTrip()) {
        return 1;
    }
    std::cout << "[PASS] 0.13.0 scouting directives schema and round-trip\n";

    if (!testLockedWorldBaselineImport()) {
        return 1;
    }
    std::cout << "[PASS] 0.10.0 LOCKED v11 bridge, season teams, and standings guardrails\n";

    if (!testFactSafeHandoffV545Import()) {
        return 1;
    }
    std::cout << "[PASS] v5.4.5 fact-safe handoff raw mirror and runtime-safe competition seeds\n";

    if (!testFactSafeRosterActivationBridgeV545Import()) {
        return 1;
    }
    std::cout << "[PASS] v5.4.5 fact-safe roster activation bridge and review queues\n";

    if (!testFactSafeClubFinanceModelBridgeV545Import()) {
        return 1;
    }
    std::cout << "[PASS] v5.4.5 club finance model bridge and AI finance baseline\n";
    return 0;
}
