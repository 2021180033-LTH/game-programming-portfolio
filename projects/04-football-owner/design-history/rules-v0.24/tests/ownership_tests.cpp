// G2-a ownership model: valuation, asset tiers and acquisition packages are explicit gameplay
// models; these tests pin their shape (monotonic, deterministic, affordable by design).
#include "core/due_diligence.h"
#include "core/ownership.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace football_owner;

namespace {

void check(bool ok, const std::string& message)
{
    if (!ok) throw std::runtime_error(message);
}

ClubListing listing(const std::string& id, int tier, int reputation, Money cashPounds, Money debtPounds, Money weeklyWagePounds)
{
    ClubListing club;
    club.clubId = id;
    club.name = id;
    club.competitionName = tier == 1 ? "FA Premier League" : "Football League First Division";
    club.tier = tier;
    club.reputation = reputation;
    club.cashPence = cashPounds * 100;
    club.debtPence = debtPounds * 100;
    club.weeklyWageBudgetPence = weeklyWagePounds * 100;
    club.weeklyWageCommitmentPence = weeklyWagePounds * 95;
    club.transferBudgetPence = cashPounds * 50;
    club.squadSize = 24;
    club.managerName = "Manager";
    return club;
}

// A bare career state for applyAcquisition: the club's finance mirrors the listing.
GameState careerState(const std::string& clubId, Money cashPounds, Money debtPounds, Money weeklyWagePounds)
{
    GameState state;
    state.currentDate = GameDate {1996, 7, 1};
    state.club.id = clubId;
    state.club.name = clubId == "arsenal" ? "Arsenal" : clubId;
    state.club.finance.cashPence = cashPounds * 100;
    state.club.finance.debtPence = debtPounds * 100;
    state.club.finance.weeklyWageBudgetPence = weeklyWagePounds * 100;
    state.club.finance.weeklyWageCommitmentPence = weeklyWagePounds * 95;
    return state;
}

// Terms as the new-game flow would build them for a one-club market of that season.
AcquisitionTerms termsFor(const ClubListing& club, AcquisitionStructure structure, AssetTier tier, std::vector<std::string> pledges)
{
    AcquisitionTerms terms;
    terms.tier = tier;
    terms.valuation = valueClub(club, 1996);
    terms.tierCashPence = personalCashForTier(tier, marketContext({terms.valuation}));
    for (const auto& package : acquisitionPackages(club, terms.valuation, terms.tierCashPence, 1996)) {
        if (package.structure == structure) terms.package = package;
    }
    if (terms.package.label.empty()) throw std::runtime_error("package not offered for this club");
    terms.pledgeIds = std::move(pledges);
    return terms;
}

std::vector<ClubListing> season()
{
    return {
        listing("top", 1, 85, 4'000'000, 2'000'000, 140'000),
        listing("mid", 1, 65, 2'000'000, 1'000'000, 80'000),
        listing("low", 1, 50, 800'000, 1'500'000, 45'000),
        listing("second-a", 2, 45, 300'000, 900'000, 20'000),
        listing("second-b", 2, 40, -200'000, 2'500'000, 18'000),
    };
}

std::vector<ClubValuation> valuations(const std::vector<ClubListing>& clubs, int year = 1996)
{
    std::vector<ClubValuation> result;
    for (const auto& club : clubs) result.push_back(valueClub(club, year));
    return result;
}

const AcquisitionPackage* packageOf(const std::vector<AcquisitionPackage>& packages, AcquisitionStructure structure)
{
    for (const auto& package : packages) {
        if (package.structure == structure) return &package;
    }
    return nullptr;
}

} // namespace

int runOwnershipCoreTests()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"asset tier ids round-trip and keep plan order", [] {
            check(allAssetTiers().size() == 5 && allAssetTiers().front() == AssetTier::SmallInvestor
                && allAssetTiers().back() == AssetTier::SovereignScale, "tier order");
            for (const AssetTier tier : allAssetTiers()) {
                check(assetTierFromId(assetTierId(tier)) == tier && !assetTierLabel(tier).empty()
                    && !assetTierReach(tier).empty(), "tier round-trip");
            }
            check(!assetTierFromId("billionaire").has_value(), "unknown tier accepted");
        }},
        {"structure ids round-trip", [] {
            for (const AcquisitionStructure structure : {AcquisitionStructure::SoleOwnership,
                     AcquisitionStructure::LeveragedBuyout, AcquisitionStructure::RescueTakeover}) {
                check(acquisitionStructureFromId(acquisitionStructureId(structure)) == structure
                    && !acquisitionStructureLabel(structure).empty(), "structure round-trip");
            }
            check(!acquisitionStructureFromId("consortium").has_value(), "G2-b structure accepted early");
        }},
        {"valuation is positive and rises with reputation", [] {
            auto a = listing("a", 1, 50, 1'000'000, 0, 60'000);
            auto b = a; b.clubId = "b"; b.reputation = 80;
            const auto va = valueClub(a, 1996), vb = valueClub(b, 1996);
            check(va.enterpriseValuePence > 0 && va.askingPricePence > 0, "non-positive value");
            check(vb.enterpriseValuePence > va.enterpriseValuePence, "reputation ignored");
            check(va.revenueProxyPence == 60'000LL * 100 * 52 * 100 / 60, "revenue proxy formula");
        }},
        {"net debt lowers equity and asking price", [] {
            auto rich = listing("rich", 1, 60, 5'000'000, 0, 60'000);
            auto indebted = rich; indebted.clubId = "rich"; indebted.cashPence = 0; indebted.debtPence = 5'000'000 * 100;
            const auto vr = valueClub(rich, 1996), vi = valueClub(indebted, 1996);
            check(vr.netDebtPence < 0 && vi.netDebtPence > 0, "net debt sign");
            check(vi.equityValuePence < vr.equityValuePence && vi.askingPricePence < vr.askingPricePence, "debt not priced");
            check(vi.equityValuePence >= vi.enterpriseValuePence / 10, "equity floor");
        }},
        {"second tier multiple is lower", [] {
            auto one = listing("x", 1, 50, 1'000'000, 0, 50'000);
            auto two = one; two.tier = 2;
            check(valueClub(two, 1996).enterpriseValuePence < valueClub(one, 1996).enterpriseValuePence, "tier ignored");
        }},
        {"asking price is rounded to ten thousand pounds and deterministic", [] {
            const auto club = listing("det", 1, 55, 1'234'567, 321'000, 61'234);
            const auto v1 = valueClub(club, 1996), v2 = valueClub(club, 1996);
            check(v1.askingPricePence % (10'000 * 100) == 0, "rounding");
            check(v1.askingPricePence == v2.askingPricePence && v1.willingness == v2.willingness, "non-deterministic");
        }},
        {"willingness varies across the season and follows the hash", [] {
            const auto clubs = season();
            bool reluctant = false, open = false, eager = false;
            std::vector<ClubListing> many;
            for (int index = 0; index < 40; ++index) many.push_back(listing("club-" + std::to_string(index), 1, 55, 1'000'000, 0, 50'000));
            for (const auto& valuation : valuations(many)) {
                reluctant |= valuation.willingness == SaleWillingness::Reluctant;
                open |= valuation.willingness == SaleWillingness::Open;
                eager |= valuation.willingness == SaleWillingness::Eager;
            }
            check(reluctant && open && eager, "willingness never varies");
            (void)clubs;
        }},
        {"distressed club is eager and gets a rescue package", [] {
            const auto club = listing("broke", 2, 40, -200'000, 2'500'000, 18'000);
            const auto valuation = valueClub(club, 1996);
            check(valuation.distressed && valuation.willingness == SaleWillingness::Eager, "distress not detected");
            const auto packages = acquisitionPackages(club, valuation, 0, 1996);
            const auto* rescue = packageOf(packages, AcquisitionStructure::RescueTakeover);
            check(rescue != nullptr, "no rescue package");
            check(rescue->pricePence < valuation.askingPricePence, "rescue not cheaper");
            check(rescue->emergencyInjectionPence >= 200'000 * 100 + club.weeklyWageBudgetPence * 13, "emergency injection too small");
            check(rescue->assumedDebtPence == club.debtPence, "rescue debt not assumed");
        }},
        {"healthy club has no rescue package", [] {
            const auto club = listing("fine", 1, 60, 2'000'000, 500'000, 60'000);
            const auto packages = acquisitionPackages(club, valueClub(club, 1996), 0, 1996);
            check(packages.size() == 2 && packageOf(packages, AcquisitionStructure::RescueTakeover) == nullptr, "rescue offered to healthy club");
        }},
        {"leveraged buyout pays less cash but puts the loan on the club", [] {
            const auto club = listing("lev", 1, 70, 2'000'000, 500'000, 90'000);
            const auto valuation = valueClub(club, 1996);
            const auto packages = acquisitionPackages(club, valuation, 0, 1996);
            const auto* sole = packageOf(packages, AcquisitionStructure::SoleOwnership);
            const auto* leveraged = packageOf(packages, AcquisitionStructure::LeveragedBuyout);
            check(sole && leveraged, "packages missing");
            check(sole->cashPaidPence == valuation.askingPricePence && sole->loanPence == 0 && sole->assumedDebtPence == club.debtPence, "sole terms");
            check(leveraged->cashPaidPence < sole->cashPaidPence && leveraged->loanPence + leveraged->cashPaidPence == valuation.askingPricePence, "leveraged split");
            check(leveraged->loanOnClub && leveraged->assumedDebtPence == club.debtPence + leveraged->loanPence, "loan not on club");
            check(leveraged->loanAnnualRateBps == loanAnnualRateBpsForYear(1996) && leveraged->loanTermMonths == 120, "loan terms");
        }},
        {"required assets include a 26 week wage reserve", [] {
            const auto club = listing("res", 1, 60, 1'000'000, 0, 50'000);
            const auto packages = acquisitionPackages(club, valueClub(club, 1996), 0, 1996);
            const auto* sole = packageOf(packages, AcquisitionStructure::SoleOwnership);
            check(sole->reservePence == club.weeklyWageBudgetPence * 26, "reserve weeks");
            check(sole->requiredPersonalAssetsPence == sole->cashPaidPence + sole->reservePence, "required sum");
        }},
        {"loan rate follows the era", [] {
            check(loanAnnualRateBpsForYear(1992) == 900 && loanAnnualRateBpsForYear(2003) == 650
                && loanAnnualRateBpsForYear(2015) == 450 && loanAnnualRateBpsForYear(2025) == 600, "era rates");
        }},
        {"asset tiers are ratios of the top tier median and strictly increase", [] {
            const auto market = marketContext(valuations(season()));
            check(market.listedClubs == 5 && market.medianTopTierAskingPence > 0, "market context");
            Money previous = 0;
            for (const AssetTier tier : allAssetTiers()) {
                const Money cash = personalCashForTier(tier, market);
                check(cash > previous && cash % (100'000 * 100) == 0, "tier cash not increasing or unrounded");
                previous = cash;
            }
            check(personalCashForTier(AssetTier::LargeTycoon, market) == std::max<Money>(market.medianTopTierAskingPence / 100'000 / 100 * 100'000 * 100, 1'000'000 * 100)
                || personalCashForTier(AssetTier::LargeTycoon, market) >= market.medianTopTierAskingPence * 95 / 100, "large tycoon ~ median top club");
        }},
        {"empty market falls back to the minimum personal cash", [] {
            check(personalCashForTier(AssetTier::SmallInvestor, MarketContext {}) == 1'000'000 * 100, "minimum cash");
        }},
        {"small investor cannot buy the top club outright but the sovereign tier can", [] {
            const auto clubs = season();
            const auto vals = valuations(clubs);
            const auto market = marketContext(vals);
            const auto top = acquisitionPackages(clubs[0], vals[0], personalCashForTier(AssetTier::SmallInvestor, market), 1996);
            check(!packageOf(top, AcquisitionStructure::SoleOwnership)->affordable, "small investor buys top club");
            const auto rich = acquisitionPackages(clubs[0], vals[0], personalCashForTier(AssetTier::SovereignScale, market), 1996);
            check(packageOf(rich, AcquisitionStructure::SoleOwnership)->affordable, "sovereign tier cannot buy top club");
            const auto cheap = acquisitionPackages(clubs[4], vals[4], personalCashForTier(AssetTier::SmallInvestor, market), 1996);
            check(packageOf(cheap, AcquisitionStructure::RescueTakeover) != nullptr, "distressed second tier club lacks rescue");
        }},
        {"difficulty and expectation follow reputation, debt and squad size", [] {
            auto club = listing("hard", 1, 85, 0, 12'000'000, 100'000);
            club.squadSize = 15;
            const auto valuation = valueClub(club, 1996);
            check(valuation.difficulty == 5 && valuation.difficultyLabel == "극심", "max difficulty");
            check(valuation.fanExpectationLabel == "즉각적인 우승 요구", "expectation label");
            const auto easy = valueClub(listing("easy", 2, 40, 500'000, 0, 15'000), 1996);
            check(easy.difficulty == 1 && easy.difficultyLabel == "낮음", "min difficulty");
        }},
        {"zero wage listing still values from cash", [] {
            auto club = listing("nowage", 2, 40, 500'000, 0, 0);
            club.weeklyWageCommitmentPence = 0;
            const auto valuation = valueClub(club, 1996);
            check(valuation.enterpriseValuePence >= 250'000 * 100 && valuation.askingPricePence >= 10'000 * 100, "zero wage collapse");
        }},
        // ---- G2 step 2 ----
        {"setting ids round-trip and have labels", [] {
            for (const auto preset : allDelegationPresets()) {
                check(delegationPresetFromId(delegationPresetId(preset)) == preset, "delegation id");
                check(!delegationPresetLabel(preset).empty() && !delegationPresetDescription(preset).empty(), "delegation text");
            }
            for (const auto mode : allInfoModes()) check(infoModeFromId(infoModeId(mode)) == mode && !infoModeDescription(mode).empty(), "info mode id");
            for (const auto mode : allGameModes()) check(gameModeFromId(gameModeId(mode)) == mode && !gameModeDescription(mode).empty(), "game mode id");
            for (const auto area : {PledgeArea::Investment, PledgeArea::FootballDirection, PledgeArea::Finance,
                     PledgeArea::FacilitiesOrganisation, PledgeArea::TraditionFans}) {
                check(pledgeAreaFromId(pledgeAreaId(area)) == area && !pledgeAreaLabel(area).empty(), "pledge area id");
            }
            check(!delegationPresetFromId("nope") && !infoModeFromId("nope") && !gameModeFromId("nope") && !pledgeAreaFromId("nope"), "unknown ids");
        }},
        {"pledge options cover the five areas with unique ids", [] {
            const auto& options = ownerPledgeOptions();
            check(options.size() >= 10, "option count");
            std::set<std::string> ids;
            std::set<PledgeArea> areas;
            for (const auto& option : options) {
                check(ids.insert(option.id).second, "unique option id");
                check(!option.title.empty() && !option.detail.empty() && option.horizonDays > 0, "option text");
                areas.insert(option.area);
            }
            check(areas.size() == 5, "all five areas");
            check(ownerPledgeOption("invest-first-window") != nullptr && ownerPledgeOption("nope") == nullptr, "lookup");
        }},
        {"settings tags follow the modes", [] {
            GameSettings settings;
            const auto defaults = settingsTags(settings);
            check(defaults.size() == 2 && defaults[0] == "realistic" && defaults[1] == "delegation_balanced", "default tags");
            settings.gameMode = GameMode::Sandbox;
            settings.infoMode = InfoMode::FullDisclosure;
            settings.delegation = DelegationPreset::Investor;
            const auto sandbox = settingsTags(settings);
            check(sandbox.size() == 3 && sandbox[0] == "sandbox" && sandbox[1] == "full_information" && sandbox[2] == "delegation_investor", "sandbox tags");
        }},
        {"sole acquisition sets the profile, personal cash and pledges", [] {
            auto state = careerState("arsenal", 3'975'000, 2'000'000, 400'000);
            const auto club = listing("arsenal", 1, 75, 3'975'000, 2'000'000, 400'000);
            const auto terms = termsFor(club, AcquisitionStructure::SoleOwnership, AssetTier::SovereignScale,
                {"invest-first-window", "finance-no-new-debt", "fans-identity"});
            const Money cashBefore = state.club.finance.cashPence;
            check(applyAcquisition(state, terms).empty(), "apply sole");
            const auto& profile = state.ownerProfile;
            check(profile.initialized && profile.tier == AssetTier::SovereignScale && profile.structure == AcquisitionStructure::SoleOwnership, "profile identity");
            check(profile.purchasePricePence == terms.package.pricePence && profile.cashPaidPence == terms.package.pricePence, "price paid");
            check(profile.personalCashPence == terms.tierCashPence - terms.package.pricePence, "personal cash after");
            check(profile.acquisitionLoanPence == 0 && profile.acquisitionDebtId.empty() && profile.emergencyInjectionPence == 0, "no loan or injection");
            check(state.club.finance.cashPence == cashBefore && state.debts.empty(), "club untouched");
            check(state.ownerPledges.size() == 3 && state.ownerPledges[0].id == "invest-first-window" && state.ownerPledges[0].status == "open", "pledges");
            check(state.ownerPledges[0].madeDate == state.currentDate && state.ownerPledges[0].deadline == state.currentDate.addDays(120), "pledge deadline");
            check(state.inbox.size() == 1 && state.inbox[0].category == InboxCategory::Board
                && state.inbox[0].title.find("Arsenal") != std::string::npos && state.inbox[0].body.find("취임 약속 3개") != std::string::npos, "inbox item");
            check(state.gameSettings.tags.size() == 2, "default settings tags");
            check(validateOwnership(state).empty(), "valid after apply");
            check(!applyAcquisition(state, terms).empty(), "second apply rejected");
        }},
        {"leveraged acquisition puts the loan on the club", [] {
            auto state = careerState("arsenal", 3'975'000, 2'000'000, 400'000);
            const auto club = listing("arsenal", 1, 75, 3'975'000, 2'000'000, 400'000);
            const auto terms = termsFor(club, AcquisitionStructure::LeveragedBuyout, AssetTier::LargeTycoon,
                {"invest-no-fire-sale", "football-keep-manager", "finance-wage-discipline", "organisation-staff"});
            check(terms.package.affordable, "large tycoon affords the leveraged buyout");
            check(applyAcquisition(state, terms).empty(), "apply leveraged");
            const auto& profile = state.ownerProfile;
            check(profile.acquisitionLoanPence == terms.package.loanPence && profile.acquisitionDebtId == "acquisition-loan-arsenal", "loan recorded");
            check(profile.personalCashPence == terms.tierCashPence - terms.package.cashPaidPence, "personal cash after");
            const auto debt = std::find_if(state.debts.begin(), state.debts.end(), [](const auto& item) { return item.id == "acquisition-loan-arsenal"; });
            check(debt != state.debts.end() && debt->kind == "bank_loan" && debt->status == "active"
                && debt->remainingPrincipalPence == terms.package.loanPence && debt->annualRateBps == loanAnnualRateBpsForYear(1996), "debt ledger");
            std::size_t installments = 0;
            Money scheduled = 0;
            for (const auto& repayment : state.debtRepayments) {
                if (repayment.debtId != "acquisition-loan-arsenal") continue;
                ++installments;
                scheduled += repayment.principalPence;
            }
            check(installments == 120 && scheduled == terms.package.loanPence, "120 monthly repayments");
            check(state.club.finance.debtPence == 2'000'000 * 100 + terms.package.loanPence, "club debt includes the loan");
            check(state.ownerPledges.size() == 4 && validateOwnership(state).empty(), "four pledges, valid");
        }},
        {"rescue acquisition injects emergency cash", [] {
            auto state = careerState("crisis", -500'000, 5'000'000, 50'000);
            const auto club = listing("crisis", 1, 45, -500'000, 5'000'000, 50'000);
            check(valueClub(club, 1996).distressed, "distressed club");
            const auto terms = termsFor(club, AcquisitionStructure::RescueTakeover, AssetTier::SovereignScale,
                {"finance-reduce-debt", "fans-transparency", "organisation-meetings"});
            check(terms.package.emergencyInjectionPence > 0 && terms.package.affordable, "rescue package");
            check(applyAcquisition(state, terms).empty(), "apply rescue");
            check(state.club.finance.cashPence == -500'000 * 100 + terms.package.emergencyInjectionPence, "cash injected");
            check(state.ownerProfile.emergencyInjectionPence == terms.package.emergencyInjectionPence, "injection recorded");
            check(state.ownerProfile.personalCashPence == terms.tierCashPence - terms.package.pricePence - terms.package.emergencyInjectionPence, "personal cash after");
            check(state.operatingTransactions.empty() && state.debts.empty(), "no operating profile: no ledger row, no loan");
        }},
        {"acquisition rejects what the flow would not allow", [] {
            const auto club = listing("arsenal", 1, 75, 3'975'000, 2'000'000, 400'000);
            {
                auto state = careerState("arsenal", 3'975'000, 2'000'000, 400'000);
                const auto terms = termsFor(club, AcquisitionStructure::SoleOwnership, AssetTier::SmallInvestor,
                    {"invest-first-window", "finance-no-new-debt", "fans-identity"});
                check(!terms.package.affordable && !applyAcquisition(state, terms).empty(), "unaffordable rejected");
                check(!state.ownerProfile.initialized && state.inbox.empty() && state.ownerPledges.empty(), "state unchanged");
            }
            {
                auto state = careerState("arsenal", 3'975'000, 2'000'000, 400'000);
                const auto sovereign = [&](std::vector<std::string> pledges) {
                    return termsFor(club, AcquisitionStructure::SoleOwnership, AssetTier::SovereignScale, std::move(pledges));
                };
                check(!applyAcquisition(state, sovereign({"invest-first-window", "finance-no-new-debt"})).empty(), "two pledges rejected");
                check(!applyAcquisition(state, sovereign({"invest-first-window", "finance-no-new-debt", "nope"})).empty(), "unknown pledge rejected");
                check(!applyAcquisition(state, sovereign({"invest-first-window", "invest-first-window", "fans-identity"})).empty(), "duplicate pledge rejected");
                check(!applyAcquisition(state, sovereign({"a", "b", "c", "d", "e", "f"})).empty(), "six pledges rejected");
                check(!state.ownerProfile.initialized && state.inbox.empty(), "still uninitialized");
                state.ownerPledges.push_back({"x", PledgeArea::Finance, "t", "d", state.currentDate, state.currentDate, "open", ""});
                check(!validateOwnership(state).empty(), "pledges without a profile are invalid");
            }
        }},
        {"due-diligence area ids round-trip", [] {
            check(allDiligenceAreas().size() == 4, "four areas");
            for (const auto area : allDiligenceAreas()) {
                check(diligenceAreaFromId(diligenceAreaId(area)) == area && !diligenceAreaLabel(area).empty(), "area id");
            }
            check(!diligenceAreaFromId("hr").has_value(), "unknown area accepted");
            check(infoModeDetectionLevel(InfoMode::Realistic) == 1 && infoModeDetectionLevel(InfoMode::Enhanced) == 2
                && infoModeDetectionLevel(InfoMode::FullDisclosure) == 3, "detection levels");
            check(severityLabel(1) == "낮음" && severityLabel(2) == "중간" && severityLabel(3) == "높음", "severity labels");
        }},
        {"hidden risks are deterministic: one per club, two for a distressed club", [] {
            const auto healthy = listing("mid", 1, 65, 2'000'000, 1'000'000, 80'000);
            const auto valuation = valueClub(healthy, 1996);
            const auto first = hiddenRisksFor(healthy, valuation, 1996);
            const auto again = hiddenRisksFor(healthy, valuation, 1996);
            check(first.size() == 1 && again.size() == 1 && first[0].id == again[0].id && first[0].amountPence == again[0].amountPence, "deterministic single risk");
            const auto crisis = listing("crisis", 1, 45, -500'000, 5'000'000, 50'000);
            const auto risks = hiddenRisksFor(crisis, valueClub(crisis, 1996), 1996);
            check(risks.size() == 2 && risks[0].area == DiligenceArea::Finance && risks[1].area != DiligenceArea::Finance, "distressed: a finance risk plus one more");
            std::set<std::string> seen;
            for (const auto& club : season()) {
                for (const auto& risk : hiddenRisksFor(club, valueClub(club, 1996), 1996)) {
                    check(!risk.id.empty() && !risk.title.empty() && !risk.detail.empty(), "risk text");
                    check(risk.severity >= 1 && risk.severity <= 3 && risk.detectability >= 1 && risk.detectability <= 3 && risk.surfacesAfterDays > 0, "risk ranges");
                    if (risk.id == "hidden-debt" || risk.id == "wage-arrears" || risk.id == "stadium-repair" || risk.id == "regulatory-probe")
                        check(risk.amountPence >= 10'000 * 100, "money risk without an amount");
                    else
                        check(risk.amountPence == 0, "non-monetary risk with an amount");
                    seen.insert(risk.id);
                }
            }
            check(!seen.empty(), "season risks");
        }},
        {"due-diligence bands narrow with the information mode and full disclosure is exact", [] {
            auto club = listing("arsenal", 1, 75, 3'975'000, 2'000'000, 400'000);
            club.financeConfidence = "fact_safe_ledger"; // a strong source; an empty or placeholder source counts as weak
            const auto valuation = valueClub(club, 1996);
            const auto realistic = dueDiligenceReport(club, valuation, 1996, InfoMode::Realistic);
            const auto enhanced = dueDiligenceReport(club, valuation, 1996, InfoMode::Enhanced);
            const auto full = dueDiligenceReport(club, valuation, 1996, InfoMode::FullDisclosure);
            const auto item = [](const DueDiligenceReport& report, const std::string& id) -> const DiligenceItem& {
                for (const auto& candidate : report.items) if (candidate.id == id) return candidate;
                throw std::runtime_error("missing item " + id);
            };
            const auto& cashR = item(realistic, "cash");
            const auto& cashE = item(enhanced, "cash");
            const auto& cashF = item(full, "cash");
            check(cashF.exact && cashF.lowPence == club.cashPence && cashF.highPence == club.cashPence && cashF.display == formatMoney(club.cashPence), "full disclosure exact");
            check(!cashE.exact && cashE.lowPence <= club.cashPence && club.cashPence <= cashE.highPence, "enhanced band contains the truth");
            check(cashE.highPence - cashE.lowPence < cashR.highPence - cashR.lowPence, "enhanced narrower than realistic");
            check(cashR.confidence < cashE.confidence && cashE.confidence < cashF.confidence, "confidence rises with the mode");
            check(cashR.display.find(" ~ ") != std::string::npos && cashE.display.find(" ~ ") != std::string::npos, "band display");
            check(realistic.items.size() == full.items.size() && full.items.size() >= 12, "item count");
            check(item(realistic, "squad-size").exact && item(realistic, "manager").exact && item(realistic, "squad-size").display == "24명", "public facts exact");
            check(!item(realistic, "contracts-expiring").exact && item(full, "contracts-expiring").exact, "count band follows the mode");
            std::set<DiligenceArea> areas;
            for (const auto& entry : full.items) areas.insert(entry.area);
            check(areas.size() == 4, "every area has an item");
            check(item(full, "facilities-data").confidence == 0 && item(full, "facilities-data").confidenceLabel == "자료 없음", "no facilities data");
            check(realistic.model == "ownership_v1_gameplay" && realistic.summary.find("현실적") != std::string::npos
                && full.summary.find("전부 공개") != std::string::npos, "summary text");
            const auto repeat = dueDiligenceReport(club, valuation, 1996, InfoMode::Realistic);
            check(repeat.items.size() == realistic.items.size() && repeat.items[0].display == realistic.items[0].display, "deterministic report");
            // A weak finance source widens the realistic band and lowers the confidence.
            auto weak = club;
            weak.financeConfidence = "placeholder_tm_squad_value_scaled";
            const auto weakReport = dueDiligenceReport(weak, valuation, 1996, InfoMode::Realistic);
            check(item(weakReport, "cash").confidence < cashR.confidence && item(weakReport, "cash").confidenceLabel == "낮음", "weak source confidence");
        }},
        {"the information mode decides which hidden risks the report discloses", [] {
            const auto crisis = listing("crisis", 1, 45, -500'000, 5'000'000, 50'000);
            const auto valuation = valueClub(crisis, 1996);
            const auto risks = hiddenRisksFor(crisis, valuation, 1996);
            const auto realistic = dueDiligenceReport(crisis, valuation, 1996, InfoMode::Realistic);
            const auto enhanced = dueDiligenceReport(crisis, valuation, 1996, InfoMode::Enhanced);
            const auto full = dueDiligenceReport(crisis, valuation, 1996, InfoMode::FullDisclosure);
            check(full.warnings.size() == risks.size() && full.disclosedRisks == static_cast<int>(risks.size())
                && full.hiddenRisks == static_cast<int>(risks.size()), "full disclosure shows every risk");
            check(realistic.warnings.size() <= enhanced.warnings.size() && enhanced.warnings.size() <= full.warnings.size(), "monotonic disclosure");
            const auto disclosed = [](const DueDiligenceReport& report, const std::string& id) {
                return std::any_of(report.warnings.begin(), report.warnings.end(), [&](const auto& warning) { return warning.riskId == id; });
            };
            for (const auto& risk : risks) {
                check(disclosed(realistic, risk.id) == (risk.detectability <= 1), "realistic detectability rule");
                check(disclosed(enhanced, risk.id) == (risk.detectability <= 2), "enhanced detectability rule");
            }
            for (const auto& warning : realistic.warnings) check(!warning.exact && warning.display.find("규모") != std::string::npos, "realistic warning is vague");
            for (const auto& warning : full.warnings) check(warning.exact && warning.display.find(warning.title) == std::string::npos, "full warning exact with detail");
            check(realistic.summary.find("남아 있을 수") != std::string::npos && enhanced.summary.find("남아 있을 수") != std::string::npos, "undisclosed risks never counted");
        }},
        {"carried risks are stored by the acquisition and only disclosed ones reach the inbox", [] {
            const auto club = listing("crisis", 1, 45, -500'000, 5'000'000, 50'000);
            auto terms = termsFor(club, AcquisitionStructure::RescueTakeover, AssetTier::SovereignScale,
                {"finance-reduce-debt", "fans-transparency", "organisation-meetings"});
            terms.settings.infoMode = InfoMode::Enhanced;
            const auto risks = hiddenRisksFor(club, terms.valuation, 1996);
            auto state = careerState("crisis", -500'000, 5'000'000, 50'000);
            terms.hiddenRisks = carriedRisks(risks, InfoMode::Enhanced, state.currentDate);
            check(terms.hiddenRisks.size() == risks.size() && risks.size() == 2, "carried count");
            check(applyAcquisition(state, terms).empty(), "apply with risks");
            check(state.ownerHiddenRisks.size() == risks.size(), "risks stored");
            for (std::size_t i = 0; i < risks.size(); ++i) {
                const auto& stored = state.ownerHiddenRisks[i];
                check(stored.id == risks[i].id && stored.status == "latent" && stored.area == diligenceAreaId(risks[i].area), "carried identity");
                check(stored.surfacesOn == state.currentDate.addDays(risks[i].surfacesAfterDays) && stored.disclosed == (risks[i].detectability <= 2), "carried date and disclosure");
                check(stored.amountPence == risks[i].amountPence && stored.severity == risks[i].severity, "carried amount");
            }
            const auto& body = state.inbox.back().body;
            for (const auto& risk : state.ownerHiddenRisks) {
                check((body.find(risk.title) != std::string::npos) == risk.disclosed, "inbox leaks or omits a risk: " + risk.id);
            }
            check(validateOwnership(state).empty(), "valid with risks");
            {
                auto duplicate = terms;
                duplicate.hiddenRisks.push_back(duplicate.hiddenRisks.front());
                auto fresh = careerState("crisis", -500'000, 5'000'000, 50'000);
                check(!applyAcquisition(fresh, duplicate).empty() && !fresh.ownerProfile.initialized, "duplicate risk rejected");
            }
            {
                GameState orphan;
                orphan.currentDate = GameDate {1996, 7, 1};
                orphan.ownerHiddenRisks.push_back(state.ownerHiddenRisks.front());
                check(!validateOwnership(orphan).empty(), "risks need a profile");
            }
            {
                auto broken = state;
                broken.ownerHiddenRisks.front().status = "gone";
                check(!validateOwnership(broken).empty(), "status checked");
                broken = state;
                broken.ownerHiddenRisks.front().area = "hr";
                check(!validateOwnership(broken).empty(), "area checked");
            }
        }},
    };

    int failed = 0;
    for (const auto& [name, run] : tests) {
        try {
            run();
            std::cout << "[PASS] ownership " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] ownership " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failed << '/' << tests.size() << " ownership tests passed\n";
    return failed;
}
