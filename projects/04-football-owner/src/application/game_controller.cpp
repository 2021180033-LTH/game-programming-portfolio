#include "application/game_controller.h"

#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

namespace football_owner {
namespace {

QString toQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

QString displayDate(const GameDate& date)
{
    return QStringLiteral("%1년 %2월 %3일").arg(date.year).arg(date.month).arg(date.day);
}

QString shortDate(const GameDate& date)
{
    return QStringLiteral("%1.%2.%3").arg(date.year).arg(date.month, 2, 10, QLatin1Char('0')).arg(date.day, 2, 10, QLatin1Char('0'));
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

QString categoryLabel(const InboxCategory category)
{
    switch (category) {
    case InboxCategory::Board:
        return QStringLiteral("이사회");
    case InboxCategory::Football:
        return QStringLiteral("축구 운영");
    case InboxCategory::Finance:
        return QStringLiteral("재정");
    case InboxCategory::Match:
        return QStringLiteral("경기");
    case InboxCategory::System:
        return QStringLiteral("시스템");
    }
    return QStringLiteral("시스템");
}

QString categoryColor(const InboxCategory category)
{
    switch (category) {
    case InboxCategory::Board:
        return QStringLiteral("#c7a35a");
    case InboxCategory::Football:
        return QStringLiteral("#4db58b");
    case InboxCategory::Finance:
        return QStringLiteral("#63a7d8");
    case InboxCategory::Match:
        return QStringLiteral("#d46b65");
    case InboxCategory::System:
        return QStringLiteral("#8c96a8");
    }
    return QStringLiteral("#8c96a8");
}

QString fixtureScore(const Fixture& fixture)
{
    if (!fixture.played) {
        return QStringLiteral("예정");
    }
    return QStringLiteral("%1  %2–%3  %4")
        .arg(toQString(fixture.homeClub))
        .arg(fixture.homeGoals)
        .arg(fixture.awayGoals)
        .arg(toQString(fixture.awayClub));
}

QString squadGroupLabel(const std::string& group)
{
    if (group == "First Team") {
        return QStringLiteral("1군");
    }
    if (group == "Reserve") {
        return QStringLiteral("리저브");
    }
    if (group == "Youth") {
        return QStringLiteral("유스");
    }
    if (group == "Loan") {
        return QStringLiteral("임대 중");
    }
    return toQString(group);
}

QString playerStatusLabel(const std::string& status)
{
    if (status == "Available") {
        return QStringLiteral("출전 가능");
    }
    if (status == "Rehabilitation") {
        return QStringLiteral("재활 중");
    }
    if (status == "Trainee") {
        return QStringLiteral("훈련생");
    }
    if (status == "Youth Player") {
        return QStringLiteral("유스 선수");
    }
    if (status == "Schoolboy") {
        return QStringLiteral("스쿨보이");
    }
    if (status == "On Loan") {
        return QStringLiteral("임대 중");
    }
    if (status == "Loaned In") {
        return QStringLiteral("임대 합류");
    }
    if (status == "Contract Expired · Registration Retained") {
        return QStringLiteral("계약 만료·등록권 유지");
    }
    if (status == "Contract Expired") {
        return QStringLiteral("계약 만료");
    }
    return toQString(status);
}

QString confidenceLabel(const std::string& confidence)
{
    if (confidence == "confirmed_1991_92_first_team_membership") {
        return QStringLiteral("1군 기록 확인");
    }
    if (confidence == "membership_confirmed_group_provisional") {
        return QStringLiteral("소속 확인·그룹 잠정");
    }
    if (confidence == "official_1992_youth_cohort") {
        return QStringLiteral("유스 기록 확인");
    }
    if (confidence == "academy_membership_provisional") {
        return QStringLiteral("아카데미 잠정");
    }
    if (confidence == "identity_and_club_confirmed_finance_estimated") {
        return QStringLiteral("신원·소속 확인");
    }
    if (confidence.find("identity_and_1992") != std::string::npos
        || confidence.find("identity_and_club_confirmed") != std::string::npos) {
        return QStringLiteral("신원·소속 확인 · 조건 추정");
    }
    return QStringLiteral("잠정 데이터");
}

struct SquadDiagnosticBucket {
    QString key;
    QString label;
    QString color;
    int sortOrder {};
    int total {};
    int firstTeam {};
    int reserve {};
    int youth {};
    int outgoingLoan {};
    int incomingLoan {};
    int under21 {};
    int over30 {};
    int expiringWithinYear {};
    int saleWatch {};
    int releaseWatch {};
    int ownerOpinionCount {};
    int managerOpinionCount {};
    int directorOpinionCount {};
    Money weeklyWagePence {};
};

QString squadDiagnosticGroupKey(const std::string& positionValue)
{
    const QString position = toQString(positionValue).toUpper();
    if (position.contains(QStringLiteral("GK"))) {
        return QStringLiteral("GK");
    }
    if (position.contains(QStringLiteral("CB"))
        || position.contains(QStringLiteral("RB"))
        || position.contains(QStringLiteral("LB"))
        || position.contains(QStringLiteral("DF"))) {
        return QStringLiteral("DEF");
    }
    if (position.contains(QStringLiteral("CM"))
        || position.contains(QStringLiteral("DM"))
        || position.contains(QStringLiteral("AM"))
        || position.contains(QStringLiteral("LM"))
        || position.contains(QStringLiteral("RM"))
        || position.contains(QStringLiteral("MF"))) {
        return QStringLiteral("MID");
    }
    if (position.contains(QStringLiteral("ST"))
        || position.contains(QStringLiteral("FW"))
        || position.contains(QStringLiteral("LW"))
        || position.contains(QStringLiteral("RW"))) {
        return QStringLiteral("ATT");
    }
    return QStringLiteral("OTHER");
}

int squadDiagnosticFirstTeamMinimum(const QString& key)
{
    if (key == QStringLiteral("GK")) {
        return 2;
    }
    if (key == QStringLiteral("DEF")) {
        return 6;
    }
    if (key == QStringLiteral("MID")) {
        return 6;
    }
    if (key == QStringLiteral("ATT")) {
        return 3;
    }
    return 0;
}

QString squadDiagnosticNeedLabel(const SquadDiagnosticBucket& bucket)
{
    const int minimum = squadDiagnosticFirstTeamMinimum(bucket.key);
    if (minimum > 0 && bucket.firstTeam < minimum) {
        return QStringLiteral("즉시 보강");
    }
    if (bucket.expiringWithinYear >= 2) {
        return QStringLiteral("계약 정리");
    }
    if (bucket.over30 >= 2 && bucket.under21 == 0) {
        return QStringLiteral("세대교체 관찰");
    }
    if (bucket.outgoingLoan > 0) {
        return QStringLiteral("임대 복귀 확인");
    }
    return QStringLiteral("안정");
}

QString squadDiagnosticNeedColor(const QString& label)
{
    if (label == QStringLiteral("즉시 보강")) {
        return QStringLiteral("#d78376");
    }
    if (label == QStringLiteral("계약 정리")) {
        return QStringLiteral("#d6a45f");
    }
    if (label == QStringLiteral("세대교체 관찰")) {
        return QStringLiteral("#c8b66b");
    }
    if (label == QStringLiteral("임대 복귀 확인")) {
        return QStringLiteral("#8ec5e7");
    }
    return QStringLiteral("#6fc29f");
}

QString squadDiagnosticSummary(const SquadDiagnosticBucket& bucket)
{
    const QString need = squadDiagnosticNeedLabel(bucket);
    if (need == QStringLiteral("즉시 보강")) {
        return QStringLiteral("1군 즉시전력 수가 기준보다 부족합니다.");
    }
    if (need == QStringLiteral("계약 정리")) {
        return QStringLiteral("1년 내 계약 만료 선수가 많아 재계약·매각 판단이 필요합니다.");
    }
    if (need == QStringLiteral("세대교체 관찰")) {
        return QStringLiteral("30세 이상 비중이 높고 21세 이하 후속 자원이 부족합니다.");
    }
    if (need == QStringLiteral("임대 복귀 확인")) {
        return QStringLiteral("임대 중인 선수를 다음 시즌 전력·매각 후보와 함께 재검토해야 합니다.");
    }
    return QStringLiteral("현재 구조상 즉시 경고는 없습니다.");
}

QString squadPlanningOpinionSourceLabel(
    const SquadPlanningOpinionSource source)
{
    switch (source) {
    case SquadPlanningOpinionSource::Owner:
        return QStringLiteral("구단주");
    case SquadPlanningOpinionSource::Manager:
        return QStringLiteral("감독");
    case SquadPlanningOpinionSource::Director:
        return QStringLiteral("단장");
    }
    return QStringLiteral("구단주");
}

QString squadPlanningOpinionTypeLabel(
    const SquadPlanningOpinionType type)
{
    switch (type) {
    case SquadPlanningOpinionType::Sale:
        return QStringLiteral("매각 후보");
    case SquadPlanningOpinionType::Release:
        return QStringLiteral("방출 후보");
    case SquadPlanningOpinionType::Reinforce:
        return QStringLiteral("보강 필요");
    case SquadPlanningOpinionType::Loan:
        return QStringLiteral("임대 검토");
    case SquadPlanningOpinionType::Keep:
        return QStringLiteral("유지");
    case SquadPlanningOpinionType::Monitor:
        return QStringLiteral("관찰");
    }
    return QStringLiteral("관찰");
}

QString squadPlanningOpinionTypeColor(
    const SquadPlanningOpinionType type)
{
    switch (type) {
    case SquadPlanningOpinionType::Sale:
        return QStringLiteral("#d78376");
    case SquadPlanningOpinionType::Release:
        return QStringLiteral("#b96c6c");
    case SquadPlanningOpinionType::Reinforce:
        return QStringLiteral("#8ec5e7");
    case SquadPlanningOpinionType::Loan:
        return QStringLiteral("#c8b66b");
    case SquadPlanningOpinionType::Keep:
        return QStringLiteral("#6fc29f");
    case SquadPlanningOpinionType::Monitor:
        return QStringLiteral("#8c96a8");
    }
    return QStringLiteral("#8c96a8");
}

QString squadPlanningOpinionStatusLabel(
    const SquadPlanningOpinionStatus status)
{
    switch (status) {
    case SquadPlanningOpinionStatus::Active:
        return QStringLiteral("활성");
    case SquadPlanningOpinionStatus::Resolved:
        return QStringLiteral("해결");
    case SquadPlanningOpinionStatus::Dismissed:
        return QStringLiteral("해제");
    case SquadPlanningOpinionStatus::Superseded:
        return QStringLiteral("대체됨");
    }
    return QStringLiteral("활성");
}

OwnerActionImpact ownerImpactForSquadOpinion(
    const SquadPlanningOpinionType type,
    const Player& player,
    const GameDate& date)
{
    OwnerActionImpact impact;
    switch (type) {
    case SquadPlanningOpinionType::Sale:
    case SquadPlanningOpinionType::Release:
        impact.prudenceDelta = 1;
        impact.fanTrustDelta = -1;
        impact.skepticismDelta = 1;
        break;
    case SquadPlanningOpinionType::Loan:
        impact.balancedPreferenceDelta = 1;
        impact.loyaltyDelta = 1;
        if (const auto age = ageOnDate(player, date);
            age.has_value() && *age <= 23) {
            impact.youthPreferenceDelta = 1;
        }
        break;
    case SquadPlanningOpinionType::Keep:
        impact.loyaltyDelta = 1;
        impact.balancedPreferenceDelta = 1;
        break;
    case SquadPlanningOpinionType::Reinforce:
        impact.ambitionDelta = 1;
        break;
    case SquadPlanningOpinionType::Monitor:
        break;
    }
    return impact;
}

OwnerActionImpact ownerImpactForOperatingMeeting(
    const OperatingMeetingAgendaType agenda,
    const std::vector<OperatingMeetingAttendee>& attendees)
{
    OwnerActionImpact impact;
    impact.reputationDelta = 1;
    impact.fanTrustDelta = 1;
    const bool managerIncluded = std::any_of(
        attendees.begin(),
        attendees.end(),
        [](const OperatingMeetingAttendee& attendee) {
            return attendee.role == "manager";
        });
    if (managerIncluded) {
        impact.autonomyDelta = 1;
    } else if (agenda == OperatingMeetingAgendaType::SquadPlanning
        || agenda == OperatingMeetingAgendaType::Staff) {
        impact.autonomyDelta = -1;
        impact.skepticismDelta = 1;
    }

    switch (agenda) {
    case OperatingMeetingAgendaType::TransferMarket:
        impact.ambitionDelta = 1;
        break;
    case OperatingMeetingAgendaType::Finance:
        impact.prudenceDelta = 1;
        break;
    case OperatingMeetingAgendaType::SquadPlanning:
        impact.balancedPreferenceDelta = 1;
        break;
    case OperatingMeetingAgendaType::Scouting:
        impact.ambitionDelta = 1;
        impact.youthPreferenceDelta = 1;
        break;
    case OperatingMeetingAgendaType::Staff:
        impact.autonomyDelta += 1;
        break;
    case OperatingMeetingAgendaType::Crisis:
        impact.skepticismDelta = 1;
        break;
    case OperatingMeetingAgendaType::General:
        break;
    }
    return impact;
}

QString fanMoodDisplayLabel(const std::string& mood)
{
    if (mood == "backing_owner") {
        return QStringLiteral("구단주 지지");
    }
    if (mood == "cautiously_supportive") {
        return QStringLiteral("조심스러운 지지");
    }
    if (mood == "skeptical") {
        return QStringLiteral("의심 증가");
    }
    if (mood == "hostile") {
        return QStringLiteral("불만 고조");
    }
    return QStringLiteral("관망");
}

QString ownerActionTypeLabel(const std::string& actionType)
{
    if (actionType == "approve_transfer") {
        return QStringLiteral("영입 승인");
    }
    if (actionType == "reject_transfer") {
        return QStringLiteral("영입 거절");
    }
    if (actionType == "target_scouting_directive") {
        return QStringLiteral("특정 스카우팅");
    }
    if (actionType == "range_scouting_directive") {
        return QStringLiteral("범위 스카우팅");
    }
    if (actionType == "set_squad_opinion") {
        return QStringLiteral("선수단 의견");
    }
    if (actionType == "clear_squad_opinion") {
        return QStringLiteral("의견 해제");
    }
    if (actionType == "call_operating_meeting") {
        return QStringLiteral("운영 회의");
    }
    return QStringLiteral("구단주 행동");
}

QString ownerActionCategoryLabel(const std::string& category)
{
    if (category == "transfer_market") {
        return QStringLiteral("이적시장");
    }
    if (category == "scouting") {
        return QStringLiteral("스카우팅");
    }
    if (category == "squad_planning") {
        return QStringLiteral("선수단 계획");
    }
    if (category == "owner_ops") {
        return QStringLiteral("구단 운영");
    }
    return QStringLiteral("운영");
}

QString ownerActionImpactLabel(const OwnerActionLogEntry& action)
{
    QStringList parts;
    if (action.reputationDelta != 0) {
        parts << QStringLiteral("평판 %1%2")
                     .arg(action.reputationDelta > 0 ? "+" : "")
                     .arg(action.reputationDelta);
    }
    if (action.fanTrustDelta != 0) {
        parts << QStringLiteral("팬 신뢰 %1%2")
                     .arg(action.fanTrustDelta > 0 ? "+" : "")
                     .arg(action.fanTrustDelta);
    }
    if (action.ambitionDelta != 0) {
        parts << QStringLiteral("야망 %1%2")
                     .arg(action.ambitionDelta > 0 ? "+" : "")
                     .arg(action.ambitionDelta);
    }
    if (action.prudenceDelta != 0) {
        parts << QStringLiteral("재정 신중성 %1%2")
                     .arg(action.prudenceDelta > 0 ? "+" : "")
                     .arg(action.prudenceDelta);
    }
    if (action.loyaltyDelta != 0) {
        parts << QStringLiteral("선수단 충성 %1%2")
                     .arg(action.loyaltyDelta > 0 ? "+" : "")
                     .arg(action.loyaltyDelta);
    }
    if (action.autonomyDelta != 0) {
        parts << QStringLiteral("감독 자율성 %1%2")
                     .arg(action.autonomyDelta > 0 ? "+" : "")
                     .arg(action.autonomyDelta);
    }
    return parts.isEmpty() ? QStringLiteral("기록만 남김") : parts.join(QStringLiteral(" · "));
}

QString dominantFanTendencyLabel(const FanTendencyState& fanState)
{
    const struct {
        int value;
        const char* label;
    } tendencies[] {
        {fanState.starSigningPreference, "스타 영입 선호"},
        {fanState.homegrownPreference, "홈그로운 중시"},
        {fanState.youthDevelopmentPreference, "유망주 육성 선호"},
        {fanState.balancedSquadPreference, "신구 조화 선호"}
    };
    const auto top = std::max_element(
        std::begin(tendencies),
        std::end(tendencies),
        [](const auto& left, const auto& right) {
            return left.value < right.value;
        });
    return QStringLiteral("%1 %2/100")
        .arg(QString::fromUtf8(top->label))
        .arg(top->value);
}

std::string currentSquadPlanningSeasonId(const GameState& state)
{
    if (!state.competitionSeasonTeams.empty()) {
        return state.competitionSeasonTeams.front().seasonId;
    }
    return std::to_string(state.currentDate.year) + "-"
        + std::to_string((state.currentDate.year + 1) % 100);
}

const SquadPlanningOpinion* activePlayerSquadOpinion(
    const GameState& state,
    const std::string& playerId,
    const SquadPlanningOpinionSource source)
{
    const auto opinion = std::find_if(
        state.squadPlanningOpinions.begin(),
        state.squadPlanningOpinions.end(),
        [&playerId, source](const SquadPlanningOpinion& item) {
            return item.status == SquadPlanningOpinionStatus::Active
                && item.source == source
                && item.playerId.has_value()
                && *item.playerId == playerId;
        });
    return opinion == state.squadPlanningOpinions.end() ? nullptr : &*opinion;
}

QString squadPlanningOpinionDisplay(
    const SquadPlanningOpinion* opinion)
{
    return opinion == nullptr
        ? QStringLiteral("미지정")
        : squadPlanningOpinionTypeLabel(opinion->type);
}

QString operatingMeetingCadenceDisplay(
    const OperatingMeetingCadence cadence)
{
    switch (cadence) {
    case OperatingMeetingCadence::Off:
        return QStringLiteral("개최 안 함");
    case OperatingMeetingCadence::UrgentOnly:
        return QStringLiteral("긴급 안건만");
    case OperatingMeetingCadence::Monthly:
        return QStringLiteral("월간");
    case OperatingMeetingCadence::Quarterly:
        return QStringLiteral("분기");
    case OperatingMeetingCadence::TransferWindows:
        return QStringLiteral("이적시장 전후");
    case OperatingMeetingCadence::SeasonMilestones:
        return QStringLiteral("시즌 주요 시점");
    case OperatingMeetingCadence::Custom:
        return QStringLiteral("사용자 지정");
    }
    return QStringLiteral("개최 안 함");
}

QString operatingMeetingAgendaLabel(
    const OperatingMeetingAgendaType agenda)
{
    switch (agenda) {
    case OperatingMeetingAgendaType::General:
        return QStringLiteral("종합 운영");
    case OperatingMeetingAgendaType::SquadPlanning:
        return QStringLiteral("선수단 계획");
    case OperatingMeetingAgendaType::TransferMarket:
        return QStringLiteral("이적시장");
    case OperatingMeetingAgendaType::Finance:
        return QStringLiteral("재정");
    case OperatingMeetingAgendaType::Staff:
        return QStringLiteral("스태프/권한");
    case OperatingMeetingAgendaType::Scouting:
        return QStringLiteral("스카우팅");
    case OperatingMeetingAgendaType::Crisis:
        return QStringLiteral("긴급 안건");
    }
    return QStringLiteral("종합 운영");
}

QString operatingMeetingAgendaColor(
    const OperatingMeetingAgendaType agenda)
{
    switch (agenda) {
    case OperatingMeetingAgendaType::SquadPlanning:
        return QStringLiteral("#8ec5e7");
    case OperatingMeetingAgendaType::TransferMarket:
        return QStringLiteral("#d0b86b");
    case OperatingMeetingAgendaType::Finance:
        return QStringLiteral("#7fbea6");
    case OperatingMeetingAgendaType::Staff:
        return QStringLiteral("#b89bd6");
    case OperatingMeetingAgendaType::Scouting:
        return QStringLiteral("#6fc29f");
    case OperatingMeetingAgendaType::Crisis:
        return QStringLiteral("#d78376");
    case OperatingMeetingAgendaType::General:
        return QStringLiteral("#8c96a8");
    }
    return QStringLiteral("#8c96a8");
}

QString operatingMeetingStatusLabel(const OperatingMeetingStatus status)
{
    switch (status) {
    case OperatingMeetingStatus::Scheduled:
        return QStringLiteral("예정");
    case OperatingMeetingStatus::Held:
        return QStringLiteral("완료");
    case OperatingMeetingStatus::Cancelled:
        return QStringLiteral("취소");
    }
    return QStringLiteral("완료");
}

std::optional<GameDate> nextOperatingMeetingDateForCadence(
    const GameDate& currentDate,
    const OperatingMeetingCadence cadence)
{
    switch (cadence) {
    case OperatingMeetingCadence::Monthly:
        return currentDate.addMonths(1);
    case OperatingMeetingCadence::Quarterly:
        return currentDate.addMonths(3);
    case OperatingMeetingCadence::TransferWindows:
        if (currentDate < GameDate {1992, 8, 1}) {
            return GameDate {1992, 8, 1};
        }
        if (currentDate < GameDate {1993, 3, 31}) {
            return GameDate {1993, 3, 31};
        }
        return currentDate.addMonths(4);
    case OperatingMeetingCadence::SeasonMilestones:
        if (currentDate < GameDate {1992, 8, 15}) {
            return GameDate {1992, 8, 15};
        }
        if (currentDate < GameDate {1993, 1, 1}) {
            return GameDate {1993, 1, 1};
        }
        if (currentDate < GameDate {1993, 6, 1}) {
            return GameDate {1993, 6, 1};
        }
        return currentDate.addMonths(3);
    case OperatingMeetingCadence::Off:
    case OperatingMeetingCadence::UrgentOnly:
    case OperatingMeetingCadence::Custom:
        return std::nullopt;
    }
    return std::nullopt;
}

QString operatingMeetingAttendeeRoleLabel(const std::string& role)
{
    if (role == "owner") {
        return QStringLiteral("구단주");
    }
    if (role == "manager") {
        return QStringLiteral("감독");
    }
    if (role == "director") {
        return QStringLiteral("단장/운영");
    }
    if (role == "finance") {
        return QStringLiteral("재정");
    }
    if (role == "scouting") {
        return QStringLiteral("스카우팅");
    }
    if (role == "medical") {
        return QStringLiteral("의료");
    }
    return toQString(role);
}

std::string scoutingLeadName(const GameState& state)
{
    const auto chief = std::find_if(
        state.scouts.begin(),
        state.scouts.end(),
        [](const ScoutProfile& scout) {
            return scout.role.find("Chief") != std::string::npos;
        });
    if (chief != state.scouts.end()) {
        return chief->fullName;
    }
    return state.scouts.empty()
        ? std::string {"스카우팅 담당"}
        : state.scouts.front().fullName;
}

void addOperatingMeetingAttendee(
    std::vector<OperatingMeetingAttendee>& attendees,
    std::string role,
    std::string displayName,
    const bool autoAssigned,
    std::string notes)
{
    const auto existing = std::find_if(
        attendees.begin(),
        attendees.end(),
        [&role](const OperatingMeetingAttendee& attendee) {
            return attendee.role == role;
        });
    if (existing != attendees.end()) {
        return;
    }
    attendees.push_back({
        std::move(role),
        std::move(displayName),
        autoAssigned,
        std::move(notes)
    });
}

std::vector<OperatingMeetingAttendee> makeOperatingMeetingAttendees(
    const GameState& state,
    const OperatingMeetingAgendaType agenda,
    const bool autoAssign,
    const bool includeManager,
    const bool includeDirector,
    const bool includeFinance,
    const bool includeScouting,
    const bool includeMedical)
{
    std::vector<OperatingMeetingAttendee> attendees;
    addOperatingMeetingAttendee(
        attendees,
        "owner",
        state.ownerName,
        autoAssign,
        "회의 소집자");

    const auto addManager = [&] {
        addOperatingMeetingAttendee(
            attendees,
            "manager",
            state.club.manager.name,
            autoAssign,
            "경기·선수단 책임자");
    };
    const auto addDirector = [&] {
        addOperatingMeetingAttendee(
            attendees,
            "director",
            "구단 운영 책임자",
            autoAssign,
            "1992년 초기 조직에서는 단장/운영 역할을 겸임할 수 있음");
    };
    const auto addFinance = [&] {
        addOperatingMeetingAttendee(
            attendees,
            "finance",
            "재정 담당",
            autoAssign,
            "예산·현금·부채 관점");
    };
    const auto addScouting = [&] {
        addOperatingMeetingAttendee(
            attendees,
            "scouting",
            scoutingLeadName(state),
            autoAssign,
            "스카우팅·시장 정보");
    };
    const auto addMedical = [&] {
        addOperatingMeetingAttendee(
            attendees,
            "medical",
            "의료 담당",
            autoAssign,
            "부상·의료 리스크");
    };

    if (autoAssign) {
        addManager();
        switch (agenda) {
        case OperatingMeetingAgendaType::SquadPlanning:
            addDirector();
            addScouting();
            break;
        case OperatingMeetingAgendaType::TransferMarket:
            addDirector();
            addFinance();
            addScouting();
            break;
        case OperatingMeetingAgendaType::Finance:
            addDirector();
            addFinance();
            break;
        case OperatingMeetingAgendaType::Staff:
            addDirector();
            addFinance();
            break;
        case OperatingMeetingAgendaType::Scouting:
            addDirector();
            addScouting();
            break;
        case OperatingMeetingAgendaType::Crisis:
            addDirector();
            addFinance();
            addScouting();
            addMedical();
            break;
        case OperatingMeetingAgendaType::General:
            addDirector();
            addFinance();
            break;
        }
    } else {
        if (includeManager) {
            addManager();
        }
        if (includeDirector) {
            addDirector();
        }
        if (includeFinance) {
            addFinance();
        }
        if (includeScouting) {
            addScouting();
        }
        if (includeMedical) {
            addMedical();
        }
    }

    return attendees;
}

QString operatingMeetingAttendeeSummary(
    const std::vector<OperatingMeetingAttendee>& attendees)
{
    QStringList labels;
    for (const auto& attendee : attendees) {
        labels.push_back(
            operatingMeetingAttendeeRoleLabel(attendee.role)
            + QStringLiteral(" ")
            + toQString(attendee.displayName));
    }
    return labels.join(QStringLiteral(" · "));
}

QString operatingMeetingSummary(
    const GameState& state,
    const OperatingMeetingAgendaType agenda,
    const std::vector<OperatingMeetingAttendee>& attendees)
{
    const QString attendeeText = operatingMeetingAttendeeSummary(attendees);
    switch (agenda) {
    case OperatingMeetingAgendaType::SquadPlanning:
    {
        const int ownerOpinionCount = static_cast<int>(std::count_if(
            state.squadPlanningOpinions.begin(),
            state.squadPlanningOpinions.end(),
            [](const SquadPlanningOpinion& opinion) {
                return opinion.source == SquadPlanningOpinionSource::Owner
                    && opinion.status == SquadPlanningOpinionStatus::Active;
            }));
        const int activeOpinionCount = static_cast<int>(std::count_if(
            state.squadPlanningOpinions.begin(),
            state.squadPlanningOpinions.end(),
            [](const SquadPlanningOpinion& opinion) {
                return opinion.status == SquadPlanningOpinionStatus::Active;
            }));
        return QStringLiteral(
            "선수단 구조 진단과 저장된 계획 의견을 검토했습니다. "
            "구단주 의견 %1건, 전체 의견 %2건을 기준으로 감독·단장 후속 의견을 정리합니다. 참석: %3")
            .arg(ownerOpinionCount)
            .arg(activeOpinionCount)
            .arg(attendeeText);
    }
    case OperatingMeetingAgendaType::TransferMarket:
        return QStringLiteral(
            "진행 중인 영입 협상 %1건, 판매 제안 %2건, AI 시장 거래 %3건을 확인했습니다. 참석: %4")
            .arg(static_cast<int>(state.transferPursuits.size()))
            .arg(static_cast<int>(state.outgoingTransferOffers.size()))
            .arg(static_cast<int>(state.aiMarketDeals.size()))
            .arg(attendeeText);
    case OperatingMeetingAgendaType::Finance:
        return QStringLiteral(
            "현재 현금 %1, 이적 가용 현금 %2, 주급 사용률을 점검했습니다. 참석: %3")
            .arg(toQString(formatMoney(state.club.finance.cashPence)))
            .arg(toQString(formatMoney(football_owner::availableTransferCash(state))))
            .arg(attendeeText);
    case OperatingMeetingAgendaType::Staff:
        return QStringLiteral(
            "감독 권한, 스태프 역할 겸임, 추후 조직 분리 필요성을 확인했습니다. 참석: %1")
            .arg(attendeeText);
    case OperatingMeetingAgendaType::Scouting:
        return QStringLiteral(
            "활성 스카우팅 배정 %1건, 완료 보고서 %2건과 다음 지시 확장 필요성을 검토했습니다. 참석: %3")
            .arg(static_cast<int>(state.scoutingAssignments.size()))
            .arg(static_cast<int>(state.scoutingReports.size()))
            .arg(attendeeText);
    case OperatingMeetingAgendaType::Crisis:
        return QStringLiteral(
            "긴급 안건을 중심으로 축구·재정·시장 리스크를 동시에 검토했습니다. 참석: %1")
            .arg(attendeeText);
    case OperatingMeetingAgendaType::General:
        return QStringLiteral(
            "구단 운영 전반, 감독 신뢰, 재정, 선수단 위험을 종합 점검했습니다. 참석: %1")
            .arg(attendeeText);
    }
    return QStringLiteral("운영 회의를 완료했습니다. 참석: %1").arg(attendeeText);
}

void upsertMeetingPositionOpinion(
    GameState& state,
    const OperatingMeeting& meeting,
    const SquadPlanningOpinionSource source,
    const SquadPlanningOpinionType type,
    const QString& positionGroup,
    const int priority,
    const QString& reason)
{
    const std::string group = toStdString(positionGroup);
    auto opinion = std::find_if(
        state.squadPlanningOpinions.begin(),
        state.squadPlanningOpinions.end(),
        [source, &group](const SquadPlanningOpinion& item) {
            return item.source == source
                && !item.playerId.has_value()
                && item.positionGroup.has_value()
                && *item.positionGroup == group;
        });
    if (opinion == state.squadPlanningOpinions.end()) {
        SquadPlanningOpinion created;
        created.id = squadPlanningOpinionSourceKey(source)
            + "-position-" + group;
        created.createdDate = meeting.meetingDate;
        state.squadPlanningOpinions.push_back(std::move(created));
        opinion = std::prev(state.squadPlanningOpinions.end());
    }

    opinion->seasonId = currentSquadPlanningSeasonId(state);
    opinion->clubId = state.club.id;
    opinion->playerId.reset();
    opinion->positionGroup = group;
    opinion->source = source;
    opinion->type = type;
    opinion->priority = std::clamp(priority, 1, 5);
    opinion->reason = toStdString(reason);
    opinion->targetWindow = "current_season";
    opinion->status = SquadPlanningOpinionStatus::Active;
    if (opinion->createdDate.year == 1970) {
        opinion->createdDate = meeting.meetingDate;
    }
    opinion->updatedDate = meeting.meetingDate;
    opinion->dataConfidence = "meeting_generated_staff_opinion";
}

void applySquadPlanningMeetingOutputs(
    GameState& state,
    const OperatingMeeting& meeting)
{
    std::vector<SquadDiagnosticBucket> buckets {
        {QStringLiteral("GK"), QStringLiteral("골키퍼"), QStringLiteral("#63a7d8"), 0},
        {QStringLiteral("DEF"), QStringLiteral("수비"), QStringLiteral("#6fc29f"), 1},
        {QStringLiteral("MID"), QStringLiteral("미드필드"), QStringLiteral("#d0b86b"), 2},
        {QStringLiteral("ATT"), QStringLiteral("공격"), QStringLiteral("#d78376"), 3},
        {QStringLiteral("OTHER"), QStringLiteral("기타"), QStringLiteral("#8c96a8"), 4}
    };
    const auto findBucket = [&buckets](const QString& key) -> SquadDiagnosticBucket& {
        const auto bucket = std::find_if(
            buckets.begin(),
            buckets.end(),
            [&key](const SquadDiagnosticBucket& item) {
                return item.key == key;
            });
        return bucket != buckets.end() ? *bucket : buckets.back();
    };

    for (const auto& player : state.players) {
        auto& bucket = findBucket(squadDiagnosticGroupKey(player.position));
        ++bucket.total;
        if (player.squadGroup == "First Team") {
            ++bucket.firstTeam;
        }

        const auto contract = std::find_if(
            state.playerContracts.begin(),
            state.playerContracts.end(),
            [&player](const PlayerContract& item) {
                return item.playerId == player.id
                    && item.lifecycleStatus != "expired_free_agent";
            });
        if (contract == state.playerContracts.end()) {
            continue;
        }
        const int daysUntilExpiry =
            state.currentDate.daysUntil(contract->endDate);
        if (daysUntilExpiry <= 365) {
            ++bucket.expiringWithinYear;
        }
        const auto age = ageOnDate(player, state.currentDate);
        const bool highWage =
            state.club.finance.weeklyWageBudgetPence > 0
            && contract->weeklyWagePence
                >= state.club.finance.weeklyWageBudgetPence / 10;
        if (age.has_value()
            && *age >= 30
            && (daysUntilExpiry <= 730 || highWage)) {
            ++bucket.saleWatch;
        }
    }

    const auto managerBucket = std::find_if(
        buckets.begin(),
        buckets.end(),
        [](const SquadDiagnosticBucket& bucket) {
            return squadDiagnosticFirstTeamMinimum(bucket.key) > 0
                && bucket.firstTeam < squadDiagnosticFirstTeamMinimum(bucket.key);
        });
    if (managerBucket != buckets.end()) {
        upsertMeetingPositionOpinion(
            state,
            meeting,
            SquadPlanningOpinionSource::Manager,
            SquadPlanningOpinionType::Reinforce,
            managerBucket->key,
            4,
            QStringLiteral("운영 회의에서 감독이 %1 1군 즉시전력 보강 필요를 제기")
                .arg(managerBucket->label));
    }

    const auto directorBucket = std::find_if(
        buckets.begin(),
        buckets.end(),
        [](const SquadDiagnosticBucket& bucket) {
            return bucket.expiringWithinYear > 0 || bucket.saleWatch > 0;
        });
    if (directorBucket != buckets.end()) {
        upsertMeetingPositionOpinion(
            state,
            meeting,
            SquadPlanningOpinionSource::Director,
            SquadPlanningOpinionType::Monitor,
            directorBucket->key,
            3,
            QStringLiteral("운영 회의에서 단장이 %1 계약·매각 리스크 관찰을 권고")
                .arg(directorBucket->label));
    }
}

QString transferStatusLabel(const TransferStatus status)
{
    switch (status) {
    case TransferStatus::Pending:
        return QStringLiteral("승인 대기");
    case TransferStatus::Approved:
        return QStringLiteral("승인 · 합류 대기");
    case TransferStatus::Rejected:
        return QStringLiteral("거절");
    case TransferStatus::Completed:
        return QStringLiteral("영입 완료");
    case TransferStatus::Expired:
        return QStringLiteral("기한 만료");
    }
    return QStringLiteral("승인 대기");
}

QString transferStatusColor(const TransferStatus status)
{
    switch (status) {
    case TransferStatus::Pending:
        return QStringLiteral("#e2bd6b");
    case TransferStatus::Approved:
        return QStringLiteral("#63a7d8");
    case TransferStatus::Rejected:
    case TransferStatus::Expired:
        return QStringLiteral("#d46b65");
    case TransferStatus::Completed:
        return QStringLiteral("#4db58b");
    }
    return QStringLiteral("#8c96a8");
}

QString transferPursuitStageLabel(const TransferPursuitStage stage)
{
    switch (stage) {
    case TransferPursuitStage::AwaitingClubResponse:
        return QStringLiteral("구단 답변 대기");
    case TransferPursuitStage::ClubCountered:
        return QStringLiteral("역제안 결정 필요");
    case TransferPursuitStage::AwaitingPlayerTerms:
        return QStringLiteral("선수 계약 제안 필요");
    case TransferPursuitStage::AwaitingPlayerResponse:
        return QStringLiteral("선수 답변 대기");
    case TransferPursuitStage::PlayerCountered:
        return QStringLiteral("선수 역제안 결정 필요");
    case TransferPursuitStage::AwaitingMedical:
        return QStringLiteral("메디컬 대기");
    case TransferPursuitStage::AwaitingRegistration:
        return QStringLiteral("등록 확인 대기");
    case TransferPursuitStage::AwaitingOwnerApproval:
        return QStringLiteral("최종 승인 대기");
    case TransferPursuitStage::AwaitingCompletion:
        return QStringLiteral("승인 · 합류 대기");
    case TransferPursuitStage::ClubRejected:
        return QStringLiteral("구단 협상 결렬");
    case TransferPursuitStage::PlayerRejected:
        return QStringLiteral("선수 협상 결렬");
    case TransferPursuitStage::MedicalFailed:
        return QStringLiteral("메디컬 실패");
    case TransferPursuitStage::RegistrationFailed:
        return QStringLiteral("등록 실패");
    case TransferPursuitStage::OwnerRejected:
        return QStringLiteral("구단주 거절");
    case TransferPursuitStage::Withdrawn:
        return QStringLiteral("협상 철회");
    case TransferPursuitStage::Completed:
        return QStringLiteral("영입 완료");
    case TransferPursuitStage::Expired:
        return QStringLiteral("기한 만료");
    }
    return QStringLiteral("협상 진행");
}

QString transferPursuitStageColor(const TransferPursuitStage stage)
{
    switch (stage) {
    case TransferPursuitStage::AwaitingClubResponse:
    case TransferPursuitStage::AwaitingPlayerResponse:
    case TransferPursuitStage::AwaitingMedical:
    case TransferPursuitStage::AwaitingRegistration:
    case TransferPursuitStage::AwaitingCompletion:
        return QStringLiteral("#63a7d8");
    case TransferPursuitStage::ClubCountered:
    case TransferPursuitStage::AwaitingPlayerTerms:
    case TransferPursuitStage::PlayerCountered:
    case TransferPursuitStage::AwaitingOwnerApproval:
        return QStringLiteral("#e2bd6b");
    case TransferPursuitStage::Completed:
        return QStringLiteral("#4db58b");
    case TransferPursuitStage::ClubRejected:
    case TransferPursuitStage::PlayerRejected:
    case TransferPursuitStage::MedicalFailed:
    case TransferPursuitStage::RegistrationFailed:
    case TransferPursuitStage::OwnerRejected:
    case TransferPursuitStage::Withdrawn:
    case TransferPursuitStage::Expired:
        return QStringLiteral("#d46b65");
    }
    return QStringLiteral("#8c96a8");
}

QString outgoingTransferStatusLabel(const OutgoingTransferStatus status)
{
    switch (status) {
    case OutgoingTransferStatus::Pending:
        return QStringLiteral("결정 대기");
    case OutgoingTransferStatus::Accepted:
        return QStringLiteral("수락 · 이적 대기");
    case OutgoingTransferStatus::Rejected:
        return QStringLiteral("거절");
    case OutgoingTransferStatus::Completed:
        return QStringLiteral("판매 완료");
    case OutgoingTransferStatus::Expired:
        return QStringLiteral("기한 만료");
    }
    return QStringLiteral("결정 대기");
}

QString outgoingTransferStatusColor(const OutgoingTransferStatus status)
{
    switch (status) {
    case OutgoingTransferStatus::Pending:
        return QStringLiteral("#e2bd6b");
    case OutgoingTransferStatus::Accepted:
        return QStringLiteral("#63a7d8");
    case OutgoingTransferStatus::Completed:
        return QStringLiteral("#4db58b");
    case OutgoingTransferStatus::Rejected:
    case OutgoingTransferStatus::Expired:
        return QStringLiteral("#d46b65");
    }
    return QStringLiteral("#8c96a8");
}

QString outgoingNegotiationStageLabel(const OutgoingNegotiationStage stage)
{
    switch (stage) {
    case OutgoingNegotiationStage::DecisionPending:
        return QStringLiteral("최종 결정 대기");
    case OutgoingNegotiationStage::AwaitingBuyerResponse:
        return QStringLiteral("구매 구단 답변 대기");
    case OutgoingNegotiationStage::BuyerCountered:
        return QStringLiteral("구매 구단 역제안");
    case OutgoingNegotiationStage::AgreementReached:
        return QStringLiteral("세부 조건 합의");
    case OutgoingNegotiationStage::Closed:
        return QStringLiteral("협상 종료");
    }
    return QStringLiteral("최종 결정 대기");
}

QString loanDirectionLabel(const LoanDirection direction)
{
    return direction == LoanDirection::Incoming
        ? QStringLiteral("들어오는 임대")
        : QStringLiteral("내보내는 임대");
}

QString loanStageLabel(const LoanStage stage)
{
    switch (stage) {
    case LoanStage::DecisionPending:
        return QStringLiteral("결정 대기");
    case LoanStage::AwaitingClubResponse:
        return QStringLiteral("상대 구단 답변 대기");
    case LoanStage::ClubCountered:
        return QStringLiteral("상대 구단 역제안");
    case LoanStage::AwaitingStart:
        return QStringLiteral("합의 · 임대 시작 대기");
    case LoanStage::Active:
        return QStringLiteral("임대 중");
    case LoanStage::RecallPending:
        return QStringLiteral("조기 복귀 대기");
    case LoanStage::Recalled:
        return QStringLiteral("조기 복귀 완료");
    case LoanStage::Completed:
        return QStringLiteral("임대 종료");
    case LoanStage::Rejected:
        return QStringLiteral("거절·결렬");
    case LoanStage::Withdrawn:
        return QStringLiteral("철회");
    case LoanStage::Expired:
        return QStringLiteral("기한 만료");
    }
    return QStringLiteral("임대 안건");
}

QString loanStageColor(const LoanStage stage)
{
    switch (stage) {
    case LoanStage::DecisionPending:
    case LoanStage::ClubCountered:
        return QStringLiteral("#e2bd6b");
    case LoanStage::AwaitingClubResponse:
    case LoanStage::AwaitingStart:
    case LoanStage::RecallPending:
        return QStringLiteral("#63a7d8");
    case LoanStage::Active:
    case LoanStage::Completed:
    case LoanStage::Recalled:
        return QStringLiteral("#4db58b");
    case LoanStage::Rejected:
    case LoanStage::Withdrawn:
    case LoanStage::Expired:
        return QStringLiteral("#d46b65");
    }
    return QStringLiteral("#8c96a8");
}

QString contractRenewalStageLabel(const ContractRenewalStage stage)
{
    switch (stage) {
    case ContractRenewalStage::Recommended:
        return QStringLiteral("협상 권고");
    case ContractRenewalStage::AwaitingPlayerResponse:
        return QStringLiteral("선수 답변 대기");
    case ContractRenewalStage::PlayerCountered:
        return QStringLiteral("선수 측 역제안");
    case ContractRenewalStage::AgreementReached:
        return QStringLiteral("합의 · 최종 서명 대기");
    case ContractRenewalStage::Completed:
        return QStringLiteral("재계약 완료");
    case ContractRenewalStage::Rejected:
        return QStringLiteral("협상 결렬");
    case ContractRenewalStage::Withdrawn:
        return QStringLiteral("협상 철회");
    case ContractRenewalStage::Expired:
        return QStringLiteral("협상 기한 만료");
    }
    return QStringLiteral("재계약 검토");
}

QString contractRenewalStageColor(const ContractRenewalStage stage)
{
    switch (stage) {
    case ContractRenewalStage::Recommended:
    case ContractRenewalStage::PlayerCountered:
    case ContractRenewalStage::AgreementReached:
        return QStringLiteral("#e2bd6b");
    case ContractRenewalStage::AwaitingPlayerResponse:
        return QStringLiteral("#63a7d8");
    case ContractRenewalStage::Completed:
        return QStringLiteral("#4db58b");
    case ContractRenewalStage::Rejected:
    case ContractRenewalStage::Withdrawn:
    case ContractRenewalStage::Expired:
        return QStringLiteral("#d46b65");
    }
    return QStringLiteral("#8c96a8");
}

QString aiMarketDealTypeLabel(const AiMarketDealType type)
{
    switch (type) {
    case AiMarketDealType::Transfer:
        return QStringLiteral("영입");
    case AiMarketDealType::Loan:
        return QStringLiteral("임대");
    case AiMarketDealType::Renewal:
        return QStringLiteral("재계약");
    case AiMarketDealType::Buyout:
        return QStringLiteral("바이아웃");
    case AiMarketDealType::LoanOption:
        return QStringLiteral("임대 선택 영입");
    case AiMarketDealType::LoanObligation:
        return QStringLiteral("임대 의무 영입");
    }
    return QStringLiteral("AI 거래");
}

QString aiMarketDealStageLabel(const AiMarketDealStage stage)
{
    switch (stage) {
    case AiMarketDealStage::Proposed:
        return QStringLiteral("협상 중");
    case AiMarketDealStage::AwaitingCompletion:
        return QStringLiteral("합의 · 완료 대기");
    case AiMarketDealStage::Completed:
        return QStringLiteral("완료");
    case AiMarketDealStage::Rejected:
        return QStringLiteral("무산");
    case AiMarketDealStage::Expired:
        return QStringLiteral("기한 만료");
    }
    return QStringLiteral("AI 거래");
}

QString aiMarketDealStageColor(const AiMarketDealStage stage)
{
    switch (stage) {
    case AiMarketDealStage::Proposed:
        return QStringLiteral("#e2bd6b");
    case AiMarketDealStage::AwaitingCompletion:
        return QStringLiteral("#63a7d8");
    case AiMarketDealStage::Completed:
        return QStringLiteral("#4db58b");
    case AiMarketDealStage::Rejected:
    case AiMarketDealStage::Expired:
        return QStringLiteral("#d46b65");
    }
    return QStringLiteral("#8c96a8");
}

QString loanTermsSummary(const LoanTerms& terms)
{
    QString summary = QStringLiteral("임대료 %1 · 차입 구단 주급 %2% · %3 · 종료 %4")
        .arg(toQString(formatMoney(terms.loanFeePence)))
        .arg(terms.borrowingClubWagePercentage)
        .arg(toQString(terms.promisedRole))
        .arg(toQString(terms.endDate.toIsoString()));
    if (terms.canRecall) {
        summary += QStringLiteral(" · %1일 리콜").arg(terms.recallNoticeDays);
    }
    if (terms.futureFeeType != "none") {
        summary += QStringLiteral(" · 완전 영입 %1 %2")
            .arg(
                terms.futureFeeType == "mandatory"
                    ? QStringLiteral("의무")
                    : QStringLiteral("선택"))
            .arg(toQString(formatMoney(terms.futureFeePence)));
    }
    return summary;
}

QString feeTermsSummary(const TransferFeeTerms& terms)
{
    QString summary = QStringLiteral("즉시 %1")
        .arg(toQString(formatMoney(terms.upfrontFeePence)));
    if (terms.installmentTotalPence > 0) {
        summary += QStringLiteral(" · %1회 할부 %2")
            .arg(terms.installmentCount)
            .arg(toQString(formatMoney(terms.installmentTotalPence)));
    }
    if (terms.appearanceBonusPence > 0) {
        summary += QStringLiteral(" · %1경기 시 %2")
            .arg(terms.appearanceThreshold)
            .arg(toQString(formatMoney(terms.appearanceBonusPence)));
    }
    if (terms.sellOnPercentage > 0) {
        summary += QStringLiteral(" · 다음 이적 수익 %1%")
            .arg(terms.sellOnPercentage);
    }
    return summary;
}

QString contractTermsSummary(const PlayerContractTerms& terms)
{
    return QStringLiteral("%1 · %2년 · 계약금 %3 · 로열티 %4 · 에이전트 %5 · %6")
        .arg(toQString(formatMoney(terms.weeklyWagePence)))
        .arg(terms.contractYears)
        .arg(toQString(formatMoney(terms.signingBonusPence)))
        .arg(toQString(formatMoney(terms.loyaltyBonusPence)))
        .arg(toQString(formatMoney(terms.agentFeePence)))
        .arg(toQString(terms.squadRole));
}

qint64 poundsValue(const Money pence)
{
    return static_cast<qint64>(pence / 100);
}

bool canConvertPounds(const qint64 pounds)
{
    return pounds >= 0
        && pounds <= std::numeric_limits<Money>::max() / 100;
}

Money toPence(const qint64 pounds)
{
    return static_cast<Money>(pounds) * 100;
}

QString paymentStatusLabel(const TransferPaymentStatus status)
{
    switch (status) {
    case TransferPaymentStatus::Scheduled:
        return QStringLiteral("예정");
    case TransferPaymentStatus::Settled:
        return QStringLiteral("정산 완료");
    case TransferPaymentStatus::Overdue:
        return QStringLiteral("연체");
    case TransferPaymentStatus::Cancelled:
        return QStringLiteral("취소");
    }
    return QStringLiteral("예정");
}

QString paymentStatusColor(const TransferPaymentStatus status)
{
    switch (status) {
    case TransferPaymentStatus::Scheduled:
        return QStringLiteral("#63a7d8");
    case TransferPaymentStatus::Settled:
        return QStringLiteral("#4db58b");
    case TransferPaymentStatus::Overdue:
        return QStringLiteral("#d46b65");
    case TransferPaymentStatus::Cancelled:
        return QStringLiteral("#8c96a8");
    }
    return QStringLiteral("#8c96a8");
}

QString paymentCategoryLabel(const std::string& category)
{
    if (category == "upfront_fee") {
        return QStringLiteral("즉시 이적료");
    }
    if (category == "installment") {
        return QStringLiteral("이적료 할부");
    }
    if (category == "signing_bonus") {
        return QStringLiteral("계약금");
    }
    if (category == "agent_fee") {
        return QStringLiteral("에이전트 수수료");
    }
    if (category == "loyalty_bonus") {
        return QStringLiteral("로열티 보너스");
    }
    if (category == "sell_on") {
        return QStringLiteral("다음 이적 수익");
    }
    return toQString(category);
}

QString scoutingDepthLabel(const ScoutingDepth depth)
{
    switch (depth) {
    case ScoutingDepth::Quick:
        return QStringLiteral("빠른 관찰 · 3일");
    case ScoutingDepth::Standard:
        return QStringLiteral("표준 관찰 · 7일");
    case ScoutingDepth::Comprehensive:
        return QStringLiteral("종합 관찰 · 14일");
    }
    return QStringLiteral("표준 관찰 · 7일");
}

QString scoutingFocusLabel(const std::string& focus)
{
    if (focus == "current_ability") {
        return QStringLiteral("현재 능력");
    }
    if (focus == "potential") {
        return QStringLiteral("잠재력");
    }
    if (focus == "availability") {
        return QStringLiteral("이적 가능성·조건");
    }
    return QStringLiteral("종합");
}

QString scoutingStatusLabel(const ScoutingAssignmentStatus status)
{
    switch (status) {
    case ScoutingAssignmentStatus::Active:
        return QStringLiteral("관찰 중");
    case ScoutingAssignmentStatus::Completed:
        return QStringLiteral("완료");
    case ScoutingAssignmentStatus::Cancelled:
        return QStringLiteral("중단");
    }
    return QStringLiteral("관찰 중");
}

QString scoutingStatusColor(const ScoutingAssignmentStatus status)
{
    switch (status) {
    case ScoutingAssignmentStatus::Active:
        return QStringLiteral("#63a7d8");
    case ScoutingAssignmentStatus::Completed:
        return QStringLiteral("#4db58b");
    case ScoutingAssignmentStatus::Cancelled:
        return QStringLiteral("#8c96a8");
    }
    return QStringLiteral("#8c96a8");
}

QString scoutingDirectiveTypeLabel(const ScoutingDirectiveType type)
{
    switch (type) {
    case ScoutingDirectiveType::Range:
        return QStringLiteral("범위형 지시");
    case ScoutingDirectiveType::Target:
        return QStringLiteral("특정 선수 지시");
    }
    return QStringLiteral("범위형 지시");
}

QString scoutingDirectiveStatusLabel(const ScoutingDirectiveStatus status)
{
    switch (status) {
    case ScoutingDirectiveStatus::Active:
        return QStringLiteral("진행 중");
    case ScoutingDirectiveStatus::Completed:
        return QStringLiteral("완료");
    case ScoutingDirectiveStatus::Cancelled:
        return QStringLiteral("취소");
    }
    return QStringLiteral("진행 중");
}

QString scoutingDirectiveStatusColor(const ScoutingDirectiveStatus status)
{
    switch (status) {
    case ScoutingDirectiveStatus::Active:
        return QStringLiteral("#63a7d8");
    case ScoutingDirectiveStatus::Completed:
        return QStringLiteral("#4db58b");
    case ScoutingDirectiveStatus::Cancelled:
        return QStringLiteral("#8c96a8");
    }
    return QStringLiteral("#8c96a8");
}

QString scoutFamiliarityCategoryLabel(const std::string& category)
{
    if (category == "country") {
        return QStringLiteral("국가");
    }
    if (category == "region") {
        return QStringLiteral("지역");
    }
    if (category == "position_group") {
        return QStringLiteral("포지션");
    }
    if (category == "age_band") {
        return QStringLiteral("연령대");
    }
    if (category == "market") {
        return QStringLiteral("시장");
    }
    if (category == "style") {
        return QStringLiteral("스타일");
    }
    return toQString(category);
}

QString scoutingPositionGroupLabel(const std::string& group)
{
    if (group == "GK") {
        return QStringLiteral("GK");
    }
    if (group == "DEF") {
        return QStringLiteral("수비");
    }
    if (group == "MID") {
        return QStringLiteral("미드필드");
    }
    if (group == "ATT") {
        return QStringLiteral("공격");
    }
    return toQString(group.empty() ? std::string {"전체"} : group);
}

QString scoutingDirectiveFilterSummary(const ScoutingDirective& directive)
{
    QStringList filters;
    if (!directive.region.empty() && directive.region != "any") {
        filters.push_back(QStringLiteral("지역 ") + toQString(directive.region));
    }
    if (!directive.country.empty() && directive.country != "any") {
        filters.push_back(QStringLiteral("국가 ") + toQString(directive.country));
    }
    if (!directive.league.empty() && directive.league != "any") {
        filters.push_back(QStringLiteral("리그 ") + toQString(directive.league));
    }
    if (!directive.position.empty() && directive.position != "any") {
        filters.push_back(
            QStringLiteral("포지션 ")
            + scoutingPositionGroupLabel(directive.position));
    }
    if (!directive.style.empty() && directive.style != "any") {
        filters.push_back(QStringLiteral("스타일 ") + toQString(directive.style));
    }
    if (directive.minAge > 0 || directive.maxAge > 0) {
        filters.push_back(
            QStringLiteral("나이 %1–%2")
                .arg(directive.minAge > 0
                    ? QString::number(directive.minAge)
                    : QStringLiteral("하한 없음"))
                .arg(directive.maxAge > 0
                    ? QString::number(directive.maxAge)
                    : QStringLiteral("상한 없음")));
    }
    if (directive.homegrownOnly) {
        filters.push_back(QStringLiteral("홈그로운 후보"));
    }
    if (directive.maxBudgetPence > 0) {
        filters.push_back(
            QStringLiteral("예산 ")
            + toQString(formatMoney(directive.maxBudgetPence)));
    }
    return filters.isEmpty()
        ? QStringLiteral("전체 후보 풀")
        : filters.join(QStringLiteral(" · "));
}

QString scoutTopFamiliaritySummary(
    const GameState& state,
    const std::string& scoutId)
{
    std::vector<ScoutFamiliarity> rows;
    for (const auto& familiarity : state.scoutFamiliarities) {
        if (familiarity.scoutId == scoutId) {
            rows.push_back(familiarity);
        }
    }
    std::sort(
        rows.begin(),
        rows.end(),
        [](const ScoutFamiliarity& left, const ScoutFamiliarity& right) {
            if (left.level != right.level) {
                return left.level > right.level;
            }
            return left.key < right.key;
        });

    QStringList labels;
    for (const auto& row : rows) {
        if (labels.size() >= 3) {
            break;
        }
        labels.push_back(
            scoutFamiliarityCategoryLabel(row.category)
            + QStringLiteral(" ")
            + toQString(row.key)
            + QStringLiteral(" ")
            + QString::number(row.level));
    }
    return labels.isEmpty()
        ? QStringLiteral("친숙도 기록 없음")
        : labels.join(QStringLiteral(" · "));
}

QString scoutingKnowledgeLabel(const int knowledge)
{
    if (knowledge >= 90) {
        return QStringLiteral("매우 높음");
    }
    if (knowledge >= 70) {
        return QStringLiteral("높음");
    }
    if (knowledge >= 45) {
        return QStringLiteral("보통");
    }
    if (knowledge >= 25) {
        return QStringLiteral("낮음");
    }
    return QStringLiteral("매우 낮음");
}

QString moneyRange(const Money low, const Money high)
{
    if (low == high) {
        return toQString(formatMoney(low));
    }
    return QStringLiteral("%1–%2")
        .arg(toQString(formatMoney(low)))
        .arg(toQString(formatMoney(high)));
}

QString recruitmentPlanStatusLabel(const RecruitmentPlanStatus status)
{
    switch (status) {
    case RecruitmentPlanStatus::Active:
        return QStringLiteral("진행 중");
    case RecruitmentPlanStatus::Achieved:
        return QStringLiteral("목표 달성");
    case RecruitmentPlanStatus::Acceptable:
        return QStringLiteral("최소 기대 충족");
    case RecruitmentPlanStatus::UnderDelivered:
        return QStringLiteral("부분 미달");
    case RecruitmentPlanStatus::Failed:
        return QStringLiteral("미달");
    }
    return QStringLiteral("진행 중");
}

QString recruitmentPlanStatusColor(const RecruitmentPlanStatus status)
{
    switch (status) {
    case RecruitmentPlanStatus::Active:
        return QStringLiteral("#63a7d8");
    case RecruitmentPlanStatus::Achieved:
        return QStringLiteral("#4db58b");
    case RecruitmentPlanStatus::Acceptable:
        return QStringLiteral("#8fc7ac");
    case RecruitmentPlanStatus::UnderDelivered:
        return QStringLiteral("#d2ab61");
    case RecruitmentPlanStatus::Failed:
        return QStringLiteral("#d46b65");
    }
    return QStringLiteral("#8c96a8");
}

QString displayPlayTime(const qint64 totalSeconds)
{
    const qint64 safeSeconds = std::max<qint64>(0, totalSeconds);
    const qint64 hours = safeSeconds / 3'600;
    const qint64 minutes = (safeSeconds % 3'600) / 60;
    if (hours > 0) {
        return QStringLiteral("%1시간 %2분").arg(hours).arg(minutes);
    }
    return QStringLiteral("%1분").arg(minutes);
}

QString displaySavedAt(const QString& isoDate)
{
    const QDateTime parsed = QDateTime::fromString(isoDate, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        return QStringLiteral("저장 시각 미상");
    }
    return parsed.toLocalTime().toString(QStringLiteral("yyyy.MM.dd HH:mm"));
}

QString displayIsoDate(const QString& isoDate)
{
    const auto parsed = GameDate::fromIsoString(toStdString(isoDate));
    return parsed.has_value() ? displayDate(*parsed) : QStringLiteral("날짜 미상");
}

} // namespace

GameController::GameController(QString dataDirectory, QObject* parent)
    : QObject(parent)
    , saveManager_(std::make_unique<SaveGameManager>(std::move(dataDirectory)))
{
}

bool GameController::initialize()
{
    QString initializeError;
    if (!saveManager_->initialize(&initializeError)) {
        setError(initializeError);
        ready_ = false;
        emit readyChanged();
        return false;
    }

    ready_ = true;
    errorMessage_.clear();
    statusMessage_ = saveManager_->legacyImportedThisRun()
        ? QStringLiteral("기존 0.2.0 세이브를 새 저장 목록으로 승계했습니다")
        : QStringLiteral("저장 목록을 불러왔습니다");
    emit readyChanged();
    emit errorMessageChanged();
    emit statusMessageChanged();
    emit savesChanged();
    emit careerChanged();
    emit stateChanged();
    return true;
}

bool GameController::ready() const noexcept
{
    return ready_;
}

bool GameController::careerLoaded() const noexcept
{
    return engine_ != nullptr && !activeCareerId_.isEmpty();
}

bool GameController::canContinue() const
{
    return saveManager_ != nullptr && saveManager_->canContinue();
}

QString GameController::errorMessage() const
{
    return errorMessage_;
}

QString GameController::statusMessage() const
{
    return statusMessage_;
}

QString GameController::currentSaveId() const
{
    return activeCareerId_;
}

QString GameController::playTime() const
{
    return displayPlayTime(currentPlayTimeSeconds());
}

QVariantList GameController::saveEntries() const
{
    QVariantList result;
    if (saveManager_ == nullptr) {
        return result;
    }

    std::vector<SaveGameDescriptor> entries = saveManager_->descriptors();
    std::sort(
        entries.begin(),
        entries.end(),
        [](const SaveGameDescriptor& left, const SaveGameDescriptor& right) {
            if (left.isAutosave() != right.isAutosave()) {
                return !left.isAutosave();
            }
            return left.savedAt > right.savedAt;
        });

    result.reserve(static_cast<qsizetype>(entries.size()));
    for (const auto& entry : entries) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), entry.id);
        row.insert(QStringLiteral("name"), entry.displayName);
        row.insert(QStringLiteral("clubName"), entry.clubName);
        row.insert(QStringLiteral("clubShortName"), entry.clubShortName);
        row.insert(QStringLiteral("gameDate"), displayIsoDate(entry.gameDate));
        row.insert(QStringLiteral("gameDateIso"), entry.gameDate);
        row.insert(QStringLiteral("season"), entry.seasonLabel);
        row.insert(QStringLiteral("savedAt"), displaySavedAt(entry.savedAt));
        row.insert(QStringLiteral("playTime"), displayPlayTime(entry.playTimeSeconds));
        row.insert(QStringLiteral("autosave"), entry.isAutosave());
        row.insert(
            QStringLiteral("typeLabel"),
            entry.isAutosave()
                ? QStringLiteral("자동 저장 %1").arg(entry.autosaveSlot)
                : QStringLiteral("커리어"));
        row.insert(QStringLiteral("parentId"), entry.parentId);
        result.push_back(row);
    }
    return result;
}

