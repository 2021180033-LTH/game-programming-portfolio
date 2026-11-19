#include "core/ownership.h"

#include "core/owner_capital.h"

#include "core/due_diligence.h"
#include "core/funding.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>

namespace football_owner {
namespace {

constexpr Money kPound = 100;
constexpr Money kMinimumEnterpriseValue = 250'000 * kPound;
constexpr Money kMinimumPersonalCash = 1'000'000 * kPound;
constexpr int kControlPremiumPercent = 125;
constexpr int kReserveWeeks = 26;
constexpr int kRescueWeeks = 13;
constexpr int kLeveragedEquityPercent = 35;
constexpr int kLeveragedTermMonths = 120;

Money roundTo(Money value, Money unit)
{
    if (unit <= 0) return value;
    const Money half = unit / 2;
    return ((value + (value >= 0 ? half : -half)) / unit) * unit;
}

std::uint32_t fnv1a(const std::string& text)
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char character : text) {
        hash ^= character;
        hash *= 16777619u;
    }
    return hash;
}

int willingnessPercent(SaleWillingness willingness)
{
    switch (willingness) {
    case SaleWillingness::Reluctant: return 135;
    case SaleWillingness::Open: return 100;
    case SaleWillingness::Eager: return 85;
    }
    return 100;
}

Money weeklyWages(const ClubListing& listing)
{
    return std::max(listing.weeklyWageCommitmentPence, listing.weeklyWageBudgetPence);
}

} // namespace

const std::vector<AssetTier>& allAssetTiers()
{
    static const std::vector<AssetTier> tiers {
        AssetTier::SmallInvestor, AssetTier::MidTycoon, AssetTier::LargeTycoon,
        AssetTier::MegaTycoon, AssetTier::SovereignScale};
    return tiers;
}

const char* assetTierId(AssetTier tier)
{
    switch (tier) {
    case AssetTier::SmallInvestor: return "small_investor";
    case AssetTier::MidTycoon: return "mid_tycoon";
    case AssetTier::LargeTycoon: return "large_tycoon";
    case AssetTier::MegaTycoon: return "mega_tycoon";
    case AssetTier::SovereignScale: return "sovereign_scale";
    }
    return "mid_tycoon";
}

std::string assetTierLabel(AssetTier tier)
{
    switch (tier) {
    case AssetTier::SmallInvestor: return "소규모 투자자";
    case AssetTier::MidTycoon: return "중견 부호";
    case AssetTier::LargeTycoon: return "대형 부호";
    case AssetTier::MegaTycoon: return "초대형 부호";
    case AssetTier::SovereignScale: return "국가급·초갑부";
    }
    return "중견 부호";
}

std::string assetTierReach(AssetTier tier)
{
    switch (tier) {
    case AssetTier::SmallInvestor: return "하부·소규모 구단. 큰 구단은 소수 지분과 컨소시엄(다음 묶음)으로만.";
    case AssetTier::MidTycoon: return "소규모 1부·재정 위기 구단, 중형 구단은 대출 인수.";
    case AssetTier::LargeTycoon: return "대부분의 중상위 구단, 일부 빅클럽은 대출 인수.";
    case AssetTier::MegaTycoon: return "대형 구단 인수 경쟁, 대규모 부채 정리와 시설 투자 여력.";
    case AssetTier::SovereignScale: return "거의 모든 매물 구단. 매각 의사·승인·경쟁은 그대로.";
    }
    return {};
}

std::optional<AssetTier> assetTierFromId(const std::string& id)
{
    for (const AssetTier tier : allAssetTiers()) {
        if (id == assetTierId(tier)) return tier;
    }
    return std::nullopt;
}

const char* acquisitionStructureId(AcquisitionStructure structure)
{
    switch (structure) {
    case AcquisitionStructure::SoleOwnership: return "sole_ownership";
    case AcquisitionStructure::LeveragedBuyout: return "leveraged_buyout";
    case AcquisitionStructure::RescueTakeover: return "rescue_takeover";
    }
    return "sole_ownership";
}

std::string acquisitionStructureLabel(AcquisitionStructure structure)
{
    switch (structure) {
    case AcquisitionStructure::SoleOwnership: return "완전 단독 인수";
    case AcquisitionStructure::LeveragedBuyout: return "대출 인수";
    case AcquisitionStructure::RescueTakeover: return "구조조정·구제 인수";
    }
    return {};
}

