#include "core/game_date.h"
#include "core/game_state.h"
#include "core/simulation_engine.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct TestCase {
    std::string name;
    std::function<void()> run;
};

void require(const bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Left, typename Right>
void requireEqual(const Left& actual, const Right& expected, const std::string& message)
{
    if (!(actual == expected)) {
        throw std::runtime_error(message);
    }
}

const football_owner::TransferProposal& firstProposal(const football_owner::GameState& state)
{
    require(!state.transferProposals.empty(), "Prototype needs a transfer proposal");
    return state.transferProposals.front();
}

std::size_t squadGroupCount(const football_owner::GameState& state, const std::string& group)
{
    return static_cast<std::size_t>(std::count_if(
        state.players.begin(),
        state.players.end(),
        [&group](const football_owner::Player& player) {
            return player.squadGroup == group;
        }));
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

const football_owner::TransferPursuit& pursuitFor(
    const football_owner::GameState& state,
    const std::string_view playerId)
{
    const auto pursuit = std::find_if(
        state.transferPursuits.begin(),
        state.transferPursuits.end(),
        [playerId](const football_owner::TransferPursuit& item) {
            return item.candidatePlayerId == playerId;
        });
    require(pursuit != state.transferPursuits.end(), "Transfer pursuit is missing");
    return *pursuit;
}

const football_owner::TransferProposal& proposalById(
    const football_owner::GameState& state,
    const std::string& proposalId)
{
    const auto proposal = std::find_if(
        state.transferProposals.begin(),
        state.transferProposals.end(),
        [&proposalId](const football_owner::TransferProposal& item) {
            return item.id == proposalId;
        });
    require(proposal != state.transferProposals.end(), "Linked transfer proposal is missing");
    return *proposal;
}

const football_owner::OutgoingTransferOffer& outgoingOffer(
    const football_owner::GameState& state,
    const std::string& offerId)
{
    const auto offer = std::find_if(
        state.outgoingTransferOffers.begin(),
        state.outgoingTransferOffers.end(),
        [&offerId](const football_owner::OutgoingTransferOffer& item) {
            return item.id == offerId;
        });
    require(offer != state.outgoingTransferOffers.end(), "Outgoing transfer offer is missing");
    return *offer;
}

const football_owner::ScoutingReport& scoutingReportFor(
    const football_owner::GameState& state,
    const std::string_view playerId)
{
    const auto report = std::find_if(
        state.scoutingReports.begin(),
        state.scoutingReports.end(),
        [playerId](const football_owner::ScoutingReport& item) {
            return item.playerId == playerId;
        });
    require(report != state.scoutingReports.end(), "Scouting report is missing");
    return *report;
}

const football_owner::LoanDeal& loanById(
    const football_owner::GameState& state,
    const std::string_view loanId)
{
    const auto loan = std::find_if(
        state.loanDeals.begin(),
        state.loanDeals.end(),
        [loanId](const football_owner::LoanDeal& item) {
            return item.id == loanId;
        });
    require(loan != state.loanDeals.end(), "Loan deal is missing");
    return *loan;
}

const football_owner::LoanDeal& loanForPlayer(
    const football_owner::GameState& state,
    const std::string_view playerId)
{
    const auto loan = std::find_if(
        state.loanDeals.begin(),
        state.loanDeals.end(),
        [playerId](const football_owner::LoanDeal& item) {
            return item.player.id == playerId;
        });
    require(loan != state.loanDeals.end(), "Player loan deal is missing");
    return *loan;
}

const football_owner::ContractRenewalCase& renewalForPlayer(
    const football_owner::GameState& state,
    const std::string_view playerId)
{
    const auto renewal = std::find_if(
        state.contractRenewals.begin(),
        state.contractRenewals.end(),
        [playerId](const football_owner::ContractRenewalCase& item) {
            return item.playerId == playerId;
        });
    require(
        renewal != state.contractRenewals.end(),
        "Contract renewal case is missing");
    return *renewal;
}

const football_owner::PlayerContract& contractForPlayer(
    const football_owner::GameState& state,
    const std::string_view playerId)
{
    const auto contract = std::find_if(
        state.playerContracts.begin(),
        state.playerContracts.end(),
        [playerId](const football_owner::PlayerContract& item) {
            return item.playerId == playerId;
        });
    require(
        contract != state.playerContracts.end(),
        "Player contract is missing");
    return *contract;
}

const football_owner::AiClubMarketState& aiClubById(
    const football_owner::GameState& state,
    const std::string_view clubId)
{
    const auto club = std::find_if(
        state.aiClubMarketStates.begin(),
        state.aiClubMarketStates.end(),
        [clubId](const football_owner::AiClubMarketState& item) {
            return item.clubId == clubId;
        });
    require(club != state.aiClubMarketStates.end(), "AI club market state is missing");
    return *club;
}

const football_owner::AiMarketDeal& aiDealById(
    const football_owner::GameState& state,
    const std::string_view dealId)
{
    const auto deal = std::find_if(
        state.aiMarketDeals.begin(),
        state.aiMarketDeals.end(),
        [dealId](const football_owner::AiMarketDeal& item) {
            return item.id == dealId;
        });
    require(deal != state.aiMarketDeals.end(), "AI market deal is missing");
    return *deal;
}

} // namespace

int main()
{
    using namespace football_owner;

    const std::vector<TestCase> tests {
        {
            "GameDate parses, validates, and advances across leap day",
            [] {
                const auto parsed = GameDate::fromIsoString("1992-02-28");
                require(parsed.has_value(), "Expected a valid ISO date");
                requireEqual(parsed->addDays(1).toIsoString(), std::string {"1992-02-29"}, "Leap day was skipped");
                requireEqual(parsed->addDays(2).toIsoString(), std::string {"1992-03-01"}, "Month transition failed");
                requireEqual(
                    GameDate {1992, 2, 29}.addMonths(12).toIsoString(),
                    std::string {"1993-02-28"},
                    "Month addition did not clamp leap day");
                require(!GameDate::fromIsoString("1992-02-31").has_value(), "Invalid date was accepted");
            }
        },
        {
            "Prototype starts at the confirmed club and season",
            [] {
                const auto state = makeManchesterUnitedPrototype();
                requireEqual(state.currentDate.toIsoString(), std::string {"1992-07-01"}, "Unexpected career start date");
                requireEqual(state.club.id, std::string {"man-utd"}, "Unexpected playable club");
                requireEqual(state.club.leagueName, std::string {"Premier League"}, "Unexpected active league");
                requireEqual(state.players.size(), std::size_t {45}, "Corrected squad seed count changed");
                requireEqual(squadGroupCount(state, "First Team"), std::size_t {23}, "First-team count changed");
                requireEqual(squadGroupCount(state, "Reserve"), std::size_t {8}, "Reserve count changed");
                requireEqual(squadGroupCount(state, "Youth"), std::size_t {14}, "Youth count changed");
                require(hasPlayer(state, "ian-wilkinson"), "Ian Wilkinson is missing");
                require(hasPlayer(state, "mal-donaghy"), "Mal Donaghy is missing");
                require(hasPlayer(state, "darren-ferguson"), "Darren Ferguson is missing");
                require(hasPlayer(state, "david-beckham"), "David Beckham is missing from the youth squad");
                const auto unknownBirthDate = std::find_if(
                    state.players.begin(),
                    state.players.end(),
                    [](const Player& player) {
                        return player.id == "colin-telford";
                    });
                require(unknownBirthDate != state.players.end(), "Colin Telford is missing");
                require(!ageOnDate(*unknownBirthDate, state.currentDate).has_value(), "Unknown birth date was invented");
                requireEqual(firstProposal(state).status, TransferStatus::Pending, "Opening transfer proposal is not pending");
                requireEqual(state.transferCandidates.size(), std::size_t {8}, "Transfer candidate seed count changed");
                requireEqual(state.transferPursuits.size(), std::size_t {1}, "Opening transfer pursuit is missing");
                requireEqual(state.outgoingTransferOffers.size(), std::size_t {1}, "Opening sale offer is missing");
                require(!state.fixtures.empty(), "Prototype needs a first fixture");
            }
        },
        {
            "Event queue advances to the next owner report once",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const std::size_t openingInboxCount = engine.state().inbox.size();
                requireEqual(engine.nextPendingDate()->toIsoString(), std::string {"1992-07-02"}, "Wrong first pending date");

                engine.advanceToNextEvent();
                requireEqual(engine.state().currentDate.toIsoString(), std::string {"1992-07-02"}, "Did not stop on next event");
                requireEqual(engine.state().inbox.size(), openingInboxCount + 1, "Opening finance report was not delivered");

                engine.advanceDays(1);
                requireEqual(engine.state().inbox.size(), openingInboxCount + 2, "Manager meeting was not delivered");
                engine.advanceDays(0);
                requireEqual(engine.state().inbox.size(), openingInboxCount + 2, "An event was delivered twice");
            }
        },
        {
            "Match result engine is deterministic for the same save seed",
            [] {
                SimulationEngine first(makeManchesterUnitedPrototype());
                SimulationEngine second(makeManchesterUnitedPrototype());

                first.advanceDays(45);
                second.advanceDays(45);

                const auto& firstFixture = first.state().fixtures.front();
                const auto& secondFixture = second.state().fixtures.front();
                require(firstFixture.played, "Fixture was not simulated on its date");
                requireEqual(firstFixture.homeGoals, secondFixture.homeGoals, "Home score changed with identical seed");
                requireEqual(firstFixture.awayGoals, secondFixture.awayGoals, "Away score changed with identical seed");
                require(firstFixture.homeGoals >= 0 && firstFixture.awayGoals >= 0, "Negative score generated");
            }
        },
        {
            "League standings are derived from completed fixtures once",
            [] {
                GameState state = makeManchesterUnitedPrototype();
                state.fixtures = {
                    {
                        "test-fixture-1",
                        {1992, 8, 15},
                        "Premier League",
                        "Manchester United",
                        "Sheffield United",
                        78,
                        65,
                        true,
                        2,
                        1
                    },
                    {
                        "test-fixture-2",
                        {1992, 8, 18},
                        "Premier League",
                        "Manchester United",
                        "Arsenal",
                        78,
                        74,
                        false,
                        0,
                        0
                    }
                };
                state.competitionSeasonTeams = {
                    {
                        "eng-premier-league-1992-93",
                        "Premier League",
                        "arsenal",
                        "Arsenal",
                        "ENG001",
                        1
                    },
                    {
                        "eng-premier-league-1992-93",
                        "Premier League",
                        "man-utd",
                        "Manchester United",
                        "ENG012",
                        12
                    },
                    {
                        "eng-premier-league-1992-93",
                        "Premier League",
                        "sheffield-utd",
                        "Sheffield United",
                        "ENG018",
                        18
                    }
                };

                recalculateLeagueStandings(state);
                const auto manUtd = std::find_if(
                    state.leagueStandings.begin(),
                    state.leagueStandings.end(),
                    [](const LeagueStandingRow& row) {
                        return row.clubId == "man-utd";
                    });
                const auto arsenal = std::find_if(
                    state.leagueStandings.begin(),
                    state.leagueStandings.end(),
                    [](const LeagueStandingRow& row) {
                        return row.clubId == "arsenal";
                    });
                const auto sheffield = std::find_if(
                    state.leagueStandings.begin(),
                    state.leagueStandings.end(),
                    [](const LeagueStandingRow& row) {
                        return row.clubId == "sheffield-utd";
                    });
                require(manUtd != state.leagueStandings.end(), "Manchester United standing row missing");
                require(arsenal != state.leagueStandings.end(), "Arsenal standing row missing");
                require(sheffield != state.leagueStandings.end(), "Sheffield United standing row missing");
                requireEqual(manUtd->played, 1, "Completed fixture did not count as played");
                requireEqual(manUtd->points, 3, "Win did not award three points");
                requireEqual(manUtd->goalDifference, 1, "Goal difference was not calculated");
                requireEqual(sheffield->points, 0, "Loss awarded points");
                requireEqual(arsenal->played, 0, "Unplayed fixture affected standings");

                recalculateLeagueStandings(state);
                const auto reloadedManUtd = std::find_if(
                    state.leagueStandings.begin(),
                    state.leagueStandings.end(),
                    [](const LeagueStandingRow& row) {
                        return row.clubId == "man-utd";
                    });
                require(reloadedManUtd != state.leagueStandings.end(), "Manchester United standing row missing after recalculation");
                requireEqual(reloadedManUtd->played, 1, "Recalculation double-counted played fixtures");
                requireEqual(reloadedManUtd->points, 3, "Recalculation double-counted points");
            }
        },
        {
            "Inbox read state and money formatting behave predictably",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                engine.markInboxRead(1);
                require(!engine.state().inbox.front().unread, "Inbox item stayed unread");
                engine.markAllInboxRead();
                for (const auto& item : engine.state().inbox) {
                    require(!item.unread, "Mark-all-read left an unread item");
                }
                requireEqual(formatMoney(600'000'000), std::string {"£6.00m"}, "Million-pound formatting changed");
                requireEqual(formatMoney(9'000'000), std::string {"£90.0k"}, "Thousand-pound formatting changed");
            }
        },
        {
            "Reading a transfer request does not resolve the owner decision",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const auto request = std::find_if(
                    engine.state().inbox.begin(),
                    engine.state().inbox.end(),
                    [](const InboxItem& item) {
                        return item.eventKey == "transfer-proposal-alan-shearer-1992-opening";
                    });
                require(request != engine.state().inbox.end(), "Transfer request inbox item is missing");
                const int requestId = request->id;

                engine.markInboxRead(requestId);

                const auto reloadedRequest = std::find_if(
                    engine.state().inbox.begin(),
                    engine.state().inbox.end(),
                    [requestId](const InboxItem& item) {
                        return item.id == requestId;
                    });
                require(reloadedRequest != engine.state().inbox.end(), "Transfer request disappeared");
                require(!reloadedRequest->unread, "Transfer request stayed unread");
                require(reloadedRequest->requiresDecision, "Reading silently resolved the decision");
                requireEqual(firstProposal(engine.state()).status, TransferStatus::Pending, "Reading changed proposal status");
            }
        },
        {
            "Approved transfer reserves budget and joins the first team after three days",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const Money openingCash = engine.state().club.finance.cashPence;
                const Money openingTransferBudget = engine.state().club.finance.transferBudgetPence;
                const Money openingWages = engine.state().club.finance.weeklyWageCommitmentPence;
                const int openingConfidence = engine.state().club.manager.boardConfidence;
                const auto proposalBefore = firstProposal(engine.state());

                const auto result = engine.approveTransfer(proposalBefore.id);

                require(result.success, "Valid transfer approval failed");
                const auto& approved = firstProposal(engine.state());
                requireEqual(approved.status, TransferStatus::Approved, "Proposal was not approved");
                requireEqual(engine.state().club.finance.cashPence, openingCash - proposalBefore.feePence, "Cash was not charged");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingTransferBudget - proposalBefore.feePence,
                    "Transfer budget was not charged");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingWages + proposalBefore.weeklyWagePence,
                    "Wage commitment was not reserved");
                requireEqual(
                    engine.state().club.manager.boardConfidence,
                    openingConfidence,
                    "A single approval changed confidence before the recruitment plan review");
                require(!hasPlayer(engine.state(), "alan-shearer"), "Player joined immediately instead of on the join date");

                engine.advanceDays(2);
                require(!hasPlayer(engine.state(), "alan-shearer"), "Player joined before the agreed date");
                engine.advanceDays(1);
                requireEqual(firstProposal(engine.state()).status, TransferStatus::Completed, "Approved deal was not completed");
                require(hasPlayer(engine.state(), "alan-shearer"), "Completed transfer did not add the player");
                requireEqual(squadGroupCount(engine.state(), "First Team"), std::size_t {24}, "New signing was not placed in the first team");
            }
        },
        {
            "Rejected transfer leaves finance and confidence untouched until plan review",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const auto openingFinance = engine.state().club.finance;
                const int openingConfidence = engine.state().club.manager.boardConfidence;

                const auto result = engine.rejectTransfer(firstProposal(engine.state()).id);

                require(result.success, "Valid transfer rejection failed");
                requireEqual(firstProposal(engine.state()).status, TransferStatus::Rejected, "Proposal was not rejected");
                requireEqual(engine.state().club.finance.cashPence, openingFinance.cashPence, "Rejection changed cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence,
                    "Rejection changed transfer budget");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Rejection changed wage commitment");
                requireEqual(
                    engine.state().club.manager.boardConfidence,
                    openingConfidence,
                    "A single rejection changed confidence before the recruitment plan review");
            }
        },
        {
            "Transfer approval fails atomically when the budget is insufficient",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                engine.mutableState().club.finance.transferBudgetPence = firstProposal(engine.state()).feePence - 1;
                const auto openingFinance = engine.state().club.finance;
                const int openingConfidence = engine.state().club.manager.boardConfidence;

                const auto result = engine.approveTransfer(firstProposal(engine.state()).id);

                require(!result.success, "Insufficient budget was accepted");
                requireEqual(firstProposal(engine.state()).status, TransferStatus::Pending, "Failed approval changed proposal status");
                requireEqual(engine.state().club.finance.cashPence, openingFinance.cashPence, "Failed approval changed cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence,
                    "Failed approval changed transfer budget");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Failed approval changed wages");
                requireEqual(engine.state().club.manager.boardConfidence, openingConfidence, "Failed approval changed confidence");
            }
        },
        {
            "Unanswered transfer proposal expires after its deadline",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const int openingConfidence = engine.state().club.manager.boardConfidence;

                engine.advanceDays(10);

                requireEqual(firstProposal(engine.state()).status, TransferStatus::Expired, "Proposal did not expire");
                requireEqual(
                    engine.state().club.manager.boardConfidence,
                    openingConfidence,
                    "A single expiry changed confidence before the recruitment plan review");
                require(!hasPlayer(engine.state(), "alan-shearer"), "Expired proposal added the player");
            }
        },
        {
            "A recommended target advances through club, player, owner, and registration stages",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const Money openingBudget = engine.state().club.finance.transferBudgetPence;

                const auto started = engine.startTransferPursuit("dion-dublin");
                require(started.success, "Could not start a recommended-player pursuit");
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::AwaitingClubResponse,
                    "Pursuit did not wait for the selling club");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingBudget,
                    "Starting talks reserved the transfer budget too early");

                engine.advanceDays(1);
                const auto& countered = pursuitFor(engine.state(), "dion-dublin");
                requireEqual(
                    countered.stage,
                    TransferPursuitStage::ClubCountered,
                    "Selling club did not counter the low opening offer");

                const auto counterAccepted = engine.acceptClubCounter(countered.id);
                require(counterAccepted.success, "Could not accept the selling club counteroffer");
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::AwaitingPlayerTerms,
                    "Accepted club terms did not start player talks");

                const auto playerTerms =
                    pursuitFor(engine.state(), "dion-dublin").playerOfferTerms;
                const auto playerOffer = engine.submitPlayerContractOffer(
                    countered.id,
                    playerTerms);
                require(playerOffer.success, "Could not send recommended player terms");
                engine.advanceDays(1);
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::AwaitingMedical,
                    "Accepted player terms did not schedule a medical");
                engine.advanceDays(1);
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::AwaitingRegistration,
                    "Passed medical did not start registration checks");
                engine.advanceDays(1);
                const auto& awaitingOwner = pursuitFor(engine.state(), "dion-dublin");
                requireEqual(
                    awaitingOwner.stage,
                    TransferPursuitStage::AwaitingOwnerApproval,
                    "Player agreement did not produce owner approval");
                require(!awaitingOwner.linkedProposalId.empty(), "Owner approval has no linked proposal");
                const auto& negotiatedProposal = proposalById(
                    engine.state(),
                    awaitingOwner.linkedProposalId);
                requireEqual(
                    negotiatedProposal.status,
                    TransferStatus::Pending,
                    "Negotiated proposal is not pending");

                const auto approved = engine.approveTransfer(negotiatedProposal.id);
                require(approved.success, "Owner could not approve negotiated terms");
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::AwaitingCompletion,
                    "Approved pursuit did not wait for completion");

                engine.advanceDays(3);
                require(hasPlayer(engine.state(), "dion-dublin"), "Negotiated signing did not join the squad");
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::Completed,
                    "Completed signing did not close the pursuit");
                require(
                    std::any_of(
                        engine.state().playerContracts.begin(),
                        engine.state().playerContracts.end(),
                        [](const PlayerContract& contract) {
                            return contract.playerId == "dion-dublin"
                                && contract.sourceProposalId.starts_with("negotiated-");
                        }),
                    "Completed signing did not create a player contract");
            }
        },
        {
            "An accepted sale removes the player and credits the fee exactly once",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const std::string offerId = "norwich-mark-robins-1992-opening";
                const Money openingCash = engine.state().club.finance.cashPence;
                const Money openingBudget = engine.state().club.finance.transferBudgetPence;
                const Money openingWages = engine.state().club.finance.weeklyWageCommitmentPence;
                const Money fee = outgoingOffer(engine.state(), offerId).feePence;

                const auto accepted = engine.acceptOutgoingTransfer(offerId);
                require(accepted.success, "Valid outgoing offer could not be accepted");
                requireEqual(
                    outgoingOffer(engine.state(), offerId).status,
                    OutgoingTransferStatus::Accepted,
                    "Sale was not marked accepted");
                require(hasPlayer(engine.state(), "mark-robins"), "Player left before completion");

                engine.advanceDays(2);
                require(hasPlayer(engine.state(), "mark-robins"), "Player left before the agreed sale date");
                engine.advanceDays(1);
                require(!hasPlayer(engine.state(), "mark-robins"), "Completed sale left the player in the squad");
                requireEqual(
                    outgoingOffer(engine.state(), offerId).status,
                    OutgoingTransferStatus::Completed,
                    "Sale did not complete");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingCash + fee,
                    "Sale fee was not credited to cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingBudget + fee,
                    "Sale fee was not credited to the transfer budget");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingWages - 300'000,
                    "Completed sale did not release the saved wage commitment");

                engine.advanceDays(2);
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingCash + fee,
                    "Completed sale fee was credited more than once");
            }
        },
        {
            "An improved club offer reaches a viable compromise without spending early",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const Money openingBudget = engine.state().club.finance.transferBudgetPence;

                require(
                    engine.startTransferPursuit("les-ferdinand").success,
                    "Could not start Ferdinand pursuit");
                engine.advanceDays(1);
                const auto& countered = pursuitFor(engine.state(), "les-ferdinand");
                requireEqual(
                    countered.stage,
                    TransferPursuitStage::ClubCountered,
                    "Ferdinand pursuit did not receive a counter");

                require(
                    engine.improveClubOffer(countered.id).success,
                    "Could not submit an improved club offer");
                requireEqual(
                    pursuitFor(engine.state(), "les-ferdinand").stage,
                    TransferPursuitStage::AwaitingClubResponse,
                    "Improved offer did not return to club review");
                engine.advanceDays(1);
                requireEqual(
                    pursuitFor(engine.state(), "les-ferdinand").stage,
                    TransferPursuitStage::AwaitingPlayerTerms,
                    "Viable improved offer was not accepted by the club");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingBudget,
                    "Improved offer spent budget before owner approval");
            }
        },
        {
            "Detailed transfer terms separate immediate cash from future commitments",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const TransferFeeTerms clubTerms {
                    60'000'000,
                    45'000'000,
                    2,
                    10'000'000,
                    30,
                    5
                };
                require(
                    engine.startTransferPursuit("dion-dublin", clubTerms).success,
                    "Could not start a detailed Dublin offer");
                engine.advanceDays(1);
                const auto& playerTermsStage =
                    pursuitFor(engine.state(), "dion-dublin");
                requireEqual(
                    playerTermsStage.stage,
                    TransferPursuitStage::AwaitingPlayerTerms,
                    "Viable detailed club terms were not accepted");

                const PlayerContractTerms contractTerms {
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
                require(
                    engine.submitPlayerContractOffer(
                        playerTermsStage.id,
                        contractTerms).success,
                    "Could not submit detailed player terms");
                engine.advanceDays(3);

                const auto& awaitingOwner =
                    pursuitFor(engine.state(), "dion-dublin");
                requireEqual(
                    awaitingOwner.stage,
                    TransferPursuitStage::AwaitingOwnerApproval,
                    "Detailed deal did not reach final approval");
                const auto& proposal =
                    proposalById(engine.state(), awaitingOwner.linkedProposalId);
                const Money openingCash =
                    engine.state().club.finance.cashPence;
                const Money openingBudget =
                    engine.state().club.finance.transferBudgetPence;
                const Money expectedImmediate =
                    immediateTransferCashCost(clubTerms, contractTerms);
                const Money expectedCommitment =
                    transferBudgetCommitment(clubTerms, contractTerms);
                const Money openingAvailableCash =
                    availableTransferCash(
                        engine.state(),
                        {},
                        proposal.id);

                require(
                    engine.approveTransfer(proposal.id).success,
                    "Could not approve detailed transfer terms");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingCash - expectedImmediate,
                    "Approval charged the full future fee instead of immediate cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingBudget - expectedCommitment,
                    "Maximum transfer commitment was not reserved");
                requireEqual(
                    availableTransferCash(engine.state()),
                    openingAvailableCash - expectedCommitment,
                    "Conditional and scheduled exposure was not reserved from cash");

                const auto firstInstallment = std::find_if(
                    engine.state().transferPayments.begin(),
                    engine.state().transferPayments.end(),
                    [](const TransferPayment& payment) {
                        return payment.category == "installment"
                            && payment.direction
                                == TransferPaymentDirection::Payable;
                    });
                require(
                    firstInstallment != engine.state().transferPayments.end(),
                    "Future installment schedule was not created");
                const GameDate firstDueDate = firstInstallment->dueDate;
                const Money firstAmount = firstInstallment->amountPence;
                const int daysToPayment =
                    engine.state().currentDate.daysUntil(firstDueDate);
                require(daysToPayment > 0, "Installment was not scheduled in the future");
                engine.advanceDays(daysToPayment - 1);
                const Money cashBeforeDue =
                    engine.state().club.finance.cashPence;
                engine.advanceDays(1);
                requireEqual(
                    engine.state().club.finance.cashPence,
                    cashBeforeDue - firstAmount,
                    "Installment was not deducted on its due date");
            }
        },
        {
            "Player and agent can counter a weak contract offer",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                require(
                    engine.startTransferPursuit("dion-dublin").success,
                    "Could not start Dublin talks");
                engine.advanceDays(1);
                const std::string pursuitId =
                    pursuitFor(engine.state(), "dion-dublin").id;
                require(
                    engine.acceptClubCounter(pursuitId).success,
                    "Could not agree the club fee");

                const PlayerContractTerms weakTerms {
                    250'000,
                    4,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    "Rotation"
                };
                require(
                    engine.submitPlayerContractOffer(
                        pursuitId,
                        weakTerms).success,
                    "Weak but valid player offer could not be sent");
                engine.advanceDays(1);
                const auto& countered =
                    pursuitFor(engine.state(), "dion-dublin");
                requireEqual(
                    countered.stage,
                    TransferPursuitStage::PlayerCountered,
                    "Player side did not counter weak terms");
                require(
                    countered.playerCounterTerms.weeklyWagePence
                        > weakTerms.weeklyWagePence,
                    "Player counter did not improve the wage");
                require(
                    engine.acceptPlayerCounter(pursuitId).success,
                    "Could not accept player counterterms");
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::AwaitingMedical,
                    "Accepted player counter did not schedule a medical");
            }
        },
        {
            "Failed medical ends the deal before any money is spent",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                require(
                    engine.startTransferPursuit("dion-dublin").success,
                    "Could not start medical-failure setup");
                engine.advanceDays(1);
                const std::string pursuitId =
                    pursuitFor(engine.state(), "dion-dublin").id;
                require(
                    engine.acceptClubCounter(pursuitId).success,
                    "Could not accept the club counter");
                const auto terms =
                    pursuitFor(engine.state(), "dion-dublin").playerOfferTerms;
                require(
                    engine.submitPlayerContractOffer(pursuitId, terms).success,
                    "Could not submit player terms");
                engine.advanceDays(1);
                auto pursuit = std::find_if(
                    engine.mutableState().transferPursuits.begin(),
                    engine.mutableState().transferPursuits.end(),
                    [&pursuitId](const TransferPursuit& item) {
                        return item.id == pursuitId;
                    });
                require(
                    pursuit != engine.mutableState().transferPursuits.end(),
                    "Medical pursuit disappeared");
                pursuit->medicalRiskPercent = 100;
                const auto openingFinance = engine.state().club.finance;

                engine.advanceDays(1);
                requireEqual(
                    pursuitFor(engine.state(), "dion-dublin").stage,
                    TransferPursuitStage::MedicalFailed,
                    "Forced medical risk did not fail");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Failed medical changed cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence,
                    "Failed medical changed transfer budget");
            }
        },
        {
            "Outgoing counteroffer creates separate sale receipts",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const std::string offerId =
                    "norwich-mark-robins-1992-opening";
                const TransferFeeTerms requested {
                    90'000'000,
                    20'000'000,
                    1,
                    10'000'000,
                    25,
                    10
                };
                require(
                    engine.submitOutgoingTransferCounter(
                        offerId,
                        requested).success,
                    "Could not counter the outgoing offer");
                engine.advanceDays(1);
                const auto& buyerCounter =
                    outgoingOffer(engine.state(), offerId);
                requireEqual(
                    buyerCounter.negotiationStage,
                    OutgoingNegotiationStage::BuyerCountered,
                    "Buyer did not return a counteroffer");
                require(
                    engine.acceptOutgoingTransferCounter(offerId).success,
                    "Could not accept buyer counterterms");

                const TransferFeeTerms agreed =
                    outgoingOffer(engine.state(), offerId).feeTerms;
                const Money openingCash =
                    engine.state().club.finance.cashPence;
                const Money openingBudget =
                    engine.state().club.finance.transferBudgetPence;
                require(
                    engine.acceptOutgoingTransfer(offerId).success,
                    "Could not approve the countered sale");
                engine.advanceDays(3);
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingCash + agreed.upfrontFeePence,
                    "Sale credited more than the immediate receipt");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingBudget + guaranteedTransferFee(agreed),
                    "Sale did not credit the guaranteed transfer budget");

                const auto futureReceipt = std::find_if(
                    engine.state().transferPayments.begin(),
                    engine.state().transferPayments.end(),
                    [](const TransferPayment& payment) {
                        return payment.category == "installment"
                            && payment.direction
                                == TransferPaymentDirection::Receivable;
                    });
                require(
                    futureReceipt != engine.state().transferPayments.end(),
                    "Sale installment receipt was not scheduled");
                const GameDate dueDate = futureReceipt->dueDate;
                const Money dueAmount = futureReceipt->amountPence;
                const int daysToReceipt =
                    engine.state().currentDate.daysUntil(dueDate);
                engine.advanceDays(daysToReceipt);
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingCash + agreed.upfrontFeePence + dueAmount,
                    "Sale installment was not received on schedule");
            }
        },
        {
            "A negotiated sell-on clause is paid when the signed player is sold",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const TransferFeeTerms incomingFee {
                    60'000'000,
                    45'000'000,
                    2,
                    10'000'000,
                    30,
                    10
                };
                require(
                    engine.startTransferPursuit(
                        "dion-dublin",
                        incomingFee).success,
                    "Could not start sell-on setup");
                engine.advanceDays(1);
                const std::string pursuitId =
                    pursuitFor(engine.state(), "dion-dublin").id;
                const PlayerContractTerms contractTerms {
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
                require(
                    engine.submitPlayerContractOffer(
                        pursuitId,
                        contractTerms).success,
                    "Could not submit sell-on setup contract");
                engine.advanceDays(3);
                const auto& pursuit =
                    pursuitFor(engine.state(), "dion-dublin");
                requireEqual(
                    pursuit.stage,
                    TransferPursuitStage::AwaitingOwnerApproval,
                    "Sell-on setup did not reach owner approval");
                require(
                    engine.approveTransfer(
                        pursuit.linkedProposalId).success,
                    "Could not approve sell-on setup");
                engine.advanceDays(3);
                require(
                    hasPlayer(engine.state(), "dion-dublin"),
                    "Sell-on setup player did not join");

                const TransferFeeTerms saleTerms {
                    200'000'000,
                    0,
                    0,
                    0,
                    0,
                    0
                };
                engine.mutableState().outgoingTransferOffers.push_back({
                    "blackburn-dion-dublin-test",
                    "dion-dublin",
                    "Dion Dublin",
                    "blackburn",
                    "Blackburn Rovers",
                    guaranteedTransferFee(saleTerms),
                    contractTerms.weeklyWagePence,
                    engine.state().currentDate,
                    engine.state().currentDate.addDays(7),
                    std::nullopt,
                    std::nullopt,
                    OutgoingTransferStatus::Pending,
                    "First Team",
                    "Sell-on clause regression",
                    "test_fixture",
                    saleTerms,
                    {},
                    OutgoingNegotiationStage::DecisionPending,
                    0,
                    std::nullopt,
                    guaranteedTransferFee(saleTerms)
                });

                const Money cashBeforeSale =
                    engine.state().club.finance.cashPence;
                const Money budgetBeforeSale =
                    engine.state().club.finance.transferBudgetPence;
                require(
                    engine.acceptOutgoingTransfer(
                        "blackburn-dion-dublin-test").success,
                    "Could not approve sale with sell-on clause");
                engine.advanceDays(3);

                const Money expectedSellOn =
                    guaranteedTransferFee(saleTerms) / 10;
                requireEqual(
                    engine.state().club.finance.cashPence,
                    cashBeforeSale
                        + saleTerms.upfrontFeePence
                        - expectedSellOn,
                    "Sell-on obligation was not deducted from sale cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    budgetBeforeSale
                        + guaranteedTransferFee(saleTerms)
                        - expectedSellOn,
                    "Sell-on obligation was not removed from sale budget");
                require(
                    std::any_of(
                        engine.state().transferPayments.begin(),
                        engine.state().transferPayments.end(),
                        [expectedSellOn](const TransferPayment& payment) {
                            return payment.category == "sell_on"
                                && payment.direction
                                    == TransferPaymentDirection::Payable
                                && payment.amountPence == expectedSellOn
                                && payment.status
                                    == TransferPaymentStatus::Settled;
                        }),
                    "Settled sell-on payment record is missing");
            }
        },
        {
            "Maximum transfer exposure cannot exceed available club cash",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                engine.mutableState().club.finance.transferBudgetPence =
                    1'000'000'000;
                const TransferFeeTerms doubleCommittedTerms {
                    310'000'000,
                    0,
                    0,
                    0,
                    0,
                    0
                };
                require(
                    !engine.startTransferPursuit(
                        "dion-dublin",
                        doubleCommittedTerms).success,
                    "A second negotiation reused cash reserved by Shearer");
                require(
                    engine.rejectTransfer(firstProposal(engine.state()).id).success,
                    "Could not clear the opening approval case");
                const TransferFeeTerms clubTerms {
                    60'000'000,
                    45'000'000,
                    2,
                    10'000'000,
                    30,
                    5
                };
                require(
                    engine.startTransferPursuit(
                        "dion-dublin",
                        clubTerms).success,
                    "Could not start cash-cap setup");
                engine.advanceDays(1);
                const std::string pursuitId =
                    pursuitFor(engine.state(), "dion-dublin").id;
                const PlayerContractTerms contractTerms {
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
                require(
                    engine.submitPlayerContractOffer(
                        pursuitId,
                        contractTerms).success,
                    "Could not submit cash-cap contract");
                engine.advanceDays(3);
                const auto& awaitingOwner =
                    pursuitFor(engine.state(), "dion-dublin");
                const auto& proposal = proposalById(
                    engine.state(),
                    awaitingOwner.linkedProposalId);
                const Money immediate =
                    immediateTransferCashCost(clubTerms, contractTerms);
                const Money maximum =
                    transferBudgetCommitment(clubTerms, contractTerms);
                require(immediate < maximum, "Test deal has no future exposure");
                engine.mutableState().club.finance.cashPence = immediate;
                const auto openingFinance = engine.state().club.finance;

                const auto result = engine.approveTransfer(proposal.id);

                require(!result.success, "Future commitments exceeded club cash");
                requireEqual(
                    proposalById(engine.state(), proposal.id).status,
                    TransferStatus::Pending,
                    "Failed cash-cap approval changed proposal status");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Failed cash-cap approval changed cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence,
                    "Failed cash-cap approval changed budget");
            }
        },
        {
            "Scouting assignments produce intermediate and final reports",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const int startingKnowledge =
                    scoutingReportFor(engine.state(), "dion-dublin").knowledgeLevel;
                const auto started = engine.startScoutingAssignment(
                    "dion-dublin",
                    "les-kershaw",
                    ScoutingDepth::Standard,
                    "overall");
                require(started.success, "Could not assign a standard scout report");
                requireEqual(
                    engine.state().scoutingAssignments.size(),
                    std::size_t {1},
                    "Scouting assignment was not created");
                const int adjustedDuration =
                    engine.state().currentDate.daysUntil(
                        engine.state().scoutingAssignments.front().dueDate);
                const int expectedTargetKnowledge =
                    engine.state().scoutingAssignments.front().targetKnowledge;
                require(
                    adjustedDuration
                        < scoutingDurationDays(ScoutingDepth::Standard),
                    "Scout familiarity did not shorten the observation duration");
                require(
                    expectedTargetKnowledge
                        > std::min(
                            100,
                            startingKnowledge
                                + scoutingKnowledgeGain(
                                    ScoutingDepth::Standard)),
                    "Scout familiarity did not improve target knowledge");

                engine.advanceDays(3);
                const int intermediateKnowledge =
                    scoutingReportFor(engine.state(), "dion-dublin").knowledgeLevel;
                require(
                    intermediateKnowledge > startingKnowledge
                        && intermediateKnowledge < 90,
                    "Intermediate scouting report did not improve knowledge");

                engine.advanceDays(4);
                const auto& finalReport =
                    scoutingReportFor(engine.state(), "dion-dublin");
                requireEqual(
                    finalReport.knowledgeLevel,
                    expectedTargetKnowledge,
                    "Final scouting knowledge did not use familiarity-adjusted target");
                requireEqual(
                    engine.state().scoutingAssignments.front().status,
                    ScoutingAssignmentStatus::Completed,
                    "Scouting assignment did not complete");
                require(
                    finalReport.currentAbilityHigh
                        - finalReport.currentAbilityLow
                        < 30,
                    "Final scouting range did not narrow");
            }
        },
        {
            "Scout workload limits concurrent observations",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                require(
                    engine.startScoutingAssignment(
                        "dion-dublin",
                        "les-kershaw",
                        ScoutingDepth::Quick,
                        "overall").success,
                    "First scouting slot was unavailable");
                require(
                    engine.startScoutingAssignment(
                        "teddy-sheringham",
                        "les-kershaw",
                        ScoutingDepth::Quick,
                        "potential").success,
                    "Second scouting slot was unavailable");
                const auto third = engine.startScoutingAssignment(
                    "eric-cantona",
                    "les-kershaw",
                    ScoutingDepth::Quick,
                    "availability");
                require(third.success == false, "Scout exceeded concurrent capacity");
            }
        },
        {
            "Scouting directives create assignments and grow familiarity",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
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
                require(directive.success, "Range scouting directive was rejected");
                requireEqual(
                    engine.state().scoutingDirectives.size(),
                    std::size_t {1},
                    "Range directive was not recorded");
                requireEqual(
                    engine.state().scoutingAssignments.size(),
                    std::size_t {1},
                    "Range directive did not create an observation assignment");
                require(
                    engine.state().scoutingDirectives.front().matchedCandidateCount
                        >= 1,
                    "Range directive did not record matched candidates");
                require(
                    engine.state().scoutingDirectives.front().estimatedDurationDays
                        < scoutingDurationDays(ScoutingDepth::Standard),
                    "Range directive did not record familiarity-adjusted duration");
                require(
                    engine.state().scoutingDirectives.front().accuracyModifier
                        > 0,
                    "Range directive did not record familiarity accuracy modifier");

                engine.advanceDays(7);
                requireEqual(
                    engine.state().scoutingDirectives.front().status,
                    ScoutingDirectiveStatus::Completed,
                    "Range directive did not complete with its assignment");
                require(
                    scoutFamiliarityLevel(
                        engine.state(),
                        "les-kershaw",
                        "country",
                        "England")
                        > openingEngland,
                    "Completed scouting directive did not improve country familiarity");
                require(
                    scoutFamiliarityLevel(
                        engine.state(),
                        "les-kershaw",
                        "region",
                        "England")
                        > openingEnglandRegion,
                    "Completed scouting directive did not improve region familiarity");
            }
        },
        {
            "Recruitment confidence is graded by the overall plan",
            [] {
                const auto markApproved = [](GameState& state, const std::string& playerId) {
                    const auto candidate = std::find_if(
                        state.transferCandidates.begin(),
                        state.transferCandidates.end(),
                        [&playerId](const TransferCandidate& item) {
                            return item.player.id == playerId;
                        });
                    require(
                        candidate != state.transferCandidates.end(),
                        "Recruitment-plan candidate is missing");
                    if (playerId == "alan-shearer") {
                        state.transferProposals.front().status =
                            TransferStatus::Approved;
                        state.transferProposals.front().decisionDate =
                            state.currentDate;
                        state.transferProposals.front().joinDate.reset();
                        return;
                    }
                    TransferProposal proposal =
                        state.transferProposals.front();
                    proposal.id = "plan-test-" + playerId;
                    proposal.target = candidate->player;
                    proposal.fromClubId = candidate->currentClubId;
                    proposal.fromClubName = candidate->currentClubName;
                    proposal.status = TransferStatus::Approved;
                    proposal.decisionDate = state.currentDate;
                    proposal.joinDate.reset();
                    state.transferProposals.push_back(std::move(proposal));
                };

                SimulationEngine acceptable(makeManchesterUnitedPrototype());
                const int acceptableOpening =
                    acceptable.state().club.manager.boardConfidence;
                markApproved(acceptable.mutableState(), "alan-shearer");
                markApproved(acceptable.mutableState(), "dion-dublin");
                acceptable.advanceDays(45);
                requireEqual(
                    acceptable.state().recruitmentPlans.front().status,
                    RecruitmentPlanStatus::Acceptable,
                    "Two of three signings were not treated as acceptable");
                requireEqual(
                    acceptable.state().club.manager.boardConfidence,
                    acceptableOpening,
                    "Acceptable partial recruitment lowered confidence");

                SimulationEngine partial(makeManchesterUnitedPrototype());
                const int partialOpening =
                    partial.state().club.manager.boardConfidence;
                markApproved(partial.mutableState(), "dion-dublin");
                partial.advanceDays(45);
                requireEqual(
                    partial.state().recruitmentPlans.front().status,
                    RecruitmentPlanStatus::UnderDelivered,
                    "One signing was not treated as partial delivery");
                requireEqual(
                    partial.state().club.manager.boardConfidence,
                    partialOpening - 1,
                    "Partial delivery received the wrong confidence change");

                SimulationEngine failed(makeManchesterUnitedPrototype());
                const int failedOpening =
                    failed.state().club.manager.boardConfidence;
                failed.advanceDays(45);
                requireEqual(
                    failed.state().recruitmentPlans.front().status,
                    RecruitmentPlanStatus::Failed,
                    "Zero signings did not fail the plan");
                requireEqual(
                    failed.state().club.manager.boardConfidence,
                    failedOpening - 4,
                    "Complete recruitment failure received the wrong penalty");
            }
        },
        {
            "Outgoing loan applies fee and wage relief then recalls exactly once",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const std::string loanId {
                    "cambridge-kieran-toal-1992-opening"};
                const auto openingFinance = engine.state().club.finance;
                const Money wageShare =
                    contractForPlayer(engine.state(), "kieran-toal")
                        .weeklyWagePence
                    * 60 / 100;

                require(
                    engine.acceptLoanOffer(loanId).success,
                    "Could not accept the opening outgoing loan");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Outgoing loan fee was credited before registration");

                engine.advanceDays(3);
                requireEqual(
                    loanById(engine.state(), loanId).stage,
                    LoanStage::Active,
                    "Outgoing loan did not become active");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence + 500'000,
                    "Outgoing loan fee was not credited once");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence - wageShare,
                    "Outgoing loan wage relief was not applied");
                const auto loanedPlayer = std::find_if(
                    engine.state().players.begin(),
                    engine.state().players.end(),
                    [](const Player& player) {
                        return player.id == "kieran-toal";
                    });
                require(
                    loanedPlayer != engine.state().players.end(),
                    "Outgoing loanee disappeared from club registration");
                requireEqual(
                    loanedPlayer->squadGroup,
                    std::string {"Loan"},
                    "Outgoing loanee did not move to the loan group");

                require(
                    engine.recallLoan(loanId).success,
                    "Could not request the agreed recall");
                engine.advanceDays(7);
                requireEqual(
                    loanById(engine.state(), loanId).stage,
                    LoanStage::Recalled,
                    "Recall notice did not return the player");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Player wage was not restored after recall");
                requireEqual(
                    loanedPlayer->squadGroup,
                    std::string {"Reserve"},
                    "Recalled player did not return to the original squad group");

                const auto settledCash =
                    engine.state().club.finance.cashPence;
                engine.advanceDays(1);
                requireEqual(
                    engine.state().club.finance.cashPence,
                    settledCash,
                    "Closed outgoing loan credited the fee twice");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Closed outgoing loan restored wages twice");
            }
        },
        {
            "Incoming loan negotiates, registers, and leaves without duplicate charges",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const LoanTerms terms {
                    1'700'000,
                    100,
                    "Squad Player",
                    {1992, 7, 30},
                    true,
                    7,
                    "option",
                    105'000'000,
                    true,
                    0,
                    "loan_club"
                };
                const auto openingFinance = engine.state().club.finance;

                require(
                    engine.startIncomingLoan(
                        "dion-dublin",
                        terms).success,
                    "Could not open a valid incoming loan");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Incoming loan spent money during negotiation");

                engine.advanceDays(1);
                const std::string loanId =
                    loanForPlayer(engine.state(), "dion-dublin").id;
                requireEqual(
                    loanById(engine.state(), loanId).stage,
                    LoanStage::AwaitingStart,
                    "Matching incoming terms were not accepted");
                engine.advanceDays(3);
                requireEqual(
                    loanById(engine.state(), loanId).stage,
                    LoanStage::Active,
                    "Incoming loan did not register");
                require(
                    hasPlayer(engine.state(), "dion-dublin"),
                    "Incoming loanee was not added to the squad");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence - terms.loanFeePence,
                    "Incoming loan fee was not charged once");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence + 500'000,
                    "Incoming loan wage share was not applied");

                engine.advanceDays(25);
                requireEqual(
                    loanById(engine.state(), loanId).stage,
                    LoanStage::Completed,
                    "Incoming loan did not end on the agreed date");
                require(
                    !hasPlayer(engine.state(), "dion-dublin"),
                    "Completed incoming loanee stayed in the squad");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Incoming loan wage share was not released");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence - terms.loanFeePence,
                    "Incoming loan end refunded or duplicated the fee");
            }
        },
        {
            "Loan finance limits reject an unaffordable deal atomically",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const auto openingFinance = engine.state().club.finance;
                const std::size_t openingLoans =
                    engine.state().loanDeals.size();
                const LoanTerms unaffordable {
                    1'000'000,
                    100,
                    "Regular Starter",
                    {1993, 5, 31},
                    false,
                    0,
                    "mandatory",
                    400'000'000,
                    true,
                    0,
                    "loan_club"
                };

                const auto result =
                    engine.startIncomingLoan(
                        "dion-dublin",
                        unaffordable);

                require(
                    !result.success,
                    "Unaffordable incoming loan was accepted");
                requireEqual(
                    engine.state().loanDeals.size(),
                    openingLoans,
                    "Failed incoming loan created a negotiation");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Failed incoming loan changed cash");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence,
                    "Failed incoming loan changed transfer budget");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Failed incoming loan changed wages");
            }
        },
        {
            "Renewal counteroffer and final signature update the contract and finance",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const auto openingFinance = engine.state().club.finance;
                const auto oldContract =
                    contractForPlayer(engine.state(), "bryan-robson");
                const PlayerContractTerms weakTerms {
                    650'000,
                    2,
                    2'000'000,
                    1'000'000,
                    50'000,
                    25'000,
                    2'000'000,
                    0,
                    "Key Player"
                };

                require(
                    engine.submitContractRenewal(
                        "bryan-robson",
                        weakTerms).success,
                    "Could not submit a viable renewal offer");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Renewal negotiation spent cash before signature");
                engine.advanceDays(1);
                const std::string renewalId =
                    renewalForPlayer(
                        engine.state(),
                        "bryan-robson").id;
                requireEqual(
                    renewalForPlayer(
                        engine.state(),
                        "bryan-robson").stage,
                    ContractRenewalStage::PlayerCountered,
                    "Weak but viable renewal did not produce a counteroffer");
                require(
                    engine.acceptContractRenewalCounter(
                        renewalId).success,
                    "Could not accept the player's renewal counteroffer");
                const auto agreed =
                    renewalForPlayer(
                        engine.state(),
                        "bryan-robson").offerTerms;
                require(
                    engine.confirmContractRenewal(
                        renewalId).success,
                    "Could not sign the agreed renewal");

                const auto& renewed =
                    contractForPlayer(engine.state(), "bryan-robson");
                requireEqual(
                    renewalForPlayer(
                        engine.state(),
                        "bryan-robson").stage,
                    ContractRenewalStage::Completed,
                    "Signed renewal did not complete");
                requireEqual(
                    renewed.weeklyWagePence,
                    agreed.weeklyWagePence,
                    "Signed renewal did not update weekly wage");
                requireEqual(
                    renewed.endDate,
                    engine.state().currentDate.addMonths(
                        agreed.contractYears * 12),
                    "Signed renewal did not update the contract end date");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence
                        - agreed.signingBonusPence
                        - agreed.agentFeePence,
                    "Renewal immediate costs were not charged once");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence
                        - oldContract.weeklyWagePence
                        + agreed.weeklyWagePence,
                    "Renewal wage delta was not applied");
                require(
                    std::any_of(
                        engine.state().transferPayments.begin(),
                        engine.state().transferPayments.end(),
                        [&renewalId, &agreed](
                            const TransferPayment& payment) {
                            return payment.sourceId == renewalId
                                && payment.category
                                    == "renewal_loyalty_bonus"
                                && payment.amountPence
                                    == agreed.loyaltyBonusPence
                                && payment.status
                                    == TransferPaymentStatus::Scheduled;
                        }),
                    "Renewal loyalty bonus was not scheduled");
            }
        },
        {
            "Renewal cash and wage caps leave the existing contract untouched",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const auto openingContract =
                    contractForPlayer(engine.state(), "bryan-robson");
                const auto openingFinance = engine.state().club.finance;
                const auto recommended =
                    renewalForPlayer(
                        engine.state(),
                        "bryan-robson").recommendedTerms;
                engine.mutableState().club.finance.cashPence =
                    contractRenewalCommitment(recommended) - 1;

                const auto result =
                    engine.submitContractRenewal(
                        "bryan-robson",
                        recommended);

                require(
                    !result.success,
                    "Renewal above available cash was accepted");
                const auto& unchanged =
                    contractForPlayer(engine.state(), "bryan-robson");
                requireEqual(
                    unchanged.endDate,
                    openingContract.endDate,
                    "Failed renewal changed contract end date");
                requireEqual(
                    unchanged.weeklyWagePence,
                    openingContract.weeklyWagePence,
                    "Failed renewal changed weekly wage");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence,
                    "Failed renewal changed transfer budget");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Failed renewal changed wage commitment");
            }
        },
        {
            "Renewing an outgoing loanee preserves the agreed wage split",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const std::string loanId {
                    "cambridge-kieran-toal-1992-opening"};
                const Money openingWages =
                    engine.state().club.finance.weeklyWageCommitmentPence;
                require(
                    engine.acceptLoanOffer(loanId).success,
                    "Could not start the outgoing loan for renewal testing");
                engine.advanceDays(3);

                const PlayerContractTerms renewalTerms {
                    200'000,
                    2,
                    100'000,
                    100'000,
                    5'000,
                    5'000,
                    100'000,
                    0,
                    "Prospect"
                };
                require(
                    engine.submitContractRenewal(
                        "kieran-toal",
                        renewalTerms).success,
                    "Could not offer a renewal to the outgoing loanee");
                engine.advanceDays(1);
                const std::string renewalId =
                    renewalForPlayer(
                        engine.state(),
                        "kieran-toal").id;
                requireEqual(
                    renewalForPlayer(
                        engine.state(),
                        "kieran-toal").stage,
                    ContractRenewalStage::AgreementReached,
                    "Outgoing loanee did not agree to the recommended renewal");
                require(
                    engine.confirmContractRenewal(
                        renewalId).success,
                    "Could not sign the outgoing loanee renewal");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingWages - 160'000 * 60 / 100
                        - 160'000 * 40 / 100
                        + 200'000 * 40 / 100,
                    "Renewal ignored the borrowing club's wage share");

                require(
                    engine.recallLoan(loanId).success,
                    "Could not recall the renewed outgoing loanee");
                engine.advanceDays(7);
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingWages - 160'000 + 200'000,
                    "Recall did not restore the renewed full wage exactly once");
            }
        },
        {
            "The 1992 registration cutoff blocks new loans after March",
            [] {
                auto state = makeManchesterUnitedPrototype();
                state.currentDate = {1993, 4, 1};
                SimulationEngine engine(std::move(state));
                const auto openingFinance = engine.state().club.finance;
                const std::size_t openingLoans =
                    engine.state().loanDeals.size();
                const LoanTerms terms {
                    1'700'000,
                    100,
                    "Squad Player",
                    {1993, 5, 31},
                    false,
                    0,
                    "none",
                    0,
                    true,
                    0,
                    "loan_club"
                };

                const auto result =
                    engine.startIncomingLoan("dion-dublin", terms);

                require(
                    !result.success,
                    "Post-cutoff incoming loan negotiation was opened");
                requireEqual(
                    engine.state().loanDeals.size(),
                    openingLoans,
                    "Post-cutoff attempt created a loan row");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence,
                    "Post-cutoff attempt changed cash");
                requireEqual(
                    engine.state().club.finance.weeklyWageCommitmentPence,
                    openingFinance.weeklyWageCommitmentPence,
                    "Post-cutoff attempt changed wages");
            }
        },
        {
            "Pre-Bosman contract expiry retains the player registration",
            [] {
                auto state = makeManchesterUnitedPrototype();
                auto contract = std::find_if(
                    state.playerContracts.begin(),
                    state.playerContracts.end(),
                    [](const PlayerContract& item) {
                        return item.playerId == "lee-martin";
                    });
                require(
                    contract != state.playerContracts.end(),
                    "Expiry test contract is missing");
                contract->endDate = state.currentDate;
                SimulationEngine engine(std::move(state));

                engine.advanceDays(1);

                require(
                    hasPlayer(engine.state(), "lee-martin"),
                    "Pre-Bosman expiry released the player as a free agent");
                const auto& expired =
                    contractForPlayer(engine.state(), "lee-martin");
                requireEqual(
                    expired.lifecycleStatus,
                    std::string {"expired_retained"},
                    "Pre-Bosman registration rights were not retained");
                require(
                    expired.expiryNotified,
                    "Contract expiry notification flag was not stored");
                const auto player = std::find_if(
                    engine.state().players.begin(),
                    engine.state().players.end(),
                    [](const Player& item) {
                        return item.id == "lee-martin";
                    });
                require(
                    player != engine.state().players.end(),
                    "Expired player disappeared");
                requireEqual(
                    player->status,
                    std::string {
                        "Contract Expired · Registration Retained"},
                    "Expired player status ignored the 1992 rule profile");
            }
        },
        {
            "AI transfer market is deterministic for the same seed",
            [] {
                SimulationEngine left(makeManchesterUnitedPrototype());
                SimulationEngine right(makeManchesterUnitedPrototype());

                left.advanceDays(12);
                right.advanceDays(12);

                require(
                    !left.state().aiMarketDeals.empty(),
                    "AI market did not create any deals");
                requireEqual(
                    left.state().aiMarketDeals.size(),
                    right.state().aiMarketDeals.size(),
                    "Same seed produced a different AI deal count");
                for (std::size_t index = 0;
                     index < left.state().aiMarketDeals.size();
                     ++index) {
                    const auto& a = left.state().aiMarketDeals[index];
                    const auto& b = right.state().aiMarketDeals[index];
                    requireEqual(a.id, b.id, "AI deal id changed for same seed");
                    requireEqual(
                        aiMarketDealStageKey(a.stage),
                        aiMarketDealStageKey(b.stage),
                        "AI deal stage changed for same seed");
                    requireEqual(
                        a.buyingClubId,
                        b.buyingClubId,
                        "AI buyer changed for same seed");
                    requireEqual(
                        a.sellingClubId,
                        b.sellingClubId,
                        "AI seller changed for same seed");
                }
            }
        },
        {
            "AI transfer applies atomically on the completion date once",
            [] {
                auto state = makeManchesterUnitedPrototype();
                require(
                    !state.aiClubMarketStates.empty(),
                    "Prototype needs AI club market states");
                for (auto& club : state.aiClubMarketStates) {
                    club.nextEvaluationDate = state.currentDate.addDays(90);
                    club.finance.cashPence = 2'000'000'000;
                    club.finance.transferBudgetPence = 2'000'000'000;
                    club.finance.weeklyWageBudgetPence = 100'000'000;
                    club.finance.weeklyWageCommitmentPence = 10'000'000;
                }
                state.aiClubMarketStates.front().nextEvaluationDate =
                    state.currentDate.addDays(1);

                SimulationEngine engine(std::move(state));
                engine.advanceDays(1);

                const auto proposed = std::find_if(
                    engine.state().aiMarketDeals.begin(),
                    engine.state().aiMarketDeals.end(),
                    [](const AiMarketDeal& deal) {
                        return deal.type == AiMarketDealType::Transfer
                            && deal.stage == AiMarketDealStage::Proposed;
                    });
                require(
                    proposed != engine.state().aiMarketDeals.end(),
                    "AI transfer proposal was not created");
                const std::string dealId = proposed->id;
                const std::string playerId = proposed->player.id;
                const std::string buyerId = proposed->buyingClubId;
                const std::string originalClubId = proposed->sellingClubId;
                const FinanceState buyerFinanceBefore =
                    aiClubById(engine.state(), buyerId).finance;

                engine.advanceDays(1);
                const auto& awaiting = aiDealById(engine.state(), dealId);
                requireEqual(
                    awaiting.stage,
                    AiMarketDealStage::AwaitingCompletion,
                    "AI deal did not move to awaiting completion");
                requireEqual(
                    aiClubById(engine.state(), buyerId).finance.cashPence,
                    buyerFinanceBefore.cashPence,
                    "AI deal spent cash before completion");
                const auto candidateBeforeCompletion = std::find_if(
                    engine.state().transferCandidates.begin(),
                    engine.state().transferCandidates.end(),
                    [&playerId](const TransferCandidate& candidate) {
                        return candidate.player.id == playerId;
                    });
                require(
                    candidateBeforeCompletion
                        != engine.state().transferCandidates.end(),
                    "AI target candidate disappeared before completion");
                requireEqual(
                    candidateBeforeCompletion->currentClubId,
                    originalClubId,
                    "AI transfer moved the player before completion");

                engine.advanceDays(3);
                const auto& completed = aiDealById(engine.state(), dealId);
                requireEqual(
                    completed.stage,
                    AiMarketDealStage::Completed,
                    "AI transfer did not complete on the scheduled date");
                require(
                    completed.completionApplied,
                    "AI completed deal did not record applied state");
                require(
                    aiClubById(engine.state(), buyerId).finance.cashPence
                        < buyerFinanceBefore.cashPence,
                    "AI transfer completion did not spend buyer cash");
                const auto candidateAfterCompletion = std::find_if(
                    engine.state().transferCandidates.begin(),
                    engine.state().transferCandidates.end(),
                    [&playerId](const TransferCandidate& candidate) {
                        return candidate.player.id == playerId;
                    });
                require(
                    candidateAfterCompletion
                        != engine.state().transferCandidates.end(),
                    "AI target candidate disappeared after completion");
                requireEqual(
                    candidateAfterCompletion->currentClubId,
                    buyerId,
                    "AI transfer completion did not move the candidate club");

                const FinanceState buyerFinanceAfter =
                    aiClubById(engine.state(), buyerId).finance;
                engine.advanceDays(1);
                requireEqual(
                    aiClubById(engine.state(), buyerId).finance.cashPence,
                    buyerFinanceAfter.cashPence,
                    "Completed AI transfer applied twice after another day");
            }
        },
        {
            "AI transfer completion respects the 1992 registration cutoff",
            [] {
                auto state = makeManchesterUnitedPrototype();
                state.currentDate = {1993, 3, 31};
                for (auto& club : state.aiClubMarketStates) {
                    club.nextEvaluationDate = state.currentDate.addDays(90);
                    club.finance.cashPence = 2'000'000'000;
                    club.finance.transferBudgetPence = 2'000'000'000;
                    club.finance.weeklyWageBudgetPence = 100'000'000;
                    club.finance.weeklyWageCommitmentPence = 10'000'000;
                }

                const auto candidate = std::find_if(
                    state.transferCandidates.begin(),
                    state.transferCandidates.end(),
                    [](const TransferCandidate& item) {
                        return item.player.id == "dion-dublin";
                    });
                require(
                    candidate != state.transferCandidates.end(),
                    "AI cutoff test candidate is missing");
                const auto buyer = std::find_if(
                    state.aiClubMarketStates.begin(),
                    state.aiClubMarketStates.end(),
                    [&candidate](const AiClubMarketState& club) {
                        return club.clubId != candidate->currentClubId;
                    });
                require(
                    buyer != state.aiClubMarketStates.end(),
                    "AI cutoff test buyer club is missing");

                const std::string dealId = "ai-cutoff-registration-test";
                const std::string playerId = candidate->player.id;
                const std::string originalClubId = candidate->currentClubId;
                const std::string buyerId = buyer->clubId;
                const FinanceState buyerFinanceBefore = buyer->finance;

                AiMarketDeal deal;
                deal.id = dealId;
                deal.type = AiMarketDealType::Transfer;
                deal.stage = AiMarketDealStage::AwaitingCompletion;
                deal.player = candidate->player;
                deal.sellingClubId = candidate->currentClubId;
                deal.sellingClubName = candidate->currentClubName;
                deal.buyingClubId = buyer->clubId;
                deal.buyingClubName = buyer->clubName;
                deal.proposedDate = {1993, 3, 27};
                deal.nextActionDate = {1993, 4, 1};
                deal.completionDate = {1993, 4, 1};
                deal.feeTerms = {75'000'000, 25'000'000, 2, 0, 0, 0};
                deal.contractTerms = {
                    500'000,
                    3,
                    2'000'000,
                    0,
                    0,
                    0,
                    500'000,
                    0,
                    "First Team"};
                deal.rationale = "등록 마감 회귀 검증";
                deal.budgetReserved = true;
                state.aiMarketDeals.clear();
                state.aiMarketDeals.push_back(std::move(deal));

                SimulationEngine engine(std::move(state));
                engine.advanceDays(1);

                const auto& rejected = aiDealById(engine.state(), dealId);
                requireEqual(
                    rejected.stage,
                    AiMarketDealStage::Rejected,
                    "Post-cutoff AI transfer was not rejected");
                require(
                    !rejected.completionApplied,
                    "Post-cutoff AI transfer should not be applied");
                requireEqual(
                    aiClubById(engine.state(), buyerId).finance.cashPence,
                    buyerFinanceBefore.cashPence,
                    "Post-cutoff AI transfer changed buyer cash");
                const auto candidateAfter = std::find_if(
                    engine.state().transferCandidates.begin(),
                    engine.state().transferCandidates.end(),
                    [&playerId](const TransferCandidate& item) {
                        return item.player.id == playerId;
                    });
                require(
                    candidateAfter != engine.state().transferCandidates.end(),
                    "AI cutoff test candidate disappeared");
                requireEqual(
                    candidateAfter->currentClubId,
                    originalClubId,
                    "Post-cutoff AI transfer moved the candidate club");
            }
        },
        {
            "Incoming mandatory loan converts permanently exactly once",
            [] {
                auto state = makeManchesterUnitedPrototype();
                for (auto& club : state.aiClubMarketStates) {
                    club.nextEvaluationDate = state.currentDate.addDays(90);
                    club.finance.cashPence = 2'000'000'000;
                    club.finance.transferBudgetPence = 2'000'000'000;
                    club.finance.weeklyWageBudgetPence = 100'000'000;
                    club.finance.weeklyWageCommitmentPence = 10'000'000;
                }
                SimulationEngine engine(std::move(state));
                const LoanTerms terms {
                    1'700'000,
                    100,
                    "Squad Player",
                    {1992, 7, 30},
                    true,
                    7,
                    "mandatory",
                    105'000'000,
                    true,
                    0,
                    "loan_club"
                };
                const auto openingFinance = engine.state().club.finance;

                require(
                    engine.startIncomingLoan("dion-dublin", terms).success,
                    "Could not open mandatory incoming loan");
                engine.advanceDays(1);
                const std::string loanId =
                    loanForPlayer(engine.state(), "dion-dublin").id;
                engine.advanceDays(3);
                requireEqual(
                    loanById(engine.state(), loanId).stage,
                    LoanStage::Active,
                    "Mandatory incoming loan did not become active");
                engine.advanceDays(25);

                const auto& completedLoan =
                    loanById(engine.state(), loanId);
                requireEqual(
                    completedLoan.stage,
                    LoanStage::Completed,
                    "Mandatory future fee loan did not complete");
                require(
                    hasPlayer(engine.state(), "dion-dublin"),
                    "Mandatory future fee did not keep the player");
                (void)contractForPlayer(engine.state(), "dion-dublin");
                requireEqual(
                    engine.state().club.finance.cashPence,
                    openingFinance.cashPence
                        - terms.loanFeePence
                        - terms.futureFeePence,
                    "Mandatory future fee cash was not charged exactly once");
                requireEqual(
                    engine.state().club.finance.transferBudgetPence,
                    openingFinance.transferBudgetPence
                        - terms.loanFeePence
                        - terms.futureFeePence,
                    "Mandatory future fee budget was duplicated or missed");
                const std::string conversionId =
                    "ai-loan-future-" + loanId;
                const auto& conversion =
                    aiDealById(engine.state(), conversionId);
                requireEqual(
                    conversion.type,
                    AiMarketDealType::LoanObligation,
                    "Mandatory conversion was not recorded as an AI obligation");
                require(
                    conversion.completionApplied,
                    "Mandatory conversion did not record applied state");

                const auto cashAfterConversion =
                    engine.state().club.finance.cashPence;
                engine.advanceDays(1);
                requireEqual(
                    engine.state().club.finance.cashPence,
                    cashAfterConversion,
                    "Mandatory loan conversion charged cash twice");
            }
        },
        {
            "AI buyout creates a player-club offer at the release clause",
            [] {
                auto state = makeManchesterUnitedPrototype();
                for (auto& club : state.aiClubMarketStates) {
                    club.nextEvaluationDate = state.currentDate.addDays(90);
                    club.finance.cashPence = 2'000'000'000;
                    club.finance.transferBudgetPence = 2'000'000'000;
                    club.finance.weeklyWageBudgetPence = 100'000'000;
                    club.finance.weeklyWageCommitmentPence = 10'000'000;
                }
                state.aiClubMarketStates.front().nextEvaluationDate =
                    state.currentDate.addDays(1);

                auto contract = std::find_if(
                    state.playerContracts.begin(),
                    state.playerContracts.end(),
                    [](PlayerContract& item) {
                        return item.playerId == "bryan-robson";
                    });
                require(
                    contract != state.playerContracts.end(),
                    "Buyout test contract is missing");
                contract->terms.releaseClausePence = 150'000'000;
                auto player = std::find_if(
                    state.players.begin(),
                    state.players.end(),
                    [](Player& item) {
                        return item.id == "bryan-robson";
                    });
                require(
                    player != state.players.end(),
                    "Buyout test player is missing");
                player->status = "Available";
                player->squadGroup = "First Team";
                state.outgoingTransferOffers.erase(
                    std::remove_if(
                        state.outgoingTransferOffers.begin(),
                        state.outgoingTransferOffers.end(),
                        [](const OutgoingTransferOffer& item) {
                            return item.playerId == "bryan-robson";
                        }),
                    state.outgoingTransferOffers.end());

                SimulationEngine engine(std::move(state));
                engine.advanceDays(1);

                const auto offer = std::find_if(
                    engine.state().outgoingTransferOffers.begin(),
                    engine.state().outgoingTransferOffers.end(),
                    [](const OutgoingTransferOffer& item) {
                        return item.playerId == "bryan-robson"
                            && item.id.rfind("ai-buyout-", 0) == 0;
                    });
                require(
                    offer != engine.state().outgoingTransferOffers.end(),
                    "AI buyout offer was not created");
                requireEqual(
                    offer->feeTerms.upfrontFeePence,
                    150'000'000,
                    "AI buyout did not match the release clause");
                requireEqual(
                    offer->status,
                    OutgoingTransferStatus::Pending,
                    "AI buyout should wait for player-club decision");
            }
        },
        {
            "Squad planning opinion keys round-trip",
            [] {
                requireEqual(
                    squadPlanningOpinionSourceKey(
                        SquadPlanningOpinionSource::Owner),
                    std::string("owner"),
                    "Owner source key changed");
                requireEqual(
                    squadPlanningOpinionSourceFromKey("manager"),
                    SquadPlanningOpinionSource::Manager,
                    "Manager source key did not round-trip");
                requireEqual(
                    squadPlanningOpinionTypeKey(
                        SquadPlanningOpinionType::Reinforce),
                    std::string("reinforce"),
                    "Reinforce opinion type key changed");
                requireEqual(
                    squadPlanningOpinionTypeFromKey("loan"),
                    SquadPlanningOpinionType::Loan,
                    "Loan opinion type key did not round-trip");
                requireEqual(
                    squadPlanningOpinionStatusKey(
                        SquadPlanningOpinionStatus::Dismissed),
                    std::string("dismissed"),
                    "Dismissed opinion status key changed");
                requireEqual(
                    squadPlanningOpinionStatusFromKey("superseded"),
                    SquadPlanningOpinionStatus::Superseded,
                    "Superseded opinion status key did not round-trip");
            }
        },
        {
            "Operating meeting keys round-trip",
            [] {
                requireEqual(
                    operatingMeetingCadenceKey(
                        OperatingMeetingCadence::TransferWindows),
                    std::string("transfer_windows"),
                    "Transfer-window meeting cadence key changed");
                requireEqual(
                    operatingMeetingCadenceFromKey("season_milestones"),
                    OperatingMeetingCadence::SeasonMilestones,
                    "Season-milestone meeting cadence key did not round-trip");
                requireEqual(
                    operatingMeetingAgendaTypeKey(
                        OperatingMeetingAgendaType::SquadPlanning),
                    std::string("squad_planning"),
                    "Squad-planning agenda key changed");
                requireEqual(
                    operatingMeetingAgendaTypeFromKey("finance"),
                    OperatingMeetingAgendaType::Finance,
                    "Finance agenda key did not round-trip");
                requireEqual(
                    operatingMeetingAttendeeModeKey(
                        OperatingMeetingAttendeeMode::Manual),
                    std::string("manual"),
                    "Manual attendee mode key changed");
                requireEqual(
                    operatingMeetingStatusFromKey("scheduled"),
                    OperatingMeetingStatus::Scheduled,
                    "Scheduled meeting status key did not round-trip");
            }
        },
        {
            "Scouting directive keys round-trip",
            [] {
                requireEqual(
                    scoutingDirectiveTypeKey(ScoutingDirectiveType::Target),
                    std::string("target"),
                    "Target directive type key changed");
                requireEqual(
                    scoutingDirectiveTypeFromKey("range"),
                    ScoutingDirectiveType::Range,
                    "Range directive type key did not round-trip");
                requireEqual(
                    scoutingDirectiveStatusKey(
                        ScoutingDirectiveStatus::Cancelled),
                    std::string("cancelled"),
                    "Cancelled directive status key changed");
                requireEqual(
                    scoutingDirectiveStatusFromKey("completed"),
                    ScoutingDirectiveStatus::Completed,
                    "Completed directive status key did not round-trip");
                requireEqual(
                    scoutingPositionGroup("CB/ST"),
                    std::string("DEF"),
                    "Scouting position group changed");
            }
        },
        {
            "Invalid installment structure is rejected without opening talks",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                const TransferFeeTerms invalid {
                    75'000'000,
                    20'000'000,
                    0,
                    0,
                    0,
                    0
                };
                const auto result =
                    engine.startTransferPursuit("dion-dublin", invalid);
                require(!result.success, "Invalid installment terms were accepted");
                require(
                    std::none_of(
                        engine.state().transferPursuits.begin(),
                        engine.state().transferPursuits.end(),
                        [](const TransferPursuit& pursuit) {
                            return pursuit.candidatePlayerId == "dion-dublin";
                        }),
                    "Invalid offer created a pursuit");
            }
        },
        {
            "Owner actions update reputation, fan trust, and tendencies",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                requireEqual(
                    engine.state().ownerReputation.overallScore,
                    50,
                    "Owner reputation default changed");
                require(
                    engine.state().ownerActionLog.empty(),
                    "Prototype should not start with owner action log entries");

                const auto approved =
                    engine.approveTransfer("alan-shearer-1992-opening");
                require(approved.success, "Opening approval failed");
                requireEqual(
                    engine.state().ownerActionLog.size(),
                    std::size_t {1},
                    "Approval did not record one owner action");
                requireEqual(
                    engine.state().ownerActionLog.front().actionType,
                    std::string("approve_transfer"),
                    "Approval owner action type changed");
                requireEqual(
                    engine.state().ownerReputation.overallScore,
                    51,
                    "Approval did not improve owner reputation");
                requireEqual(
                    engine.state().fanTendency.trustScore,
                    51,
                    "Approval did not improve fan trust");
                require(
                    engine.state().fanTendency.homegrownPreference > 60
                        && engine.state().fanTendency.youthDevelopmentPreference > 50,
                    "Approval did not move fan tendencies for a young homegrown target");

                const auto duplicate =
                    engine.approveTransfer("alan-shearer-1992-opening");
                require(!duplicate.success, "Duplicate approval unexpectedly succeeded");
                requireEqual(
                    engine.state().ownerActionLog.size(),
                    std::size_t {1},
                    "Failed duplicate approval recorded an owner action");
            }
        },
        {
            "Simulation rejects backwards time travel",
            [] {
                SimulationEngine engine(makeManchesterUnitedPrototype());
                bool threw = false;
                try {
                    engine.advanceDays(-1);
                } catch (const std::invalid_argument&) {
                    threw = true;
                }
                require(threw, "Negative date advance did not throw");
            }
        }
    };

    int failures = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << tests.size() - static_cast<std::size_t>(failures)
              << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