bool GameController::createNewCareer(const QString& saveNameValue, const QString& ownerNameValue)
{
    QString createdId;
    QString createError;
    auto created = saveManager_->createCareer(
        saveNameValue,
        ownerNameValue,
        &createdId,
        &createError);
    if (!created.has_value()) {
        setError(createError);
        return false;
    }

    emit savesChanged();
    return activateCareer(
        std::move(created),
        createdId,
        0,
        QStringLiteral("새 커리어를 시작하고 저장했습니다"));
}

bool GameController::continueCareer()
{
    const QString saveId = saveManager_->continueSaveId();
    if (saveId.isEmpty()) {
        setError(QStringLiteral("이어할 커리어가 없습니다."));
        return false;
    }
    return loadSave(saveId);
}

bool GameController::loadSave(const QString& saveId)
{
    QString activeId;
    qint64 loadedPlayTime = 0;
    bool restoredFromAutosave = false;
    QString loadError;
    auto loaded = saveManager_->loadCareer(
        saveId,
        &activeId,
        &loadedPlayTime,
        &restoredFromAutosave,
        &loadError);
    if (!loaded.has_value()) {
        setError(loadError);
        return false;
    }

    emit savesChanged();
    return activateCareer(
        std::move(loaded),
        activeId,
        loadedPlayTime,
        restoredFromAutosave
            ? QStringLiteral("자동 저장 복원본을 불러왔습니다. 다음 저장은 원본 커리어에 반영됩니다")
            : QStringLiteral("세이브를 불러왔습니다"));
}

