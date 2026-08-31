#include "core/game_state.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string_view>

namespace football_owner {
namespace {

bool isClosedPursuit(const TransferPursuitStage stage)
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

Money percentageBelow(const Money value, const int percentage)
{
    return std::max<Money>(
        0,
        value - value * static_cast<Money>(percentage) / 100);
}

Money percentageAbove(const Money value, const int percentage)
{
    return value + value * static_cast<Money>(percentage) / 100;
}

std::uint32_t stableHash(const std::string_view value)
{
    std::uint32_t hash = 2'166'136'261U;
    for (const unsigned char character : value) {
        hash ^= character;
        hash *= 16'777'619U;
    }
    return hash;
}

} // namespace

std::optional<int> ageOnDate(const Player& player, const GameDate& date)
{
    if (!player.birthDate.has_value()) {
        return std::nullopt;
    }

    int age = date.year - player.birthDate->year;
    if (date.month < player.birthDate->month
        || (date.month == player.birthDate->month && date.day < player.birthDate->day)) {
        --age;
    }
    return age;
}

std::string formatMoney(const Money pence)
{
    const bool negative = pence < 0;
    const long double pounds = std::abs(static_cast<long double>(pence)) / 100.0L;

    std::ostringstream stream;
    if (negative) {
        stream << '-';
    }
    stream << "£";

    if (pounds >= 1'000'000.0L) {
        stream << std::fixed << std::setprecision(2) << (pounds / 1'000'000.0L) << 'm';
    } else if (pounds >= 1'000.0L) {
        stream << std::fixed << std::setprecision(1) << (pounds / 1'000.0L) << 'k';
    } else {
        stream << std::fixed << std::setprecision(0) << pounds;
    }

    return stream.str();
}

std::string inboxCategoryKey(const InboxCategory category)
{
    switch (category) {
    case InboxCategory::Board:
        return "board";
    case InboxCategory::Football:
        return "football";
    case InboxCategory::Finance:
        return "finance";
    case InboxCategory::Match:
        return "match";
    case InboxCategory::System:
        return "system";
    }
    return "system";
}

InboxCategory inboxCategoryFromKey(const std::string& value)
{
    if (value == "board") {
        return InboxCategory::Board;
    }
    if (value == "football") {
        return InboxCategory::Football;
    }
    if (value == "finance") {
        return InboxCategory::Finance;
    }
    if (value == "match") {
        return InboxCategory::Match;
    }
    return InboxCategory::System;
}

std::string transferStatusKey(const TransferStatus status)
{
    switch (status) {
    case TransferStatus::Pending:
        return "pending";
    case TransferStatus::Approved:
        return "approved";
    case TransferStatus::Rejected:
        return "rejected";
    case TransferStatus::Completed:
        return "completed";
    case TransferStatus::Expired:
        return "expired";
    }
    return "pending";
}

TransferStatus transferStatusFromKey(const std::string& value)
{
    if (value == "approved") {
        return TransferStatus::Approved;
    }
    if (value == "rejected") {
        return TransferStatus::Rejected;
    }
    if (value == "completed") {
        return TransferStatus::Completed;
    }
    if (value == "expired") {
        return TransferStatus::Expired;
    }
    return TransferStatus::Pending;
}

std::string transferPursuitStageKey(const TransferPursuitStage stage)
{
    switch (stage) {
    case TransferPursuitStage::AwaitingClubResponse:
        return "awaiting_club_response";
    case TransferPursuitStage::ClubCountered:
        return "club_countered";
    case TransferPursuitStage::AwaitingPlayerTerms:
        return "awaiting_player_terms";
    case TransferPursuitStage::AwaitingPlayerResponse:
        return "awaiting_player_response";
    case TransferPursuitStage::PlayerCountered:
        return "player_countered";
    case TransferPursuitStage::AwaitingMedical:
        return "awaiting_medical";
    case TransferPursuitStage::AwaitingRegistration:
        return "awaiting_registration";
    case TransferPursuitStage::AwaitingOwnerApproval:
        return "awaiting_owner_approval";
    case TransferPursuitStage::AwaitingCompletion:
        return "awaiting_completion";
    case TransferPursuitStage::ClubRejected:
        return "club_rejected";
    case TransferPursuitStage::PlayerRejected:
        return "player_rejected";
    case TransferPursuitStage::MedicalFailed:
        return "medical_failed";
    case TransferPursuitStage::RegistrationFailed:
        return "registration_failed";
    case TransferPursuitStage::OwnerRejected:
        return "owner_rejected";
    case TransferPursuitStage::Withdrawn:
        return "withdrawn";
    case TransferPursuitStage::Completed:
        return "completed";
    case TransferPursuitStage::Expired:
        return "expired";
    }
    return "awaiting_club_response";
}

TransferPursuitStage transferPursuitStageFromKey(const std::string& value)
{
    if (value == "club_countered") {
        return TransferPursuitStage::ClubCountered;
    }
    if (value == "awaiting_player_terms") {
        return TransferPursuitStage::AwaitingPlayerTerms;
    }
    if (value == "awaiting_player_response") {
        return TransferPursuitStage::AwaitingPlayerResponse;
    }
    if (value == "player_countered") {
        return TransferPursuitStage::PlayerCountered;
    }
    if (value == "awaiting_medical") {
        return TransferPursuitStage::AwaitingMedical;
    }
    if (value == "awaiting_registration") {
        return TransferPursuitStage::AwaitingRegistration;
    }
    if (value == "awaiting_owner_approval") {
        return TransferPursuitStage::AwaitingOwnerApproval;
    }
    if (value == "awaiting_completion") {
        return TransferPursuitStage::AwaitingCompletion;
    }
    if (value == "club_rejected") {
        return TransferPursuitStage::ClubRejected;
    }
    if (value == "player_rejected") {
        return TransferPursuitStage::PlayerRejected;
    }
    if (value == "medical_failed") {
        return TransferPursuitStage::MedicalFailed;
    }
    if (value == "registration_failed") {
        return TransferPursuitStage::RegistrationFailed;
    }
    if (value == "owner_rejected") {
        return TransferPursuitStage::OwnerRejected;
    }
    if (value == "withdrawn") {
        return TransferPursuitStage::Withdrawn;
    }
    if (value == "completed") {
        return TransferPursuitStage::Completed;
    }
    if (value == "expired") {
        return TransferPursuitStage::Expired;
    }
    return TransferPursuitStage::AwaitingClubResponse;
}

std::string outgoingTransferStatusKey(const OutgoingTransferStatus status)
{
    switch (status) {
    case OutgoingTransferStatus::Pending:
        return "pending";
    case OutgoingTransferStatus::Accepted:
        return "accepted";
    case OutgoingTransferStatus::Rejected:
        return "rejected";
    case OutgoingTransferStatus::Completed:
        return "completed";
    case OutgoingTransferStatus::Expired:
        return "expired";
    }
    return "pending";
}

OutgoingTransferStatus outgoingTransferStatusFromKey(const std::string& value)
{
    if (value == "accepted") {
        return OutgoingTransferStatus::Accepted;
    }
    if (value == "rejected") {
        return OutgoingTransferStatus::Rejected;
    }
    if (value == "completed") {
        return OutgoingTransferStatus::Completed;
    }
    if (value == "expired") {
        return OutgoingTransferStatus::Expired;
    }
    return OutgoingTransferStatus::Pending;
}

std::string outgoingNegotiationStageKey(const OutgoingNegotiationStage stage)
{
    switch (stage) {
    case OutgoingNegotiationStage::DecisionPending:
        return "decision_pending";
    case OutgoingNegotiationStage::AwaitingBuyerResponse:
        return "awaiting_buyer_response";
    case OutgoingNegotiationStage::BuyerCountered:
        return "buyer_countered";
    case OutgoingNegotiationStage::AgreementReached:
        return "agreement_reached";
    case OutgoingNegotiationStage::Closed:
        return "closed";
    }
    return "decision_pending";
}

OutgoingNegotiationStage outgoingNegotiationStageFromKey(const std::string& value)
{
    if (value == "awaiting_buyer_response") {
        return OutgoingNegotiationStage::AwaitingBuyerResponse;
    }
    if (value == "buyer_countered") {
        return OutgoingNegotiationStage::BuyerCountered;
    }
    if (value == "agreement_reached") {
        return OutgoingNegotiationStage::AgreementReached;
    }
    if (value == "closed") {
        return OutgoingNegotiationStage::Closed;
    }
    return OutgoingNegotiationStage::DecisionPending;
}

std::string loanDirectionKey(const LoanDirection direction)
{
    switch (direction) {
    case LoanDirection::Incoming:
        return "incoming";
    case LoanDirection::Outgoing:
        return "outgoing";
    }
    return "outgoing";
}

LoanDirection loanDirectionFromKey(const std::string& value)
{
    return value == "incoming"
        ? LoanDirection::Incoming
        : LoanDirection::Outgoing;
}

std::string loanStageKey(const LoanStage stage)
{
    switch (stage) {
    case LoanStage::DecisionPending:
        return "decision_pending";
    case LoanStage::AwaitingClubResponse:
        return "awaiting_club_response";
    case LoanStage::ClubCountered:
        return "club_countered";
    case LoanStage::AwaitingStart:
        return "awaiting_start";
    case LoanStage::Active:
        return "active";
    case LoanStage::RecallPending:
        return "recall_pending";
    case LoanStage::Recalled:
        return "recalled";
    case LoanStage::Completed:
        return "completed";
    case LoanStage::Rejected:
        return "rejected";
    case LoanStage::Withdrawn:
        return "withdrawn";
    case LoanStage::Expired:
        return "expired";
    }
    return "decision_pending";
}

LoanStage loanStageFromKey(const std::string& value)
{
    if (value == "awaiting_club_response") {
        return LoanStage::AwaitingClubResponse;
    }
    if (value == "club_countered") {
        return LoanStage::ClubCountered;
    }
    if (value == "awaiting_start") {
        return LoanStage::AwaitingStart;
    }
    if (value == "active") {
        return LoanStage::Active;
    }
    if (value == "recall_pending") {
        return LoanStage::RecallPending;
    }
    if (value == "recalled") {
        return LoanStage::Recalled;
    }
    if (value == "completed") {
        return LoanStage::Completed;
    }
    if (value == "rejected") {
        return LoanStage::Rejected;
    }
    if (value == "withdrawn") {
        return LoanStage::Withdrawn;
    }
    if (value == "expired") {
        return LoanStage::Expired;
    }
    return LoanStage::DecisionPending;
}

std::string contractRenewalStageKey(const ContractRenewalStage stage)
{
    switch (stage) {
    case ContractRenewalStage::Recommended:
        return "recommended";
    case ContractRenewalStage::AwaitingPlayerResponse:
        return "awaiting_player_response";
    case ContractRenewalStage::PlayerCountered:
        return "player_countered";
    case ContractRenewalStage::AgreementReached:
        return "agreement_reached";
    case ContractRenewalStage::Completed:
        return "completed";
    case ContractRenewalStage::Rejected:
        return "rejected";
    case ContractRenewalStage::Withdrawn:
        return "withdrawn";
    case ContractRenewalStage::Expired:
        return "expired";
    }
    return "recommended";
}

ContractRenewalStage contractRenewalStageFromKey(const std::string& value)
{
    if (value == "awaiting_player_response") {
        return ContractRenewalStage::AwaitingPlayerResponse;
    }
    if (value == "player_countered") {
        return ContractRenewalStage::PlayerCountered;
    }
    if (value == "agreement_reached") {
        return ContractRenewalStage::AgreementReached;
    }
    if (value == "completed") {
        return ContractRenewalStage::Completed;
    }
    if (value == "rejected") {
        return ContractRenewalStage::Rejected;
    }
    if (value == "withdrawn") {
        return ContractRenewalStage::Withdrawn;
    }
    if (value == "expired") {
        return ContractRenewalStage::Expired;
    }
    return ContractRenewalStage::Recommended;
}

std::string transferPaymentDirectionKey(const TransferPaymentDirection direction)
{
    switch (direction) {
    case TransferPaymentDirection::Payable:
        return "payable";
    case TransferPaymentDirection::Receivable:
        return "receivable";
    }
    return "payable";
}

TransferPaymentDirection transferPaymentDirectionFromKey(const std::string& value)
{
    return value == "receivable"
        ? TransferPaymentDirection::Receivable
        : TransferPaymentDirection::Payable;
}

std::string transferPaymentStatusKey(const TransferPaymentStatus status)
{
    switch (status) {
    case TransferPaymentStatus::Scheduled:
        return "scheduled";
    case TransferPaymentStatus::Settled:
        return "settled";
    case TransferPaymentStatus::Overdue:
        return "overdue";
    case TransferPaymentStatus::Cancelled:
        return "cancelled";
    }
    return "scheduled";
}

TransferPaymentStatus transferPaymentStatusFromKey(const std::string& value)
{
    if (value == "settled") {
        return TransferPaymentStatus::Settled;
    }
    if (value == "overdue") {
        return TransferPaymentStatus::Overdue;
    }
    if (value == "cancelled") {
        return TransferPaymentStatus::Cancelled;
    }
    return TransferPaymentStatus::Scheduled;
}

std::string aiMarketDealTypeKey(const AiMarketDealType type)
{
    switch (type) {
    case AiMarketDealType::Transfer:
        return "transfer";
    case AiMarketDealType::Loan:
        return "loan";
    case AiMarketDealType::Renewal:
        return "renewal";
    case AiMarketDealType::Buyout:
        return "buyout";
    case AiMarketDealType::LoanOption:
        return "loan_option";
    case AiMarketDealType::LoanObligation:
        return "loan_obligation";
    }
    return "transfer";
}

AiMarketDealType aiMarketDealTypeFromKey(const std::string& value)
{
    if (value == "loan") {
        return AiMarketDealType::Loan;
    }
    if (value == "renewal") {
        return AiMarketDealType::Renewal;
    }
    if (value == "buyout") {
        return AiMarketDealType::Buyout;
    }
    if (value == "loan_option") {
        return AiMarketDealType::LoanOption;
    }
    if (value == "loan_obligation") {
        return AiMarketDealType::LoanObligation;
    }
    return AiMarketDealType::Transfer;
}

std::string aiMarketDealStageKey(const AiMarketDealStage stage)
{
    switch (stage) {
    case AiMarketDealStage::Proposed:
        return "proposed";
    case AiMarketDealStage::AwaitingCompletion:
        return "awaiting_completion";
    case AiMarketDealStage::Completed:
        return "completed";
    case AiMarketDealStage::Rejected:
        return "rejected";
    case AiMarketDealStage::Expired:
        return "expired";
    }
    return "proposed";
}

AiMarketDealStage aiMarketDealStageFromKey(const std::string& value)
{
    if (value == "awaiting_completion") {
        return AiMarketDealStage::AwaitingCompletion;
    }
    if (value == "completed") {
        return AiMarketDealStage::Completed;
    }
    if (value == "rejected") {
        return AiMarketDealStage::Rejected;
    }
    if (value == "expired") {
        return AiMarketDealStage::Expired;
    }
    return AiMarketDealStage::Proposed;
}

std::string squadPlanningOpinionSourceKey(
    const SquadPlanningOpinionSource source)
{
    switch (source) {
    case SquadPlanningOpinionSource::Owner:
        return "owner";
    case SquadPlanningOpinionSource::Manager:
        return "manager";
    case SquadPlanningOpinionSource::Director:
        return "director";
    }
    return "owner";
}

SquadPlanningOpinionSource squadPlanningOpinionSourceFromKey(
    const std::string& value)
{
    if (value == "manager") {
        return SquadPlanningOpinionSource::Manager;
    }
    if (value == "director") {
        return SquadPlanningOpinionSource::Director;
    }
    return SquadPlanningOpinionSource::Owner;
}

std::string squadPlanningOpinionTypeKey(
    const SquadPlanningOpinionType type)
{
    switch (type) {
    case SquadPlanningOpinionType::Sale:
        return "sale";
    case SquadPlanningOpinionType::Release:
        return "release";
    case SquadPlanningOpinionType::Reinforce:
        return "reinforce";
    case SquadPlanningOpinionType::Loan:
        return "loan";
    case SquadPlanningOpinionType::Keep:
        return "keep";
    case SquadPlanningOpinionType::Monitor:
        return "monitor";
    }
    return "monitor";
}

SquadPlanningOpinionType squadPlanningOpinionTypeFromKey(
    const std::string& value)
{
    if (value == "sale") {
        return SquadPlanningOpinionType::Sale;
    }
    if (value == "release") {
        return SquadPlanningOpinionType::Release;
    }
    if (value == "reinforce") {
        return SquadPlanningOpinionType::Reinforce;
    }
    if (value == "loan") {
        return SquadPlanningOpinionType::Loan;
    }
    if (value == "keep") {
        return SquadPlanningOpinionType::Keep;
    }
    return SquadPlanningOpinionType::Monitor;
}

std::string squadPlanningOpinionStatusKey(
    const SquadPlanningOpinionStatus status)
{
    switch (status) {
    case SquadPlanningOpinionStatus::Active:
        return "active";
    case SquadPlanningOpinionStatus::Resolved:
        return "resolved";
    case SquadPlanningOpinionStatus::Dismissed:
        return "dismissed";
    case SquadPlanningOpinionStatus::Superseded:
        return "superseded";
    }
    return "active";
}

SquadPlanningOpinionStatus squadPlanningOpinionStatusFromKey(
    const std::string& value)
{
    if (value == "resolved") {
        return SquadPlanningOpinionStatus::Resolved;
    }
    if (value == "dismissed") {
        return SquadPlanningOpinionStatus::Dismissed;
    }
    if (value == "superseded") {
        return SquadPlanningOpinionStatus::Superseded;
    }
    return SquadPlanningOpinionStatus::Active;
}

std::string operatingMeetingCadenceKey(
    const OperatingMeetingCadence cadence)
{
    switch (cadence) {
    case OperatingMeetingCadence::Off:
        return "off";
    case OperatingMeetingCadence::UrgentOnly:
        return "urgent_only";
    case OperatingMeetingCadence::Monthly:
        return "monthly";
    case OperatingMeetingCadence::Quarterly:
        return "quarterly";
    case OperatingMeetingCadence::TransferWindows:
        return "transfer_windows";
    case OperatingMeetingCadence::SeasonMilestones:
        return "season_milestones";
    case OperatingMeetingCadence::Custom:
        return "custom";
    }
    return "off";
}

OperatingMeetingCadence operatingMeetingCadenceFromKey(
    const std::string& value)
{
    if (value == "urgent_only") {
        return OperatingMeetingCadence::UrgentOnly;
    }
    if (value == "monthly") {
        return OperatingMeetingCadence::Monthly;
    }
    if (value == "quarterly") {
        return OperatingMeetingCadence::Quarterly;
    }
    if (value == "transfer_windows") {
        return OperatingMeetingCadence::TransferWindows;
    }
    if (value == "season_milestones") {
        return OperatingMeetingCadence::SeasonMilestones;
    }
    if (value == "custom") {
        return OperatingMeetingCadence::Custom;
    }
    return OperatingMeetingCadence::Off;
}

std::string operatingMeetingAgendaTypeKey(
    const OperatingMeetingAgendaType agenda)
{
    switch (agenda) {
    case OperatingMeetingAgendaType::General:
        return "general";
    case OperatingMeetingAgendaType::SquadPlanning:
        return "squad_planning";
    case OperatingMeetingAgendaType::TransferMarket:
        return "transfer_market";
    case OperatingMeetingAgendaType::Finance:
        return "finance";
    case OperatingMeetingAgendaType::Staff:
        return "staff";
    case OperatingMeetingAgendaType::Scouting:
        return "scouting";
    case OperatingMeetingAgendaType::Crisis:
        return "crisis";
    }
    return "general";
}

OperatingMeetingAgendaType operatingMeetingAgendaTypeFromKey(
    const std::string& value)
{
    if (value == "squad_planning") {
        return OperatingMeetingAgendaType::SquadPlanning;
    }
    if (value == "transfer_market") {
        return OperatingMeetingAgendaType::TransferMarket;
    }
    if (value == "finance") {
        return OperatingMeetingAgendaType::Finance;
    }
    if (value == "staff") {
        return OperatingMeetingAgendaType::Staff;
    }
    if (value == "scouting") {
        return OperatingMeetingAgendaType::Scouting;
    }
    if (value == "crisis") {
        return OperatingMeetingAgendaType::Crisis;
    }
    return OperatingMeetingAgendaType::General;
}

std::string operatingMeetingAttendeeModeKey(
    const OperatingMeetingAttendeeMode mode)
{
    switch (mode) {
    case OperatingMeetingAttendeeMode::Auto:
        return "auto";
    case OperatingMeetingAttendeeMode::Manual:
        return "manual";
    }
    return "auto";
}

OperatingMeetingAttendeeMode operatingMeetingAttendeeModeFromKey(
    const std::string& value)
{
    return value == "manual"
        ? OperatingMeetingAttendeeMode::Manual
        : OperatingMeetingAttendeeMode::Auto;
}

std::string operatingMeetingStatusKey(
    const OperatingMeetingStatus status)
{
    switch (status) {
    case OperatingMeetingStatus::Scheduled:
        return "scheduled";
    case OperatingMeetingStatus::Held:
        return "held";
    case OperatingMeetingStatus::Cancelled:
        return "cancelled";
    }
    return "held";
}

OperatingMeetingStatus operatingMeetingStatusFromKey(
    const std::string& value)
{
    if (value == "scheduled") {
        return OperatingMeetingStatus::Scheduled;
    }
    if (value == "cancelled") {
        return OperatingMeetingStatus::Cancelled;
    }
    return OperatingMeetingStatus::Held;
}

std::string scoutingAssignmentStatusKey(const ScoutingAssignmentStatus status)
{
    switch (status) {
    case ScoutingAssignmentStatus::Active:
        return "active";
    case ScoutingAssignmentStatus::Completed:
        return "completed";
    case ScoutingAssignmentStatus::Cancelled:
        return "cancelled";
    }
    return "active";
}

ScoutingAssignmentStatus scoutingAssignmentStatusFromKey(
    const std::string& value)
{
    if (value == "completed") {
        return ScoutingAssignmentStatus::Completed;
    }
    if (value == "cancelled") {
        return ScoutingAssignmentStatus::Cancelled;
    }
    return ScoutingAssignmentStatus::Active;
}

std::string scoutingDepthKey(const ScoutingDepth depth)
{
    switch (depth) {
    case ScoutingDepth::Quick:
        return "quick";
    case ScoutingDepth::Standard:
        return "standard";
    case ScoutingDepth::Comprehensive:
        return "comprehensive";
    }
    return "standard";
}

ScoutingDepth scoutingDepthFromKey(const std::string& value)
{
    if (value == "quick") {
        return ScoutingDepth::Quick;
    }
    if (value == "comprehensive") {
        return ScoutingDepth::Comprehensive;
    }
    return ScoutingDepth::Standard;
}

std::string scoutingDirectiveTypeKey(const ScoutingDirectiveType type)
{
    switch (type) {
    case ScoutingDirectiveType::Range:
        return "range";
    case ScoutingDirectiveType::Target:
        return "target";
    }
    return "range";
}

ScoutingDirectiveType scoutingDirectiveTypeFromKey(const std::string& value)
{
    return value == "target"
        ? ScoutingDirectiveType::Target
        : ScoutingDirectiveType::Range;
}

std::string scoutingDirectiveStatusKey(const ScoutingDirectiveStatus status)
{
    switch (status) {
    case ScoutingDirectiveStatus::Active:
        return "active";
    case ScoutingDirectiveStatus::Completed:
        return "completed";
    case ScoutingDirectiveStatus::Cancelled:
        return "cancelled";
    }
    return "active";
}

ScoutingDirectiveStatus scoutingDirectiveStatusFromKey(
    const std::string& value)
{
    if (value == "completed") {
        return ScoutingDirectiveStatus::Completed;
    }
    if (value == "cancelled") {
        return ScoutingDirectiveStatus::Cancelled;
    }
    return ScoutingDirectiveStatus::Active;
}

int scoutingDurationDays(const ScoutingDepth depth)
{
    switch (depth) {
    case ScoutingDepth::Quick:
        return 3;
    case ScoutingDepth::Standard:
        return 7;
    case ScoutingDepth::Comprehensive:
        return 14;
    }
    return 7;
}

int scoutingKnowledgeGain(const ScoutingDepth depth)
{
    switch (depth) {
    case ScoutingDepth::Quick:
        return 20;
    case ScoutingDepth::Standard:
        return 45;
    case ScoutingDepth::Comprehensive:
        return 70;
    }
    return 45;
}

std::string scoutingPositionGroup(const std::string& position)
{
    if (position.find("GK") != std::string::npos) {
        return "GK";
    }
    if (position.find("CB") != std::string::npos
        || position.find("LB") != std::string::npos
        || position.find("RB") != std::string::npos
        || position.find("DF") != std::string::npos) {
        return "DEF";
    }
    if (position.find("CM") != std::string::npos
        || position.find("DM") != std::string::npos
        || position.find("AM") != std::string::npos
        || position.find("LM") != std::string::npos
        || position.find("RM") != std::string::npos
        || position.find("MF") != std::string::npos) {
        return "MID";
    }
    if (position.find("ST") != std::string::npos
        || position.find("SS") != std::string::npos
        || position.find("FW") != std::string::npos
        || position.find("LW") != std::string::npos
        || position.find("RW") != std::string::npos) {
        return "ATT";
    }
    return "OTHER";
}

std::string scoutingAgeBand(const Player& player, const GameDate& date)
{
    const auto age = ageOnDate(player, date);
    if (!age.has_value()) {
        return "unknown";
    }
    if (*age <= 21) {
        return "u21";
    }
    if (*age <= 25) {
        return "u25";
    }
    if (*age >= 30) {
        return "veteran";
    }
    return "prime";
}

int scoutFamiliarityLevel(
    const GameState& state,
    const std::string& scoutId,
    const std::string& category,
    const std::string& key)
{
    const auto familiarity = std::find_if(
        state.scoutFamiliarities.begin(),
        state.scoutFamiliarities.end(),
        [&scoutId, &category, &key](const ScoutFamiliarity& item) {
            return item.scoutId == scoutId
                && item.category == category
                && item.key == key;
        });
    return familiarity == state.scoutFamiliarities.end()
        ? 0
        : familiarity->level;
}

void recordScoutFamiliarity(
    GameState& state,
    const std::string& scoutId,
    const std::string& category,
    const std::string& key,
    const int delta,
    const GameDate& updatedDate,
    const std::string& dataConfidence)
{
    if (scoutId.empty() || category.empty() || key.empty()) {
        return;
    }

    auto familiarity = std::find_if(
        state.scoutFamiliarities.begin(),
        state.scoutFamiliarities.end(),
        [&scoutId, &category, &key](const ScoutFamiliarity& item) {
            return item.scoutId == scoutId
                && item.category == category
                && item.key == key;
        });
    if (familiarity == state.scoutFamiliarities.end()) {
        state.scoutFamiliarities.push_back({
            scoutId,
            category,
            key,
            std::clamp(delta, 0, 100),
            updatedDate,
            dataConfidence
        });
        return;
    }

    familiarity->level = std::clamp(familiarity->level + delta, 0, 100);
    familiarity->updatedDate = updatedDate;
    familiarity->dataConfidence = dataConfidence;
}

void ensureDefaultScoutFamiliarities(GameState& state)
{
    const auto hasFamiliarity = [&state](
                                    const std::string& scoutId,
                                    const std::string& category,
                                    const std::string& key) {
        return std::any_of(
            state.scoutFamiliarities.begin(),
            state.scoutFamiliarities.end(),
            [&scoutId, &category, &key](const ScoutFamiliarity& item) {
                return item.scoutId == scoutId
                    && item.category == category
                    && item.key == key;
            });
    };
    const auto addDefault = [&state, &hasFamiliarity](
                                const std::string& scoutId,
                                const std::string& category,
                                const std::string& key,
                                const int level) {
        if (hasFamiliarity(scoutId, category, key)) {
            auto familiarity = std::find_if(
                state.scoutFamiliarities.begin(),
                state.scoutFamiliarities.end(),
                [&scoutId, &category, &key](const ScoutFamiliarity& item) {
                    return item.scoutId == scoutId
                        && item.category == category
                        && item.key == key;
                });
            if (familiarity != state.scoutFamiliarities.end()) {
                familiarity->level = std::max(
                    familiarity->level,
                    std::clamp(level, 0, 100));
            }
            return;
        }
        state.scoutFamiliarities.push_back({
            scoutId,
            category,
            key,
            std::clamp(level, 0, 100),
            state.currentDate,
            "initial_scout_profile_specialism_gameplay_estimate"
        });
    };

    for (const auto& scout : state.scouts) {
        addDefault(scout.id, "region", "United Kingdom & Ireland", 35);
        addDefault(scout.id, "market", "domestic", 35);
        addDefault(scout.id, "position_group", "ATT", 30);

        if (scout.id == "les-kershaw") {
            addDefault(scout.id, "country", "England", 72);
            addDefault(scout.id, "region", "England", 70);
            addDefault(scout.id, "market", "domestic", 66);
            addDefault(scout.id, "age_band", "u21", 58);
            addDefault(scout.id, "position_group", "ATT", 48);
        } else if (scout.id == "alex-ferguson-observation") {
            addDefault(scout.id, "country", "England", 68);
            addDefault(scout.id, "country", "Republic of Ireland", 55);
            addDefault(scout.id, "region", "United Kingdom & Ireland", 72);
            addDefault(scout.id, "market", "domestic", 70);
            addDefault(scout.id, "position_group", "ATT", 62);
            addDefault(scout.id, "position_group", "MID", 54);
        }
    }
}

std::string ownerReputationHeadline(const int score)
{
    if (score >= 80) {
        return "신뢰받는 야심가";
    }
    if (score >= 65) {
        return "안정적인 구단주";
    }
    if (score >= 45) {
        return "평가 형성 중";
    }
    if (score >= 30) {
        return "의심받는 구단주";
    }
    return "불신받는 구단주";
}

std::string fanMoodKey(const FanTendencyState& fanState)
{
    if (fanState.trustScore >= 70 && fanState.skepticismScore <= 45) {
        return "backing_owner";
    }
    if (fanState.trustScore >= 55) {
        return "cautiously_supportive";
    }
    if (fanState.trustScore <= 30 || fanState.skepticismScore >= 75) {
        return "hostile";
    }
    if (fanState.trustScore <= 42 || fanState.skepticismScore >= 62) {
        return "skeptical";
    }
    return "cautiously_optimistic";
}

std::string fanTrustLabel(const int score)
{
    if (score >= 75) {
        return "높은 신뢰";
    }
    if (score >= 60) {
        return "조심스러운 지지";
    }
    if (score >= 45) {
        return "관망";
    }
    if (score >= 30) {
        return "불안";
    }
    return "불신";
}

void ensureDefaultOwnerReputation(GameState& state)
{
    if (state.ownerReputation.updatedDate.year == 0) {
        state.ownerReputation.updatedDate = state.currentDate;
    }
    state.ownerReputation.overallScore =
        std::clamp(state.ownerReputation.overallScore, 0, 100);
    state.ownerReputation.sportingAmbition =
        std::clamp(state.ownerReputation.sportingAmbition, 0, 100);
    state.ownerReputation.financialPrudence =
        std::clamp(state.ownerReputation.financialPrudence, 0, 100);
    state.ownerReputation.squadLoyalty =
        std::clamp(state.ownerReputation.squadLoyalty, 0, 100);
    state.ownerReputation.autonomyRespect =
        std::clamp(state.ownerReputation.autonomyRespect, 0, 100);
    state.ownerReputation.headline =
        ownerReputationHeadline(state.ownerReputation.overallScore);
    if (state.fanTendency.updatedDate.year == 0) {
        state.fanTendency.updatedDate = state.currentDate;
    }
    state.fanTendency.trustScore =
        std::clamp(state.fanTendency.trustScore, 0, 100);
    state.fanTendency.patienceScore =
        std::clamp(state.fanTendency.patienceScore, 0, 100);
    state.fanTendency.skepticismScore =
        std::clamp(state.fanTendency.skepticismScore, 0, 100);
    state.fanTendency.starSigningPreference =
        std::clamp(state.fanTendency.starSigningPreference, 0, 100);
    state.fanTendency.homegrownPreference =
        std::clamp(state.fanTendency.homegrownPreference, 0, 100);
    state.fanTendency.youthDevelopmentPreference =
        std::clamp(state.fanTendency.youthDevelopmentPreference, 0, 100);
    state.fanTendency.balancedSquadPreference =
        std::clamp(state.fanTendency.balancedSquadPreference, 0, 100);
    state.fanTendency.mood = fanMoodKey(state.fanTendency);
}

void recordOwnerAction(
    GameState& state,
    const std::string& actionType,
    const std::string& category,
    const std::string& subjectId,
    const std::string& subjectName,
    const std::string& summary,
    const OwnerActionImpact& impact,
    const std::string& dataConfidence)
{
    ensureDefaultOwnerReputation(state);

    std::string compactDate = state.currentDate.toIsoString();
    compactDate.erase(
        std::remove(compactDate.begin(), compactDate.end(), '-'),
        compactDate.end());
    const std::string id = "owner-action-" + compactDate + "-"
        + std::to_string(state.ownerActionLog.size() + 1);

    state.ownerActionLog.push_back({
        id,
        state.currentDate,
        actionType,
        category,
        subjectId,
        subjectName,
        summary,
        impact.reputationDelta,
        impact.fanTrustDelta,
        impact.ambitionDelta,
        impact.prudenceDelta,
        impact.loyaltyDelta,
        impact.autonomyDelta,
        impact.starPreferenceDelta,
        impact.homegrownPreferenceDelta,
        impact.youthPreferenceDelta,
        impact.balancedPreferenceDelta,
        impact.skepticismDelta,
        dataConfidence
    });

    state.ownerReputation.overallScore =
        std::clamp(state.ownerReputation.overallScore + impact.reputationDelta, 0, 100);
    state.ownerReputation.sportingAmbition =
        std::clamp(state.ownerReputation.sportingAmbition + impact.ambitionDelta, 0, 100);
    state.ownerReputation.financialPrudence =
        std::clamp(state.ownerReputation.financialPrudence + impact.prudenceDelta, 0, 100);
    state.ownerReputation.squadLoyalty =
        std::clamp(state.ownerReputation.squadLoyalty + impact.loyaltyDelta, 0, 100);
    state.ownerReputation.autonomyRespect =
        std::clamp(state.ownerReputation.autonomyRespect + impact.autonomyDelta, 0, 100);
    state.ownerReputation.headline =
        ownerReputationHeadline(state.ownerReputation.overallScore);
    state.ownerReputation.updatedDate = state.currentDate;
    state.ownerReputation.dataConfidence = "gameplay_owner_reputation_v1";

    state.fanTendency.trustScore =
        std::clamp(state.fanTendency.trustScore + impact.fanTrustDelta, 0, 100);
    state.fanTendency.patienceScore = std::clamp(
        state.fanTendency.patienceScore
            + std::max(0, impact.fanTrustDelta) / 2
            - std::max(0, -impact.fanTrustDelta) / 2,
        0,
        100);
    state.fanTendency.skepticismScore = std::clamp(
        state.fanTendency.skepticismScore
            + impact.skepticismDelta
            - std::max(0, impact.fanTrustDelta) / 2,
        0,
        100);
    state.fanTendency.starSigningPreference = std::clamp(
        state.fanTendency.starSigningPreference + impact.starPreferenceDelta,
        0,
        100);
    state.fanTendency.homegrownPreference = std::clamp(
        state.fanTendency.homegrownPreference + impact.homegrownPreferenceDelta,
        0,
        100);
    state.fanTendency.youthDevelopmentPreference = std::clamp(
        state.fanTendency.youthDevelopmentPreference + impact.youthPreferenceDelta,
        0,
        100);
    state.fanTendency.balancedSquadPreference = std::clamp(
        state.fanTendency.balancedSquadPreference + impact.balancedPreferenceDelta,
        0,
        100);
    state.fanTendency.mood = fanMoodKey(state.fanTendency);
    state.fanTendency.updatedDate = state.currentDate;
    state.fanTendency.dataConfidence = "gameplay_fan_tendency_v1";
}

std::string recruitmentPlanStatusKey(const RecruitmentPlanStatus status)
{
    switch (status) {
    case RecruitmentPlanStatus::Active:
        return "active";
    case RecruitmentPlanStatus::Achieved:
        return "achieved";
    case RecruitmentPlanStatus::Acceptable:
        return "acceptable";
    case RecruitmentPlanStatus::UnderDelivered:
        return "under_delivered";
    case RecruitmentPlanStatus::Failed:
        return "failed";
    }
    return "active";
}

RecruitmentPlanStatus recruitmentPlanStatusFromKey(const std::string& value)
{
    if (value == "achieved") {
        return RecruitmentPlanStatus::Achieved;
    }
    if (value == "acceptable") {
        return RecruitmentPlanStatus::Acceptable;
    }
    if (value == "under_delivered") {
        return RecruitmentPlanStatus::UnderDelivered;
    }
    if (value == "failed") {
        return RecruitmentPlanStatus::Failed;
    }
    return RecruitmentPlanStatus::Active;
}

TransferFeeTerms effectiveFeeTerms(
    const Money legacyFeePence,
    const TransferFeeTerms& terms)
{
    TransferFeeTerms result = terms;
    if (guaranteedTransferFee(result) == 0 && legacyFeePence > 0) {
        result.upfrontFeePence = legacyFeePence;
        result.installmentTotalPence = 0;
        result.installmentCount = 0;
    }
    return result;
}

PlayerContractTerms effectiveContractTerms(
    const Money legacyWeeklyWagePence,
    const int legacyContractYears,
    const PlayerContractTerms& terms)
{
    PlayerContractTerms result = terms;
    if (result.weeklyWagePence == 0 && legacyWeeklyWagePence > 0) {
        result.weeklyWagePence = legacyWeeklyWagePence;
    }
    if (result.contractYears == 0 && legacyContractYears > 0) {
        result.contractYears = legacyContractYears;
    }
    if (result.squadRole.empty()) {
        result.squadRole = "First Team";
    }
    return result;
}

Money guaranteedTransferFee(const TransferFeeTerms& terms)
{
    return terms.upfrontFeePence + terms.installmentTotalPence;
}

Money maximumTransferFee(const TransferFeeTerms& terms)
{
    return guaranteedTransferFee(terms) + terms.appearanceBonusPence;
}

Money immediateTransferCashCost(
    const TransferFeeTerms& feeTerms,
    const PlayerContractTerms& contractTerms)
{
    return feeTerms.upfrontFeePence
        + contractTerms.signingBonusPence
        + contractTerms.agentFeePence;
}

Money transferBudgetCommitment(
    const TransferFeeTerms& feeTerms,
    const PlayerContractTerms& contractTerms)
{
    return maximumTransferFee(feeTerms)
        + contractTerms.signingBonusPence
        + contractTerms.loyaltyBonusPence
        + contractTerms.agentFeePence;
}

bool validTransferFeeTerms(const TransferFeeTerms& terms)
{
    if (terms.upfrontFeePence < 0
        || terms.installmentTotalPence < 0
        || terms.appearanceBonusPence < 0
        || terms.installmentCount < 0
        || terms.installmentCount > 4
        || terms.appearanceThreshold < 0
        || terms.appearanceThreshold > 100
        || terms.sellOnPercentage < 0
        || terms.sellOnPercentage > 50) {
        return false;
    }
    if (guaranteedTransferFee(terms) <= 0) {
        return false;
    }
    if ((terms.installmentTotalPence == 0) != (terms.installmentCount == 0)) {
        return false;
    }
    if ((terms.appearanceBonusPence == 0) != (terms.appearanceThreshold == 0)) {
        return false;
    }
    return true;
}

bool validPlayerContractTerms(const PlayerContractTerms& terms)
{
    return terms.weeklyWagePence > 0
        && terms.contractYears >= 1
        && terms.contractYears <= 5
        && terms.signingBonusPence >= 0
        && terms.loyaltyBonusPence >= 0
        && terms.appearanceFeePence >= 0
        && terms.goalBonusPence >= 0
        && terms.agentFeePence >= 0
        && terms.releaseClausePence >= 0
        && !terms.squadRole.empty();
}

bool validLoanTerms(
    const LoanTerms& terms,
    const GameDate& currentDate)
{
    if (!terms.endDate.isValid()
        || terms.endDate <= currentDate
        || currentDate.daysUntil(terms.endDate) < 28
        || currentDate.daysUntil(terms.endDate) > 550
        || terms.loanFeePence < 0
        || terms.borrowingClubWagePercentage < 0
        || terms.borrowingClubWagePercentage > 100
        || terms.promisedRole.empty()
        || terms.recallNoticeDays < 0
        || terms.recallNoticeDays > 30
        || terms.futureFeePence < 0
        || terms.unusedPlayerFeePence < 0
        || (terms.futureFeeType != "none"
            && terms.futureFeeType != "option"
            && terms.futureFeeType != "mandatory")
        || (terms.futureFeeType == "none" && terms.futureFeePence != 0)
        || (terms.futureFeeType != "none" && terms.futureFeePence <= 0)
        || (terms.treatmentResponsibility != "parent_club"
            && terms.treatmentResponsibility != "loan_club"
            && terms.treatmentResponsibility != "shared")) {
        return false;
    }
    return true;
}

Money loanFinancialCommitment(const LoanTerms& terms)
{
    return terms.loanFeePence
        + (terms.futureFeeType == "mandatory"
            ? terms.futureFeePence
            : Money {});
}

Money contractRenewalCommitment(const PlayerContractTerms& terms)
{
    return terms.signingBonusPence
        + terms.loyaltyBonusPence
        + terms.agentFeePence;
}

ScoutingReport makeScoutingReport(
    const TransferCandidate& candidate,
    const ScoutProfile& scout,
    const int knowledgeLevel,
    const std::string& focus,
    const GameDate& observedDate,
    const int reportVersion)
{
    const int knowledge = std::clamp(knowledgeLevel, 0, 100);
    const int currentFocusBonus =
        focus == "current_ability" || focus == "overall" ? 5 : 0;
    const int potentialFocusBonus =
        focus == "potential" || focus == "overall" ? 5 : 0;
    const int currentQuality = std::clamp(
        (knowledge * 3 + scout.judgingCurrentAbility) / 4
            + currentFocusBonus,
        0,
        100);
    const int potentialQuality = std::clamp(
        (knowledge * 3 + scout.judgingPotential) / 4
            + potentialFocusBonus,
        0,
        100);
    const int currentUncertainty = std::max(0, (100 - currentQuality + 2) / 3);
    const int potentialUncertainty =
        std::max(0, (100 - potentialQuality + 2) / 3);
    const int commercialQuality = std::clamp(
        (knowledge * 3 + scout.adaptability) / 4
            + (focus == "availability" ? 8 : 0),
        0,
        100);
    const int valueBandPercent = std::clamp(
        (110 - commercialQuality) / 2,
        5,
        45);
    const int wageBandPercent = std::clamp(
        (115 - commercialQuality) / 2,
        7,
        50);

    ScoutingReport report {
        candidate.player.id,
        knowledge,
        std::max(0, candidate.currentAbilityLow - currentUncertainty),
        std::min(100, candidate.currentAbilityHigh + currentUncertainty),
        std::max(0, candidate.potentialLow - potentialUncertainty),
        std::min(100, candidate.potentialHigh + potentialUncertainty),
        percentageBelow(candidate.marketValuePence, valueBandPercent),
        percentageAbove(candidate.marketValuePence, valueBandPercent),
        percentageBelow(candidate.askingPricePence, valueBandPercent),
        percentageAbove(candidate.askingPricePence, valueBandPercent),
        percentageBelow(candidate.expectedWeeklyWagePence, wageBandPercent),
        percentageAbove(candidate.expectedWeeklyWagePence, wageBandPercent),
        knowledge >= 35
            ? candidate.roleFit
            : "역할 적합성 추가 관찰 필요",
        {},
        knowledge >= 65
            ? candidate.availability
            : "판매 가능성과 경쟁 구단 상황을 추가로 확인해야 합니다.",
        observedDate,
        observedDate.addDays(30),
        scout.id,
        focus,
        std::max(1, reportVersion),
        false
    };

    if (knowledge < 35) {
        report.summary =
            "신원·소속과 기본 포지션만 확인했습니다. 능력과 계약 조건의 오차가 큽니다.";
    } else if (knowledge < 70) {
        report.summary = report.roleFit
            + " 후보입니다. 현재 평가는 중간 보고서이며 추가 관찰이 필요합니다.";
    } else {
        report.summary = candidate.scoutSummary;
    }
    return report;
}

Money outstandingTransferPayables(const GameState& state)
{
    Money total = 0;
    for (const auto& payment : state.transferPayments) {
        if (payment.direction == TransferPaymentDirection::Payable
            && (payment.status == TransferPaymentStatus::Scheduled
                || payment.status == TransferPaymentStatus::Overdue)) {
            total += payment.amountPence;
        }
    }
    return total;
}

Money outstandingConditionalTransferCommitments(const GameState& state)
{
    Money total = 0;
    for (const auto& proposal : state.transferProposals) {
        if (proposal.status != TransferStatus::Approved
            && proposal.status != TransferStatus::Completed) {
            continue;
        }
        if (proposal.status == TransferStatus::Completed
            && std::none_of(
                state.players.begin(),
                state.players.end(),
                [&proposal](const Player& player) {
                    return player.id == proposal.target.id;
                })) {
            continue;
        }
        total += effectiveFeeTerms(
            proposal.feePence,
            proposal.feeTerms).appearanceBonusPence;
    }
    return total;
}

Money reservedTransferNegotiationCommitments(
    const GameState& state,
    const std::string& excludedPursuitId,
    const std::string& excludedProposalId,
    const std::string& excludedLoanId,
    const std::string& excludedRenewalId)
{
    Money total = 0;
    for (const auto& proposal : state.transferProposals) {
        if (proposal.id == excludedProposalId
            || proposal.status != TransferStatus::Pending) {
            continue;
        }
        total += transferBudgetCommitment(
            effectiveFeeTerms(proposal.feePence, proposal.feeTerms),
            effectiveContractTerms(
                proposal.weeklyWagePence,
                proposal.contractYears,
                proposal.contractTerms));
    }

    for (const auto& pursuit : state.transferPursuits) {
        if (pursuit.id == excludedPursuitId
            || isClosedPursuit(pursuit.stage)
            || !pursuit.linkedProposalId.empty()) {
            continue;
        }
        const TransferFeeTerms feeTerms = effectiveFeeTerms(
            pursuit.currentOfferPence,
            pursuit.offerTerms);
        Money commitment = maximumTransferFee(feeTerms);
        if (pursuit.playerOfferSubmitted
            && validPlayerContractTerms(pursuit.playerOfferTerms)) {
            commitment = transferBudgetCommitment(
                feeTerms,
                pursuit.playerOfferTerms);
        }
        total += commitment;
    }

    for (const auto& loan : state.loanDeals) {
        if (loan.id == excludedLoanId
            || loan.direction != LoanDirection::Incoming
            || loan.stage == LoanStage::Recalled
            || loan.stage == LoanStage::Rejected
            || loan.stage == LoanStage::Withdrawn
            || loan.stage == LoanStage::Expired) {
            continue;
        }
        if (loan.stage == LoanStage::Active
            || loan.stage == LoanStage::RecallPending
            || loan.stage == LoanStage::Completed) {
            if (loan.terms.futureFeeType == "mandatory") {
                total += loan.terms.futureFeePence;
            }
            continue;
        }
        const LoanTerms& terms = loan.stage == LoanStage::ClubCountered
            ? loan.counterTerms
            : loan.terms;
        total += loanFinancialCommitment(terms);
    }

    for (const auto& renewal : state.contractRenewals) {
        if (renewal.id == excludedRenewalId
            || (renewal.stage != ContractRenewalStage::AwaitingPlayerResponse
                && renewal.stage != ContractRenewalStage::PlayerCountered
                && renewal.stage != ContractRenewalStage::AgreementReached)) {
            continue;
        }
        const PlayerContractTerms& terms =
            renewal.stage == ContractRenewalStage::PlayerCountered
            ? renewal.counterTerms
            : renewal.offerTerms;
        total += contractRenewalCommitment(terms);
    }
    return total;
}

Money availableTransferCash(
    const GameState& state,
    const std::string& excludedPursuitId,
    const std::string& excludedProposalId,
    const std::string& excludedLoanId,
    const std::string& excludedRenewalId)
{
    return std::max<Money>(
        0,
        state.club.finance.cashPence
            - outstandingTransferPayables(state)
            - outstandingConditionalTransferCommitments(state)
            - reservedTransferNegotiationCommitments(
                state,
                excludedPursuitId,
                excludedProposalId,
                excludedLoanId,
                excludedRenewalId));
}

Money availableTransferBudget(
    const GameState& state,
    const std::string&,
    const std::string&)
{
    return std::max<Money>(0, state.club.finance.transferBudgetPence);
}

Money aiReservedMarketCommitments(
    const GameState& state,
    const std::string& clubId,
    const std::string& excludedDealId)
{
    Money total = 0;
    for (const auto& deal : state.aiMarketDeals) {
        if (deal.id == excludedDealId
            || deal.buyingClubId != clubId
            || deal.completionApplied
            || (deal.stage != AiMarketDealStage::Proposed
                && deal.stage != AiMarketDealStage::AwaitingCompletion)) {
            continue;
        }

        if (deal.type == AiMarketDealType::Loan) {
            total += loanFinancialCommitment(deal.loanTerms);
            continue;
        }
        if (deal.type == AiMarketDealType::Renewal) {
            total += contractRenewalCommitment(deal.contractTerms);
            continue;
        }
        total += transferBudgetCommitment(
            deal.feeTerms,
            deal.contractTerms);
    }
    return total;
}

Money aiAvailableTransferCash(
    const GameState& state,
    const AiClubMarketState& club,
    const std::string& excludedDealId)
{
    return std::max<Money>(
        0,
        club.finance.cashPence
            - aiReservedMarketCommitments(
                state,
                club.clubId,
                excludedDealId));
}

Money aiAvailableTransferBudget(
    const GameState& state,
    const AiClubMarketState& club,
    const std::string& excludedDealId)
{
    return std::max<Money>(
        0,
        club.finance.transferBudgetPence
            - aiReservedMarketCommitments(
                state,
                club.clubId,
                excludedDealId));
}

void ensureDefaultAiMarketState(GameState& state)
{
    const auto hasClub = [&state](const std::string& clubId) {
        return std::any_of(
            state.aiClubMarketStates.begin(),
            state.aiClubMarketStates.end(),
            [&clubId](const AiClubMarketState& club) {
                return club.clubId == clubId;
            });
    };

    const auto addClub = [&state, &hasClub](
                             const std::string& clubId,
                             const std::string& clubName) {
        if (clubId.empty()
            || clubId == state.club.id
            || hasClub(clubId)) {
            return;
        }

        const std::uint32_t seed = stableHash(
            clubId + ":" + std::to_string(state.rngSeed));
        FinanceState finance;
        finance.cashPence =
            260'000'000 + static_cast<Money>(seed % 180'000'000U);
        finance.transferBudgetPence =
            130'000'000 + static_cast<Money>((seed / 7U) % 170'000'000U);
        finance.weeklyWageBudgetPence =
            5'500'000 + static_cast<Money>((seed / 17U) % 3'500'000U);
        finance.weeklyWageCommitmentPence =
            finance.weeklyWageBudgetPence * 55 / 100;
        finance.debtPence = static_cast<Money>((seed / 23U) % 90'000'000U);
        finance.confidence = "AI 시장 추정";

        state.aiClubMarketStates.push_back({
            clubId,
            clubName.empty() ? clubId : clubName,
            finance,
            40 + static_cast<int>(seed % 35U),
            (seed % 3U == 0U
                    ? "first_team"
                    : (seed % 3U == 1U ? "development" : "depth")),
            std::nullopt,
            state.currentDate.addDays(3 + static_cast<int>(seed % 5U)),
            seed,
            "gameplay_estimate_current_dataset"
        });
    };

    for (const auto& candidate : state.transferCandidates) {
        addClub(candidate.currentClubId, candidate.currentClubName);
        addClub(candidate.rivalClubId, candidate.rivalClubName);
    }
    for (const auto& loan : state.loanDeals) {
        addClub(loan.parentClubId, loan.parentClubName);
        addClub(loan.loanClubId, loan.loanClubName);
    }
    for (const auto& offer : state.outgoingTransferOffers) {
        addClub(offer.buyerClubId, offer.buyerClubName);
    }
}

void recalculateLeagueStandings(GameState& state)
{
    std::vector<LeagueStandingRow> rows;
    rows.reserve(
        state.competitionSeasonTeams.empty()
            ? state.fixtures.size() * 2U
            : state.competitionSeasonTeams.size());

    const auto findRow = [&rows](const std::string& competition, const std::string& clubName) {
        return std::find_if(
            rows.begin(),
            rows.end(),
            [&competition, &clubName](const LeagueStandingRow& row) {
                return row.competition == competition && row.clubName == clubName;
            });
    };

    for (const auto& team : state.competitionSeasonTeams) {
        rows.push_back({
            team.seasonId,
            team.competition,
            team.clubId,
            team.clubName,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        });
    }

    const auto ensureFixtureClub = [&](const Fixture& fixture, const std::string& clubName) -> std::size_t {
        auto row = findRow(fixture.competition, clubName);
        if (row != rows.end()) {
            return static_cast<std::size_t>(std::distance(rows.begin(), row));
        }
        rows.push_back({
            {},
            fixture.competition,
            {},
            clubName,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0
        });
        return rows.size() - 1U;
    };

    for (const auto& fixture : state.fixtures) {
        if (!fixture.played) {
            continue;
        }

        const std::size_t homeIndex = ensureFixtureClub(fixture, fixture.homeClub);
        const std::size_t awayIndex = ensureFixtureClub(fixture, fixture.awayClub);
        LeagueStandingRow& home = rows.at(homeIndex);
        LeagueStandingRow& away = rows.at(awayIndex);

        home.played += 1;
        away.played += 1;
        home.goalsFor += fixture.homeGoals;
        home.goalsAgainst += fixture.awayGoals;
        away.goalsFor += fixture.awayGoals;
        away.goalsAgainst += fixture.homeGoals;

        if (fixture.homeGoals > fixture.awayGoals) {
            home.won += 1;
            away.lost += 1;
            home.points += 3;
        } else if (fixture.homeGoals < fixture.awayGoals) {
            away.won += 1;
            home.lost += 1;
            away.points += 3;
        } else {
            home.drawn += 1;
            away.drawn += 1;
            home.points += 1;
            away.points += 1;
        }

        home.goalDifference = home.goalsFor - home.goalsAgainst;
        away.goalDifference = away.goalsFor - away.goalsAgainst;
    }

    for (auto& row : rows) {
        row.goalDifference = row.goalsFor - row.goalsAgainst;
    }

    std::stable_sort(
        rows.begin(),
        rows.end(),
        [](const LeagueStandingRow& left, const LeagueStandingRow& right) {
            if (left.competition != right.competition) {
                return left.competition < right.competition;
            }
            if (left.points != right.points) {
                return left.points > right.points;
            }
            if (left.goalDifference != right.goalDifference) {
                return left.goalDifference > right.goalDifference;
            }
            if (left.goalsFor != right.goalsFor) {
                return left.goalsFor > right.goalsFor;
            }
            return left.clubName < right.clubName;
        });

    std::string currentCompetition;
    int position = 0;
    for (auto& row : rows) {
        if (row.competition != currentCompetition) {
            currentCompetition = row.competition;
            position = 1;
        } else {
            ++position;
        }
        row.position = position;
    }

    state.leagueStandings = std::move(rows);
}

int recruitmentPlanCompletedSignings(
    const GameState& state,
    const RecruitmentPlan& plan)
{
    std::vector<std::string> countedPlayers;
    for (const auto& proposal : state.transferProposals) {
        if ((proposal.status != TransferStatus::Approved
                && proposal.status != TransferStatus::Completed)
            || !proposal.decisionDate.has_value()
            || *proposal.decisionDate < plan.startDate
            || plan.evaluationDate < *proposal.decisionDate
            || std::find(
                countedPlayers.begin(),
                countedPlayers.end(),
                proposal.target.id) != countedPlayers.end()) {
            continue;
        }
        countedPlayers.push_back(proposal.target.id);
    }
    return static_cast<int>(countedPlayers.size());
}

int recruitmentPlanPriorityScore(
    const GameState& state,
    const RecruitmentPlan& plan)
{
    int score = 0;
    std::vector<std::string> countedPlayers;
    for (const auto& proposal : state.transferProposals) {
        if ((proposal.status != TransferStatus::Approved
                && proposal.status != TransferStatus::Completed)
            || !proposal.decisionDate.has_value()
            || *proposal.decisionDate < plan.startDate
            || plan.evaluationDate < *proposal.decisionDate
            || std::find(
                countedPlayers.begin(),
                countedPlayers.end(),
                proposal.target.id) != countedPlayers.end()) {
            continue;
        }
        countedPlayers.push_back(proposal.target.id);
        const auto candidate = std::find_if(
            state.transferCandidates.begin(),
            state.transferCandidates.end(),
            [&proposal](const TransferCandidate& item) {
                return item.player.id == proposal.target.id;
            });
        score += candidate != state.transferCandidates.end()
            ? candidate->managerPriority
            : 50;
    }
    return score;
}

GameState makeManchesterUnitedPrototype()
{
    GameState state;
    state.saveName = "맨체스터 유나이티드 1992-93 MVP";
    state.currentDate = {1992, 7, 1};
    state.ownerName = "신임 구단주";
    state.operatingMeetingSettings.updatedDate = state.currentDate;
    state.dataNotice = "1992-07-01 기준 맨유 1군·리저브·유스와 잉글랜드 이적시장 1차 시드입니다. 선수 신원·당시 소속과 시대 규정은 출처를 따로 관리하고, 스카우팅 능력 범위·금액·계약·임대 조건은 기능 검증용 추정치로 표시합니다.";

    state.club = {
        "man-utd",
        "Manchester United",
        "Man Utd",
        "Premier League",
        88,
        78,
        {
            600'000'000,
            300'000'000,
            9'000'000,
            7'800'000,
            0,
            "prototype_estimate"
        },
        {
            "Alex Ferguson",
            "Manager",
            82,
            "공격적인 전환과 강한 선수단 규율",
            "이사회는 감독을 신뢰하며 장기적인 선수단 강화를 기대합니다."
        }
    };

    const std::string firstTeamConfidence = "confirmed_1991_92_first_team_membership";
    const std::string reserveConfidence = "membership_confirmed_group_provisional";
    const std::string youthConfidence = "official_1992_youth_cohort";
    const std::string academyProvisional = "academy_membership_provisional";
    state.players = {
        {"peter-schmeichel", "Peter Schmeichel", GameDate {1963, 11, 18}, "Denmark", "GK", "First Team", "Available", firstTeamConfidence},
        {"gary-walsh", "Gary Walsh", GameDate {1968, 3, 21}, "England", "GK", "First Team", "Available", firstTeamConfidence},
        {"ian-wilkinson", "Ian Wilkinson", GameDate {1973, 7, 2}, "England", "GK", "First Team", "Available", firstTeamConfidence},
        {"paul-parker", "Paul Parker", GameDate {1964, 4, 4}, "England", "RB", "First Team", "Available", firstTeamConfidence},
        {"steve-bruce", "Steve Bruce", GameDate {1960, 12, 31}, "England", "CB", "First Team", "Available", firstTeamConfidence},
        {"gary-pallister", "Gary Pallister", GameDate {1965, 6, 30}, "England", "CB", "First Team", "Available", firstTeamConfidence},
        {"mal-donaghy", "Mal Donaghy", GameDate {1957, 9, 13}, "Northern Ireland", "CB", "First Team", "Available", firstTeamConfidence},
        {"denis-irwin", "Denis Irwin", GameDate {1965, 10, 31}, "Republic of Ireland", "LB", "First Team", "Available", firstTeamConfidence},
        {"clayton-blackmore", "Clayton Blackmore", GameDate {1964, 9, 23}, "Wales", "DF/MF", "First Team", "Available", firstTeamConfidence},
        {"lee-martin", "Lee Martin", GameDate {1968, 2, 5}, "England", "LB", "First Team", "Available", firstTeamConfidence},
        {"bryan-robson", "Bryan Robson", GameDate {1957, 1, 11}, "England", "CM", "First Team", "Available", firstTeamConfidence},
        {"paul-ince", "Paul Ince", GameDate {1967, 10, 21}, "England", "CM", "First Team", "Available", firstTeamConfidence},
        {"mike-phelan", "Mike Phelan", GameDate {1962, 9, 24}, "England", "RB/CM", "First Team", "Available", firstTeamConfidence},
        {"neil-webb", "Neil Webb", GameDate {1963, 7, 30}, "England", "CM", "First Team", "Available", firstTeamConfidence},
        {"darren-ferguson", "Darren Ferguson", GameDate {1972, 2, 9}, "Scotland", "CM", "First Team", "Available", firstTeamConfidence},
        {"ryan-giggs", "Ryan Giggs", GameDate {1973, 11, 29}, "Wales", "LM", "First Team", "Available", firstTeamConfidence},
        {"lee-sharpe", "Lee Sharpe", GameDate {1971, 5, 27}, "England", "LM", "First Team", "Available", firstTeamConfidence},
        {"andrei-kanchelskis", "Andrei Kanchelskis", GameDate {1969, 1, 23}, "Russia", "RM", "First Team", "Available", firstTeamConfidence},
        {"russell-beardsmore", "Russell Beardsmore", GameDate {1968, 9, 28}, "England", "MF", "First Team", "Available", firstTeamConfidence},
        {"mark-hughes", "Mark Hughes", GameDate {1963, 11, 1}, "Wales", "ST", "First Team", "Available", firstTeamConfidence},
        {"brian-mcclair", "Brian McClair", GameDate {1963, 12, 8}, "Scotland", "ST", "First Team", "Available", firstTeamConfidence},
        {"mark-robins", "Mark Robins", GameDate {1969, 12, 22}, "England", "ST", "First Team", "Available", firstTeamConfidence},
        {"danny-wallace", "Danny Wallace", GameDate {1964, 1, 21}, "England", "FW", "First Team", "Available", firstTeamConfidence},

        {"kevin-pilkington", "Kevin Pilkington", GameDate {1974, 3, 8}, "England", "GK", "Reserve", "Available", reserveConfidence, GameDate {1992, 7, 1}},
        {"brian-carey", "Brian Carey", GameDate {1968, 5, 31}, "Republic of Ireland", "CB", "Reserve", "Available", reserveConfidence},
        {"derek-brazil", "Derek Brazil", GameDate {1968, 12, 14}, "Republic of Ireland", "CB", "Reserve", "Available", reserveConfidence},
        {"neil-whitworth", "Neil Whitworth", GameDate {1972, 4, 12}, "England", "CB", "Reserve", "Available", reserveConfidence},
        {"giuliano-maiorana", "Giuliano Maiorana", GameDate {1969, 4, 18}, "England", "LM", "Reserve", "Available", reserveConfidence},
        {"adrian-doherty", "Adrian Doherty", GameDate {1973, 6, 10}, "Northern Ireland", "RM", "Reserve", "Rehabilitation", reserveConfidence},
        {"colin-telford", "Colin Telford", std::nullopt, "Northern Ireland", "FW", "Reserve", "Available", reserveConfidence},
        {"kieran-toal", "Kieran Toal", GameDate {1971, 12, 14}, "Republic of Ireland", "MF", "Reserve", "Available", reserveConfidence},

        {"david-beckham", "David Beckham", GameDate {1975, 5, 2}, "England", "RM", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"nicky-butt", "Nicky Butt", GameDate {1975, 1, 21}, "England", "CM", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"gary-neville", "Gary Neville", GameDate {1975, 2, 18}, "England", "RB", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"paul-scholes", "Paul Scholes", GameDate {1974, 11, 16}, "England", "CM", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"keith-gillespie", "Keith Gillespie", GameDate {1975, 2, 18}, "Northern Ireland", "RM", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"ben-thornley", "Ben Thornley", GameDate {1975, 4, 21}, "England", "LM", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"chris-casper", "Chris Casper", GameDate {1975, 4, 28}, "England", "CB", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"john-okane", "John O'Kane", GameDate {1974, 11, 15}, "England", "RB", "Youth", "Trainee", youthConfidence, GameDate {1991, 7, 8}},
        {"george-switzer", "George Switzer", GameDate {1973, 10, 13}, "England", "LB", "Youth", "Youth Player", youthConfidence},
        {"simon-davies", "Simon Davies", GameDate {1974, 4, 23}, "Wales", "CM", "Youth", "Youth Player", youthConfidence},
        {"robbie-savage", "Robbie Savage", GameDate {1974, 10, 18}, "Wales", "FW", "Youth", "Youth Player", youthConfidence},
        {"colin-mckee", "Colin McKee", GameDate {1973, 8, 22}, "Scotland", "FW", "Youth", "Youth Player", youthConfidence},
        {"phil-neville", "Phil Neville", GameDate {1977, 1, 21}, "England", "DF", "Youth", "Schoolboy", academyProvisional},
        {"raphael-burke", "Raphael Burke", GameDate {1974, 7, 3}, "England", "FW", "Youth", "Youth Player", academyProvisional}
    };

    state.transferProposals = {
        {
            "alan-shearer-1992-opening",
            {
                "alan-shearer",
                "Alan Shearer",
                GameDate {1970, 8, 13},
                "England",
                "ST",
                "Transfer Target",
                "Available",
                "identity_and_club_confirmed_finance_estimated"
            },
            "southampton",
            "Southampton",
            300'000'000,
            1'000'000,
            4,
            "Alex Ferguson",
            "지난 시즌 득점력 부족을 해결할 최우선 공격수입니다. 감독은 즉시 주전으로 활용할 수 있다고 평가합니다.",
            {1992, 7, 1},
            {1992, 7, 10},
            std::nullopt,
            std::nullopt,
            TransferStatus::Pending,
            "prototype_financial_terms",
            {},
            {},
            "not_checked",
            "not_checked"
        }
    };
    state.transferProposals.front().feeTerms = {
        300'000'000,
        0,
        0,
        0,
        0,
        0
    };
    state.transferProposals.front().contractTerms = {
        1'000'000,
        4,
        0,
        0,
        0,
        0,
        0,
        0,
        "Key Player"
    };
    state.transferProposals.front().medicalStatus = "passed";
    state.transferProposals.front().registrationStatus = "eligible_1992_domestic";

    state.transferCandidates = {
        {
            {"alan-shearer", "Alan Shearer", GameDate {1970, 8, 13}, "England", "ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "southampton", "Southampton", 1,
            82, 88, 88, 94, 95, 78,
            300'000'000, 300'000'000, 340'000'000, 360'000'000,
            1'000'000, 4,
            "즉시 주전 공격수 · 득점력 최우선",
            "박스 안 결정력과 장기적인 에이스 가능성이 가장 높은 후보입니다.",
            "Southampton은 높은 금액만 검토하며 Blackburn의 관심이 매우 강합니다.",
            "blackburn", "Blackburn Rovers", 360'000'000,
            "identity_and_1992_transfer_confirmed_finance_partly_sourced"
        },
        {
            {"teddy-sheringham", "Teddy Sheringham", GameDate {1966, 4, 2}, "England", "ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "nottingham-forest", "Nottingham Forest", 2,
            80, 86, 82, 88, 86, 72,
            190'000'000, 175'000'000, 205'000'000, 220'000'000,
            850'000, 4,
            "연계형 주전 공격수 · 즉시 전력",
            "득점뿐 아니라 동료를 살리는 움직임이 강점이며 적응 위험이 낮습니다.",
            "Forest는 적정가 협상이 가능하지만 Tottenham이 상황을 지켜보고 있습니다.",
            "tottenham", "Tottenham Hotspur", 210'000'000,
            "identity_and_1992_club_change_confirmed_finance_estimated"
        },
        {
            {"dion-dublin", "Dion Dublin", GameDate {1969, 4, 22}, "England", "ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "cambridge-utd", "Cambridge United", 3,
            70, 78, 78, 86, 78, 84,
            85'000'000, 75'000'000, 95'000'000, 105'000'000,
            500'000, 4,
            "성장형 공격수 · 제공권과 활동량",
            "즉시 슈퍼스타는 아니지만 가격 대비 성장 여지가 크고 감독의 직접 관찰 평가가 좋습니다.",
            "Cambridge는 대체 선수를 구할 수 있는 금액이면 협상할 의사가 있습니다.",
            "", "", 0,
            "identity_and_1992_club_change_confirmed_finance_estimated"
        },
        {
            {"eric-cantona", "Eric Cantona", GameDate {1966, 5, 24}, "France", "SS/ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "leeds", "Leeds United", 4,
            82, 90, 83, 90, 74, 48,
            130'000'000, 110'000'000, 155'000'000, 175'000'000,
            900'000, 4,
            "창조형 공격수 · 높은 변동성",
            "기술과 창조성은 최상급이지만 라이벌 구단과의 협상, 선수 성향과 감독 관계 위험이 큽니다.",
            "현시점 Leeds는 판매 의사가 낮고 라이벌 구단 거래라 요구액이 높습니다.",
            "", "", 0,
            "identity_and_club_confirmed_finance_gameplay_estimate"
        },
        {
            {"les-ferdinand", "Les Ferdinand", GameDate {1966, 12, 8}, "England", "ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "qpr", "Queens Park Rangers", 5,
            76, 84, 82, 89, 72, 62,
            145'000'000, 125'000'000, 170'000'000, 190'000'000,
            700'000, 4,
            "직선적인 주전 공격수 · 신체 능력",
            "공간 침투와 제공권이 좋고 잉글랜드 무대 적응 위험이 낮습니다.",
            "QPR은 핵심 공격수로 평가해 시장 가치 이상의 제안을 원합니다.",
            "", "", 0,
            "identity_and_club_confirmed_finance_gameplay_estimate"
        },
        {
            {"roy-keane", "Roy Keane", GameDate {1971, 8, 10}, "Republic of Ireland", "CM",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "nottingham-forest", "Nottingham Forest", 6,
            79, 86, 90, 96, 80, 58,
            225'000'000, 190'000'000, 245'000'000, 270'000'000,
            750'000, 5,
            "장기 핵심 미드필더 · 리더십",
            "당장 공격수 문제를 해결하지는 않지만 중장기적으로 팀의 중심이 될 잠재력이 큽니다.",
            "Forest는 핵심 유망주로 분류해 매우 높은 제안이 아니면 협상하지 않습니다.",
            "", "", 0,
            "identity_and_club_confirmed_finance_gameplay_estimate"
        },
        {
            {"paul-warhurst", "Paul Warhurst", GameDate {1969, 9, 26}, "England", "CB/ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "sheffield-wed", "Sheffield Wednesday", 7,
            71, 79, 78, 85, 66, 68,
            95'000'000, 80'000'000, 110'000'000, 125'000'000,
            450'000, 4,
            "다목적 보강 · 수비와 공격 비상 옵션",
            "여러 역할을 소화할 수 있어 작은 선수단에서 가치가 높지만 전문성은 더 관찰해야 합니다.",
            "Sheffield Wednesday는 핵심 로테이션 가치까지 보상받길 원합니다.",
            "", "", 0,
            "identity_and_club_confirmed_finance_gameplay_estimate"
        },
        {
            {"brian-deane", "Brian Deane", GameDate {1968, 2, 7}, "England", "ST",
                "Transfer Target", "Available", "identity_and_club_confirmed_finance_estimated"},
            "sheffield-utd", "Sheffield United", 8,
            73, 81, 78, 85, 70, 64,
            120'000'000, 100'000'000, 140'000'000, 155'000'000,
            550'000, 4,
            "피지컬형 공격수 · 즉시 로테이션",
            "잉글랜드 리그 경험과 신체 능력이 장점이며 득점 상한은 추가 관찰이 필요합니다.",
            "Sheffield United는 같은 리그 경쟁 구단에 판매할 경우 프리미엄을 요구합니다.",
            "", "", 0,
            "identity_and_club_confirmed_finance_gameplay_estimate"
        }
    };

    state.scouts = {
        {
            "les-kershaw",
            "Les Kershaw",
            "Chief Scout",
            "England · youth and domestic recruitment",
            84,
            88,
            80,
            2,
            "identity_and_recruitment_role_confirmed_1992_title_provisional"
        },
        {
            "alex-ferguson-observation",
            "Alex Ferguson",
            "Manager Observation",
            "United Kingdom & Ireland",
            90,
            82,
            86,
            1,
            "manager_identity_confirmed_scouting_profile_gameplay_estimate"
        }
    };
    for (const auto& candidate : state.transferCandidates) {
        const bool isShearer = candidate.player.id == "alan-shearer";
        const ScoutProfile& scout = isShearer
            ? state.scouts.at(1)
            : state.scouts.front();
        const int initialKnowledge = isShearer
            ? 90
            : (candidate.recommendationRank <= 3 ? 45 : 30);
        state.scoutingReports.push_back(makeScoutingReport(
            candidate,
            scout,
            initialKnowledge,
            isShearer ? "current_ability" : "overall",
            {1992, 6, 25}));
    }
    ensureDefaultScoutFamiliarities(state);

    state.transferPursuits = {
        {
            "pursuit-alan-shearer-opening",
            "alan-shearer",
            TransferPursuitStage::AwaitingOwnerApproval,
            300'000'000,
            0,
            2,
            {1992, 6, 26},
            {1992, 7, 1},
            {1992, 7, 10},
            GameDate {1992, 7, 1},
            "감독과 구단 실무진이 구단·선수 측 협상을 마쳤으며 구단주 최종 승인만 남았습니다.",
            true,
            "blackburn",
            "Blackburn Rovers",
            360'000'000,
            "alan-shearer-1992-opening",
            {},
            {},
            {},
            {},
            0,
            false,
            0,
            "not_started",
            "not_checked"
        }
    };
    state.transferPursuits.front().offerTerms =
        state.transferProposals.front().feeTerms;
    state.transferPursuits.front().playerOfferTerms =
        state.transferProposals.front().contractTerms;
    state.transferPursuits.front().playerOfferSubmitted = true;
    state.transferPursuits.front().medicalStatus = "passed";
    state.transferPursuits.front().registrationStatus = "eligible_1992_domestic";

    state.outgoingTransferOffers = {
        {
            "norwich-mark-robins-1992-opening",
            "mark-robins",
            "Mark Robins",
            "norwich",
            "Norwich City",
            80'000'000,
            300'000,
            {1992, 7, 1},
            {1992, 7, 9},
            std::nullopt,
            std::nullopt,
            OutgoingTransferStatus::Pending,
            "First Team",
            "Norwich는 꾸준한 1군 출전 기회를 제시하며 완전 영입을 원합니다. 감독은 적정가라면 매각을 검토할 수 있다는 의견입니다.",
            "historical_destination_confirmed_timing_and_finance_gameplay_estimate",
            {},
            {},
            OutgoingNegotiationStage::DecisionPending,
            0,
            std::nullopt,
            0
        }
    };
    state.outgoingTransferOffers.front().feeTerms = {
        80'000'000,
        0,
        0,
        0,
        0,
        0
    };
    state.outgoingTransferOffers.front().buyerMaximumValuePence = 100'000'000;

    state.playerContracts = {
        {
            "mark-robins",
            "man-utd",
            {1990, 7, 1},
            {1993, 6, 30},
            300'000,
            "Rotation",
            "",
            "gameplay_contract_estimate",
            {
                300'000,
                3,
                0,
                0,
                0,
                0,
                0,
                0,
                "Rotation"
            }
        },
        {
            "bryan-robson",
            "man-utd",
            {1990, 7, 1},
            {1993, 6, 30},
            650'000,
            "Key Player",
            "",
            "gameplay_contract_estimate",
            {
                650'000,
                3,
                0,
                0,
                25'000,
                20'000,
                0,
                0,
                "Key Player"
            }
        },
        {
            "mark-hughes",
            "man-utd",
            {1991, 7, 1},
            {1994, 6, 30},
            700'000,
            "Key Player",
            "",
            "gameplay_contract_estimate",
            {
                700'000,
                3,
                0,
                0,
                40'000,
                50'000,
                0,
                0,
                "Key Player"
            }
        },
        {
            "lee-martin",
            "man-utd",
            {1990, 7, 1},
            {1993, 6, 30},
            250'000,
            "Reserve",
            "",
            "gameplay_contract_estimate",
            {
                250'000,
                3,
                0,
                0,
                10'000,
                5'000,
                0,
                0,
                "Reserve"
            }
        },
        {
            "kieran-toal",
            "man-utd",
            {1991, 7, 1},
            {1993, 6, 30},
            160'000,
            "Prospect",
            "",
            "gameplay_contract_estimate",
            {
                160'000,
                2,
                0,
                0,
                5'000,
                5'000,
                0,
                0,
                "Prospect"
            }
        }
    };

    const auto kieranToal = std::find_if(
        state.players.begin(),
        state.players.end(),
        [](const Player& player) {
            return player.id == "kieran-toal";
        });
    if (kieranToal != state.players.end()) {
        LoanTerms openingLoanTerms {
            500'000,
            60,
            "Regular Starter",
            {1993, 5, 31},
            true,
            7,
            "none",
            0,
            true,
            25'000,
            "loan_club"
        };
        state.loanDeals.push_back({
            "cambridge-kieran-toal-1992-opening",
            *kieranToal,
            LoanDirection::Outgoing,
            "man-utd",
            "Manchester United",
            "cambridge-utd",
            "Cambridge United",
            "성인 축구 적응과 정기 출전",
            {1992, 7, 1},
            {1992, 7, 8},
            {1992, 7, 5},
            LoanStage::DecisionPending,
            openingLoanTerms,
            openingLoanTerms,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            "Reserve",
            0,
            0,
            0,
            "Cambridge는 정기 선발 기회와 주급 60% 부담을 제안했습니다.",
            "gameplay_loan_scenario_not_historical_transaction"
        });
    }

    state.contractRenewals = {
        {
            "renewal-bryan-robson-1992",
            "bryan-robson",
            "Bryan Robson",
            "Alex Ferguson",
            "주장과 핵심 전력의 계약이 1년 남아 조기 재계약을 권고합니다.",
            {1993, 6, 30},
            {1992, 8, 15},
            ContractRenewalStage::Recommended,
            {
                750'000,
                2,
                4'000'000,
                2'000'000,
                50'000,
                25'000,
                2'000'000,
                0,
                "Key Player"
            },
            {},
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
            90,
            3,
            0,
            "선수는 잔류 의사가 높지만 주장과 핵심 선수 지위 유지를 기대합니다.",
            "identity_confirmed_contract_terms_gameplay_estimate"
        },
        {
            "renewal-lee-martin-1992",
            "lee-martin",
            "Lee Martin",
            "Alex Ferguson",
            "계약 만료 1년 전이며 선수단 역할과 임금 수준을 함께 재검토해야 합니다.",
            {1993, 6, 30},
            {1992, 9, 1},
            ContractRenewalStage::Recommended,
            {
                300'000,
                3,
                1'000'000,
                500'000,
                15'000,
                10'000,
                500'000,
                0,
                "Squad Player"
            },
            {},
            {},
            std::nullopt,
            std::nullopt,
            std::nullopt,
            58,
            3,
            0,
            "선수 측은 출전 경로가 분명해야 장기 계약을 검토합니다.",
            "identity_confirmed_contract_terms_gameplay_estimate"
        }
    };

    state.fixtures = {
        {
            "pl-1992-001",
            {1992, 8, 15},
            "Premier League",
            "Sheffield United",
            "Manchester United",
            65,
            78,
            false,
            0,
            0
        }
    };

    state.competitionSeasonTeams = {
        {
            "eng-premier-league-1992-93",
            "Premier League",
            "sheffield-utd",
            "Sheffield United",
            "ENG018",
            18
        },
        {
            "eng-premier-league-1992-93",
            "Premier League",
            "man-utd",
            "Manchester United",
            "ENG012",
            12
        }
    };
    recalculateLeagueStandings(state);

    state.inbox = {
        {
            1,
            "career-welcome",
            {1992, 7, 1},
            InboxCategory::Board,
            "맨체스터 유나이티드 인수가 완료되었습니다",
            "오늘부터 구단 운영의 최종 책임을 맡습니다. 기존 조직과 계약은 그대로 승계되며 역사는 이 시점부터 분기합니다.",
            true,
            false
        },
        {
            2,
            "first-100-days",
            {1992, 7, 1},
            InboxCategory::Board,
            "첫 100일 우선순위를 정해야 합니다",
            "감독 면담, 재정 검토, 선수단 계획을 먼저 확인하십시오. 이 MVP에서는 각 보고가 날짜 이벤트로 순차 도착합니다.",
            true,
            false
        },
        {
            3,
            "data-provenance",
            {1992, 7, 1},
            InboxCategory::System,
            "역사 데이터 신뢰도 안내",
            "현재 로스터는 1군·리저브·유스를 구분한 2차 시드입니다. 그룹 잠정값과 기능 검증용 재정·계약 수치는 역사적 사실로 간주하지 않습니다.",
            false,
            false
        },
        {
            4,
            "transfer-proposal-alan-shearer-1992-opening",
            {1992, 7, 1},
            InboxCategory::Football,
            "Alex Ferguson 감독이 Alan Shearer 영입 승인을 요청했습니다",
            "Southampton과의 주요 거래를 추진하려면 구단주 최종 승인이 필요합니다. 제안 이적료 £3.00m, 주급 £10.0k는 기능 검증용 추정 조건입니다.",
            true,
            true
        },
        {
            5,
            "transfer-market-opening-report",
            {1992, 7, 1},
            InboxCategory::Football,
            "여름 이적시장 1차 후보 보고가 도착했습니다",
            "감독과 스카우트 조직이 8명의 후보를 우선순위와 정보 신뢰도별로 정리했습니다. 이적 화면에서 추가 협상을 지시할 수 있습니다.",
            true,
            false
        },
        {
            6,
            "outgoing-offer-norwich-mark-robins-1992-opening",
            {1992, 7, 1},
            InboxCategory::Football,
            "Norwich City가 Mark Robins 영입을 제안했습니다",
            "제안 이적료는 £800.0k이며 7월 9일까지 구단주 결정을 요청합니다. 승인하면 3일 뒤 거래가 완료됩니다.",
            true,
            true
        },
        {
            7,
            "loan-offer-cambridge-kieran-toal-1992-opening",
            {1992, 7, 1},
            InboxCategory::Football,
            "Cambridge United가 Kieran Toal 임대를 제안했습니다",
            "시즌 임대료 £5.0k, 주급 60% 부담, 정기 선발 약속과 7일 리콜 조항이 포함된 기능 검증용 제안입니다.",
            true,
            true
        },
        {
            8,
            "contract-renewal-recommendation-renewal-bryan-robson-1992",
            {1992, 7, 1},
            InboxCategory::Football,
            "Bryan Robson 조기 재계약 검토가 필요합니다",
            "계약 만료까지 1년이 남았습니다. 주장과 핵심 선수 지위를 유지하는 조건으로 협상을 시작할 수 있습니다.",
            true,
            false
        }
    };

    state.eventQueue = {
        {
            1,
            "finance-opening-review",
            {1992, 7, 2},
            InboxCategory::Finance,
            "취임 재정 검토가 도착했습니다",
            "현금, 이적 예산, 주급 예산의 개발용 기준값을 확인할 수 있습니다. 실제 1992년 수치는 추후 출처와 신뢰도를 붙여 교체합니다.",
            false,
            false
        },
        {
            2,
            "manager-opening-meeting",
            {1992, 7, 3},
            InboxCategory::Football,
            "Alex Ferguson 감독과의 첫 면담",
            "감독은 현재 선수단의 경쟁력을 신뢰하지만 공격진 보강을 최우선 과제로 보고 있습니다. Alan Shearer 승인 안건은 받은 편지함에서 처리할 수 있습니다.",
            false,
            false
        },
        {
            3,
            "squad-opening-review",
            {1992, 7, 6},
            InboxCategory::Football,
            "1군 선수단 1차 검토 완료",
            "1992년 당시 구조에 맞춘 1군·리저브·유스 명단을 선수단 화면에서 나누어 확인할 수 있습니다.",
            false,
            false
        },
        {
            4,
            "monthly-finance-1992-08",
            {1992, 8, 1},
            InboxCategory::Finance,
            "7월 월간 재정 요약",
            "첫 월간 결산이 준비되었습니다. 현재는 현금과 예산 스냅샷을 제공하며 수입·지출 원장은 다음 단계에서 확장됩니다.",
            false,
            false
        }
    };

    state.recruitmentPlans = {
        {
            "preseason-recruitment-1992",
            "1992-93 프리시즌 선수단 보강",
            "Alex Ferguson",
            "추천 후보 중 세 명 영입이 목표입니다. 두 명 이상 영입하면 일부 후보를 놓쳐도 감독 신뢰는 하락하지 않습니다.",
            {1992, 7, 1},
            {1992, 8, 15},
            3,
            2,
            RecruitmentPlanStatus::Active,
            0,
            std::nullopt
        }
    };

    ensureDefaultAiMarketState(state);
    ensureDefaultOwnerReputation(state);

    return state;
}

} // namespace football_owner
