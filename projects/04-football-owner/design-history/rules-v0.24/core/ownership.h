#pragma once

// G2 ownership model (plan §2, §66.4-66.7): what a club is worth before a career exists, what
// the owner's asset tier means in that season's market, and the acquisition packages the player
// chooses from. Everything here is an explicit gameplay model with integer pence; nothing is a
// historical valuation fact. Pure functions, no Qt, no GameState mutation.
#include "core/game_state.h"

#include <optional>
#include <string>
#include <vector>

namespace football_owner {

// What the new-game club list knows about a club. Built from the career database the game
// would create for that season (see infrastructure/club_listings.h), so ids and money match
// what the player gets after the purchase.
struct ClubListing {
    std::string clubId;
    std::string name;
    std::string competitionName;
    int tier {1};
    int reputation {50}; // 0-100, historical_club_snapshots.reputation
    Money cashPence {};
    Money debtPence {};
    Money weeklyWageBudgetPence {};
    Money weeklyWageCommitmentPence {};
    Money transferBudgetPence {};
    int squadSize {};
    std::string managerName;
    std::string financeConfidence;
    // Due-diligence facts (G2 step 5), read from the same snapshot.
    int contractsExpiringWithinYear {}; // historical_player_contracts ending within 365 days of the snapshot
    int averageAgeYears {};             // registered players, 0 when unknown
    Money topWeeklyWagePence {};
};

// AssetTier, AcquisitionStructure and the persisted OwnerProfile/OwnerPledge/GameSettings live in
// core/owner_profile.h (included through game_state.h).

enum class SaleWillingness { Reluctant, Open, Eager };

struct ClubValuation {
    std::string clubId;
    Money revenueProxyPence {};     // wage budget x 52 / 0.6 (wages ~ 60% of revenue)
    Money enterpriseValuePence {};  // revenue proxy x (1 + reputation/100), tier 2 x 0.8
    Money netDebtPence {};          // debt - cash (negative = net cash)
    Money equityValuePence {};      // max(EV - net debt, EV / 10)
    Money askingPricePence {};      // equity x 1.25 control premium x willingness factor, to £10k
    SaleWillingness willingness {SaleWillingness::Open};
    bool distressed {false};        // negative cash or net debt above 60% of EV
    int difficulty {1};             // 1 (낮음) .. 5 (극심)
    std::string difficultyLabel;
    std::string fanExpectationLabel;
    std::string model {"ownership_v1_gameplay"};
};

struct AcquisitionPackage {
    AcquisitionStructure structure {AcquisitionStructure::SoleOwnership};
    std::string label;
    int ownershipPercent {100};
    Money pricePence {};
    Money cashPaidPence {};
    Money loanPence {};
    int loanAnnualRateBps {};
    int loanTermMonths {};
    bool loanOnClub {false};          // the club carries the acquisition loan as its own debt
    Money assumedDebtPence {};        // club debt after the purchase (existing debt + any club loan)
    Money emergencyInjectionPence {}; // rescue: cash the owner puts into the club on day one
    Money reservePence {};            // operating reserve the owner should keep: 26 weeks of wages
    Money requiredPersonalAssetsPence {}; // cash paid + emergency injection + reserve
    bool affordable {false};
    std::string fanReaction;
    std::string summary;
};

// Season-wide context: the asset tiers are ratios of the median top-tier asking price, so a
// tier means the same thing in 1992 and in 2025.
struct MarketContext {
    Money medianTopTierAskingPence {};
    int listedClubs {};
};

[[nodiscard]] const std::vector<AssetTier>& allAssetTiers();
[[nodiscard]] const char* assetTierId(AssetTier tier);
[[nodiscard]] std::string assetTierLabel(AssetTier tier);
[[nodiscard]] std::string assetTierReach(AssetTier tier); // plan §66.4 "기본 접근 범위"
[[nodiscard]] std::optional<AssetTier> assetTierFromId(const std::string& id);
[[nodiscard]] const char* acquisitionStructureId(AcquisitionStructure structure);
[[nodiscard]] std::string acquisitionStructureLabel(AcquisitionStructure structure);
[[nodiscard]] std::optional<AcquisitionStructure> acquisitionStructureFromId(const std::string& id);
[[nodiscard]] const char* saleWillingnessId(SaleWillingness willingness);
[[nodiscard]] std::string saleWillingnessLabel(SaleWillingness willingness);

[[nodiscard]] ClubValuation valueClub(const ClubListing& listing, int startYear);
[[nodiscard]] MarketContext marketContext(const std::vector<ClubValuation>& valuations);
[[nodiscard]] Money personalCashForTier(AssetTier tier, const MarketContext& market);
[[nodiscard]] int loanAnnualRateBpsForYear(int year);
[[nodiscard]] std::vector<AcquisitionPackage> acquisitionPackages(
    const ClubListing& listing, const ClubValuation& valuation, Money personalCashPence, int startYear);

// ---- G2 step 2: settings, pledges and applying the deal to a new career (plan §66.8-66.12) ----

[[nodiscard]] const std::vector<DelegationPreset>& allDelegationPresets();
[[nodiscard]] const char* delegationPresetId(DelegationPreset preset);
[[nodiscard]] std::string delegationPresetLabel(DelegationPreset preset);
[[nodiscard]] std::string delegationPresetDescription(DelegationPreset preset);
[[nodiscard]] std::optional<DelegationPreset> delegationPresetFromId(const std::string& id);

[[nodiscard]] const std::vector<InfoMode>& allInfoModes();
[[nodiscard]] const char* infoModeId(InfoMode mode);
[[nodiscard]] std::string infoModeLabel(InfoMode mode);
[[nodiscard]] std::string infoModeDescription(InfoMode mode);
[[nodiscard]] std::optional<InfoMode> infoModeFromId(const std::string& id);

[[nodiscard]] const std::vector<GameMode>& allGameModes();
[[nodiscard]] const char* gameModeId(GameMode mode);
[[nodiscard]] std::string gameModeLabel(GameMode mode);
[[nodiscard]] std::string gameModeDescription(GameMode mode);
[[nodiscard]] std::optional<GameMode> gameModeFromId(const std::string& id);

[[nodiscard]] const char* pledgeAreaId(PledgeArea area);
[[nodiscard]] std::string pledgeAreaLabel(PledgeArea area);
[[nodiscard]] std::optional<PledgeArea> pledgeAreaFromId(const std::string& id);

// A pledge the owner can make at inauguration. The deadline is madeDate + horizonDays.
struct OwnerPledgeOption {
    std::string id;
    PledgeArea area {PledgeArea::Investment};
    std::string title;
    std::string detail;
    int horizonDays {365};
};
[[nodiscard]] const std::vector<OwnerPledgeOption>& ownerPledgeOptions();
[[nodiscard]] const OwnerPledgeOption* ownerPledgeOption(const std::string& id);
constexpr int kMinimumPledges = 3;
constexpr int kMaximumPledges = 5;

// Plan §66.12 save tags derived from the settings (game mode, information, delegation).
[[nodiscard]] std::vector<std::string> settingsTags(const GameSettings& settings);

// Everything the deal needs, chosen in the new-game flow and checked again here.
struct AcquisitionTerms {
    AssetTier tier {AssetTier::MidTycoon};
    Money tierCashPence {};
    ClubValuation valuation;
    AcquisitionPackage package;
    GameSettings settings;
    std::vector<std::string> pledgeIds;
    std::vector<OwnerHiddenRisk> hiddenRisks; // from carriedRisks() in core/due_diligence.h; may be empty
};

// Applies the deal to a freshly created career: owner profile and personal cash, the club-carried
// loan (leveraged) or the emergency injection (rescue), the pledges, the settings and an inbox
// item. Returns an error message (empty on success); on error the state is unchanged.
[[nodiscard]] std::string applyAcquisition(GameState& state, const AcquisitionTerms& terms);

// Consistency of the persisted ownership state; empty when valid.
[[nodiscard]] std::string validateOwnership(const GameState& state);

} // namespace football_owner