bool GameController::saveNameExists(const QString& saveNameValue) const
{
    return saveManager_ != nullptr
        && saveManager_->nameExists(saveNameValue, activeCareerId_);
}

bool GameController::saveGame()
{
    if (engine_ == nullptr) {
        setError(QStringLiteral("저장할 커리어가 없습니다."));
        return false;
    }
    return persist(QStringLiteral("수동 저장을 완료했습니다"));
}

bool GameController::saveGameAs(const QString& saveNameValue, const bool overwrite)
{
    if (engine_ == nullptr) {
        setError(QStringLiteral("저장할 커리어가 없습니다."));
        return false;
    }

    const QString duplicateId = saveManager_->saveIdForName(saveNameValue);
    if (!overwrite && !duplicateId.isEmpty() && duplicateId != activeCareerId_) {
        setError(QStringLiteral("같은 이름의 세이브가 이미 있습니다. 덮어쓰기를 확인하십시오."));
        return false;
    }

    const qint64 totalPlayTime = currentPlayTimeSeconds();
    QString resultingId;
    QString saveError;
    const QString overwriteTarget = duplicateId == activeCareerId_
        ? activeCareerId_
        : (overwrite ? duplicateId : QString {});
    if (!saveManager_->saveAs(
            activeCareerId_,
            engine_->state(),
            saveNameValue,
            overwriteTarget,
            totalPlayTime,
            &resultingId,
            &saveError)) {
        setError(saveError);
        return false;
    }

    activeCareerId_ = resultingId;
    engine_->mutableState().saveName = toStdString(saveNameValue.trimmed());
    committedPlayTimeSeconds_ = totalPlayTime;
    playTimer_.restart();
    if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
        emit errorMessageChanged();
    }
    setStatus(QStringLiteral("‘%1’ 이름으로 저장했습니다").arg(saveNameValue.trimmed()));
    emit savesChanged();
    emit careerChanged();
    emit stateChanged();
    return true;
}

bool GameController::renameSave(const QString& saveId, const QString& saveNameValue)
{
    QString renameError;
    if (!saveManager_->renameCareer(saveId, saveNameValue, &renameError)) {
        setError(renameError);
        return false;
    }

    if (saveId == activeCareerId_ && engine_ != nullptr) {
        engine_->mutableState().saveName = toStdString(saveNameValue.trimmed());
        emit stateChanged();
    }
    clearError();
    setStatus(QStringLiteral("세이브 이름을 변경했습니다"));
    emit savesChanged();
    return true;
}

bool GameController::deleteSave(const QString& saveId)
{
    if (saveId == activeCareerId_) {
        setError(QStringLiteral("현재 플레이 중인 세이브는 메인 메뉴로 나온 뒤 삭제하십시오."));
        return false;
    }

    QString deleteError;
    if (!saveManager_->deleteCareer(saveId, &deleteError)) {
        setError(deleteError);
        return false;
    }
    clearError();
    setStatus(QStringLiteral("선택한 세이브를 삭제했습니다"));
    emit savesChanged();
    return true;
}

bool GameController::returnToMainMenu()
{
    if (engine_ != nullptr && !persist(QStringLiteral("메인 메뉴로 돌아가기 전에 저장했습니다"))) {
        return false;
    }

    engine_.reset();
    activeCareerId_.clear();
    committedPlayTimeSeconds_ = 0;
    playTimer_.invalidate();
    emit careerChanged();
    emit savesChanged();
    emit stateChanged();
    return true;
}

void GameController::exitGame()
{
    if (engine_ != nullptr && !persist(QStringLiteral("종료 전에 저장했습니다"))) {
        return;
    }
    emit quitRequested();
}

void GameController::clearError()
{
    if (errorMessage_.isEmpty()) {
        return;
    }
    errorMessage_.clear();
    emit errorMessageChanged();
}

QString GameController::currentDate() const
{
    return state() != nullptr ? displayDate(state()->currentDate) : QStringLiteral("커리어 미선택");
}

QString GameController::saveName() const
{
    return state() != nullptr ? toQString(state()->saveName) : QString {};
}

QString GameController::clubName() const
{
    return state() != nullptr ? toQString(state()->club.name) : QString {};
}

QString GameController::clubShortName() const
{
    return state() != nullptr ? toQString(state()->club.shortName) : QString {};
}

QString GameController::leagueName() const
{
    return state() != nullptr ? toQString(state()->club.leagueName) : QString {};
}

QString GameController::ownerName() const
{
    return state() != nullptr ? toQString(state()->ownerName) : QString {};
}

int GameController::ownerReputationScore() const
{
    return state() != nullptr ? state()->ownerReputation.overallScore : 0;
}

QString GameController::ownerReputationLabel() const
{
    return state() != nullptr
        ? toQString(state()->ownerReputation.headline)
        : QString {};
}

int GameController::fanTrustScore() const
{
    return state() != nullptr ? state()->fanTendency.trustScore : 0;
}

QString GameController::fanTrustLabel() const
{
    return state() != nullptr
        ? toQString(football_owner::fanTrustLabel(state()->fanTendency.trustScore))
        : QString {};
}

QString GameController::fanMoodLabel() const
{
    return state() != nullptr
        ? fanMoodDisplayLabel(state()->fanTendency.mood)
        : QString {};
}

QString GameController::dominantFanTendency() const
{
    return state() != nullptr
        ? dominantFanTendencyLabel(state()->fanTendency)
        : QString {};
}

QString GameController::managerName() const
{
    return state() != nullptr ? toQString(state()->club.manager.name) : QString {};
}

int GameController::managerConfidence() const
{
    return state() != nullptr ? state()->club.manager.boardConfidence : 0;
}

QString GameController::managerAssessment() const
{
    return state() != nullptr ? toQString(state()->club.manager.currentAssessment) : QString {};
}

QString GameController::managerIdentity() const
{
    return state() != nullptr ? toQString(state()->club.manager.tacticalIdentity) : QString {};
}