std::optional<AcquisitionStructure> acquisitionStructureFromId(const std::string& id)
{
    for (const AcquisitionStructure structure : {AcquisitionStructure::SoleOwnership,
             AcquisitionStructure::LeveragedBuyout, AcquisitionStructure::RescueTakeover}) {
        if (id == acquisitionStructureId(structure)) return structure;
    }
    return std::nullopt;
}

const char* saleWillingnessId(SaleWillingness willingness)
{
    switch (willingness) {
    case SaleWillingness::Reluctant: return "reluctant";
    case SaleWillingness::Open: return "open";
    case SaleWillingness::Eager: return "eager";
    }
    return "open";
}

std::string saleWillingnessLabel(SaleWillingness willingness)
{
    switch (willingness) {
    case SaleWillingness::Reluctant: return "매각 의사 낮음 (프리미엄 요구)";
    case SaleWillingness::Open: return "매각 협의 가능";
    case SaleWillingness::Eager: return "매각 희망";
    }
    return {};
}

ClubValuation valueClub(const ClubListing& listing, int startYear)
{
    ClubValuation valuation;
    valuation.clubId = listing.clubId;

    Money wages = weeklyWages(listing);
    if (wages <= 0) wages = std::max<Money>(listing.cashPence / 250, 0);
    valuation.revenueProxyPence = wages * 52 * 100 / 60;

    const int reputation = std::clamp(listing.reputation, 0, 100);
    int multiplePercent = 100 + reputation;
    if (listing.tier >= 2) multiplePercent = multiplePercent * 80 / 100;
    valuation.enterpriseValuePence =
        std::max(valuation.revenueProxyPence * multiplePercent / 100, kMinimumEnterpriseValue);

    valuation.netDebtPence = listing.debtPence - listing.cashPence;
    valuation.equityValuePence = std::max(
        valuation.enterpriseValuePence - valuation.netDebtPence, valuation.enterpriseValuePence / 10);
    valuation.distressed = listing.cashPence < 0
        || valuation.netDebtPence > valuation.enterpriseValuePence * 60 / 100;

    // Deterministic per club and season: the same listing always shows the same willingness.
    const std::uint32_t roll = fnv1a(listing.clubId + "|" + std::to_string(startYear)) % 4u;
    valuation.willingness = valuation.distressed ? SaleWillingness::Eager
        : roll == 0 ? SaleWillingness::Reluctant
        : roll == 3 ? SaleWillingness::Eager
                    : SaleWillingness::Open;
    valuation.askingPricePence = roundTo(
        valuation.equityValuePence * kControlPremiumPercent / 100 * willingnessPercent(valuation.willingness) / 100,
        10'000 * kPound);
    if (valuation.askingPricePence < 10'000 * kPound) valuation.askingPricePence = 10'000 * kPound;

    int difficulty = 1;
    if (reputation >= 80) difficulty += 2;
    else if (reputation >= 65) difficulty += 1;
    if (valuation.netDebtPence > valuation.enterpriseValuePence / 2) difficulty += 1;
    if (listing.squadSize > 0 && listing.squadSize < 18) difficulty += 1;
    valuation.difficulty = std::clamp(difficulty, 1, 5);
    static const char* const kDifficultyLabels[] = {"낮음", "보통", "높음", "매우 높음", "극심"};
    valuation.difficultyLabel = kDifficultyLabels[valuation.difficulty - 1];
    valuation.fanExpectationLabel = reputation >= 80 ? "즉각적인 우승 요구"
        : reputation >= 65 ? "상위권과 대회 진출 기대"
        : reputation >= 50 ? "안정과 성장 기대"
                           : "생존과 승격 기대";
    return valuation;
}

MarketContext marketContext(const std::vector<ClubValuation>& valuations)
{
    MarketContext market;
    market.listedClubs = static_cast<int>(valuations.size());
    std::vector<Money> prices;
    for (const auto& valuation : valuations) {
        if (valuation.askingPricePence > 0) prices.push_back(valuation.askingPricePence);
    }
    if (prices.empty()) return market;
    std::sort(prices.begin(), prices.end());
    // "Top tier" = the most expensive half of the market, which is the first division in a
    // two-division season; the median of that half anchors the asset tiers.
    const std::size_t from = prices.size() / 2;
    market.medianTopTierAskingPence = prices[from + (prices.size() - from) / 2];
    return market;
}

Money personalCashForTier(AssetTier tier, const MarketContext& market)
{
    int permille = 350;
    switch (tier) {
    case AssetTier::SmallInvestor: permille = 120; break;
    case AssetTier::MidTycoon: permille = 350; break;
    case AssetTier::LargeTycoon: permille = 1000; break;
    case AssetTier::MegaTycoon: permille = 2500; break;
    case AssetTier::SovereignScale: permille = 8000; break;
    }
    const Money cash = roundTo(market.medianTopTierAskingPence * permille / 1000, 100'000 * kPound);
    return std::max(cash, kMinimumPersonalCash);
}

int loanAnnualRateBpsForYear(int year)
{
    if (year < 1997) return 900;
    if (year < 2008) return 650;
    if (year < 2022) return 450;
    return 600;
}

std::vector<AcquisitionPackage> acquisitionPackages(
    const ClubListing& listing, const ClubValuation& valuation, Money personalCashPence, int startYear)
{
    std::vector<AcquisitionPackage> packages;
    const Money reserve = std::max<Money>(weeklyWages(listing), 0) * kReserveWeeks;
    const auto finish = [&](AcquisitionPackage& package) {
        package.requiredPersonalAssetsPence =
            package.cashPaidPence + package.emergencyInjectionPence + package.reservePence;
        package.affordable = package.requiredPersonalAssetsPence <= personalCashPence;
        packages.push_back(package);
    };

    {
        AcquisitionPackage sole;
        sole.structure = AcquisitionStructure::SoleOwnership;
        sole.label = acquisitionStructureLabel(sole.structure);
        sole.ownershipPercent = 100;
        sole.pricePence = valuation.askingPricePence;
        sole.cashPaidPence = valuation.askingPricePence;
        sole.assumedDebtPence = listing.debtPence;
        sole.reservePence = reserve;
        sole.fanReaction = "기대: 단독 소유로 결정이 빠르고 외부 간섭이 없습니다.";
        sole.summary = "호가 전액을 개인 자산으로 지급합니다. 구단의 기존 부채는 구단에 남습니다.";
        finish(sole);
    }
    {
        AcquisitionPackage leveraged;
        leveraged.structure = AcquisitionStructure::LeveragedBuyout;
        leveraged.label = acquisitionStructureLabel(leveraged.structure);
        leveraged.ownershipPercent = 100;
        leveraged.pricePence = valuation.askingPricePence;
        leveraged.cashPaidPence = roundTo(valuation.askingPricePence * kLeveragedEquityPercent / 100, 10'000 * kPound);
        leveraged.loanPence = valuation.askingPricePence - leveraged.cashPaidPence;
        leveraged.loanAnnualRateBps = loanAnnualRateBpsForYear(startYear);
        leveraged.loanTermMonths = kLeveragedTermMonths;
        leveraged.loanOnClub = true;
        leveraged.assumedDebtPence = listing.debtPence + leveraged.loanPence;
        leveraged.reservePence = reserve;
        leveraged.fanReaction = "경계: 인수 부채를 구단이 떠안아 현금흐름과 팬 신뢰가 나빠집니다.";
        leveraged.summary = "호가의 35%만 개인 자산으로 내고 나머지는 구단 명의 대출(10년)로 조달합니다.";
        finish(leveraged);
    }
    if (valuation.distressed) {
        AcquisitionPackage rescue;
        rescue.structure = AcquisitionStructure::RescueTakeover;
        rescue.label = acquisitionStructureLabel(rescue.structure);
        rescue.ownershipPercent = 100;
        rescue.pricePence = std::max<Money>(roundTo(valuation.equityValuePence / 2, 10'000 * kPound), 10'000 * kPound);
        rescue.cashPaidPence = rescue.pricePence;
        rescue.assumedDebtPence = listing.debtPence;
        rescue.emergencyInjectionPence =
            std::max<Money>(-listing.cashPence, 0) + std::max<Money>(weeklyWages(listing), 0) * kRescueWeeks;
        rescue.reservePence = reserve;
        rescue.fanReaction = "안도: 위기에서 구단을 구하지만 채권자·팬·언론의 압박이 큽니다.";
        rescue.summary = "낮은 가격에 인수하되 즉시 긴급 자금을 넣어야 합니다. 부채·계약은 그대로 승계합니다.";
        finish(rescue);
    }
    return packages;
}

// ---- G2 step 2: settings, pledges and applying the deal ----

const std::vector<DelegationPreset>& allDelegationPresets()
{
    static const std::vector<DelegationPreset> presets {
        DelegationPreset::HandsOn, DelegationPreset::Balanced, DelegationPreset::Chairman, DelegationPreset::Investor};
    return presets;
}

const char* delegationPresetId(DelegationPreset preset)
{
    switch (preset) {
    case DelegationPreset::HandsOn: return "hands_on";
    case DelegationPreset::Balanced: return "balanced";
    case DelegationPreset::Chairman: return "chairman";
    case DelegationPreset::Investor: return "investor";
    }
    return "balanced";
}

std::string delegationPresetLabel(DelegationPreset preset)
{
    switch (preset) {
    case DelegationPreset::HandsOn: return "적극적 구단주";
    case DelegationPreset::Balanced: return "균형형 (기본)";
    case DelegationPreset::Chairman: return "대표 회장형";
    case DelegationPreset::Investor: return "투자자형";
    }
    return "균형형 (기본)";
}

std::string delegationPresetDescription(DelegationPreset preset)
{
    switch (preset) {
    case DelegationPreset::HandsOn: return "주요 영입·핵심 계약·시설·조직·팬·상업 사안을 폭넓게 직접 관리합니다.";
    case DelegationPreset::Balanced: return "방향과 주요 결정은 직접, 일반 업무는 승인 중심, 반복 업무는 담당자에게 위임합니다.";
    case DelegationPreset::Chairman: return "예산·감독·대형 프로젝트만 직접 하고 선수단·일반 경영은 경영진에게 위임합니다.";
    case DelegationPreset::Investor: return "자본·장기 목표와 분기·시즌 평가 중심이며 대부분은 전문 책임자가 처리합니다.";
    }
    return {};
}

std::optional<DelegationPreset> delegationPresetFromId(const std::string& id)
{
    for (const auto preset : allDelegationPresets()) if (id == delegationPresetId(preset)) return preset;
    return std::nullopt;
}

const std::vector<InfoMode>& allInfoModes()
{
    static const std::vector<InfoMode> modes {InfoMode::Realistic, InfoMode::Enhanced, InfoMode::FullDisclosure};
    return modes;
}

const char* infoModeId(InfoMode mode)
{
    switch (mode) {
    case InfoMode::Realistic: return "realistic";
    case InfoMode::Enhanced: return "enhanced";
    case InfoMode::FullDisclosure: return "full_disclosure";
    }
    return "realistic";
}

std::string infoModeLabel(InfoMode mode)
{
    switch (mode) {
    case InfoMode::Realistic: return "현실적 (기본)";
    case InfoMode::Enhanced: return "향상된 정보";
    case InfoMode::FullDisclosure: return "완전 공개";
    }
    return "현실적 (기본)";
}

std::string infoModeDescription(InfoMode mode)
{
    switch (mode) {
    case InfoMode::Realistic: return "잠재력·가치·협상·의료·재정 예측이 범위와 신뢰도로 표시되고 담당자가 틀릴 수 있습니다.";
    case InfoMode::Enhanced: return "범위가 더 정확하고 위험 경고·협상 안내가 강화된 접근성 중심 설정입니다.";
    case InfoMode::FullDisclosure: return "정확한 능력·잠재력·숨은 성향·협상 한계를 공개하는 샌드박스 성격의 설정입니다.";
    }
    return {};
}

std::optional<InfoMode> infoModeFromId(const std::string& id)
{
    for (const auto mode : allInfoModes()) if (id == infoModeId(mode)) return mode;
    return std::nullopt;
}

const std::vector<GameMode>& allGameModes()
{
    static const std::vector<GameMode> modes {GameMode::Realistic, GameMode::Relaxed, GameMode::Sandbox};
    return modes;
}

const char* gameModeId(GameMode mode)
{
    switch (mode) {
    case GameMode::Realistic: return "realistic";
    case GameMode::Relaxed: return "relaxed";
    case GameMode::Sandbox: return "sandbox";
    }
    return "realistic";
}

std::string gameModeLabel(GameMode mode)
{
    switch (mode) {
    case GameMode::Realistic: return "현실적 (권장)";
    case GameMode::Relaxed: return "완화";
    case GameMode::Sandbox: return "샌드박스";
    }
    return "현실적 (권장)";
}

std::string gameModeDescription(GameMode mode)
{
    switch (mode) {
    case GameMode::Realistic: return "실제 인수·자산·규정·공정가치·등록·의사·정보·시간·대표권과 이해관계자 반응을 전부 적용합니다.";
    case GameMode::Relaxed: return "인수·협상·정보·초기 현금·신뢰 하락과 규정 경고를 관대하게 하되 기본 인과와 역할 경계는 유지합니다.";
    case GameMode::Sandbox: return "자산·인수·재정·정보·선수 의사·성장·대표권과 세계 진행을 항목별로 조정합니다. 경기 중 전술 조작은 여기서도 없습니다.";
    }
    return {};
}

std::optional<GameMode> gameModeFromId(const std::string& id)
{
    for (const auto mode : allGameModes()) if (id == gameModeId(mode)) return mode;
    return std::nullopt;
}

const char* pledgeAreaId(PledgeArea area)
{
    switch (area) {
    case PledgeArea::Investment: return "investment";
    case PledgeArea::FootballDirection: return "football_direction";
    case PledgeArea::Finance: return "finance";
    case PledgeArea::FacilitiesOrganisation: return "facilities_organisation";
    case PledgeArea::TraditionFans: return "tradition_fans";
    }
    return "investment";
}

std::string pledgeAreaLabel(PledgeArea area)
{
    switch (area) {
    case PledgeArea::Investment: return "투자";
    case PledgeArea::FootballDirection: return "축구 방향";
    case PledgeArea::Finance: return "재정";
    case PledgeArea::FacilitiesOrganisation: return "시설·조직";
    case PledgeArea::TraditionFans: return "전통·팬 관계";
    }
    return "투자";
}

std::optional<PledgeArea> pledgeAreaFromId(const std::string& id)
{
    for (const auto area : {PledgeArea::Investment, PledgeArea::FootballDirection, PledgeArea::Finance,
             PledgeArea::FacilitiesOrganisation, PledgeArea::TraditionFans}) {
        if (id == pledgeAreaId(area)) return area;
    }
    return std::nullopt;
}

const std::vector<OwnerPledgeOption>& ownerPledgeOptions()
{
    // Plan §66.8: 3-5 core pledges across investment, football direction, finance, facilities and
    // organisation, tradition and fans. Horizons are game-model deadlines; evaluation is G3.
    static const std::vector<OwnerPledgeOption> options {
        {"invest-first-window", PledgeArea::Investment, "첫 이적시장 영입 투자",
            "첫 이적시장이 닫히기 전에 호가의 10% 이상을 선수 영입에 씁니다.", 120},
        {"invest-no-fire-sale", PledgeArea::Investment, "핵심 선수 매각 없음",
            "첫 시즌 동안 1군 핵심 선수를 재정 이유로 팔지 않습니다.", 365},
        {"football-keep-manager", PledgeArea::FootballDirection, "현 감독 첫 시즌 유지",
            "성적과 무관하게 현 감독에게 첫 시즌 전체를 맡깁니다.", 365},
        {"football-youth-minutes", PledgeArea::FootballDirection, "유스·리저브 기회 확대",
            "유스·리저브 선수의 1군 출전 기회를 늘립니다.", 365},
        {"football-top-half", PledgeArea::FootballDirection, "첫 시즌 상위권",
            "첫 시즌을 리그 상위 절반으로 마칩니다.", 365},
        {"finance-no-new-debt", PledgeArea::Finance, "신규 차입 없음",
            "첫 시즌 동안 구단 명의의 새 차입을 하지 않습니다.", 365},
        {"finance-wage-discipline", PledgeArea::Finance, "급여 예산 준수",
            "주급 약정을 급여 예산 안에서 유지합니다.", 365},
        {"finance-reduce-debt", PledgeArea::Finance, "부채 10% 감축",
            "첫 시즌 안에 구단 부채를 10% 이상 줄입니다.", 365},
        {"organisation-staff", PledgeArea::FacilitiesOrganisation, "스카우팅·의료 조직 보강",
            "90일 안에 스카우팅 또는 의료 직원을 채용합니다.", 90},
        {"organisation-meetings", PledgeArea::FacilitiesOrganisation, "정기 운영 회의 유지",
            "경영진과의 정기 운영 회의를 거르지 않습니다.", 180},
        {"fans-identity", PledgeArea::TraditionFans, "구단 정체성 유지",
            "구단 이름·상징·홈을 바꾸지 않습니다.", 365},
        {"fans-transparency", PledgeArea::TraditionFans, "재정 공개",
            "팬에게 분기마다 재정 상태를 공개합니다.", 365},
        {"fans-local-ties", PledgeArea::TraditionFans, "지역·홈그로운 존중",
            "지역 출신·자체 육성 선수를 선수단에 유지합니다.", 365},
    };
    return options;
}

const OwnerPledgeOption* ownerPledgeOption(const std::string& id)
{
    for (const auto& option : ownerPledgeOptions()) if (option.id == id) return &option;
    return nullptr;
}

std::vector<std::string> settingsTags(const GameSettings& settings)
{
    std::vector<std::string> tags;
    tags.emplace_back(gameModeId(settings.gameMode));
    if (settings.infoMode == InfoMode::FullDisclosure) tags.emplace_back("full_information");
    else if (settings.infoMode == InfoMode::Enhanced) tags.emplace_back("enhanced_information");
    tags.emplace_back(std::string("delegation_") + delegationPresetId(settings.delegation));
    return tags;
}

namespace {

std::string percentText(int bps)
{
    return std::to_string(bps / 100) + "." + std::to_string((bps % 100) / 10) + "%";
}

} // namespace

std::string applyAcquisition(GameState& state, const AcquisitionTerms& terms)
{
    if (state.ownerProfile.initialized) return "이미 구단 인수가 적용된 커리어입니다.";
    const auto& package = terms.package;
    if (package.ownershipPercent < 1 || package.ownershipPercent > 100) return "지분 비율이 올바르지 않습니다.";
    if (terms.tierCashPence < 0 || package.pricePence < 0 || package.cashPaidPence < 0 || package.loanPence < 0
        || package.emergencyInjectionPence < 0) return "인수 금액이 올바르지 않습니다.";
    if (!package.affordable || package.requiredPersonalAssetsPence > terms.tierCashPence)
        return "선택한 인수 방식은 이 자산 등급의 개인 자산으로 감당할 수 없습니다.";
    if (terms.pledgeIds.size() < static_cast<std::size_t>(kMinimumPledges)
        || terms.pledgeIds.size() > static_cast<std::size_t>(kMaximumPledges))
        return "취임 약속은 " + std::to_string(kMinimumPledges) + "~" + std::to_string(kMaximumPledges) + "개를 고릅니다.";
    std::set<std::string> seen;
    std::vector<OwnerPledge> pledges;
    for (const auto& id : terms.pledgeIds) {
        const auto* option = ownerPledgeOption(id);
        if (option == nullptr) return "알 수 없는 취임 약속입니다: " + id;
        if (!seen.insert(id).second) return "같은 취임 약속을 두 번 골랐습니다: " + id;
        OwnerPledge pledge;
        pledge.id = option->id;
        pledge.area = option->area;
        pledge.title = option->title;
        pledge.detail = option->detail;
        pledge.madeDate = state.currentDate;
        pledge.deadline = state.currentDate.addDays(option->horizonDays);
        pledges.push_back(std::move(pledge));
    }
    std::set<std::string> riskIds;
    for (const auto& risk : terms.hiddenRisks) {
        if (risk.id.empty() || !riskIds.insert(risk.id).second) return "숨은 위험 id가 비었거나 중복됩니다: " + risk.id;
        if (!diligenceAreaFromId(risk.area)) return "알 수 없는 실사 영역입니다: " + risk.area;
        if (risk.severity < 1 || risk.severity > 3 || risk.detectability < 1 || risk.detectability > 3 || risk.amountPence < 0
            || !risk.surfacesOn.isValid() || risk.surfacesOn < state.currentDate)
            return "숨은 위험 값이 올바르지 않습니다: " + risk.id;
    }
    const bool loan = package.loanOnClub && package.loanPence > 0;
    std::vector<DebtRepayment> repayments;
    const std::string debtId = "acquisition-loan-" + state.club.id;
    if (loan) {
        if (std::any_of(state.debts.begin(), state.debts.end(), [&](const auto& debt) { return debt.id == debtId; }))
            return "인수 대출이 이미 있습니다.";
        try {
            repayments = makeDebtRepayments(debtId, package.loanPence, package.loanAnnualRateBps, package.loanTermMonths, state.currentDate);
        } catch (const std::invalid_argument&) {
            return "대출 조건이 올바르지 않습니다.";
        }
    }

    OwnerProfile profile;
    profile.initialized = true;
    profile.tier = terms.tier;
    profile.structure = package.structure;
    profile.ownershipPercent = package.ownershipPercent;
    profile.acquisitionDate = state.currentDate;
    profile.tierCashPence = terms.tierCashPence;
    profile.purchasePricePence = package.pricePence;
    profile.cashPaidPence = package.cashPaidPence;
    profile.acquisitionLoanPence = loan ? package.loanPence : 0;
    profile.emergencyInjectionPence = package.emergencyInjectionPence;
    profile.personalCashPence = terms.tierCashPence - package.cashPaidPence - package.emergencyInjectionPence;
    profile.askingPricePence = terms.valuation.askingPricePence;
    profile.equityValuePence = terms.valuation.equityValuePence;
    profile.difficulty = std::clamp(terms.valuation.difficulty, 1, 5);
    profile.fanReaction = package.fanReaction;

    if (loan) {
        ensureDebtLedger(state);
        ClubDebt debt;
        debt.id = debtId;
        debt.clubId = state.club.id;
        debt.purpose = "구단 인수 차입 (" + package.label + ")";
        debt.kind = "bank_loan";
        debt.originalPrincipalPence = package.loanPence;
        debt.remainingPrincipalPence = package.loanPence;
        debt.annualRateBps = package.loanAnnualRateBps;
        debt.originatedDate = state.currentDate;
        debt.firstRepaymentDate = repayments.front().dueDate;
        debt.maturityDate = repayments.back().dueDate;
        debt.status = "active";
        debt.model = "ownership_v1_gameplay";
        state.debts.push_back(debt);
        state.debtRepayments.insert(state.debtRepayments.end(), repayments.begin(), repayments.end());
        synchronizeDebtTotals(state);
        profile.acquisitionDebtId = debtId;
    }
    if (package.emergencyInjectionPence > 0) {
        state.club.finance.cashPence += package.emergencyInjectionPence;
        const OperatingProfile* operating = nullptr;
        for (const auto& candidate : state.operatingProfiles) if (candidate.clubId == state.club.id) operating = &candidate;
        if (operating != nullptr) {
            OperatingTransaction transaction;
            transaction.id = "owner-injection-" + state.club.id;
            transaction.clubId = state.club.id;
            transaction.seasonId = operating->seasonId;
            transaction.date = state.currentDate;
            transaction.category = "owner_capital";
            transaction.direction = "inflow";
            transaction.amountPence = package.emergencyInjectionPence;
            transaction.sourceId = "owner";
            transaction.provenance = operating->provenance;
            transaction.description = "구단주 긴급 자금 투입 · gameplay model";
            transaction.applied = true;
            state.operatingTransactions.push_back(std::move(transaction));
        }
    }

    // The club as the new owner found it, after the deal itself: the 100-day account compares
    // against this (G3-b).
    profile.baselineRecorded = true;
    profile.baselineCashPence = state.club.finance.cashPence;
    profile.baselineDebtPence = state.club.finance.debtPence;
    profile.baselineFanTrust = std::clamp(state.fanTendency.trustScore, 0, 100);
    profile.baselineReputation = std::clamp(state.ownerReputation.overallScore, 0, 100);

    state.ownerProfile = profile;
    state.ownerPledges = std::move(pledges);
    state.gameSettings = terms.settings;
    state.gameSettings.tags = settingsTags(terms.settings);
    state.ownerHiddenRisks = terms.hiddenRisks;

    std::string body = state.club.name + "을(를) " + package.label + "(지분 " + std::to_string(package.ownershipPercent)
        + "%)으로 인수했습니다. 인수가 " + formatMoney(package.pricePence) + " (호가 " + formatMoney(terms.valuation.askingPricePence)
        + "), 개인 자산 지급 " + formatMoney(package.cashPaidPence) + ", 남은 개인 자산 " + formatMoney(profile.personalCashPence) + ".";
    if (loan) {
        body += " 구단 명의 인수 대출 " + formatMoney(package.loanPence) + " (연 " + percentText(package.loanAnnualRateBps) + ", "
            + std::to_string(package.loanTermMonths) + "개월)로 구단 부채가 " + formatMoney(state.club.finance.debtPence) + "이(가) 됩니다.";
    }
    if (package.emergencyInjectionPence > 0) body += " 긴급 자금 " + formatMoney(package.emergencyInjectionPence) + "을(를) 구단에 넣었습니다.";
    body += " 취임 약속 " + std::to_string(state.ownerPledges.size()) + "개:";
    for (std::size_t i = 0; i < state.ownerPledges.size(); ++i) body += (i == 0 ? " " : " · ") + state.ownerPledges[i].title;
    // Only what the due-diligence report disclosed; latent risks the owner missed stay out of the mail.
    int disclosed = 0;
    std::string disclosedTitles;
    for (const auto& risk : state.ownerHiddenRisks) {
        if (!risk.disclosed) continue;
        disclosedTitles += (disclosed++ == 0 ? " " : " · ") + risk.title;
    }
    body += disclosed > 0 ? ". 실사에서 확인한 위험 " + std::to_string(disclosed) + "건:" + disclosedTitles : ". 실사에서 확인한 위험은 없습니다";
    body += ". 정보 모드 " + infoModeLabel(state.gameSettings.infoMode) + ", 게임 모드 " + gameModeLabel(state.gameSettings.gameMode)
        + ", 위임 " + delegationPresetLabel(state.gameSettings.delegation) + ". 모든 금액은 게임 모델 값이며 역사 사실이 아닙니다.";
    int inboxId = 1;
    for (const auto& item : state.inbox) inboxId = std::max(inboxId, item.id + 1);
    InboxItem item;
    item.id = inboxId;
    item.eventKey = "owner-acquisition-" + state.club.id;
    item.date = state.currentDate;
    item.category = InboxCategory::Board;
    item.title = "구단 인수 완료 · " + state.club.name;
    item.body = body;
    item.unread = true;
    state.inbox.push_back(std::move(item));
    return {};
}

std::string validateOwnership(const GameState& state)
{
    const auto& profile = state.ownerProfile;
    if (!profile.initialized) {
        if (!state.ownerPledges.empty()) return "취임 약속은 인수가 적용된 커리어에만 있습니다.";
        if (!state.ownerHiddenRisks.empty()) return "숨은 위험은 인수가 적용된 커리어에만 있습니다.";
    } else {
        if (profile.ownershipPercent < 1 || profile.ownershipPercent > 100) return "지분 비율이 올바르지 않습니다.";
        if (profile.difficulty < 1 || profile.difficulty > 5) return "인수 난도가 올바르지 않습니다.";
        if (!profile.acquisitionDate.isValid()) return "인수일이 올바르지 않습니다.";
        for (const auto amount : {profile.tierCashPence, profile.purchasePricePence, profile.cashPaidPence,
                 profile.acquisitionLoanPence, profile.emergencyInjectionPence, profile.askingPricePence}) {
            if (amount < 0) return "구단주 프로필의 금액이 음수입니다.";
        }
        if (profile.acquisitionLoanPence > 0 && profile.acquisitionDebtId.empty()) return "인수 대출의 부채 id가 없습니다.";
        if (!profile.acquisitionDebtId.empty()
            && std::none_of(state.debts.begin(), state.debts.end(), [&](const auto& debt) { return debt.id == profile.acquisitionDebtId; }))
            return "인수 대출의 부채 원장이 없습니다: " + profile.acquisitionDebtId;
        if (state.ownerPledges.size() > static_cast<std::size_t>(kMaximumPledges)) return "취임 약속이 너무 많습니다.";
    }
    std::set<std::string> ids;
    for (const auto& pledge : state.ownerPledges) {
        if (pledge.id.empty() || !ids.insert(pledge.id).second) return "취임 약속 id가 비었거나 중복됩니다.";
        if (pledge.title.empty()) return "취임 약속 제목이 없습니다: " + pledge.id;
        if (!pledge.madeDate.isValid() || !pledge.deadline.isValid() || pledge.deadline < pledge.madeDate)
            return "취임 약속 날짜가 올바르지 않습니다: " + pledge.id;
        if (pledge.status != "open" && pledge.status != "kept" && pledge.status != "broken" && pledge.status != "withdrawn")
            return "취임 약속 상태가 올바르지 않습니다: " + pledge.status;
    }
    std::set<std::string> riskIds;
    for (const auto& risk : state.ownerHiddenRisks) {
        if (risk.id.empty() || !riskIds.insert(risk.id).second) return "숨은 위험 id가 비었거나 중복됩니다.";
        if (!diligenceAreaFromId(risk.area)) return "알 수 없는 실사 영역입니다: " + risk.area;
        if (risk.title.empty() || !risk.surfacesOn.isValid()) return "숨은 위험의 제목이나 날짜가 없습니다: " + risk.id;
        if (risk.severity < 1 || risk.severity > 3 || risk.detectability < 1 || risk.detectability > 3 || risk.amountPence < 0)
            return "숨은 위험 값이 범위를 벗어납니다: " + risk.id;
        if (risk.status != "latent" && risk.status != "surfaced" && risk.status != "resolved")
            return "숨은 위험 상태가 올바르지 않습니다: " + risk.status;
    }
    if (state.gameSettings.tags.size() > 16) return "세이브 태그가 너무 많습니다.";
    return validateOwnerCapital(state);
}

} // namespace football_owner