QString GameController::operatingMeetingCadence() const
{
    return state() != nullptr
        ? toQString(operatingMeetingCadenceKey(
            state()->operatingMeetingSettings.cadence))
        : QStringLiteral("off");
}

QString GameController::operatingMeetingCadenceLabel() const
{
    return state() != nullptr
        ? operatingMeetingCadenceDisplay(
            state()->operatingMeetingSettings.cadence)
        : QStringLiteral("개최 안 함");
}

QString GameController::nextOperatingMeetingDate() const
{
    if (state() == nullptr
        || !state()->operatingMeetingSettings.nextMeetingDate.has_value()) {
        return QStringLiteral("예약 없음");
    }
    return shortDate(*state()->operatingMeetingSettings.nextMeetingDate);
}

bool GameController::operatingMeetingAutoAssign() const
{
    return state() != nullptr
        && state()->operatingMeetingSettings.attendeeMode
            == OperatingMeetingAttendeeMode::Auto;
}

bool GameController::operatingMeetingIncludeManager() const
{
    return state() != nullptr
        && state()->operatingMeetingSettings.includeManager;
}

bool GameController::operatingMeetingIncludeDirector() const
{
    return state() != nullptr
        && state()->operatingMeetingSettings.includeDirector;
}

bool GameController::operatingMeetingIncludeFinance() const
{
    return state() != nullptr
        && state()->operatingMeetingSettings.includeFinance;
}

bool GameController::operatingMeetingIncludeScouting() const
{
    return state() != nullptr
        && state()->operatingMeetingSettings.includeScouting;
}

bool GameController::operatingMeetingIncludeMedical() const
{
    return state() != nullptr
        && state()->operatingMeetingSettings.includeMedical;
}

QString GameController::cash() const
{
    return state() != nullptr ? toQString(formatMoney(state()->club.finance.cashPence)) : QStringLiteral("—");
}

QString GameController::availableTransferCash() const
{
    return state() != nullptr
        ? toQString(formatMoney(football_owner::availableTransferCash(*state())))
        : QStringLiteral("—");
}

QString GameController::transferBudget() const
{
    return state() != nullptr ? toQString(formatMoney(state()->club.finance.transferBudgetPence)) : QStringLiteral("—");
}

QString GameController::weeklyWageBudget() const
{
    return state() != nullptr ? toQString(formatMoney(state()->club.finance.weeklyWageBudgetPence)) : QStringLiteral("—");
}

QString GameController::weeklyWageCommitment() const
{
    return state() != nullptr ? toQString(formatMoney(state()->club.finance.weeklyWageCommitmentPence)) : QStringLiteral("—");
}

int GameController::wageUsagePercent() const
{
    if (state() == nullptr || state()->club.finance.weeklyWageBudgetPence <= 0) {
        return 0;
    }
    const double ratio = static_cast<double>(state()->club.finance.weeklyWageCommitmentPence)
        / static_cast<double>(state()->club.finance.weeklyWageBudgetPence);
    return static_cast<int>(std::lround(ratio * 100.0));
}

QString GameController::debt() const
{
    return state() != nullptr ? toQString(formatMoney(state()->club.finance.debtPence)) : QStringLiteral("—");
}

int GameController::unreadCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(state()->inbox.begin(), state()->inbox.end(), [](const InboxItem& item) {
        return item.unread;
    }));
}

int GameController::decisionCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    const int pursuitDecisions = static_cast<int>(std::count_if(
        state()->transferPursuits.begin(),
        state()->transferPursuits.end(),
        [](const TransferPursuit& pursuit) {
            return pursuit.stage == TransferPursuitStage::ClubCountered
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerTerms
                || pursuit.stage == TransferPursuitStage::PlayerCountered;
        }));
    const int outgoingDecisions = static_cast<int>(std::count_if(
        state()->outgoingTransferOffers.begin(),
        state()->outgoingTransferOffers.end(),
        [](const OutgoingTransferOffer& offer) {
            return offer.status == OutgoingTransferStatus::Pending
                && offer.negotiationStage
                    != OutgoingNegotiationStage::AwaitingBuyerResponse;
        }));
    return pendingTransferCount() + pursuitDecisions + outgoingDecisions;
}

int GameController::pendingTransferCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->transferProposals.begin(),
        state()->transferProposals.end(),
        [](const TransferProposal& proposal) {
            return proposal.status == TransferStatus::Pending;
        }));
}

int GameController::activeNegotiationCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->transferPursuits.begin(),
        state()->transferPursuits.end(),
        [](const TransferPursuit& pursuit) {
            return pursuit.stage == TransferPursuitStage::AwaitingClubResponse
                || pursuit.stage == TransferPursuitStage::ClubCountered
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerTerms
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerResponse
                || pursuit.stage == TransferPursuitStage::PlayerCountered
                || pursuit.stage == TransferPursuitStage::AwaitingMedical
                || pursuit.stage == TransferPursuitStage::AwaitingRegistration
                || pursuit.stage == TransferPursuitStage::AwaitingOwnerApproval
                || pursuit.stage == TransferPursuitStage::AwaitingCompletion;
        }));
}

int GameController::pendingOutgoingOfferCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->outgoingTransferOffers.begin(),
        state()->outgoingTransferOffers.end(),
        [](const OutgoingTransferOffer& offer) {
            return offer.status == OutgoingTransferStatus::Pending;
        }));
}

int GameController::activeScoutingAssignmentCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->scoutingAssignments.begin(),
        state()->scoutingAssignments.end(),
        [](const ScoutingAssignment& assignment) {
            return assignment.status == ScoutingAssignmentStatus::Active;
        }));
}

int GameController::scoutingCapacity() const
{
    if (state() == nullptr) {
        return 0;
    }
    int capacity = 0;
    for (const auto& scout : state()->scouts) {
        capacity += scout.assignmentCapacity;
    }
    return capacity;
}

int GameController::completedScoutingReportCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->scoutingReports.begin(),
        state()->scoutingReports.end(),
        [](const ScoutingReport& report) {
            return report.knowledgeLevel >= 70;
        }));
}

int GameController::pendingLoanDecisionCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->loanDeals.begin(),
        state()->loanDeals.end(),
        [](const LoanDeal& loan) {
            return loan.stage == LoanStage::DecisionPending
                || loan.stage == LoanStage::ClubCountered;
        }));
}

int GameController::activeLoanCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->loanDeals.begin(),
        state()->loanDeals.end(),
        [](const LoanDeal& loan) {
            return loan.stage == LoanStage::Active
                || loan.stage == LoanStage::RecallPending;
        }));
}

int GameController::activeRenewalCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->contractRenewals.begin(),
        state()->contractRenewals.end(),
        [](const ContractRenewalCase& renewal) {
            return renewal.stage == ContractRenewalStage::AwaitingPlayerResponse
                || renewal.stage == ContractRenewalStage::PlayerCountered
                || renewal.stage == ContractRenewalStage::AgreementReached;
        }));
}

int GameController::expiringContractCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        state()->playerContracts.begin(),
        state()->playerContracts.end(),
        [this](const PlayerContract& contract) {
            return contract.lifecycleStatus != "expired_free_agent"
                && state()->currentDate.daysUntil(contract.endDate) <= 365;
        }));
}

int GameController::firstTeamCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(state()->players.begin(), state()->players.end(), [](const Player& player) {
        return player.squadGroup == "First Team";
    }));
}

int GameController::reserveCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(state()->players.begin(), state()->players.end(), [](const Player& player) {
        return player.squadGroup == "Reserve";
    }));
}

int GameController::youthCount() const
{
    if (state() == nullptr) {
        return 0;
    }
    return static_cast<int>(std::count_if(state()->players.begin(), state()->players.end(), [](const Player& player) {
        return player.squadGroup == "Youth";
    }));
}

QString GameController::dataNotice() const
{
    return state() != nullptr ? toQString(state()->dataNotice) : QString {};
}

QString GameController::nextFixtureTitle() const
{
    if (state() == nullptr) {
        return QStringLiteral("일정 없음");
    }
    const auto fixture = std::find_if(state()->fixtures.begin(), state()->fixtures.end(), [](const Fixture& item) {
        return !item.played;
    });
    if (fixture == state()->fixtures.end()) {
        return QStringLiteral("추가 일정 데이터 필요");
    }
    const bool home = fixture->homeClub == state()->club.name;
    return QStringLiteral("%1  %2")
        .arg(home ? QStringLiteral("홈 vs") : QStringLiteral("원정 @"))
        .arg(toQString(home ? fixture->awayClub : fixture->homeClub));
}

QString GameController::nextFixtureDate() const
{
    if (state() == nullptr) {
        return QString {};
    }
    const auto fixture = std::find_if(state()->fixtures.begin(), state()->fixtures.end(), [](const Fixture& item) {
        return !item.played;
    });
    return fixture != state()->fixtures.end() ? displayDate(fixture->date) : QStringLiteral("미정");
}

QString GameController::nextFixtureStatus() const
{
    if (state() == nullptr) {
        return QString {};
    }
    const auto fixture = std::find_if(state()->fixtures.begin(), state()->fixtures.end(), [](const Fixture& item) {
        return !item.played;
    });
    if (fixture == state()->fixtures.end()) {
        return QStringLiteral("첫 일정 이후의 데이터는 다음 구현 단위에서 추가됩니다.");
    }
    const int days = state()->currentDate.daysUntil(fixture->date);
    return days >= 0 ? QStringLiteral("%1일 후 · %2").arg(days).arg(toQString(fixture->competition))
                     : QStringLiteral("일정 처리 대기");
}

QString GameController::recentResult() const
{
    if (state() == nullptr) {
        return QStringLiteral("아직 치른 경기가 없습니다");
    }
    const auto fixture = std::find_if(state()->fixtures.rbegin(), state()->fixtures.rend(), [](const Fixture& item) {
        return item.played;
    });
    return fixture != state()->fixtures.rend() ? fixtureScore(*fixture) : QStringLiteral("아직 치른 경기가 없습니다");
}

QVariantList GameController::inbox() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->inbox.size()));
    for (auto item = state()->inbox.rbegin(); item != state()->inbox.rend(); ++item) {
        const auto linkedProposal = std::find_if(
            state()->transferProposals.begin(),
            state()->transferProposals.end(),
            [&item](const TransferProposal& proposal) {
                return item->eventKey == "transfer-proposal-" + proposal.id;
            });
        const bool hasTransferProposal = linkedProposal != state()->transferProposals.end();

        const auto linkedPursuit = std::find_if(
            state()->transferPursuits.begin(),
            state()->transferPursuits.end(),
            [&item](const TransferPursuit& pursuit) {
                return item->eventKey == "transfer-pursuit-counter-" + pursuit.id
                    || item->eventKey == "transfer-pursuit-club-accepted-" + pursuit.id
                    || item->eventKey == "transfer-pursuit-counter-accepted-" + pursuit.id
                    || item->eventKey == "transfer-player-counter-" + pursuit.id;
            });
        const bool hasTransferPursuit = linkedPursuit != state()->transferPursuits.end();

        const auto linkedOutgoingOffer = std::find_if(
            state()->outgoingTransferOffers.begin(),
            state()->outgoingTransferOffers.end(),
            [&item](const OutgoingTransferOffer& offer) {
                return item->eventKey == "outgoing-offer-" + offer.id
                    || item->eventKey == "outgoing-buyer-counter-" + offer.id
                    || item->eventKey == "outgoing-counter-agreed-" + offer.id
                    || item->eventKey == "outgoing-buyer-counter-accepted-" + offer.id;
            });
        const bool hasOutgoingOffer = linkedOutgoingOffer != state()->outgoingTransferOffers.end();

        const auto linkedLoan = std::find_if(
            state()->loanDeals.begin(),
            state()->loanDeals.end(),
            [&item](const LoanDeal& loan) {
                return item->eventKey == "loan-offer-" + loan.id
                    || item->eventKey == "loan-counter-" + loan.id;
            });
        const bool hasLoan = linkedLoan != state()->loanDeals.end();

        const auto linkedRenewal = std::find_if(
            state()->contractRenewals.begin(),
            state()->contractRenewals.end(),
            [&item](const ContractRenewalCase& renewal) {
                return item->eventKey
                        == "contract-renewal-counter-" + renewal.id
                    || item->eventKey
                        == "contract-renewal-agreed-" + renewal.id;
            });
        const bool hasRenewal =
            linkedRenewal != state()->contractRenewals.end();

        bool decisionPending = item->requiresDecision;
        QString decisionType;
        QString decisionStatus;
        QString decisionStatusColor = QStringLiteral("#8c96a8");
        if (hasTransferProposal) {
            decisionPending = linkedProposal->status == TransferStatus::Pending;
            decisionType = QStringLiteral("transferApproval");
            decisionStatus = transferStatusLabel(linkedProposal->status);
            decisionStatusColor = transferStatusColor(linkedProposal->status);
        } else if (hasTransferPursuit) {
            decisionPending = linkedPursuit->stage == TransferPursuitStage::ClubCountered
                || linkedPursuit->stage == TransferPursuitStage::AwaitingPlayerTerms
                || linkedPursuit->stage == TransferPursuitStage::PlayerCountered;
            if (linkedPursuit->stage == TransferPursuitStage::ClubCountered) {
                decisionType = QStringLiteral("clubCounter");
            } else if (linkedPursuit->stage == TransferPursuitStage::PlayerCountered) {
                decisionType = QStringLiteral("playerCounter");
            } else {
                decisionType = QStringLiteral("transferDetails");
            }
            decisionStatus = transferPursuitStageLabel(linkedPursuit->stage);
            decisionStatusColor = transferPursuitStageColor(linkedPursuit->stage);
        } else if (hasOutgoingOffer) {
            decisionPending = linkedOutgoingOffer->status == OutgoingTransferStatus::Pending
                && linkedOutgoingOffer->negotiationStage
                    != OutgoingNegotiationStage::AwaitingBuyerResponse;
            decisionType =
                linkedOutgoingOffer->negotiationStage
                    == OutgoingNegotiationStage::BuyerCountered
                ? QStringLiteral("outgoingCounter")
                : QStringLiteral("outgoingOffer");
            decisionStatus = outgoingNegotiationStageLabel(
                linkedOutgoingOffer->negotiationStage);
            decisionStatusColor = outgoingTransferStatusColor(linkedOutgoingOffer->status);
        } else if (hasLoan) {
            decisionPending =
                linkedLoan->stage == LoanStage::DecisionPending
                || linkedLoan->stage == LoanStage::ClubCountered;
            decisionType =
                linkedLoan->stage == LoanStage::ClubCountered
                ? QStringLiteral("loanCounter")
                : QStringLiteral("loanOffer");
            decisionStatus = loanStageLabel(linkedLoan->stage);
            decisionStatusColor = loanStageColor(linkedLoan->stage);
        } else if (hasRenewal) {
            decisionPending =
                linkedRenewal->stage
                    == ContractRenewalStage::PlayerCountered
                || linkedRenewal->stage
                    == ContractRenewalStage::AgreementReached;
            decisionType =
                linkedRenewal->stage
                    == ContractRenewalStage::AgreementReached
                ? QStringLiteral("renewalFinal")
                : QStringLiteral("renewalCounter");
            decisionStatus =
                contractRenewalStageLabel(linkedRenewal->stage);
            decisionStatusColor =
                contractRenewalStageColor(linkedRenewal->stage);
        }

        QVariantMap row;
        row.insert(QStringLiteral("id"), item->id);
        row.insert(QStringLiteral("key"), toQString(item->eventKey));
        row.insert(QStringLiteral("date"), shortDate(item->date));
        row.insert(QStringLiteral("category"), categoryLabel(item->category));
        row.insert(QStringLiteral("categoryColor"), categoryColor(item->category));
        row.insert(QStringLiteral("title"), toQString(item->title));
        row.insert(QStringLiteral("body"), toQString(item->body));
        row.insert(QStringLiteral("unread"), item->unread);
        row.insert(QStringLiteral("requiresDecision"), decisionPending);
        row.insert(
            QStringLiteral("transferProposalId"),
            hasTransferProposal ? toQString(linkedProposal->id) : QString {});
        row.insert(
            QStringLiteral("transferPursuitId"),
            hasTransferPursuit ? toQString(linkedPursuit->id) : QString {});
        row.insert(
            QStringLiteral("outgoingOfferId"),
            hasOutgoingOffer ? toQString(linkedOutgoingOffer->id) : QString {});
        row.insert(
            QStringLiteral("loanId"),
            hasLoan ? toQString(linkedLoan->id) : QString {});
        row.insert(
            QStringLiteral("renewalId"),
            hasRenewal ? toQString(linkedRenewal->id) : QString {});
        row.insert(QStringLiteral("decisionType"), decisionType);
        row.insert(QStringLiteral("decisionStatus"), decisionStatus);
        row.insert(QStringLiteral("decisionStatusColor"), decisionStatusColor);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::squad() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->players.size()));
    for (const auto& player : state()->players) {
        const auto age = ageOnDate(player, state()->currentDate);
        const auto* ownerOpinion = activePlayerSquadOpinion(
            *state(),
            player.id,
            SquadPlanningOpinionSource::Owner);
        const auto* managerOpinion = activePlayerSquadOpinion(
            *state(),
            player.id,
            SquadPlanningOpinionSource::Manager);
        const auto* directorOpinion = activePlayerSquadOpinion(
            *state(),
            player.id,
            SquadPlanningOpinionSource::Director);
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(player.id));
        row.insert(QStringLiteral("name"), toQString(player.fullName));
        row.insert(QStringLiteral("position"), toQString(player.position));
        row.insert(QStringLiteral("age"), age.has_value() ? QString::number(*age) : QStringLiteral("미상"));
        row.insert(QStringLiteral("nationality"), toQString(player.nationality));
        row.insert(QStringLiteral("squadGroup"), toQString(player.squadGroup));
        row.insert(QStringLiteral("squadGroupLabel"), squadGroupLabel(player.squadGroup));
        row.insert(QStringLiteral("status"), playerStatusLabel(player.status));
        row.insert(QStringLiteral("confidence"), toQString(player.dataConfidence));
        row.insert(QStringLiteral("confidenceLabel"), confidenceLabel(player.dataConfidence));
        row.insert(
            QStringLiteral("ownerOpinionType"),
            ownerOpinion != nullptr
                ? toQString(squadPlanningOpinionTypeKey(ownerOpinion->type))
                : QString {});
        row.insert(
            QStringLiteral("ownerOpinionLabel"),
            squadPlanningOpinionDisplay(ownerOpinion));
        row.insert(
            QStringLiteral("ownerOpinionColor"),
            ownerOpinion != nullptr
                ? squadPlanningOpinionTypeColor(ownerOpinion->type)
                : QStringLiteral("#5d6977"));
        row.insert(
            QStringLiteral("ownerOpinionReason"),
            ownerOpinion != nullptr ? toQString(ownerOpinion->reason) : QString {});
        row.insert(
            QStringLiteral("managerOpinionLabel"),
            squadPlanningOpinionDisplay(managerOpinion));
        row.insert(
            QStringLiteral("managerOpinionColor"),
            managerOpinion != nullptr
                ? squadPlanningOpinionTypeColor(managerOpinion->type)
                : QStringLiteral("#5d6977"));
        row.insert(
            QStringLiteral("directorOpinionLabel"),
            squadPlanningOpinionDisplay(directorOpinion));
        row.insert(
            QStringLiteral("directorOpinionColor"),
            directorOpinion != nullptr
                ? squadPlanningOpinionTypeColor(directorOpinion->type)
                : QStringLiteral("#5d6977"));
        row.insert(
            QStringLiteral("joinedDate"),
            player.joinedDate.has_value() ? shortDate(*player.joinedDate) : QStringLiteral("기록 보강 예정"));
        row.insert(
            QStringLiteral("departedDate"),
            player.departedDate.has_value() ? shortDate(*player.departedDate) : QString {});
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::squadDiagnostics() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    std::vector<SquadDiagnosticBucket> buckets {
        {QStringLiteral("GK"), QStringLiteral("골키퍼"), QStringLiteral("#63a7d8"), 0},
        {QStringLiteral("DEF"), QStringLiteral("수비"), QStringLiteral("#6fc29f"), 1},
        {QStringLiteral("MID"), QStringLiteral("미드필드"), QStringLiteral("#d0b86b"), 2},
        {QStringLiteral("ATT"), QStringLiteral("공격"), QStringLiteral("#d78376"), 3},
        {QStringLiteral("OTHER"), QStringLiteral("기타"), QStringLiteral("#8c96a8"), 4}
    };

    const auto findBucket = [&buckets](const QString& key) -> SquadDiagnosticBucket& {
        const auto bucket = std::find_if(
            buckets.begin(),
            buckets.end(),
            [&key](const SquadDiagnosticBucket& item) {
                return item.key == key;
            });
        return bucket != buckets.end() ? *bucket : buckets.back();
    };

    for (const auto& player : state()->players) {
        auto& bucket = findBucket(squadDiagnosticGroupKey(player.position));
        ++bucket.total;

        if (player.squadGroup == "First Team") {
            ++bucket.firstTeam;
        } else if (player.squadGroup == "Reserve") {
            ++bucket.reserve;
        } else if (player.squadGroup == "Youth") {
            ++bucket.youth;
        }

        if (player.squadGroup == "Loan" || player.status == "On Loan") {
            ++bucket.outgoingLoan;
        }
        if (player.status == "Loaned In") {
            ++bucket.incomingLoan;
        }

        const auto age = ageOnDate(player, state()->currentDate);
        if (age.has_value()) {
            if (*age <= 21) {
                ++bucket.under21;
            }
            if (*age >= 30) {
                ++bucket.over30;
            }
        }

        const auto contract = std::find_if(
            state()->playerContracts.begin(),
            state()->playerContracts.end(),
            [&player](const PlayerContract& item) {
                return item.playerId == player.id
                    && item.lifecycleStatus != "expired_free_agent";
            });
        if (contract == state()->playerContracts.end()) {
            continue;
        }

        bucket.weeklyWagePence += contract->weeklyWagePence;
        const int daysUntilExpiry =
            state()->currentDate.daysUntil(contract->endDate);
        if (daysUntilExpiry <= 365) {
            ++bucket.expiringWithinYear;
        }

        const bool highWage =
            state()->club.finance.weeklyWageBudgetPence > 0
            && contract->weeklyWagePence
                >= state()->club.finance.weeklyWageBudgetPence / 10;
        if (age.has_value()
            && *age >= 30
            && (daysUntilExpiry <= 730 || highWage)) {
            ++bucket.saleWatch;
        }
        if (daysUntilExpiry <= 180 && player.squadGroup != "First Team") {
            ++bucket.releaseWatch;
        }
    }

    for (const auto& opinion : state()->squadPlanningOpinions) {
        if (opinion.status != SquadPlanningOpinionStatus::Active) {
            continue;
        }

        QString groupKey;
        if (opinion.positionGroup.has_value()
            && !opinion.positionGroup->empty()) {
            groupKey = toQString(*opinion.positionGroup);
        } else if (opinion.playerId.has_value()) {
            const auto player = std::find_if(
                state()->players.begin(),
                state()->players.end(),
                [&opinion](const Player& item) {
                    return item.id == *opinion.playerId;
                });
            if (player == state()->players.end()) {
                continue;
            }
            groupKey = squadDiagnosticGroupKey(player->position);
        } else {
            continue;
        }

        auto& bucket = findBucket(groupKey);
        switch (opinion.source) {
        case SquadPlanningOpinionSource::Owner:
            ++bucket.ownerOpinionCount;
            break;
        case SquadPlanningOpinionSource::Manager:
            ++bucket.managerOpinionCount;
            break;
        case SquadPlanningOpinionSource::Director:
            ++bucket.directorOpinionCount;
            break;
        }
    }

    std::sort(
        buckets.begin(),
        buckets.end(),
        [](const SquadDiagnosticBucket& left, const SquadDiagnosticBucket& right) {
            return left.sortOrder < right.sortOrder;
        });

    result.reserve(static_cast<qsizetype>(buckets.size()));
    for (const auto& bucket : buckets) {
        if (bucket.total == 0 && bucket.key == QStringLiteral("OTHER")) {
            continue;
        }

        const QString needLabel = squadDiagnosticNeedLabel(bucket);
        QString managerSignal = QStringLiteral("특이 요청 없음");
        if (needLabel == QStringLiteral("즉시 보강")) {
            managerSignal = QStringLiteral("보강 요청 가능");
        } else if (needLabel == QStringLiteral("세대교체 관찰")) {
            managerSignal = QStringLiteral("경험·기동력 균형 확인");
        } else if (bucket.outgoingLoan > 0) {
            managerSignal = QStringLiteral("임대 복귀 전력 검토");
        }

        QString directorSignal = QStringLiteral("관찰 유지");
        if (bucket.expiringWithinYear > 0) {
            directorSignal = QStringLiteral("계약 만료 정리 필요");
        } else if (bucket.saleWatch > 0) {
            directorSignal = QStringLiteral("고주급·노장 매각 검토");
        } else if (bucket.releaseWatch > 0) {
            directorSignal = QStringLiteral("방출 후보 검토");
        }

        QVariantMap row;
        row.insert(QStringLiteral("key"), bucket.key);
        row.insert(QStringLiteral("label"), bucket.label);
        row.insert(QStringLiteral("color"), bucket.color);
        row.insert(QStringLiteral("total"), bucket.total);
        row.insert(QStringLiteral("firstTeam"), bucket.firstTeam);
        row.insert(QStringLiteral("reserve"), bucket.reserve);
        row.insert(QStringLiteral("youth"), bucket.youth);
        row.insert(QStringLiteral("outgoingLoan"), bucket.outgoingLoan);
        row.insert(QStringLiteral("incomingLoan"), bucket.incomingLoan);
        row.insert(QStringLiteral("under21"), bucket.under21);
        row.insert(QStringLiteral("over30"), bucket.over30);
        row.insert(
            QStringLiteral("expiringWithinYear"),
            bucket.expiringWithinYear);
        row.insert(QStringLiteral("saleWatch"), bucket.saleWatch);
        row.insert(QStringLiteral("releaseWatch"), bucket.releaseWatch);
        row.insert(
            QStringLiteral("weeklyWage"),
            toQString(formatMoney(bucket.weeklyWagePence)));
        row.insert(
            QStringLiteral("weeklyWagePounds"),
            poundsValue(bucket.weeklyWagePence));
        row.insert(QStringLiteral("ownerOpinionCount"), bucket.ownerOpinionCount);
        row.insert(
            QStringLiteral("managerOpinionCount"),
            bucket.managerOpinionCount);
        row.insert(
            QStringLiteral("directorOpinionCount"),
            bucket.directorOpinionCount);
        row.insert(QStringLiteral("needLabel"), needLabel);
        row.insert(
            QStringLiteral("needColor"),
            squadDiagnosticNeedColor(needLabel));
        row.insert(
            QStringLiteral("summary"),
            squadDiagnosticSummary(bucket));
        row.insert(QStringLiteral("managerSignal"), managerSignal);
        row.insert(QStringLiteral("directorSignal"), directorSignal);
        row.insert(
            QStringLiteral("ownerSignal"),
            QStringLiteral("구단주 수동 의견은 다음 단계에서 설정"));
        row.insert(QStringLiteral("source"), QStringLiteral("자동 진단"));
        result.push_back(row);
    }

    return result;
}

QVariantList GameController::squadPlanningOpinions() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(
        static_cast<qsizetype>(state()->squadPlanningOpinions.size()));
    for (const auto& opinion : state()->squadPlanningOpinions) {
        const Player* player = nullptr;
        if (opinion.playerId.has_value()) {
            const auto found = std::find_if(
                state()->players.begin(),
                state()->players.end(),
                [&opinion](const Player& item) {
                    return item.id == *opinion.playerId;
                });
            if (found != state()->players.end()) {
                player = &*found;
            }
        }

        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(opinion.id));
        row.insert(QStringLiteral("seasonId"), toQString(opinion.seasonId));
        row.insert(QStringLiteral("clubId"), toQString(opinion.clubId));
        row.insert(
            QStringLiteral("playerId"),
            opinion.playerId.has_value() ? toQString(*opinion.playerId) : QString {});
        row.insert(
            QStringLiteral("playerName"),
            player != nullptr ? toQString(player->fullName) : QStringLiteral("포지션군 의견"));
        row.insert(
            QStringLiteral("position"),
            player != nullptr ? toQString(player->position) : QString {});
        row.insert(
            QStringLiteral("positionGroup"),
            opinion.positionGroup.has_value()
                ? toQString(*opinion.positionGroup)
                : QString {});
        row.insert(
            QStringLiteral("source"),
            toQString(squadPlanningOpinionSourceKey(opinion.source)));
        row.insert(
            QStringLiteral("sourceLabel"),
            squadPlanningOpinionSourceLabel(opinion.source));
        row.insert(
            QStringLiteral("type"),
            toQString(squadPlanningOpinionTypeKey(opinion.type)));
        row.insert(
            QStringLiteral("typeLabel"),
            squadPlanningOpinionTypeLabel(opinion.type));
        row.insert(
            QStringLiteral("typeColor"),
            squadPlanningOpinionTypeColor(opinion.type));
        row.insert(QStringLiteral("priority"), opinion.priority);
        row.insert(QStringLiteral("reason"), toQString(opinion.reason));
        row.insert(
            QStringLiteral("targetWindow"),
            toQString(opinion.targetWindow));
        row.insert(
            QStringLiteral("status"),
            toQString(squadPlanningOpinionStatusKey(opinion.status)));
        row.insert(
            QStringLiteral("statusLabel"),
            squadPlanningOpinionStatusLabel(opinion.status));
        row.insert(
            QStringLiteral("createdDate"),
            shortDate(opinion.createdDate));
        row.insert(
            QStringLiteral("updatedDate"),
            shortDate(opinion.updatedDate));
        row.insert(
            QStringLiteral("confidence"),
            toQString(opinion.dataConfidence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::operatingMeetings() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->operatingMeetings.size()));
    for (const auto& meeting : state()->operatingMeetings) {
        QVariantList attendees;
        attendees.reserve(static_cast<qsizetype>(meeting.attendees.size()));
        QStringList attendeeLabels;
        for (const auto& attendee : meeting.attendees) {
            QVariantMap attendeeRow;
            attendeeRow.insert(QStringLiteral("role"), toQString(attendee.role));
            attendeeRow.insert(
                QStringLiteral("roleLabel"),
                operatingMeetingAttendeeRoleLabel(attendee.role));
            attendeeRow.insert(
                QStringLiteral("name"),
                toQString(attendee.displayName));
            attendeeRow.insert(
                QStringLiteral("autoAssigned"),
                attendee.autoAssigned);
            attendeeRow.insert(
                QStringLiteral("notes"),
                toQString(attendee.notes));
            attendees.push_back(attendeeRow);
            attendeeLabels.push_back(
                operatingMeetingAttendeeRoleLabel(attendee.role)
                + QStringLiteral(" ")
                + toQString(attendee.displayName));
        }

        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(meeting.id));
        row.insert(QStringLiteral("date"), shortDate(meeting.meetingDate));
        row.insert(
            QStringLiteral("agenda"),
            toQString(operatingMeetingAgendaTypeKey(meeting.agenda)));
        row.insert(
            QStringLiteral("agendaLabel"),
            operatingMeetingAgendaLabel(meeting.agenda));
        row.insert(
            QStringLiteral("agendaColor"),
            operatingMeetingAgendaColor(meeting.agenda));
        row.insert(
            QStringLiteral("status"),
            toQString(operatingMeetingStatusKey(meeting.status)));
        row.insert(
            QStringLiteral("statusLabel"),
            operatingMeetingStatusLabel(meeting.status));
        row.insert(
            QStringLiteral("attendeeMode"),
            toQString(operatingMeetingAttendeeModeKey(meeting.attendeeMode)));
        row.insert(
            QStringLiteral("attendeeModeLabel"),
            meeting.attendeeMode == OperatingMeetingAttendeeMode::Auto
                ? QStringLiteral("자동 참석자")
                : QStringLiteral("수동 참석자"));
        row.insert(QStringLiteral("attendees"), attendees);
        row.insert(
            QStringLiteral("attendeeSummary"),
            attendeeLabels.join(QStringLiteral(" · ")));
        row.insert(QStringLiteral("summary"), toQString(meeting.summary));
        row.insert(
            QStringLiteral("outcomeInboxEventKey"),
            toQString(meeting.outcomeInboxEventKey));
        row.insert(QStringLiteral("createdDate"), shortDate(meeting.createdDate));
        row.insert(
            QStringLiteral("confidence"),
            toQString(meeting.dataConfidence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::ownerActionLog() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->ownerActionLog.size()));
    for (auto action = state()->ownerActionLog.rbegin();
         action != state()->ownerActionLog.rend();
         ++action) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(action->id));
        row.insert(QStringLiteral("date"), shortDate(action->actionDate));
        row.insert(QStringLiteral("type"), toQString(action->actionType));
        row.insert(
            QStringLiteral("typeLabel"),
            ownerActionTypeLabel(action->actionType));
        row.insert(QStringLiteral("category"), toQString(action->category));
        row.insert(
            QStringLiteral("categoryLabel"),
            ownerActionCategoryLabel(action->category));
        row.insert(QStringLiteral("subjectId"), toQString(action->subjectId));
        row.insert(
            QStringLiteral("subjectName"),
            toQString(action->subjectName));
        row.insert(QStringLiteral("summary"), toQString(action->summary));
        row.insert(
            QStringLiteral("impact"),
            ownerActionImpactLabel(*action));
        row.insert(QStringLiteral("reputationDelta"), action->reputationDelta);
        row.insert(QStringLiteral("fanTrustDelta"), action->fanTrustDelta);
        row.insert(QStringLiteral("ambitionDelta"), action->ambitionDelta);
        row.insert(QStringLiteral("prudenceDelta"), action->prudenceDelta);
        row.insert(QStringLiteral("loyaltyDelta"), action->loyaltyDelta);
        row.insert(QStringLiteral("autonomyDelta"), action->autonomyDelta);
        row.insert(
            QStringLiteral("confidence"),
            toQString(action->dataConfidence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::transferProposals() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->transferProposals.size()));
    for (const auto& proposal : state()->transferProposals) {
        const auto targetAge = ageOnDate(proposal.target, state()->currentDate);
        const auto& finance = state()->club.finance;
        const TransferFeeTerms feeTerms =
            effectiveFeeTerms(proposal.feePence, proposal.feeTerms);
        const PlayerContractTerms contractTerms = effectiveContractTerms(
            proposal.weeklyWagePence,
            proposal.contractYears,
            proposal.contractTerms);
        const Money committedBudget =
            transferBudgetCommitment(feeTerms, contractTerms);
        const Money immediateCash =
            immediateTransferCashCost(feeTerms, contractTerms);
        const Money availableCashForProposal =
            football_owner::availableTransferCash(
                *state(),
                {},
                proposal.id);
        const Money availableBudgetForProposal =
            football_owner::availableTransferBudget(
                *state(),
                {},
                proposal.id);
        const bool affordable = availableBudgetForProposal >= committedBudget
            && availableCashForProposal >= committedBudget
            && finance.weeklyWageCommitmentPence + contractTerms.weeklyWagePence
                <= finance.weeklyWageBudgetPence;

        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(proposal.id));
        row.insert(QStringLiteral("playerId"), toQString(proposal.target.id));
        row.insert(QStringLiteral("name"), toQString(proposal.target.fullName));
        row.insert(QStringLiteral("position"), toQString(proposal.target.position));
        row.insert(
            QStringLiteral("age"),
            targetAge.has_value() ? QString::number(*targetAge) : QStringLiteral("미상"));
        row.insert(QStringLiteral("nationality"), toQString(proposal.target.nationality));
        row.insert(QStringLiteral("fromClub"), toQString(proposal.fromClubName));
        row.insert(QStringLiteral("fee"), toQString(formatMoney(guaranteedTransferFee(feeTerms))));
        row.insert(QStringLiteral("maximumFee"), toQString(formatMoney(maximumTransferFee(feeTerms))));
        row.insert(QStringLiteral("feeSummary"), feeTermsSummary(feeTerms));
        row.insert(QStringLiteral("weeklyWage"), toQString(formatMoney(contractTerms.weeklyWagePence)));
        row.insert(QStringLiteral("contractYears"), contractTerms.contractYears);
        row.insert(QStringLiteral("contractSummary"), contractTermsSummary(contractTerms));
        row.insert(QStringLiteral("immediateCash"), toQString(formatMoney(immediateCash)));
        row.insert(QStringLiteral("maximumCommitment"), toQString(formatMoney(committedBudget)));
        row.insert(QStringLiteral("availableCash"), toQString(formatMoney(availableCashForProposal)));
        row.insert(QStringLiteral("signingBonus"), toQString(formatMoney(contractTerms.signingBonusPence)));
        row.insert(QStringLiteral("loyaltyBonus"), toQString(formatMoney(contractTerms.loyaltyBonusPence)));
        row.insert(QStringLiteral("appearanceFee"), toQString(formatMoney(contractTerms.appearanceFeePence)));
        row.insert(QStringLiteral("goalBonus"), toQString(formatMoney(contractTerms.goalBonusPence)));
        row.insert(QStringLiteral("agentFee"), toQString(formatMoney(contractTerms.agentFeePence)));
        row.insert(
            QStringLiteral("releaseClause"),
            contractTerms.releaseClausePence > 0
                ? toQString(formatMoney(contractTerms.releaseClausePence))
                : QStringLiteral("없음"));
        row.insert(QStringLiteral("squadRole"), toQString(contractTerms.squadRole));
        row.insert(QStringLiteral("medicalStatus"), toQString(proposal.medicalStatus));
        row.insert(QStringLiteral("registrationStatus"), toQString(proposal.registrationStatus));
        row.insert(QStringLiteral("requestedBy"), toQString(proposal.requestedBy));
        row.insert(QStringLiteral("rationale"), toQString(proposal.rationale));
        row.insert(QStringLiteral("proposedDate"), shortDate(proposal.proposedDate));
        row.insert(QStringLiteral("decisionDeadline"), shortDate(proposal.decisionDeadline));
        row.insert(
            QStringLiteral("joinDate"),
            proposal.joinDate.has_value() ? shortDate(*proposal.joinDate) : QStringLiteral("승인 후 3일"));
        row.insert(QStringLiteral("statusKey"), toQString(transferStatusKey(proposal.status)));
        row.insert(QStringLiteral("status"), transferStatusLabel(proposal.status));
        row.insert(QStringLiteral("statusColor"), transferStatusColor(proposal.status));
        row.insert(QStringLiteral("pending"), proposal.status == TransferStatus::Pending);
        row.insert(QStringLiteral("affordable"), affordable);
        row.insert(
            QStringLiteral("budgetAfter"),
            proposal.status == TransferStatus::Pending
                ? toQString(formatMoney(finance.transferBudgetPence - committedBudget))
                : transferBudget());
        row.insert(
            QStringLiteral("cashAfter"),
            proposal.status == TransferStatus::Pending
                ? toQString(formatMoney(availableCashForProposal - committedBudget))
                : availableTransferCash());
        row.insert(
            QStringLiteral("wageAfter"),
            proposal.status == TransferStatus::Pending
                ? toQString(formatMoney(finance.weeklyWageCommitmentPence + contractTerms.weeklyWagePence))
                : weeklyWageCommitment());
        row.insert(QStringLiteral("confidenceLabel"), confidenceLabel(proposal.target.dataConfidence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::transferCandidates() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    const bool registrationOpen =
        domesticRegistrationOpen(*state(), state()->currentDate);
    const GameDate seasonEnd = state()->currentDate.month >= 6
        ? GameDate {state()->currentDate.year + 1, 5, 31}
        : GameDate {state()->currentDate.year, 5, 31};

    result.reserve(static_cast<qsizetype>(state()->transferCandidates.size()));
    for (const auto& candidate : state()->transferCandidates) {
        const auto pursuit = std::find_if(
            state()->transferPursuits.begin(),
            state()->transferPursuits.end(),
            [&candidate](const TransferPursuit& item) {
                return item.candidatePlayerId == candidate.player.id;
            });
        const bool hasPursuit = pursuit != state()->transferPursuits.end();
        const bool signedPlayer = std::any_of(
            state()->players.begin(),
            state()->players.end(),
            [&candidate](const Player& player) {
                return player.id == candidate.player.id;
            });
        const bool hasOpenLoan = std::any_of(
            state()->loanDeals.begin(),
            state()->loanDeals.end(),
            [&candidate](const LoanDeal& loan) {
                return loan.player.id == candidate.player.id
                    && loan.stage != LoanStage::Recalled
                    && loan.stage != LoanStage::Completed
                    && loan.stage != LoanStage::Rejected
                    && loan.stage != LoanStage::Withdrawn
                    && loan.stage != LoanStage::Expired;
            });
        const bool retryable = hasPursuit
            && (pursuit->stage == TransferPursuitStage::ClubRejected
                || pursuit->stage == TransferPursuitStage::PlayerRejected
                || pursuit->stage == TransferPursuitStage::MedicalFailed
                || pursuit->stage == TransferPursuitStage::RegistrationFailed
                || pursuit->stage == TransferPursuitStage::OwnerRejected
                || pursuit->stage == TransferPursuitStage::Withdrawn
                || pursuit->stage == TransferPursuitStage::Expired);
        const auto age = ageOnDate(candidate.player, state()->currentDate);
        const auto report = std::find_if(
            state()->scoutingReports.begin(),
            state()->scoutingReports.end(),
            [&candidate](const ScoutingReport& item) {
                return item.playerId == candidate.player.id;
            });
        const bool hasReport = report != state()->scoutingReports.end();
        const auto activeAssignment = std::find_if(
            state()->scoutingAssignments.begin(),
            state()->scoutingAssignments.end(),
            [&candidate](const ScoutingAssignment& assignment) {
                return assignment.playerId == candidate.player.id
                    && assignment.status == ScoutingAssignmentStatus::Active;
            });
        const bool isScouting =
            activeAssignment != state()->scoutingAssignments.end();
        const int knowledge = hasReport ? report->knowledgeLevel : 0;
        const bool stale = hasReport
            && (report->staleNotified
                || report->validUntil <= state()->currentDate);
        QString reportScoutName = QStringLiteral("미배정");
        if (hasReport) {
            const auto reportScout = std::find_if(
                state()->scouts.begin(),
                state()->scouts.end(),
                [&report](const ScoutProfile& scout) {
                    return scout.id == report->scoutId;
                });
            if (reportScout != state()->scouts.end()) {
                reportScoutName = toQString(reportScout->fullName);
            }
        }

        QVariantMap row;
        row.insert(QStringLiteral("playerId"), toQString(candidate.player.id));
        row.insert(QStringLiteral("name"), toQString(candidate.player.fullName));
        row.insert(QStringLiteral("position"), toQString(candidate.player.position));
        row.insert(QStringLiteral("age"), age.has_value() ? QString::number(*age) : QStringLiteral("미상"));
        row.insert(QStringLiteral("nationality"), toQString(candidate.player.nationality));
        row.insert(QStringLiteral("club"), toQString(candidate.currentClubName));
        row.insert(QStringLiteral("rank"), candidate.recommendationRank);
        row.insert(
            QStringLiteral("currentAbility"),
            hasReport
                ? QStringLiteral("%1–%2")
                    .arg(report->currentAbilityLow)
                    .arg(report->currentAbilityHigh)
                : QStringLiteral("미확인"));
        row.insert(
            QStringLiteral("potential"),
            hasReport
                ? QStringLiteral("%1–%2")
                    .arg(report->potentialLow)
                    .arg(report->potentialHigh)
                : QStringLiteral("미확인"));
        row.insert(QStringLiteral("managerPriority"), candidate.managerPriority);
        row.insert(QStringLiteral("playerInterest"), candidate.playerInterest);
        row.insert(
            QStringLiteral("playerInterestDisplay"),
            knowledge >= 45
                ? QStringLiteral("%1/100").arg(candidate.playerInterest)
                : QStringLiteral("확인 중"));
        row.insert(
            QStringLiteral("marketValue"),
            hasReport
                ? moneyRange(
                    report->marketValueLowPence,
                    report->marketValueHighPence)
                : QStringLiteral("미확인"));
        row.insert(QStringLiteral("openingOffer"), toQString(formatMoney(candidate.openingOfferPence)));
        row.insert(QStringLiteral("openingUpfrontPounds"), poundsValue(candidate.openingOfferPence));
        row.insert(QStringLiteral("openingInstallmentPounds"), 0);
        row.insert(QStringLiteral("openingInstallmentCount"), 0);
        row.insert(QStringLiteral("openingAppearanceBonusPounds"), 0);
        row.insert(QStringLiteral("openingAppearanceThreshold"), 0);
        row.insert(QStringLiteral("openingSellOnPercentage"), 0);
        row.insert(
            QStringLiteral("askingPrice"),
            hasReport
                ? moneyRange(
                    report->askingPriceLowPence,
                    report->askingPriceHighPence)
                : QStringLiteral("미확인"));
        row.insert(
            QStringLiteral("expectedWage"),
            hasReport
                ? moneyRange(
                    report->expectedWeeklyWageLowPence,
                    report->expectedWeeklyWageHighPence)
                : QStringLiteral("미확인"));
        row.insert(QStringLiteral("expectedYears"), candidate.expectedContractYears);
        row.insert(QStringLiteral("expectedWagePounds"), poundsValue(candidate.expectedWeeklyWagePence));
        row.insert(QStringLiteral("recommendedSigningPounds"), poundsValue(candidate.expectedWeeklyWagePence * 8));
        row.insert(QStringLiteral("recommendedLoyaltyPounds"), poundsValue(candidate.expectedWeeklyWagePence * 4));
        row.insert(QStringLiteral("recommendedAppearancePounds"), poundsValue(candidate.expectedWeeklyWagePence / 10));
        row.insert(QStringLiteral("recommendedGoalPounds"), poundsValue(candidate.expectedWeeklyWagePence / 8));
        row.insert(QStringLiteral("recommendedAgentPounds"), poundsValue(candidate.expectedWeeklyWagePence * 6));
        row.insert(QStringLiteral("recommendedReleasePounds"), 0);
        row.insert(
            QStringLiteral("recommendedRole"),
            candidate.managerPriority >= 90
                ? QStringLiteral("Key Player")
                : (candidate.managerPriority >= 75
                    ? QStringLiteral("First Team")
                    : QStringLiteral("Rotation")));
        row.insert(
            QStringLiteral("roleFit"),
            hasReport
                ? toQString(report->roleFit)
                : QStringLiteral("추가 관찰 필요"));
        row.insert(
            QStringLiteral("scoutSummary"),
            hasReport
                ? toQString(report->summary)
                : QStringLiteral("아직 정식 스카우팅 보고서가 없습니다."));
        row.insert(
            QStringLiteral("availability"),
            hasReport
                ? toQString(report->availability)
                : QStringLiteral("이적 가능성과 요구 조건이 확인되지 않았습니다."));
        row.insert(QStringLiteral("rivalClub"), toQString(candidate.rivalClubName));
        row.insert(QStringLiteral("rivalBid"), toQString(formatMoney(candidate.rivalBidPence)));
        row.insert(
            QStringLiteral("confidenceLabel"),
            QStringLiteral("스카우팅 지식 %1/100 · %2")
                .arg(knowledge)
                .arg(scoutingKnowledgeLabel(knowledge)));
        row.insert(QStringLiteral("knowledge"), knowledge);
        row.insert(QStringLiteral("knowledgeLabel"), scoutingKnowledgeLabel(knowledge));
        row.insert(QStringLiteral("reportStale"), stale);
        row.insert(
            QStringLiteral("reportFreshness"),
            !hasReport
                ? QStringLiteral("보고서 없음")
                : (stale
                    ? QStringLiteral("오래된 보고서")
                    : QStringLiteral("최신 · %1까지").arg(shortDate(report->validUntil))));
        row.insert(
            QStringLiteral("reportStatusColor"),
            stale ? QStringLiteral("#d2ab61") : QStringLiteral("#4db58b"));
        row.insert(QStringLiteral("reportScout"), reportScoutName);
        row.insert(
            QStringLiteral("lastObserved"),
            hasReport ? shortDate(report->lastObservedDate) : QStringLiteral("—"));
        row.insert(QStringLiteral("isScouting"), isScouting);
        row.insert(
            QStringLiteral("scoutingAssignmentId"),
            isScouting ? toQString(activeAssignment->id) : QString {});
        row.insert(
            QStringLiteral("canScout"),
            !signedPlayer
                && !isScouting
                && (!hasReport || knowledge < 100 || stale));
        row.insert(QStringLiteral("pursuitId"), hasPursuit ? toQString(pursuit->id) : QString {});
        row.insert(
            QStringLiteral("status"),
            signedPlayer
                ? QStringLiteral("영입 완료")
                : (hasPursuit
                    ? transferPursuitStageLabel(pursuit->stage)
                    : QStringLiteral("추천 후보")));
        row.insert(
            QStringLiteral("statusColor"),
            signedPlayer
                ? QStringLiteral("#4db58b")
                : (hasPursuit
                    ? transferPursuitStageColor(pursuit->stage)
                    : QStringLiteral("#8c96a8")));
        row.insert(QStringLiteral("canStart"), !signedPlayer && (!hasPursuit || retryable));
        row.insert(QStringLiteral("retry"), retryable);
        row.insert(
            QStringLiteral("canLoan"),
            !signedPlayer && !hasOpenLoan && registrationOpen);
        row.insert(
            QStringLiteral("recommendedLoanFeePounds"),
            poundsValue(std::max<Money>(
                100'000,
                candidate.marketValuePence / 50)));
        row.insert(QStringLiteral("recommendedLoanWagePercent"), 100);
        row.insert(
            QStringLiteral("recommendedLoanRole"),
            candidate.managerPriority >= 80
                ? QStringLiteral("Regular Starter")
                : QStringLiteral("Squad Player"));
        row.insert(
            QStringLiteral("recommendedLoanEndDate"),
            toQString(seasonEnd.toIsoString()));
        row.insert(
            QStringLiteral("recommendedLoanOptionPounds"),
            poundsValue(candidate.askingPricePence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::scouts() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }
    result.reserve(static_cast<qsizetype>(state()->scouts.size()));
    for (const auto& scout : state()->scouts) {
        const int activeAssignments = static_cast<int>(std::count_if(
            state()->scoutingAssignments.begin(),
            state()->scoutingAssignments.end(),
            [&scout](const ScoutingAssignment& assignment) {
                return assignment.scoutId == scout.id
                    && assignment.status == ScoutingAssignmentStatus::Active;
            }));
        QVariantList familiarities;
        for (const auto& familiarity : state()->scoutFamiliarities) {
            if (familiarity.scoutId != scout.id) {
                continue;
            }
            QVariantMap familiarityRow;
            familiarityRow.insert(
                QStringLiteral("category"),
                toQString(familiarity.category));
            familiarityRow.insert(
                QStringLiteral("categoryLabel"),
                scoutFamiliarityCategoryLabel(familiarity.category));
            familiarityRow.insert(
                QStringLiteral("key"),
                toQString(familiarity.key));
            familiarityRow.insert(QStringLiteral("level"), familiarity.level);
            familiarityRow.insert(
                QStringLiteral("updatedDate"),
                shortDate(familiarity.updatedDate));
            familiarities.push_back(familiarityRow);
        }
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(scout.id));
        row.insert(QStringLiteral("name"), toQString(scout.fullName));
        row.insert(QStringLiteral("role"), toQString(scout.role));
        row.insert(QStringLiteral("region"), toQString(scout.regionSpecialism));
        row.insert(QStringLiteral("currentAbility"), scout.judgingCurrentAbility);
        row.insert(QStringLiteral("potential"), scout.judgingPotential);
        row.insert(QStringLiteral("adaptability"), scout.adaptability);
        row.insert(QStringLiteral("capacity"), scout.assignmentCapacity);
        row.insert(QStringLiteral("activeAssignments"), activeAssignments);
        row.insert(
            QStringLiteral("availableSlots"),
            std::max(0, scout.assignmentCapacity - activeAssignments));
        row.insert(
            QStringLiteral("confidenceLabel"),
            scout.dataConfidence.find("gameplay_estimate") != std::string::npos
                ? QStringLiteral("역할 확인 · 능력치 추정")
                : QStringLiteral("역할 맥락 확인 · 1992 직함 잠정"));
        row.insert(
            QStringLiteral("familiaritySummary"),
            scoutTopFamiliaritySummary(*state(), scout.id));
        row.insert(QStringLiteral("familiarities"), familiarities);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::scoutingAssignments() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }
    result.reserve(static_cast<qsizetype>(state()->scoutingAssignments.size()));
    for (const auto& assignment : state()->scoutingAssignments) {
        const auto candidate = std::find_if(
            state()->transferCandidates.begin(),
            state()->transferCandidates.end(),
            [&assignment](const TransferCandidate& item) {
                return item.player.id == assignment.playerId;
            });
        const auto scout = std::find_if(
            state()->scouts.begin(),
            state()->scouts.end(),
            [&assignment](const ScoutProfile& item) {
                return item.id == assignment.scoutId;
            });
        const int duration = std::max(
            1,
            assignment.startedDate.daysUntil(assignment.dueDate));
        const int elapsed = std::clamp(
            assignment.startedDate.daysUntil(state()->currentDate),
            0,
            duration);
        const int progress = assignment.status == ScoutingAssignmentStatus::Completed
            ? 100
            : (assignment.status == ScoutingAssignmentStatus::Cancelled
                ? assignment.lastMilestone
                : elapsed * 100 / duration);

        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(assignment.id));
        row.insert(QStringLiteral("playerId"), toQString(assignment.playerId));
        row.insert(
            QStringLiteral("playerName"),
            candidate != state()->transferCandidates.end()
                ? toQString(candidate->player.fullName)
                : toQString(assignment.playerId));
        row.insert(
            QStringLiteral("scoutName"),
            scout != state()->scouts.end()
                ? toQString(scout->fullName)
                : toQString(assignment.scoutId));
        row.insert(QStringLiteral("depth"), scoutingDepthLabel(assignment.depth));
        row.insert(QStringLiteral("depthKey"), toQString(scoutingDepthKey(assignment.depth)));
        row.insert(QStringLiteral("focus"), scoutingFocusLabel(assignment.focus));
        row.insert(QStringLiteral("focusKey"), toQString(assignment.focus));
        row.insert(QStringLiteral("startedDate"), shortDate(assignment.startedDate));
        row.insert(QStringLiteral("nextUpdateDate"), shortDate(assignment.nextUpdateDate));
        row.insert(QStringLiteral("dueDate"), shortDate(assignment.dueDate));
        row.insert(QStringLiteral("status"), scoutingStatusLabel(assignment.status));
        row.insert(QStringLiteral("statusColor"), scoutingStatusColor(assignment.status));
        row.insert(QStringLiteral("progress"), progress);
        row.insert(QStringLiteral("startingKnowledge"), assignment.startingKnowledge);
        row.insert(QStringLiteral("targetKnowledge"), assignment.targetKnowledge);
        row.insert(
            QStringLiteral("canCancel"),
            assignment.status == ScoutingAssignmentStatus::Active);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::scoutingDirectives() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->scoutingDirectives.size()));
    for (const auto& directive : state()->scoutingDirectives) {
        const auto scout = std::find_if(
            state()->scouts.begin(),
            state()->scouts.end(),
            [&directive](const ScoutProfile& item) {
                return item.id == directive.scoutId;
            });
        const auto target = std::find_if(
            state()->transferCandidates.begin(),
            state()->transferCandidates.end(),
            [&directive](const TransferCandidate& item) {
                return item.player.id == directive.targetPlayerId;
            });
        const auto assignment = std::find_if(
            state()->scoutingAssignments.begin(),
            state()->scoutingAssignments.end(),
            [&directive](const ScoutingAssignment& item) {
                return item.id == directive.generatedAssignmentId;
            });

        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(directive.id));
        row.insert(
            QStringLiteral("type"),
            toQString(scoutingDirectiveTypeKey(directive.type)));
        row.insert(
            QStringLiteral("typeLabel"),
            scoutingDirectiveTypeLabel(directive.type));
        row.insert(
            QStringLiteral("status"),
            toQString(scoutingDirectiveStatusKey(directive.status)));
        row.insert(
            QStringLiteral("statusLabel"),
            scoutingDirectiveStatusLabel(directive.status));
        row.insert(
            QStringLiteral("statusColor"),
            scoutingDirectiveStatusColor(directive.status));
        row.insert(QStringLiteral("scoutId"), toQString(directive.scoutId));
        row.insert(
            QStringLiteral("scoutName"),
            scout != state()->scouts.end()
                ? toQString(scout->fullName)
                : toQString(directive.scoutId));
        row.insert(QStringLiteral("depth"), scoutingDepthLabel(directive.depth));
        row.insert(
            QStringLiteral("depthKey"),
            toQString(scoutingDepthKey(directive.depth)));
        row.insert(QStringLiteral("focus"), scoutingFocusLabel(directive.focus));
        row.insert(QStringLiteral("focusKey"), toQString(directive.focus));
        row.insert(
            QStringLiteral("targetPlayerId"),
            toQString(directive.targetPlayerId));
        row.insert(
            QStringLiteral("targetName"),
            target != state()->transferCandidates.end()
                ? toQString(target->player.fullName)
                : (directive.targetPlayerId.empty()
                    ? QStringLiteral("범위 후보")
                    : toQString(directive.targetPlayerId)));
        row.insert(
            QStringLiteral("generatedAssignmentId"),
            toQString(directive.generatedAssignmentId));
        row.insert(
            QStringLiteral("filterSummary"),
            scoutingDirectiveFilterSummary(directive));
        row.insert(
            QStringLiteral("matchedCandidateCount"),
            directive.matchedCandidateCount);
        row.insert(
            QStringLiteral("matchedPlayerIds"),
            toQString(directive.matchedPlayerIds));
        row.insert(
            QStringLiteral("recommendationScore"),
            directive.recommendationScore);
        row.insert(
            QStringLiteral("estimatedDurationDays"),
            directive.estimatedDurationDays);
        row.insert(
            QStringLiteral("accuracyModifier"),
            directive.accuracyModifier);
        row.insert(QStringLiteral("createdDate"), shortDate(directive.createdDate));
        row.insert(QStringLiteral("dueDate"), shortDate(directive.dueDate));
        row.insert(
            QStringLiteral("completedDate"),
            directive.completedDate.has_value()
                ? shortDate(*directive.completedDate)
                : QStringLiteral("—"));
        row.insert(QStringLiteral("summary"), toQString(directive.summary));
        row.insert(
            QStringLiteral("canCancel"),
            directive.status == ScoutingDirectiveStatus::Active);
        row.insert(
            QStringLiteral("assignmentStatus"),
            assignment != state()->scoutingAssignments.end()
                ? scoutingStatusLabel(assignment->status)
                : QStringLiteral("연결 배정 없음"));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::recruitmentPlans() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }
    result.reserve(static_cast<qsizetype>(state()->recruitmentPlans.size()));
    for (const auto& plan : state()->recruitmentPlans) {
        const int completed = recruitmentPlanCompletedSignings(*state(), plan);
        const int priorityScore = recruitmentPlanPriorityScore(*state(), plan);
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(plan.id));
        row.insert(QStringLiteral("title"), toQString(plan.title));
        row.insert(QStringLiteral("requestedBy"), toQString(plan.requestedBy));
        row.insert(QStringLiteral("description"), toQString(plan.description));
        row.insert(QStringLiteral("startDate"), shortDate(plan.startDate));
        row.insert(QStringLiteral("evaluationDate"), shortDate(plan.evaluationDate));
        row.insert(QStringLiteral("targetSignings"), plan.targetSignings);
        row.insert(
            QStringLiteral("minimumAcceptableSignings"),
            plan.minimumAcceptableSignings);
        row.insert(QStringLiteral("completedSignings"), completed);
        row.insert(QStringLiteral("priorityScore"), priorityScore);
        row.insert(
            QStringLiteral("progress"),
            plan.targetSignings > 0
                ? std::min(100, completed * 100 / plan.targetSignings)
                : 0);
        row.insert(QStringLiteral("status"), recruitmentPlanStatusLabel(plan.status));
        row.insert(QStringLiteral("statusColor"), recruitmentPlanStatusColor(plan.status));
        row.insert(QStringLiteral("confidenceDelta"), plan.confidenceDelta);
        row.insert(
            QStringLiteral("active"),
            plan.status == RecruitmentPlanStatus::Active);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::transferPursuits() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->transferPursuits.size()));
    for (const auto& pursuit : state()->transferPursuits) {
        const auto candidate = std::find_if(
            state()->transferCandidates.begin(),
            state()->transferCandidates.end(),
            [&pursuit](const TransferCandidate& item) {
                return item.player.id == pursuit.candidatePlayerId;
            });
        const QString playerName = candidate != state()->transferCandidates.end()
            ? toQString(candidate->player.fullName)
            : toQString(pursuit.candidatePlayerId);
        const QString clubName = candidate != state()->transferCandidates.end()
            ? toQString(candidate->currentClubName)
            : QStringLiteral("상대 구단");
        const TransferFeeTerms offerTerms =
            effectiveFeeTerms(pursuit.currentOfferPence, pursuit.offerTerms);
        const TransferFeeTerms counterTerms =
            pursuit.clubCounterPence > 0
            ? effectiveFeeTerms(pursuit.clubCounterPence, pursuit.clubCounterTerms)
            : pursuit.clubCounterTerms;
        const PlayerContractTerms playerTerms = pursuit.playerOfferTerms;
        const PlayerContractTerms playerCounterTerms = pursuit.playerCounterTerms;

        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(pursuit.id));
        row.insert(QStringLiteral("playerId"), toQString(pursuit.candidatePlayerId));
        row.insert(QStringLiteral("name"), playerName);
        row.insert(QStringLiteral("club"), clubName);
        row.insert(QStringLiteral("stageKey"), toQString(transferPursuitStageKey(pursuit.stage)));
        row.insert(QStringLiteral("stage"), transferPursuitStageLabel(pursuit.stage));
        row.insert(QStringLiteral("stageColor"), transferPursuitStageColor(pursuit.stage));
        row.insert(QStringLiteral("currentOffer"), toQString(formatMoney(guaranteedTransferFee(offerTerms))));
        row.insert(QStringLiteral("offerSummary"), feeTermsSummary(offerTerms));
        row.insert(QStringLiteral("offerUpfrontPounds"), poundsValue(offerTerms.upfrontFeePence));
        row.insert(QStringLiteral("offerInstallmentPounds"), poundsValue(offerTerms.installmentTotalPence));
        row.insert(QStringLiteral("offerInstallmentCount"), offerTerms.installmentCount);
        row.insert(QStringLiteral("offerAppearanceBonusPounds"), poundsValue(offerTerms.appearanceBonusPence));
        row.insert(QStringLiteral("offerAppearanceThreshold"), offerTerms.appearanceThreshold);
        row.insert(QStringLiteral("offerSellOnPercentage"), offerTerms.sellOnPercentage);
        row.insert(
            QStringLiteral("clubCounter"),
            pursuit.clubCounterPence > 0
                ? toQString(formatMoney(guaranteedTransferFee(counterTerms)))
                : QStringLiteral("—"));
        row.insert(
            QStringLiteral("clubCounterSummary"),
            pursuit.clubCounterPence > 0
                ? feeTermsSummary(counterTerms)
                : QStringLiteral("—"));
        row.insert(QStringLiteral("counterUpfrontPounds"), poundsValue(counterTerms.upfrontFeePence));
        row.insert(QStringLiteral("counterInstallmentPounds"), poundsValue(counterTerms.installmentTotalPence));
        row.insert(QStringLiteral("counterInstallmentCount"), counterTerms.installmentCount);
        row.insert(QStringLiteral("counterAppearanceBonusPounds"), poundsValue(counterTerms.appearanceBonusPence));
        row.insert(QStringLiteral("counterAppearanceThreshold"), counterTerms.appearanceThreshold);
        row.insert(QStringLiteral("counterSellOnPercentage"), counterTerms.sellOnPercentage);
        row.insert(QStringLiteral("playerTermsSummary"), contractTermsSummary(playerTerms));
        row.insert(QStringLiteral("playerWagePounds"), poundsValue(playerTerms.weeklyWagePence));
        row.insert(QStringLiteral("playerContractYears"), playerTerms.contractYears);
        row.insert(QStringLiteral("playerSigningPounds"), poundsValue(playerTerms.signingBonusPence));
        row.insert(QStringLiteral("playerLoyaltyPounds"), poundsValue(playerTerms.loyaltyBonusPence));
        row.insert(QStringLiteral("playerAppearancePounds"), poundsValue(playerTerms.appearanceFeePence));
        row.insert(QStringLiteral("playerGoalPounds"), poundsValue(playerTerms.goalBonusPence));
        row.insert(QStringLiteral("playerAgentPounds"), poundsValue(playerTerms.agentFeePence));
        row.insert(QStringLiteral("playerReleasePounds"), poundsValue(playerTerms.releaseClausePence));
        row.insert(QStringLiteral("playerRole"), toQString(playerTerms.squadRole));
        row.insert(QStringLiteral("playerCounterSummary"), contractTermsSummary(playerCounterTerms));
        row.insert(QStringLiteral("counterWagePounds"), poundsValue(playerCounterTerms.weeklyWagePence));
        row.insert(QStringLiteral("counterContractYears"), playerCounterTerms.contractYears);
        row.insert(QStringLiteral("counterSigningPounds"), poundsValue(playerCounterTerms.signingBonusPence));
        row.insert(QStringLiteral("counterLoyaltyPounds"), poundsValue(playerCounterTerms.loyaltyBonusPence));
        row.insert(QStringLiteral("counterAppearancePounds"), poundsValue(playerCounterTerms.appearanceFeePence));
        row.insert(QStringLiteral("counterGoalPounds"), poundsValue(playerCounterTerms.goalBonusPence));
        row.insert(QStringLiteral("counterAgentPounds"), poundsValue(playerCounterTerms.agentFeePence));
        row.insert(QStringLiteral("counterReleasePounds"), poundsValue(playerCounterTerms.releaseClausePence));
        row.insert(QStringLiteral("counterRole"), toQString(playerCounterTerms.squadRole));
        row.insert(QStringLiteral("playerRound"), pursuit.playerNegotiationRound);
        row.insert(QStringLiteral("medicalStatus"), toQString(pursuit.medicalStatus));
        row.insert(QStringLiteral("registrationStatus"), toQString(pursuit.registrationStatus));
        row.insert(QStringLiteral("round"), pursuit.negotiationRound);
        row.insert(QStringLiteral("startedDate"), shortDate(pursuit.startedDate));
        row.insert(QStringLiteral("nextActionDate"), shortDate(pursuit.nextActionDate));
        row.insert(QStringLiteral("decisionDeadline"), shortDate(pursuit.decisionDeadline));
        row.insert(QStringLiteral("responseNote"), toQString(pursuit.responseNote));
        row.insert(QStringLiteral("rivalActive"), pursuit.rivalBidActive);
        row.insert(QStringLiteral("rivalClub"), toQString(pursuit.rivalClubName));
        row.insert(
            QStringLiteral("rivalBid"),
            pursuit.rivalBidPence > 0
                ? toQString(formatMoney(pursuit.rivalBidPence))
                : QStringLiteral("—"));
        row.insert(QStringLiteral("linkedProposalId"), toQString(pursuit.linkedProposalId));
        row.insert(
            QStringLiteral("canAcceptCounter"),
            pursuit.stage == TransferPursuitStage::ClubCountered);
        row.insert(
            QStringLiteral("canImproveOffer"),
            pursuit.stage == TransferPursuitStage::ClubCountered);
        row.insert(
            QStringLiteral("canSubmitPlayerTerms"),
            pursuit.stage == TransferPursuitStage::AwaitingPlayerTerms
                || pursuit.stage == TransferPursuitStage::PlayerCountered);
        row.insert(
            QStringLiteral("canAcceptPlayerCounter"),
            pursuit.stage == TransferPursuitStage::PlayerCountered);
        row.insert(
            QStringLiteral("canWithdraw"),
            pursuit.stage == TransferPursuitStage::AwaitingClubResponse
                || pursuit.stage == TransferPursuitStage::ClubCountered
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerTerms
                || pursuit.stage == TransferPursuitStage::AwaitingPlayerResponse
                || pursuit.stage == TransferPursuitStage::PlayerCountered
                || pursuit.stage == TransferPursuitStage::AwaitingMedical
                || pursuit.stage == TransferPursuitStage::AwaitingRegistration);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::outgoingTransferOffers() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->outgoingTransferOffers.size()));
    for (const auto& offer : state()->outgoingTransferOffers) {
        const TransferFeeTerms feeTerms =
            effectiveFeeTerms(offer.feePence, offer.feeTerms);
        const TransferFeeTerms counterTerms = offer.buyerCounterTerms;
        const bool canDecide = offer.status == OutgoingTransferStatus::Pending
            && (offer.negotiationStage == OutgoingNegotiationStage::DecisionPending
                || offer.negotiationStage == OutgoingNegotiationStage::AgreementReached);
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(offer.id));
        row.insert(QStringLiteral("playerId"), toQString(offer.playerId));
        row.insert(QStringLiteral("name"), toQString(offer.playerName));
        row.insert(QStringLiteral("buyerClub"), toQString(offer.buyerClubName));
        row.insert(QStringLiteral("fee"), toQString(formatMoney(guaranteedTransferFee(feeTerms))));
        row.insert(QStringLiteral("maximumFee"), toQString(formatMoney(maximumTransferFee(feeTerms))));
        row.insert(QStringLiteral("feeSummary"), feeTermsSummary(feeTerms));
        row.insert(QStringLiteral("upfrontPounds"), poundsValue(feeTerms.upfrontFeePence));
        row.insert(QStringLiteral("installmentPounds"), poundsValue(feeTerms.installmentTotalPence));
        row.insert(QStringLiteral("installmentCount"), feeTerms.installmentCount);
        row.insert(QStringLiteral("appearanceBonusPounds"), poundsValue(feeTerms.appearanceBonusPence));
        row.insert(QStringLiteral("appearanceThreshold"), feeTerms.appearanceThreshold);
        row.insert(QStringLiteral("sellOnPercentage"), feeTerms.sellOnPercentage);
        row.insert(QStringLiteral("buyerCounterSummary"), feeTermsSummary(counterTerms));
        row.insert(QStringLiteral("buyerCounterUpfrontPounds"), poundsValue(counterTerms.upfrontFeePence));
        row.insert(QStringLiteral("buyerCounterInstallmentPounds"), poundsValue(counterTerms.installmentTotalPence));
        row.insert(QStringLiteral("buyerCounterInstallmentCount"), counterTerms.installmentCount);
        row.insert(QStringLiteral("buyerCounterAppearanceBonusPounds"), poundsValue(counterTerms.appearanceBonusPence));
        row.insert(QStringLiteral("buyerCounterAppearanceThreshold"), counterTerms.appearanceThreshold);
        row.insert(QStringLiteral("buyerCounterSellOnPercentage"), counterTerms.sellOnPercentage);
        row.insert(
            QStringLiteral("wageRelief"),
            offer.weeklyWageReliefPence > 0
                ? toQString(formatMoney(offer.weeklyWageReliefPence))
                : QStringLiteral("미상"));
        row.insert(QStringLiteral("proposedDate"), shortDate(offer.proposedDate));
        row.insert(QStringLiteral("decisionDeadline"), shortDate(offer.decisionDeadline));
        row.insert(
            QStringLiteral("completionDate"),
            offer.completionDate.has_value()
                ? shortDate(*offer.completionDate)
                : QStringLiteral("수락 후 3일"));
        row.insert(QStringLiteral("requestedRole"), toQString(offer.requestedRole));
        row.insert(QStringLiteral("rationale"), toQString(offer.rationale));
        row.insert(QStringLiteral("statusKey"), toQString(outgoingTransferStatusKey(offer.status)));
        row.insert(QStringLiteral("status"), outgoingTransferStatusLabel(offer.status));
        row.insert(QStringLiteral("statusColor"), outgoingTransferStatusColor(offer.status));
        row.insert(QStringLiteral("pending"), offer.status == OutgoingTransferStatus::Pending);
        row.insert(QStringLiteral("canDecide"), canDecide);
        row.insert(
            QStringLiteral("canCounter"),
            offer.status == OutgoingTransferStatus::Pending
                && offer.negotiationStage != OutgoingNegotiationStage::AwaitingBuyerResponse);
        row.insert(
            QStringLiteral("canAcceptBuyerCounter"),
            offer.status == OutgoingTransferStatus::Pending
                && offer.negotiationStage == OutgoingNegotiationStage::BuyerCountered);
        row.insert(
            QStringLiteral("negotiationStage"),
            outgoingNegotiationStageLabel(offer.negotiationStage));
        row.insert(
            QStringLiteral("nextActionDate"),
            offer.nextActionDate.has_value()
                ? shortDate(*offer.nextActionDate)
                : QStringLiteral("—"));
        row.insert(QStringLiteral("negotiationRound"), offer.negotiationRound);
        row.insert(QStringLiteral("confidenceLabel"), confidenceLabel(offer.dataConfidence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::aiMarketDeals() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    std::vector<const AiMarketDeal*> ordered;
    ordered.reserve(state()->aiMarketDeals.size());
    for (const auto& deal : state()->aiMarketDeals) {
        ordered.push_back(&deal);
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const AiMarketDeal* left, const AiMarketDeal* right) {
            if (left->proposedDate != right->proposedDate) {
                return right->proposedDate < left->proposedDate;
            }
            return left->id < right->id;
        });

    for (const AiMarketDeal* deal : ordered) {
        const bool loan = deal->type == AiMarketDealType::Loan;
        const bool renewal = deal->type == AiMarketDealType::Renewal;
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(deal->id));
        row.insert(QStringLiteral("type"), aiMarketDealTypeLabel(deal->type));
        row.insert(
            QStringLiteral("typeKey"),
            toQString(aiMarketDealTypeKey(deal->type)));
        row.insert(QStringLiteral("stage"), aiMarketDealStageLabel(deal->stage));
        row.insert(
            QStringLiteral("stageKey"),
            toQString(aiMarketDealStageKey(deal->stage)));
        row.insert(
            QStringLiteral("stageColor"),
            aiMarketDealStageColor(deal->stage));
        row.insert(QStringLiteral("player"), toQString(deal->player.fullName));
        row.insert(QStringLiteral("position"), toQString(deal->player.position));
        row.insert(QStringLiteral("seller"), toQString(deal->sellingClubName));
        row.insert(QStringLiteral("buyer"), toQString(deal->buyingClubName));
        row.insert(
            QStringLiteral("route"),
            toQString(deal->sellingClubName + " → " + deal->buyingClubName));
        row.insert(QStringLiteral("proposedDate"), shortDate(deal->proposedDate));
        row.insert(
            QStringLiteral("nextActionDate"),
            deal->nextActionDate.has_value()
                ? shortDate(*deal->nextActionDate)
                : QStringLiteral("—"));
        row.insert(
            QStringLiteral("completionDate"),
            deal->completionDate.has_value()
                ? shortDate(*deal->completionDate)
                : QStringLiteral("—"));
        row.insert(
            QStringLiteral("completedDate"),
            deal->completedDate.has_value()
                ? shortDate(*deal->completedDate)
                : QStringLiteral("—"));
        row.insert(
            QStringLiteral("guaranteedFee"),
            loan
                ? toQString(formatMoney(deal->loanTerms.loanFeePence))
                : (renewal
                    ? toQString(formatMoney(
                        contractRenewalCommitment(deal->contractTerms)))
                    : toQString(formatMoney(
                        guaranteedTransferFee(deal->feeTerms)))));
        row.insert(
            QStringLiteral("maximumFee"),
            loan
                ? toQString(formatMoney(
                    loanFinancialCommitment(deal->loanTerms)))
                : (renewal
                    ? toQString(formatMoney(
                        contractRenewalCommitment(deal->contractTerms)))
                    : toQString(formatMoney(
                        transferBudgetCommitment(
                            deal->feeTerms,
                            deal->contractTerms)))));
        row.insert(
            QStringLiteral("feeSummary"),
            loan
                ? loanTermsSummary(deal->loanTerms)
                : (renewal
                    ? contractTermsSummary(deal->contractTerms)
                    : feeTermsSummary(deal->feeTerms)));
        row.insert(
            QStringLiteral("contractSummary"),
            contractTermsSummary(deal->contractTerms));
        row.insert(QStringLiteral("rationale"), toQString(deal->rationale));
        row.insert(
            QStringLiteral("playerClubInvolved"),
            deal->playerClubInvolved);
        row.insert(QStringLiteral("budgetReserved"), deal->budgetReserved);
        row.insert(QStringLiteral("completionApplied"), deal->completionApplied);
        row.insert(
            QStringLiteral("confidenceLabel"),
            confidenceLabel(deal->dataConfidence));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::transferPayments() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    std::vector<const TransferPayment*> ordered;
    ordered.reserve(state()->transferPayments.size());
    for (const auto& payment : state()->transferPayments) {
        ordered.push_back(&payment);
    }
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const TransferPayment* left, const TransferPayment* right) {
            if (left->dueDate != right->dueDate) {
                return left->dueDate < right->dueDate;
            }
            return left->id < right->id;
        });

    for (const TransferPayment* payment : ordered) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(payment->id));
        row.insert(QStringLiteral("counterparty"), toQString(payment->counterpartyName));
        row.insert(
            QStringLiteral("direction"),
            payment->direction == TransferPaymentDirection::Payable
                ? QStringLiteral("지급")
                : QStringLiteral("수령"));
        row.insert(
            QStringLiteral("directionColor"),
            payment->direction == TransferPaymentDirection::Payable
                ? QStringLiteral("#d78376")
                : QStringLiteral("#6fc29f"));
        row.insert(QStringLiteral("category"), paymentCategoryLabel(payment->category));
        row.insert(QStringLiteral("dueDate"), shortDate(payment->dueDate));
        row.insert(QStringLiteral("amount"), toQString(formatMoney(payment->amountPence)));
        row.insert(QStringLiteral("status"), paymentStatusLabel(payment->status));
        row.insert(QStringLiteral("statusColor"), paymentStatusColor(payment->status));
        row.insert(QStringLiteral("description"), toQString(payment->description));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::playerContracts() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(
        static_cast<qsizetype>(state()->playerContracts.size()));
    for (const auto& contract : state()->playerContracts) {
        const auto player = std::find_if(
            state()->players.begin(),
            state()->players.end(),
            [&contract](const Player& item) {
                return item.id == contract.playerId;
            });
        if (player == state()->players.end()) {
            continue;
        }
        const auto renewal = std::find_if(
            state()->contractRenewals.begin(),
            state()->contractRenewals.end(),
            [&contract](const ContractRenewalCase& item) {
                return item.playerId == contract.playerId
                    && item.stage != ContractRenewalStage::Completed;
            });
        const bool hasRenewal =
            renewal != state()->contractRenewals.end();
        const int daysRemaining =
            state()->currentDate.daysUntil(contract.endDate);
        PlayerContractTerms recommended = contract.terms;
        recommended.weeklyWagePence =
            std::max(
                contract.weeklyWagePence,
                contract.weeklyWagePence * 110 / 100);
        recommended.contractYears =
            std::clamp(recommended.contractYears, 2, 4);
        recommended.signingBonusPence =
            std::max<Money>(
                recommended.signingBonusPence,
                recommended.weeklyWagePence * 6);
        recommended.loyaltyBonusPence =
            std::max<Money>(
                recommended.loyaltyBonusPence,
                recommended.weeklyWagePence * 3);
        recommended.agentFeePence =
            std::max<Money>(
                recommended.agentFeePence,
                recommended.weeklyWagePence * 3);
        if (hasRenewal
            && validPlayerContractTerms(renewal->recommendedTerms)) {
            recommended = renewal->recommendedTerms;
        }

        QVariantMap row;
        row.insert(QStringLiteral("playerId"), toQString(contract.playerId));
        row.insert(QStringLiteral("name"), toQString(player->fullName));
        row.insert(QStringLiteral("position"), toQString(player->position));
        row.insert(
            QStringLiteral("squadGroup"),
            squadGroupLabel(player->squadGroup));
        row.insert(
            QStringLiteral("weeklyWage"),
            toQString(formatMoney(contract.weeklyWagePence)));
        row.insert(
            QStringLiteral("weeklyWagePounds"),
            poundsValue(contract.weeklyWagePence));
        row.insert(QStringLiteral("squadRole"), toQString(contract.squadRole));
        row.insert(
            QStringLiteral("startDate"),
            shortDate(contract.startDate));
        row.insert(QStringLiteral("endDate"), shortDate(contract.endDate));
        row.insert(
            QStringLiteral("endDateIso"),
            toQString(contract.endDate.toIsoString()));
        row.insert(QStringLiteral("daysRemaining"), daysRemaining);
        row.insert(
            QStringLiteral("expiryRisk"),
            daysRemaining <= 0
                ? QStringLiteral("만료")
                : (daysRemaining <= 90
                    ? QStringLiteral("긴급")
                    : (daysRemaining <= 365
                        ? QStringLiteral("1년 이내")
                        : QStringLiteral("안정"))));
        row.insert(
            QStringLiteral("lifecycleStatus"),
            toQString(contract.lifecycleStatus));
        row.insert(
            QStringLiteral("contractSummary"),
            contractTermsSummary(contract.terms));
        row.insert(
            QStringLiteral("recommendedWagePounds"),
            poundsValue(recommended.weeklyWagePence));
        row.insert(
            QStringLiteral("recommendedYears"),
            recommended.contractYears);
        row.insert(
            QStringLiteral("recommendedSigningPounds"),
            poundsValue(recommended.signingBonusPence));
        row.insert(
            QStringLiteral("recommendedLoyaltyPounds"),
            poundsValue(recommended.loyaltyBonusPence));
        row.insert(
            QStringLiteral("recommendedAppearancePounds"),
            poundsValue(recommended.appearanceFeePence));
        row.insert(
            QStringLiteral("recommendedGoalPounds"),
            poundsValue(recommended.goalBonusPence));
        row.insert(
            QStringLiteral("recommendedAgentPounds"),
            poundsValue(recommended.agentFeePence));
        row.insert(
            QStringLiteral("recommendedReleasePounds"),
            poundsValue(recommended.releaseClausePence));
        row.insert(
            QStringLiteral("recommendedRole"),
            toQString(recommended.squadRole));
        row.insert(
            QStringLiteral("renewalId"),
            hasRenewal ? toQString(renewal->id) : QString {});
        row.insert(
            QStringLiteral("renewalStageKey"),
            hasRenewal
                ? toQString(contractRenewalStageKey(renewal->stage))
                : QStringLiteral("none"));
        row.insert(
            QStringLiteral("renewalStage"),
            hasRenewal
                ? contractRenewalStageLabel(renewal->stage)
                : QStringLiteral("미개시"));
        row.insert(
            QStringLiteral("renewalStageColor"),
            hasRenewal
                ? contractRenewalStageColor(renewal->stage)
                : QStringLiteral("#8c96a8"));
        row.insert(
            QStringLiteral("renewalNote"),
            hasRenewal ? toQString(renewal->responseNote) : QString {});
        row.insert(
            QStringLiteral("offerSummary"),
            hasRenewal
                && validPlayerContractTerms(renewal->offerTerms)
                ? contractTermsSummary(renewal->offerTerms)
                : QStringLiteral("아직 제안 없음"));
        row.insert(
            QStringLiteral("counterSummary"),
            hasRenewal
                && validPlayerContractTerms(renewal->counterTerms)
                ? contractTermsSummary(renewal->counterTerms)
                : QString {});
        row.insert(
            QStringLiteral("canAcceptCounter"),
            hasRenewal
                && renewal->stage
                    == ContractRenewalStage::PlayerCountered);
        row.insert(
            QStringLiteral("canConfirm"),
            hasRenewal
                && renewal->stage
                    == ContractRenewalStage::AgreementReached);
        row.insert(
            QStringLiteral("canWithdraw"),
            hasRenewal
                && (renewal->stage
                        == ContractRenewalStage::AwaitingPlayerResponse
                    || renewal->stage
                        == ContractRenewalStage::PlayerCountered
                    || renewal->stage
                        == ContractRenewalStage::AgreementReached));
        row.insert(
            QStringLiteral("canOffer"),
            !hasRenewal
                || renewal->stage == ContractRenewalStage::Recommended
                || renewal->stage == ContractRenewalStage::PlayerCountered
                || renewal->stage == ContractRenewalStage::Rejected
                || renewal->stage == ContractRenewalStage::Withdrawn
                || renewal->stage == ContractRenewalStage::Expired);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::loanDeals() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }

    result.reserve(static_cast<qsizetype>(state()->loanDeals.size()));
    for (const auto& loan : state()->loanDeals) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(loan.id));
        row.insert(QStringLiteral("playerId"), toQString(loan.player.id));
        row.insert(QStringLiteral("name"), toQString(loan.player.fullName));
        row.insert(QStringLiteral("position"), toQString(loan.player.position));
        row.insert(
            QStringLiteral("direction"),
            loanDirectionLabel(loan.direction));
        row.insert(
            QStringLiteral("directionKey"),
            toQString(loanDirectionKey(loan.direction)));
        row.insert(
            QStringLiteral("counterparty"),
            toQString(
                loan.direction == LoanDirection::Incoming
                    ? loan.parentClubName
                    : loan.loanClubName));
        row.insert(QStringLiteral("purpose"), toQString(loan.purpose));
        row.insert(QStringLiteral("stage"), loanStageLabel(loan.stage));
        row.insert(
            QStringLiteral("stageKey"),
            toQString(loanStageKey(loan.stage)));
        row.insert(
            QStringLiteral("stageColor"),
            loanStageColor(loan.stage));
        row.insert(
            QStringLiteral("termsSummary"),
            loanTermsSummary(loan.terms));
        row.insert(
            QStringLiteral("counterSummary"),
            loanTermsSummary(loan.counterTerms));
        row.insert(
            QStringLiteral("loanFee"),
            toQString(formatMoney(loan.terms.loanFeePence)));
        row.insert(
            QStringLiteral("loanFeePounds"),
            poundsValue(loan.terms.loanFeePence));
        row.insert(
            QStringLiteral("borrowingWagePercentage"),
            loan.terms.borrowingClubWagePercentage);
        row.insert(
            QStringLiteral("promisedRole"),
            toQString(loan.terms.promisedRole));
        row.insert(
            QStringLiteral("endDate"),
            shortDate(loan.terms.endDate));
        row.insert(
            QStringLiteral("endDateIso"),
            toQString(loan.terms.endDate.toIsoString()));
        row.insert(QStringLiteral("canRecallClause"), loan.terms.canRecall);
        row.insert(
            QStringLiteral("recallNoticeDays"),
            loan.terms.recallNoticeDays);
        row.insert(
            QStringLiteral("futureFeeType"),
            toQString(loan.terms.futureFeeType));
        row.insert(
            QStringLiteral("futureFee"),
            loan.terms.futureFeeType == "none"
                ? QStringLiteral("없음")
                : toQString(formatMoney(loan.terms.futureFeePence)));
        row.insert(
            QStringLiteral("futureFeePounds"),
            poundsValue(loan.terms.futureFeePence));
        row.insert(
            QStringLiteral("counterLoanFeePounds"),
            poundsValue(loan.counterTerms.loanFeePence));
        row.insert(
            QStringLiteral("counterWagePercentage"),
            loan.counterTerms.borrowingClubWagePercentage);
        row.insert(
            QStringLiteral("counterRole"),
            toQString(loan.counterTerms.promisedRole));
        row.insert(
            QStringLiteral("counterEndDateIso"),
            toQString(loan.counterTerms.endDate.toIsoString()));
        row.insert(
            QStringLiteral("counterCanRecall"),
            loan.counterTerms.canRecall);
        row.insert(
            QStringLiteral("counterRecallDays"),
            loan.counterTerms.recallNoticeDays);
        row.insert(
            QStringLiteral("counterFutureFeeType"),
            toQString(loan.counterTerms.futureFeeType));
        row.insert(
            QStringLiteral("counterFutureFeePounds"),
            poundsValue(loan.counterTerms.futureFeePence));
        row.insert(QStringLiteral("appearances"), loan.appearances);
        row.insert(QStringLiteral("starts"), loan.starts);
        row.insert(
            QStringLiteral("responseNote"),
            toQString(loan.responseNote));
        row.insert(
            QStringLiteral("canAcceptOffer"),
            loan.stage == LoanStage::DecisionPending);
        row.insert(
            QStringLiteral("canCounter"),
            loan.stage == LoanStage::DecisionPending
                || loan.stage == LoanStage::ClubCountered);
        row.insert(
            QStringLiteral("canAcceptCounter"),
            loan.stage == LoanStage::ClubCountered);
        row.insert(
            QStringLiteral("canReject"),
            loan.stage == LoanStage::DecisionPending
                || loan.stage == LoanStage::ClubCountered);
        row.insert(
            QStringLiteral("canRecall"),
            loan.direction == LoanDirection::Outgoing
                && loan.stage == LoanStage::Active
                && loan.terms.canRecall);
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::contractRenewals() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }
    result.reserve(
        static_cast<qsizetype>(state()->contractRenewals.size()));
    for (const auto& renewal : state()->contractRenewals) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(renewal.id));
        row.insert(
            QStringLiteral("playerId"),
            toQString(renewal.playerId));
        row.insert(QStringLiteral("name"), toQString(renewal.playerName));
        row.insert(
            QStringLiteral("requestedBy"),
            toQString(renewal.requestedBy));
        row.insert(
            QStringLiteral("rationale"),
            toQString(renewal.rationale));
        row.insert(
            QStringLiteral("currentEndDate"),
            shortDate(renewal.currentContractEndDate));
        row.insert(
            QStringLiteral("decisionDeadline"),
            shortDate(renewal.decisionDeadline));
        row.insert(
            QStringLiteral("stage"),
            contractRenewalStageLabel(renewal.stage));
        row.insert(
            QStringLiteral("stageKey"),
            toQString(contractRenewalStageKey(renewal.stage)));
        row.insert(
            QStringLiteral("stageColor"),
            contractRenewalStageColor(renewal.stage));
        row.insert(
            QStringLiteral("recommendedSummary"),
            contractTermsSummary(renewal.recommendedTerms));
        row.insert(
            QStringLiteral("offerSummary"),
            validPlayerContractTerms(renewal.offerTerms)
                ? contractTermsSummary(renewal.offerTerms)
                : QStringLiteral("아직 제안 없음"));
        row.insert(
            QStringLiteral("counterSummary"),
            validPlayerContractTerms(renewal.counterTerms)
                ? contractTermsSummary(renewal.counterTerms)
                : QString {});
        row.insert(
            QStringLiteral("responseNote"),
            toQString(renewal.responseNote));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::fixtures() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }
    result.reserve(static_cast<qsizetype>(state()->fixtures.size()));
    for (const auto& fixture : state()->fixtures) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), toQString(fixture.id));
        row.insert(QStringLiteral("date"), shortDate(fixture.date));
        row.insert(QStringLiteral("competition"), toQString(fixture.competition));
        row.insert(QStringLiteral("home"), toQString(fixture.homeClub));
        row.insert(QStringLiteral("away"), toQString(fixture.awayClub));
        row.insert(QStringLiteral("played"), fixture.played);
        row.insert(QStringLiteral("score"), fixture.played
            ? QStringLiteral("%1–%2").arg(fixture.homeGoals).arg(fixture.awayGoals)
            : QStringLiteral("—"));
        result.push_back(row);
    }
    return result;
}

QVariantList GameController::leagueStandings() const
{
    QVariantList result;
    if (state() == nullptr) {
        return result;
    }
    result.reserve(static_cast<qsizetype>(state()->leagueStandings.size()));
    for (const auto& row : state()->leagueStandings) {
        QVariantMap item;
        item.insert(QStringLiteral("position"), row.position);
        item.insert(QStringLiteral("competition"), toQString(row.competition));
        item.insert(QStringLiteral("clubId"), toQString(row.clubId));
        item.insert(QStringLiteral("clubName"), toQString(row.clubName));
        item.insert(QStringLiteral("played"), row.played);
        item.insert(QStringLiteral("won"), row.won);
        item.insert(QStringLiteral("drawn"), row.drawn);
        item.insert(QStringLiteral("lost"), row.lost);
        item.insert(QStringLiteral("goalsFor"), row.goalsFor);
        item.insert(QStringLiteral("goalsAgainst"), row.goalsAgainst);
        item.insert(QStringLiteral("goalDifference"), row.goalDifference);
        item.insert(QStringLiteral("points"), row.points);
        item.insert(QStringLiteral("isPlayerClub"), row.clubId == state()->club.id);
        result.push_back(item);
    }
    return result;
}

void GameController::advanceDays(const int dayCount)
{
    if (engine_ == nullptr || dayCount <= 0) {
        return;
    }
    const GameState rollbackState = engine_->state();
    engine_->advanceDays(dayCount);
    (void)persist(
        QStringLiteral("%1일 진행하고 자동 저장했습니다").arg(dayCount),
        &rollbackState,
        true);
}

void GameController::advanceToNextEvent()
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    engine_->advanceToNextEvent();
    (void)persist(
        QStringLiteral("다음 일정까지 진행하고 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::markInboxRead(const int inboxId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    engine_->markInboxRead(inboxId);
    (void)persist(QStringLiteral("메시지를 읽음 처리했습니다"), &rollbackState);
}

void GameController::markAllInboxRead()
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    engine_->markAllInboxRead();
    (void)persist(QStringLiteral("모든 메시지를 읽음 처리했습니다"), &rollbackState);
}

void GameController::setOwnerSquadOpinion(
    const QString& playerId,
    const QString& opinionType,
    const int priority,
    const QString& reason)
{
    if (engine_ == nullptr) {
        return;
    }

    const std::string playerIdValue = toStdString(playerId.trimmed());
    auto& mutableState = engine_->mutableState();
    const auto player = std::find_if(
        mutableState.players.begin(),
        mutableState.players.end(),
        [&playerIdValue](const Player& item) {
            return item.id == playerIdValue;
        });
    if (player == mutableState.players.end()) {
        setError(QStringLiteral("선수를 찾을 수 없습니다."));
        return;
    }

    const GameState rollbackState = engine_->state();
    const SquadPlanningOpinionType type =
        squadPlanningOpinionTypeFromKey(toStdString(opinionType.trimmed()));
    const std::string opinionId = "owner-player-" + playerIdValue;
    auto opinion = std::find_if(
        mutableState.squadPlanningOpinions.begin(),
        mutableState.squadPlanningOpinions.end(),
        [&opinionId, &playerIdValue](const SquadPlanningOpinion& item) {
            return item.id == opinionId
                || (item.source == SquadPlanningOpinionSource::Owner
                    && item.playerId.has_value()
                    && *item.playerId == playerIdValue);
        });
    if (opinion == mutableState.squadPlanningOpinions.end()) {
        SquadPlanningOpinion created;
        created.id = opinionId;
        created.createdDate = mutableState.currentDate;
        mutableState.squadPlanningOpinions.push_back(std::move(created));
        opinion = std::prev(mutableState.squadPlanningOpinions.end());
    }

    QString reasonValue = reason.trimmed();
    if (reasonValue.isEmpty()) {
        reasonValue = QStringLiteral("%1 판단").arg(
            squadPlanningOpinionTypeLabel(type));
    }

    opinion->seasonId = currentSquadPlanningSeasonId(mutableState);
    opinion->clubId = mutableState.club.id;
    opinion->playerId = playerIdValue;
    opinion->positionGroup = toStdString(
        squadDiagnosticGroupKey(player->position));
    opinion->source = SquadPlanningOpinionSource::Owner;
    opinion->type = type;
    opinion->priority = std::clamp(priority, 1, 5);
    opinion->reason = toStdString(reasonValue);
    opinion->targetWindow = "current_season";
    opinion->status = SquadPlanningOpinionStatus::Active;
    if (opinion->createdDate.year == 0) {
        opinion->createdDate = mutableState.currentDate;
    }
    opinion->updatedDate = mutableState.currentDate;
    opinion->dataConfidence = "player_entered_owner_opinion";

    recordOwnerAction(
        mutableState,
        "set_squad_opinion",
        "squad_planning",
        player->id,
        player->fullName,
        toStdString(
            QStringLiteral("%1: 구단주 의견 ‘%2’")
                .arg(toQString(player->fullName))
                .arg(squadPlanningOpinionTypeLabel(type))),
        ownerImpactForSquadOpinion(type, *player, mutableState.currentDate),
        "player_owner_squad_opinion");

    (void)persist(
        QStringLiteral("%1: 구단주 의견 ‘%2’을 저장했습니다")
            .arg(toQString(player->fullName))
            .arg(squadPlanningOpinionTypeLabel(type)),
        &rollbackState);
}

void GameController::clearOwnerSquadOpinion(const QString& playerId)
{
    if (engine_ == nullptr) {
        return;
    }

    const std::string playerIdValue = toStdString(playerId.trimmed());
    auto& mutableState = engine_->mutableState();
    const auto opinion = std::find_if(
        mutableState.squadPlanningOpinions.begin(),
        mutableState.squadPlanningOpinions.end(),
        [&playerIdValue](const SquadPlanningOpinion& item) {
            return item.source == SquadPlanningOpinionSource::Owner
                && item.playerId.has_value()
                && *item.playerId == playerIdValue
                && item.status == SquadPlanningOpinionStatus::Active;
        });
    if (opinion == mutableState.squadPlanningOpinions.end()) {
        setStatus(QStringLiteral("해제할 구단주 의견이 없습니다."));
        return;
    }

    const GameState rollbackState = engine_->state();
    opinion->status = SquadPlanningOpinionStatus::Dismissed;
    opinion->updatedDate = mutableState.currentDate;
    recordOwnerAction(
        mutableState,
        "clear_squad_opinion",
        "squad_planning",
        *opinion->playerId,
        {},
        "구단주 선수단 의견 해제",
        {},
        "player_owner_squad_opinion_cleared");

    (void)persist(QStringLiteral("구단주 선수단 의견을 해제했습니다"), &rollbackState);
}

void GameController::setOperatingMeetingCadence(const QString& cadence)
{
    if (engine_ == nullptr) {
        return;
    }

    const GameState rollbackState = engine_->state();
    auto& mutableState = engine_->mutableState();
    mutableState.operatingMeetingSettings.cadence =
        operatingMeetingCadenceFromKey(toStdString(cadence.trimmed()));
    mutableState.operatingMeetingSettings.nextMeetingDate =
        nextOperatingMeetingDateForCadence(
            mutableState.currentDate,
            mutableState.operatingMeetingSettings.cadence);
    mutableState.operatingMeetingSettings.updatedDate =
        mutableState.currentDate;
    mutableState.operatingMeetingSettings.dataConfidence =
        "player_configured_owner_ops_meeting";

    (void)persist(
        QStringLiteral("운영 회의 주기를 ‘%1’로 설정했습니다")
            .arg(operatingMeetingCadenceDisplay(
                mutableState.operatingMeetingSettings.cadence)),
        &rollbackState);
}

void GameController::callOperatingMeeting(
    const QString& agendaType,
    const bool autoAssign,
    const bool includeManager,
    const bool includeDirector,
    const bool includeFinance,
    const bool includeScouting,
    const bool includeMedical)
{
    if (engine_ == nullptr) {
        return;
    }

    const GameState rollbackState = engine_->state();
    auto& mutableState = engine_->mutableState();
    const OperatingMeetingAgendaType agenda =
        operatingMeetingAgendaTypeFromKey(toStdString(agendaType.trimmed()));
    const OperatingMeetingAttendeeMode attendeeMode = autoAssign
        ? OperatingMeetingAttendeeMode::Auto
        : OperatingMeetingAttendeeMode::Manual;
    auto attendees = makeOperatingMeetingAttendees(
        mutableState,
        agenda,
        autoAssign,
        includeManager,
        includeDirector,
        includeFinance,
        includeScouting,
        includeMedical);
    if (attendees.empty()) {
        addOperatingMeetingAttendee(
            attendees,
            "owner",
            mutableState.ownerName,
            autoAssign,
            "회의 소집자");
    }

    std::string compactDate = mutableState.currentDate.toIsoString();
    compactDate.erase(
        std::remove(compactDate.begin(), compactDate.end(), '-'),
        compactDate.end());
    const std::string meetingId = "operating-meeting-" + compactDate + "-"
        + std::to_string(mutableState.operatingMeetings.size() + 1);

    OperatingMeeting meeting;
    meeting.id = meetingId;
    meeting.meetingDate = mutableState.currentDate;
    meeting.agenda = agenda;
    meeting.status = OperatingMeetingStatus::Held;
    meeting.attendeeMode = attendeeMode;
    meeting.attendees = std::move(attendees);
    meeting.summary =
        toStdString(operatingMeetingSummary(
            mutableState,
            agenda,
            meeting.attendees));
    meeting.outcomeInboxEventKey = meetingId + "-report";
    meeting.createdDate = mutableState.currentDate;
    meeting.dataConfidence = "player_called_owner_ops_meeting";

    if (agenda == OperatingMeetingAgendaType::SquadPlanning) {
        applySquadPlanningMeetingOutputs(mutableState, meeting);
        meeting.summary +=
            " 감독·단장 포지션군 의견을 선수단 계획 기록에 반영했습니다.";
    }

    const int nextInboxId =
        mutableState.inbox.empty()
        ? 1
        : std::max_element(
            mutableState.inbox.begin(),
            mutableState.inbox.end(),
            [](const InboxItem& left, const InboxItem& right) {
                return left.id < right.id;
            })->id + 1;
    mutableState.inbox.push_back({
        nextInboxId,
        meeting.outcomeInboxEventKey,
        mutableState.currentDate,
        InboxCategory::Board,
        toStdString(
            QStringLiteral("운영 회의 결과: %1")
                .arg(operatingMeetingAgendaLabel(agenda))),
        meeting.summary,
        true,
        false
    });

    const auto hasAttendeeRole = [&meeting](const char* role) {
        return std::any_of(
            meeting.attendees.begin(),
            meeting.attendees.end(),
            [role](const OperatingMeetingAttendee& attendee) {
                return attendee.role == role;
            });
    };
    mutableState.operatingMeetingSettings.attendeeMode = attendeeMode;
    mutableState.operatingMeetingSettings.includeManager =
        hasAttendeeRole("manager");
    mutableState.operatingMeetingSettings.includeDirector =
        hasAttendeeRole("director");
    mutableState.operatingMeetingSettings.includeFinance =
        hasAttendeeRole("finance");
    mutableState.operatingMeetingSettings.includeScouting =
        hasAttendeeRole("scouting");
    mutableState.operatingMeetingSettings.includeMedical =
        hasAttendeeRole("medical");
    mutableState.operatingMeetingSettings.updatedDate =
        mutableState.currentDate;
    mutableState.operatingMeetingSettings.dataConfidence =
        "player_configured_owner_ops_meeting";
    mutableState.operatingMeetings.insert(
        mutableState.operatingMeetings.begin(),
        std::move(meeting));
    recordOwnerAction(
        mutableState,
        "call_operating_meeting",
        "owner_ops",
        mutableState.operatingMeetings.front().id,
        toStdString(operatingMeetingAgendaLabel(agenda)),
        mutableState.operatingMeetings.front().summary,
        ownerImpactForOperatingMeeting(
            agenda,
            mutableState.operatingMeetings.front().attendees),
        "player_called_owner_ops_meeting");

    (void)persist(
        QStringLiteral("운영 회의를 소집하고 결과 보고를 받은 편지함에 추가했습니다"),
        &rollbackState);
}

void GameController::approveTransfer(const QString& proposalId)
{
    if (engine_ == nullptr) {
        return;
    }

    const GameState rollbackState = engine_->state();
    const auto result = engine_->approveTransfer(toStdString(proposalId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::rejectTransfer(const QString& proposalId)
{
    if (engine_ == nullptr) {
        return;
    }

    const GameState rollbackState = engine_->state();
    const auto result = engine_->rejectTransfer(toStdString(proposalId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::startTransferPursuit(const QString& playerId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->startTransferPursuit(toStdString(playerId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::startDetailedTransferPursuit(
    const QString& playerId,
    const qint64 upfrontPounds,
    const qint64 installmentPounds,
    const int installmentCount,
    const qint64 appearanceBonusPounds,
    const int appearanceThreshold,
    const int sellOnPercentage)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(upfrontPounds)
        || !canConvertPounds(installmentPounds)
        || !canConvertPounds(appearanceBonusPounds)) {
        setError(QStringLiteral("이적료 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const TransferFeeTerms terms {
        toPence(upfrontPounds),
        toPence(installmentPounds),
        installmentCount,
        toPence(appearanceBonusPounds),
        appearanceThreshold,
        sellOnPercentage
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->startTransferPursuit(toStdString(playerId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptClubCounter(const QString& pursuitId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->acceptClubCounter(toStdString(pursuitId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::improveClubOffer(const QString& pursuitId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->improveClubOffer(toStdString(pursuitId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::submitDetailedClubOffer(
    const QString& pursuitId,
    const qint64 upfrontPounds,
    const qint64 installmentPounds,
    const int installmentCount,
    const qint64 appearanceBonusPounds,
    const int appearanceThreshold,
    const int sellOnPercentage)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(upfrontPounds)
        || !canConvertPounds(installmentPounds)
        || !canConvertPounds(appearanceBonusPounds)) {
        setError(QStringLiteral("이적료 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const TransferFeeTerms terms {
        toPence(upfrontPounds),
        toPence(installmentPounds),
        installmentCount,
        toPence(appearanceBonusPounds),
        appearanceThreshold,
        sellOnPercentage
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->submitClubOffer(toStdString(pursuitId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::submitPlayerContractOffer(
    const QString& pursuitId,
    const qint64 weeklyWagePounds,
    const int contractYears,
    const qint64 signingBonusPounds,
    const qint64 loyaltyBonusPounds,
    const qint64 appearanceFeePounds,
    const qint64 goalBonusPounds,
    const qint64 agentFeePounds,
    const qint64 releaseClausePounds,
    const QString& squadRole)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(weeklyWagePounds)
        || !canConvertPounds(signingBonusPounds)
        || !canConvertPounds(loyaltyBonusPounds)
        || !canConvertPounds(appearanceFeePounds)
        || !canConvertPounds(goalBonusPounds)
        || !canConvertPounds(agentFeePounds)
        || !canConvertPounds(releaseClausePounds)) {
        setError(QStringLiteral("선수 계약 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const PlayerContractTerms terms {
        toPence(weeklyWagePounds),
        contractYears,
        toPence(signingBonusPounds),
        toPence(loyaltyBonusPounds),
        toPence(appearanceFeePounds),
        toPence(goalBonusPounds),
        toPence(agentFeePounds),
        toPence(releaseClausePounds),
        toStdString(squadRole.trimmed())
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->submitPlayerContractOffer(toStdString(pursuitId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptPlayerCounter(const QString& pursuitId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->acceptPlayerCounter(toStdString(pursuitId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::withdrawTransferPursuit(const QString& pursuitId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->withdrawTransferPursuit(toStdString(pursuitId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptOutgoingTransfer(const QString& offerId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->acceptOutgoingTransfer(toStdString(offerId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::rejectOutgoingTransfer(const QString& offerId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->rejectOutgoingTransfer(toStdString(offerId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::submitOutgoingTransferCounter(
    const QString& offerId,
    const qint64 upfrontPounds,
    const qint64 installmentPounds,
    const int installmentCount,
    const qint64 appearanceBonusPounds,
    const int appearanceThreshold,
    const int sellOnPercentage)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(upfrontPounds)
        || !canConvertPounds(installmentPounds)
        || !canConvertPounds(appearanceBonusPounds)) {
        setError(QStringLiteral("판매 조건 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const TransferFeeTerms terms {
        toPence(upfrontPounds),
        toPence(installmentPounds),
        installmentCount,
        toPence(appearanceBonusPounds),
        appearanceThreshold,
        sellOnPercentage
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->submitOutgoingTransferCounter(toStdString(offerId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptOutgoingTransferCounter(const QString& offerId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->acceptOutgoingTransferCounter(toStdString(offerId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::startScoutingAssignment(
    const QString& playerId,
    const QString& scoutId,
    const QString& depth,
    const QString& focus)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->startScoutingAssignment(
        toStdString(playerId),
        toStdString(scoutId),
        scoutingDepthFromKey(toStdString(depth)),
        toStdString(focus));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::startTargetScoutingDirective(
    const QString& playerId,
    const QString& scoutId,
    const QString& depth,
    const QString& focus)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->startTargetScoutingDirective(
        toStdString(playerId),
        toStdString(scoutId),
        scoutingDepthFromKey(toStdString(depth)),
        toStdString(focus));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::startRangeScoutingDirective(
    const QString& scoutId,
    const QString& depth,
    const QString& focus,
    const QString& region,
    const QString& country,
    const QString& league,
    const QString& position,
    const QString& style,
    const int minAge,
    const int maxAge,
    const bool homegrownOnly,
    const qint64 maxBudgetPounds)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(maxBudgetPounds)) {
        setError(QStringLiteral("스카우팅 지시 예산 입력이 너무 큽니다."));
        emit stateChanged();
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->startRangeScoutingDirective(
        toStdString(scoutId),
        scoutingDepthFromKey(toStdString(depth)),
        toStdString(focus),
        toStdString(region.trimmed()),
        toStdString(country.trimmed()),
        toStdString(league.trimmed()),
        toStdString(position.trimmed()),
        toStdString(style.trimmed()),
        minAge,
        maxAge,
        homegrownOnly,
        toPence(maxBudgetPounds));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::cancelScoutingAssignment(const QString& assignmentId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->cancelScoutingAssignment(toStdString(assignmentId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::cancelScoutingDirective(const QString& directiveId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->cancelScoutingDirective(toStdString(directiveId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::startIncomingLoan(
    const QString& playerId,
    const qint64 loanFeePounds,
    const int borrowingWagePercentage,
    const QString& promisedRole,
    const QString& endDate,
    const bool canRecall,
    const int recallNoticeDays,
    const QString& futureFeeType,
    const qint64 futureFeePounds)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(loanFeePounds)
        || !canConvertPounds(futureFeePounds)) {
        setError(QStringLiteral("임대 금액 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const auto parsedEndDate =
        GameDate::fromIsoString(toStdString(endDate.trimmed()));
    if (!parsedEndDate.has_value()) {
        setError(QStringLiteral("임대 종료일을 YYYY-MM-DD 형식으로 입력해 주세요."));
        return;
    }
    const LoanTerms terms {
        toPence(loanFeePounds),
        borrowingWagePercentage,
        toStdString(promisedRole.trimmed()),
        *parsedEndDate,
        canRecall,
        recallNoticeDays,
        toStdString(futureFeeType.trimmed()),
        toPence(futureFeePounds),
        true,
        0,
        "loan_club"
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->startIncomingLoan(toStdString(playerId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptLoanOffer(const QString& loanId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->acceptLoanOffer(toStdString(loanId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::rejectLoanOffer(const QString& loanId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->rejectLoanOffer(toStdString(loanId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::submitLoanCounter(
    const QString& loanId,
    const qint64 loanFeePounds,
    const int borrowingWagePercentage,
    const QString& promisedRole,
    const QString& endDate,
    const bool canRecall,
    const int recallNoticeDays,
    const QString& futureFeeType,
    const qint64 futureFeePounds)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(loanFeePounds)
        || !canConvertPounds(futureFeePounds)) {
        setError(QStringLiteral("임대 금액 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const auto parsedEndDate =
        GameDate::fromIsoString(toStdString(endDate.trimmed()));
    if (!parsedEndDate.has_value()) {
        setError(QStringLiteral("임대 종료일을 YYYY-MM-DD 형식으로 입력해 주세요."));
        return;
    }
    const LoanTerms terms {
        toPence(loanFeePounds),
        borrowingWagePercentage,
        toStdString(promisedRole.trimmed()),
        *parsedEndDate,
        canRecall,
        recallNoticeDays,
        toStdString(futureFeeType.trimmed()),
        toPence(futureFeePounds),
        true,
        0,
        "loan_club"
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->submitLoanCounter(toStdString(loanId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptLoanCounter(const QString& loanId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->acceptLoanCounter(toStdString(loanId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::recallLoan(const QString& loanId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result = engine_->recallLoan(toStdString(loanId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::submitContractRenewal(
    const QString& playerId,
    const qint64 weeklyWagePounds,
    const int contractYears,
    const qint64 signingBonusPounds,
    const qint64 loyaltyBonusPounds,
    const qint64 appearanceFeePounds,
    const qint64 goalBonusPounds,
    const qint64 agentFeePounds,
    const qint64 releaseClausePounds,
    const QString& squadRole)
{
    if (engine_ == nullptr) {
        return;
    }
    if (!canConvertPounds(weeklyWagePounds)
        || !canConvertPounds(signingBonusPounds)
        || !canConvertPounds(loyaltyBonusPounds)
        || !canConvertPounds(appearanceFeePounds)
        || !canConvertPounds(goalBonusPounds)
        || !canConvertPounds(agentFeePounds)
        || !canConvertPounds(releaseClausePounds)) {
        setError(QStringLiteral("재계약 입력값이 허용 범위를 벗어났습니다."));
        return;
    }
    const PlayerContractTerms terms {
        toPence(weeklyWagePounds),
        contractYears,
        toPence(signingBonusPounds),
        toPence(loyaltyBonusPounds),
        toPence(appearanceFeePounds),
        toPence(goalBonusPounds),
        toPence(agentFeePounds),
        toPence(releaseClausePounds),
        toStdString(squadRole.trimmed())
    };
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->submitContractRenewal(toStdString(playerId), terms);
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::acceptContractRenewalCounter(
    const QString& renewalId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->acceptContractRenewalCounter(
            toStdString(renewalId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::confirmContractRenewal(
    const QString& renewalId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->confirmContractRenewal(toStdString(renewalId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

void GameController::withdrawContractRenewal(
    const QString& renewalId)
{
    if (engine_ == nullptr) {
        return;
    }
    const GameState rollbackState = engine_->state();
    const auto result =
        engine_->withdrawContractRenewal(toStdString(renewalId));
    if (!result.success) {
        setError(toQString(result.message));
        emit stateChanged();
        return;
    }
    (void)persist(
        toQString(result.message) + QStringLiteral(" 자동 저장했습니다"),
        &rollbackState,
        true);
}

const GameState* GameController::state() const noexcept
{
    return engine_ != nullptr ? &engine_->state() : nullptr;
}

qint64 GameController::currentPlayTimeSeconds() const noexcept
{
    if (engine_ == nullptr || !playTimer_.isValid()) {
        return committedPlayTimeSeconds_;
    }
    return committedPlayTimeSeconds_ + std::max<qint64>(0, playTimer_.elapsed() / 1'000);
}

bool GameController::activateCareer(
    std::optional<GameState> loadedState,
    const QString& careerId,
    const qint64 playTimeSeconds,
    const QString& successMessage)
{
    if (!loadedState.has_value() || careerId.isEmpty()) {
        setError(QStringLiteral("커리어 상태를 활성화할 수 없습니다."));
        return false;
    }

    engine_ = std::make_unique<SimulationEngine>(std::move(*loadedState));
    activeCareerId_ = careerId;
    committedPlayTimeSeconds_ = std::max<qint64>(0, playTimeSeconds);
    playTimer_.start();
    if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
        emit errorMessageChanged();
    }
    setStatus(successMessage);
    emit careerChanged();
    emit stateChanged();
    return true;
}

bool GameController::persist(
    const QString& successMessage,
    const GameState* rollbackState,
    const bool createAutosave)
{
    QString saveError;
    const qint64 totalPlayTime = currentPlayTimeSeconds();
    if (!saveManager_->saveCareer(
            activeCareerId_,
            engine_->state(),
            totalPlayTime,
            &saveError)) {
        if (rollbackState != nullptr) {
            engine_->mutableState() = *rollbackState;
        }
        setError(saveError);
        emit stateChanged();
        return false;
    }

    committedPlayTimeSeconds_ = totalPlayTime;
    playTimer_.restart();
    if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
        emit errorMessageChanged();
    }

    QString completedMessage = successMessage;
    if (createAutosave) {
        int autosaveSlot = 0;
        QString autosaveError;
        if (!saveManager_->createAutosave(
                activeCareerId_,
                engine_->state(),
                totalPlayTime,
                &autosaveSlot,
                &autosaveError)) {
            setError(
                QStringLiteral("본 세이브는 저장됐지만 자동 저장 복제본 생성에 실패했습니다: ")
                + autosaveError);
        } else {
            completedMessage += QStringLiteral(" · 자동 저장 %1/3").arg(autosaveSlot);
        }
    }

    setStatus(completedMessage);
    emit savesChanged();
    emit stateChanged();
    return true;
}

void GameController::setError(const QString& message)
{
    if (errorMessage_ == message) {
        return;
    }
    errorMessage_ = message;
    emit errorMessageChanged();
}

void GameController::setStatus(const QString& message)
{
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusMessageChanged();
}

} // namespace football_owner
