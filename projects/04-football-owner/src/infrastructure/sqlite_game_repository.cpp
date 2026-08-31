#include "infrastructure/sqlite_game_repository.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryFile>
#include <QUuid>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace football_owner {
namespace {

constexpr auto kLockedWorldDatasetId = "football_owner_1992_production";
constexpr auto kLockedWorldVersion = "LOCKED_v11";
constexpr auto kLockedWorldResourcePath =
    ":/database/world/football_owner_1992_production_LOCKED_v11.sqlite";
constexpr auto kLockedWorldCertificateResourcePath =
    ":/database/world/VALIDATION_CERTIFICATE_LOCKED_v11.json";
constexpr auto kLockedWorldSqliteSha256 =
    "8d47b24488b11270124d3bb7186d0170268ce07b0e8cbfb1e2e8ecee020aa918";
constexpr auto kLockedWorldZipSha256 =
    "6451365498f70dcbed51e6b3bdfe79215ecb813d4e0d3a310be557a1ebb121a7";
constexpr auto kFactSafeHandoffDatasetId = "football_owner_1992_fact_safe_handoff";
constexpr auto kFactSafeHandoffVersion = "v5.4.5";
constexpr auto kFactSafeHandoffResourceRoot = ":/database/fact_safe_v545";
constexpr auto kFactSafeHandoffManifestResourcePath =
    ":/database/fact_safe_v545/IMPORT_MANIFEST.csv";
constexpr auto kFactSafeCompetitionBridgeDatasetId = "football_owner_1992_fact_safe_competitions";
constexpr auto kFactSafeRosterBridgeDatasetId = "football_owner_1992_fact_safe_roster";
constexpr auto kFactSafeClubFinanceBridgeDatasetId = "football_owner_1992_fact_safe_club_finance";

constexpr qint64 kFactSafeManifestRows = 723;
constexpr qint64 kFactSafeRuntimeFiles = 38;
constexpr qint64 kFactSafeRuntimeRows = 19577;
constexpr qint64 kFactSafeCompetitionRuntimeFiles = 6;
constexpr qint64 kFactSafeCompetitionRuntimeRows = 578;
constexpr qint64 kFactSafeClubs = 67;
constexpr qint64 kFactSafePlayableClubs = 46;
constexpr qint64 kFactSafePeople = 2076;
constexpr qint64 kFactSafeOpeningRegistrations = 1481;
constexpr qint64 kFactSafeOpeningRelationshipRecords = 1336;
constexpr qint64 kFactSafeScheduledFutureGenerations = 415;
constexpr qint64 kFactSafeFutureFamilyRelationships = 4;
constexpr qint64 kFactSafePremierLeagueScheduleFixtures = 462;
constexpr qint64 kFactSafeCharityShieldScheduleFixtures = 1;
constexpr qint64 kFactSafeCompetitionParticipants = 48;
constexpr qint64 kFactSafeCompetitionDefinitions = 5;
constexpr qint64 kFactSafeCompetitionIdBridges = 5;
constexpr qint64 kFactSafeCompetitionSeasons = 5;
constexpr qint64 kFactSafeFixtureSeedRows = 463;
constexpr qint64 kFactSafeActiveFixtureRows = 463;
constexpr qint64 kFactSafeRosterPersonBridgeRows = 2076;
constexpr qint64 kFactSafeRosterRegistrationBridgeRows = 1481;
constexpr qint64 kFactSafeRosterActiveRegistrationRows = 1245;
constexpr qint64 kFactSafeRosterContractBridgeRows = 1336;
constexpr qint64 kFactSafeRosterActiveContractRows = 1244;
constexpr qint64 kFactSafeRosterLoanReviewRows = 1547;
constexpr qint64 kFactSafeClubFinanceBridgeRows = 67;
constexpr qint64 kFactSafeClubFinanceActiveRows = 46;
constexpr qint64 kFactSafeClubFinanceHistoricalFactRows = 0;

struct LockedWorldTable {
    const char* sourceName;
};

struct FactSafeCompetitionRuntimeFile {
    const char* filePath;
    const char* importRole;
    int loadOrder;
};

struct FactSafeFileCandidate {
    QString filePath;
    QString sourceArea;
    QString importRole;
    bool runtimeImportAllowed {true};
    QString authoritativeVersion;
    int loadOrder {};
    QString expectedSha256;
};

struct ParsedFactSafeFile {
    FactSafeFileCandidate candidate;
    QString sha256;
    QStringList headers;
    std::vector<QStringList> rows;
};

constexpr std::array<LockedWorldTable, 21> kLockedWorldTables {{
    {"club_staff"},
    {"clubs"},
    {"contracts_and_academy_terms"},
    {"data_provenance"},
    {"disabled_start_candidates"},
    {"external_incoming_players"},
    {"external_registered_players"},
    {"future_generations"},
    {"future_transfer_events"},
    {"injury_duration_policy"},
    {"injury_setup_optional"},
    {"metadata"},
    {"out_of_scope_injury_source_events"},
    {"people"},
    {"person_id_aliases"},
    {"person_reference_reviews"},
    {"player_attributes"},
    {"player_profile_overrides"},
    {"staff_people"},
    {"start_registrations"},
    {"unresolved_identity_reviews"},
}};

constexpr std::array<FactSafeCompetitionRuntimeFile, 6> kFactSafeCompetitionRuntimeFileList {{
    {"competitions/runtime_safe/runtime_import_policy.csv", "runtime_competition_policy", 40},
    {"competitions/runtime_safe/competition_definitions.csv", "runtime_competition_definition", 40},
    {"competitions/runtime_safe/fixtures_schedule_seed.csv", "runtime_competition_schedule_seed", 40},
    {"competitions/runtime_safe/participants_known_at_start.csv", "runtime_competition_participants", 40},
    {"competitions/runtime_safe/season_rules.csv", "runtime_competition_rules", 40},
    {"competitions/runtime_safe/v11_club_bridge.csv", "runtime_competition_bridge", 40},
}};

QString toQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toStdString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

GameDate dateOrFallback(const QString& value, const GameDate fallback)
{
    const auto parsed = GameDate::fromIsoString(toStdString(value));
    return parsed.value_or(fallback);
}

std::optional<GameDate> optionalDate(const QString& value)
{
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return GameDate::fromIsoString(toStdString(value));
}

QVariant optionalDateValue(const std::optional<GameDate>& value)
{
    if (!value.has_value()) {
        return {};
    }
    return toQString(value->toIsoString());
}

QString queryError(const QString& context, const QSqlQuery& query)
{
    return context + QStringLiteral(": ") + query.lastError().text();
}

QString lockedWorldTargetTable(const char* sourceName)
{
    return QStringLiteral("fo92_locked_") + QString::fromLatin1(sourceName);
}

QString normalizedFactSafePath(QString path)
{
    return path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
}

QString factSafeSourceArea(const QString& filePath)
{
    if (filePath.startsWith(QStringLiteral("competitions/runtime_safe/"))) {
        return QStringLiteral("competition_runtime_safe");
    }
    const qsizetype slash = filePath.indexOf(QLatin1Char('/'));
    if (slash > 0) {
        return filePath.left(slash);
    }
    return QStringLiteral("root");
}

QString gameplayCompetitionIdForFactSafe(const QString& factSafeCompetitionId)
{
    if (factSafeCompetitionId == QStringLiteral("ENG-PL-1992")) {
        return QStringLiteral("eng-premier-league");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-FLD1-1992")) {
        return QStringLiteral("eng-first-division");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-FA-CUP-1992")) {
        return QStringLiteral("eng-fa-cup");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-LEAGUE-CUP-1992")) {
        return QStringLiteral("eng-league-cup");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-CHARITY-1992")) {
        return QStringLiteral("eng-charity-shield");
    }
    QString normalized = factSafeCompetitionId.toLower();
    normalized.replace(QLatin1Char('_'), QLatin1Char('-'));
    return QStringLiteral("fact-safe-") + normalized;
}

QString gameplaySeasonIdForFactSafe(const QString& factSafeCompetitionId)
{
    if (factSafeCompetitionId == QStringLiteral("ENG-PL-1992")) {
        return QStringLiteral("eng-premier-league-1992-93");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-FLD1-1992")) {
        return QStringLiteral("eng-first-division-1992-93");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-FA-CUP-1992")) {
        return QStringLiteral("eng-fa-cup-1992-93");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-LEAGUE-CUP-1992")) {
        return QStringLiteral("eng-league-cup-1992-93");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-CHARITY-1992")) {
        return QStringLiteral("eng-charity-shield-1992");
    }
    return gameplayCompetitionIdForFactSafe(factSafeCompetitionId) + QStringLiteral("-1992-93");
}

QString gameplayFixtureIdForFactSafe(const QString& factSafeFixtureId)
{
    if (factSafeFixtureId == QStringLiteral("PL92-008")) {
        return QStringLiteral("pl-1992-001");
    }
    QString normalized = factSafeFixtureId.toLower();
    normalized.replace(QLatin1Char('_'), QLatin1Char('-'));
    return QStringLiteral("v545-") + normalized;
}

int competitionLevelForFactSafe(const QString& tier)
{
    bool ok = false;
    const int parsed = tier.toInt(&ok);
    if (ok) {
        return parsed;
    }
    return 0;
}

QString competitionDetailLevelForFactSafe(const QString& factSafeCompetitionId, const int level)
{
    if (factSafeCompetitionId == QStringLiteral("ENG-PL-1992")) {
        return QStringLiteral("playable");
    }
    if (factSafeCompetitionId == QStringLiteral("ENG-FLD1-1992")) {
        return QStringLiteral("detailed");
    }
    if (level > 0) {
        return QStringLiteral("view_only");
    }
    return QStringLiteral("background");
}

QString competitionSeasonStartForFactSafe(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("1992-07-01") : value;
}

QString competitionSeasonEndForFactSafe(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("1993-06-30") : value;
}

int strengthFromReputation(const QString& reputation)
{
    bool ok = false;
    const int parsed = reputation.toInt(&ok);
    if (!ok) {
        return 50;
    }
    return std::clamp(parsed / 100, 35, 90);
}

QString factSafeGameplayPersonIdFallback(QString factSafePersonId)
{
    factSafePersonId = factSafePersonId.trimmed().toLower();
    factSafePersonId.replace(QLatin1Char('_'), QLatin1Char('-'));
    return factSafePersonId.isEmpty()
        ? QStringLiteral("fact-safe-unknown-person")
        : factSafePersonId;
}

QString normalizedPersonName(QString name)
{
    return name.simplified().toLower();
}

QString factSafeProfileConfidence(const QString& confidence)
{
    return confidence.isEmpty()
        ? QStringLiteral("fact_safe_v545_profile")
        : QStringLiteral("fact_safe_v545_") + confidence;
}

QString factSafeDataConfidence(const QString& confidence)
{
    return confidence.isEmpty()
        ? QStringLiteral("fact_safe_v545")
        : QStringLiteral("fact_safe_v545_") + confidence;
}

QString factSafeSecondaryPositions(const QString& primaryPosition, const QString& positionCodes)
{
    const QString trimmedCodes = positionCodes.trimmed();
    if (trimmedCodes.isEmpty() || trimmedCodes == primaryPosition) {
        return {};
    }
    return trimmedCodes;
}

QString factSafeSquadGroupForGameplay(const QString& squadGroup)
{
    if (squadGroup == QStringLiteral("first_team")) {
        return QStringLiteral("First Team");
    }
    if (squadGroup == QStringLiteral("reserve")) {
        return QStringLiteral("Reserve");
    }
    if (squadGroup == QStringLiteral("youth")
        || squadGroup == QStringLiteral("academy")
        || squadGroup == QStringLiteral("future_generation")
        || squadGroup == QStringLiteral("future_activation")) {
        return QStringLiteral("Youth");
    }
    if (squadGroup == QStringLiteral("external_reference")
        || squadGroup == QStringLiteral("external_unknown")) {
        return QStringLiteral("External Reference");
    }
    if (squadGroup == QStringLiteral("inactive_snapshot")
        || squadGroup == QStringLiteral("none")) {
        return QStringLiteral("Inactive");
    }
    return squadGroup.isEmpty() ? QStringLiteral("Unknown") : squadGroup;
}

QString factSafeRegistrationStatusForGameplay(const QString& registrationStatus)
{
    if (registrationStatus == QStringLiteral("senior_registered")) {
        return QStringLiteral("Available");
    }
    if (registrationStatus == QStringLiteral("academy_registered")
        || registrationStatus == QStringLiteral("trainee_registered")) {
        return QStringLiteral("Trainee");
    }
    if (registrationStatus == QStringLiteral("youth_registered")) {
        return QStringLiteral("Youth Player");
    }
    if (registrationStatus == QStringLiteral("schoolboy_registered")) {
        return QStringLiteral("Schoolboy");
    }
    if (registrationStatus == QStringLiteral("semi_professional_registered")) {
        return QStringLiteral("Semi-Professional");
    }
    if (registrationStatus.startsWith(QStringLiteral("not_"))
        || registrationStatus == QStringLiteral("future_activation_scheduled")) {
        return QStringLiteral("Inactive");
    }
    return registrationStatus.isEmpty() ? QStringLiteral("Unknown") : registrationStatus;
}

QString factSafeContractRoleForGameplay(const QString& squadRole)
{
    if (squadRole == QStringLiteral("first_team")) {
        return QStringLiteral("First Team");
    }
    if (squadRole == QStringLiteral("reserve")) {
        return QStringLiteral("Reserve");
    }
    if (squadRole == QStringLiteral("youth")
        || squadRole == QStringLiteral("academy")) {
        return QStringLiteral("Prospect");
    }
    return squadRole.isEmpty() ? QStringLiteral("Unknown") : squadRole;
}

bool factSafeRegistrationPolicyAllowsActiveImport(const QString& policy)
{
    return policy == QStringLiteral("may_import_as_verified_opening_membership_only")
        || policy == QStringLiteral("may_import_corrected_opening_membership; retain source and confidence")
        || policy == QStringLiteral("may_import_as_verified_opening_membership; create 1992-07-29 transfer event separately")
        || policy == QStringLiteral("may_import_corrected_opening_membership; retain field-level statuses and source")
        || policy == QStringLiteral("import_club_membership_as_historical_fact_other_fields_per_field_status");
}

bool factSafeRegistrationStatusIsActive(const QString& registrationStatus)
{
    return registrationStatus == QStringLiteral("senior_registered")
        || registrationStatus == QStringLiteral("academy_registered")
        || registrationStatus == QStringLiteral("trainee_registered")
        || registrationStatus == QStringLiteral("youth_registered")
        || registrationStatus == QStringLiteral("schoolboy_registered")
        || registrationStatus == QStringLiteral("semi_professional_registered");
}

bool factSafeTrue(const QString& value)
{
    return value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

qint64 factSafeGbpToPence(const QString& value, bool* ok)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        *ok = false;
        return 0;
    }
    bool parsed = false;
    const double pounds = trimmed.toDouble(&parsed);
    *ok = parsed && pounds >= 0.0;
    return *ok ? static_cast<qint64>(std::llround(pounds * 100.0)) : 0;
}

bool factSafeLoanReviewMarker(const QString& text)
{
    return text.contains(QStringLiteral("loan"), Qt::CaseInsensitive);
}

int csvColumnIndex(const QStringList& headers, const QString& columnName)
{
    for (int index = 0; index < headers.size(); ++index) {
        if (headers.at(index) == columnName) {
            return index;
        }
    }
    return -1;
}

QString csvValueAt(const QStringList& row, const int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= row.size()) {
        return QStringLiteral("");
    }
    const QString value = row.at(columnIndex);
    return value.isNull() ? QStringLiteral("") : value;
}

struct CsvParseResult {
    std::vector<QStringList> records;
    QString error;
};

CsvParseResult parseCsvRecords(const QString& text)
{
    CsvParseResult result;
    QStringList record;
    QString field;
    bool insideQuotes = false;

    const auto finishRecord = [&]() {
        record.append(field);
        field.clear();
        result.records.push_back(record);
        record.clear();
    };

    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);

        if (insideQuotes) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < text.size() && text.at(index + 1) == QLatin1Char('"')) {
                    field += text.at(++index);
                    continue;
                }
                insideQuotes = false;
                continue;
            }
            field += character;
            continue;
        }

        if (character == QLatin1Char('"')) {
            insideQuotes = true;
            continue;
        }
        if (character == QLatin1Char(',')) {
            record.append(field);
            field.clear();
            continue;
        }
        if (character == QLatin1Char('\r') || character == QLatin1Char('\n')) {
            if (character == QLatin1Char('\r')
                && index + 1 < text.size()
                && text.at(index + 1) == QLatin1Char('\n')) {
                ++index;
            }
            finishRecord();
            continue;
        }
        field += character;
    }

    if (insideQuotes) {
        result.error = QStringLiteral("닫히지 않은 CSV 따옴표가 있습니다");
        return result;
    }
    if (!field.isEmpty() || !record.isEmpty()) {
        finishRecord();
    }

    if (!result.records.empty() && !result.records.front().isEmpty()) {
        QString& firstHeader = result.records.front().front();
        if (!firstHeader.isEmpty() && firstHeader.front() == QChar(0xfeff)) {
            firstHeader.remove(0, 1);
        }
    }
    return result;
}

QString rowToJson(const QStringList& headers, const QStringList& row)
{
    QJsonObject object;
    for (int index = 0; index < headers.size(); ++index) {
        object.insert(headers.at(index), csvValueAt(row, index));
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

bool loadFactSafeCsvFile(
    const FactSafeFileCandidate& candidate,
    ParsedFactSafeFile* parsed,
    QString* failure)
{
    const QString resourcePath =
        QString::fromLatin1(kFactSafeHandoffResourceRoot)
        + QLatin1Char('/')
        + candidate.filePath;
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        *failure = QStringLiteral("v5.4.5 CSV 리소스를 열 수 없습니다: ") + resourcePath;
        return false;
    }

    const QByteArray bytes = file.readAll();
    file.close();
    const QString actualSha =
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    if (!candidate.expectedSha256.isEmpty()
        && candidate.expectedSha256.compare(actualSha, Qt::CaseInsensitive) != 0) {
        *failure = QStringLiteral("v5.4.5 SHA-256 mismatch: ")
            + candidate.filePath
            + QStringLiteral(" expected ")
            + candidate.expectedSha256
            + QStringLiteral(", actual ")
            + actualSha;
        return false;
    }

    CsvParseResult csv = parseCsvRecords(QString::fromUtf8(bytes));
    if (!csv.error.isEmpty()) {
        *failure = QStringLiteral("v5.4.5 CSV 파싱 실패: ")
            + candidate.filePath
            + QStringLiteral(": ")
            + csv.error;
        return false;
    }
    if (csv.records.empty()) {
        *failure = QStringLiteral("v5.4.5 CSV 헤더가 없습니다: ") + candidate.filePath;
        return false;
    }

    ParsedFactSafeFile output;
    output.candidate = candidate;
    output.sha256 = actualSha;
    output.headers = csv.records.front();
    output.rows.reserve(csv.records.size() - 1);
    for (std::size_t index = 1; index < csv.records.size(); ++index) {
        if (csv.records.at(index).size() != output.headers.size()) {
            *failure = QStringLiteral("v5.4.5 CSV 열 수 mismatch: ")
                + candidate.filePath
                + QStringLiteral(" row ")
                + QString::number(static_cast<qulonglong>(index))
                + QStringLiteral(" expected ")
                + QString::number(output.headers.size())
                + QStringLiteral(", actual ")
                + QString::number(csv.records.at(index).size());
            return false;
        }
        output.rows.push_back(csv.records.at(index));
    }

    *parsed = std::move(output);
    return true;
}

std::string legacyTransferPursuitStageKey(const TransferPursuitStage stage)
{
    switch (stage) {
    case TransferPursuitStage::AwaitingPlayerResponse:
    case TransferPursuitStage::PlayerCountered:
    case TransferPursuitStage::AwaitingMedical:
    case TransferPursuitStage::AwaitingRegistration:
        return "awaiting_player_terms";
    case TransferPursuitStage::MedicalFailed:
    case TransferPursuitStage::RegistrationFailed:
        return "player_rejected";
    default:
        return transferPursuitStageKey(stage);
    }
}

PlayerContractTerms contractTermsFromQuery(
    const QSqlQuery& query,
    const int firstColumn)
{
    return {
        query.value(firstColumn).toLongLong(),
        query.value(firstColumn + 1).toInt(),
        query.value(firstColumn + 2).toLongLong(),
        query.value(firstColumn + 3).toLongLong(),
        query.value(firstColumn + 4).toLongLong(),
        query.value(firstColumn + 5).toLongLong(),
        query.value(firstColumn + 6).toLongLong(),
        query.value(firstColumn + 7).toLongLong(),
        toStdString(query.value(firstColumn + 8).toString())
    };
}

LoanTerms loanTermsFromQuery(
    const QSqlQuery& query,
    const int firstColumn,
    const GameDate& fallbackDate)
{
    return {
        query.value(firstColumn).toLongLong(),
        query.value(firstColumn + 1).toInt(),
        toStdString(query.value(firstColumn + 2).toString()),
        dateOrFallback(
            query.value(firstColumn + 3).toString(),
            fallbackDate),
        query.value(firstColumn + 4).toBool(),
        query.value(firstColumn + 5).toInt(),
        toStdString(query.value(firstColumn + 6).toString()),
        query.value(firstColumn + 7).toLongLong(),
        query.value(firstColumn + 8).toBool(),
        query.value(firstColumn + 9).toLongLong(),
        toStdString(query.value(firstColumn + 10).toString())
    };
}

} // namespace

SqliteGameRepository::SqliteGameRepository(QString databasePath)
    : databasePath_(std::move(databasePath))
    , connectionName_(QStringLiteral("football-owner-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
    , database_(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_))
{
    database_.setDatabaseName(databasePath_);
}

SqliteGameRepository::~SqliteGameRepository()
{
    if (database_.isOpen()) {
        database_.close();
    }
    database_ = QSqlDatabase {};
    QSqlDatabase::removeDatabase(connectionName_);
}

bool SqliteGameRepository::initialize(QString* errorMessage)
{
    if (initialized_) {
        return true;
    }

    if (!database_.isOpen() && !database_.open()) {
        return fail(errorMessage, QStringLiteral("SQLite 데이터베이스를 열 수 없습니다: ") + database_.lastError().text());
    }

    QSqlQuery pragma(database_);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        return fail(errorMessage, queryError(QStringLiteral("foreign_keys 설정 실패"), pragma));
    }
    pragma.finish();
    if (!pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        return fail(errorMessage, queryError(QStringLiteral("WAL 설정 실패"), pragma));
    }
    pragma.finish();
    if (!pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
        return fail(errorMessage, queryError(QStringLiteral("busy_timeout 설정 실패"), pragma));
    }
    pragma.finish();

    if (!runResourceScript(QStringLiteral(":/database/migrations/001_initial.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/002_squads_and_transfers.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/003_named_save_files.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/004_transfer_market.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/005_detailed_transfers.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/006_scouting_and_recruitment.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/007_loans_and_contract_renewals.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/008_ai_transfer_market.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/009_locked_world_baseline.sql"), errorMessage)) {
        return false;
    }
    if (!importLockedWorldBaseline(errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/seeds/1992_93_manchester_united.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/seeds/1992_93_transfer_market.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/seeds/1992_93_scouting.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/seeds/1992_93_loans_and_contracts.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/010_competition_bridge_and_standings.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/011_squad_planning_opinions.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/012_operating_meetings.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/013_scouting_directives.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/014_fact_safe_handoff_v545.sql"), errorMessage)) {
        return false;
    }
    if (!importFactSafeHandoffV545(errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/015_fact_safe_competition_schedule_bridge.sql"), errorMessage)) {
        return false;
    }
    if (!importFactSafeCompetitionScheduleBridgeV545(errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/016_fact_safe_roster_activation_bridge.sql"), errorMessage)) {
        return false;
    }
    if (!importFactSafeRosterActivationBridgeV545(errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/017_owner_reputation_and_fan_trust.sql"), errorMessage)) {
        return false;
    }
    if (!runResourceScript(QStringLiteral(":/database/migrations/018_fact_safe_club_finance_model_bridge.sql"), errorMessage)) {
        return false;
    }
    if (!importFactSafeClubFinanceModelBridgeV545(errorMessage)) {
        return false;
    }
    initialized_ = true;
    return true;
}

std::optional<GameState> SqliteGameRepository::loadOrCreateCareer(QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return std::nullopt;
    }

    GameState state = makeManchesterUnitedPrototype();
    bool created = false;
    bool hasPersistedTransferProposals = false;
    bool hasPersistedTransferPursuits = false;
    bool hasPersistedOutgoingOffers = false;
    bool hasPersistedDetailedTransferData = false;
    bool hasPersistedScoutingReports = false;
    bool hasPersistedScoutFamiliarities = false;
    bool hasPersistedRecruitmentPlans = false;
    bool hasPersistedLoanDeals = false;
    bool hasPersistedContractRenewals = false;
    bool hasPersistedAiMarketState = false;
    bool hasPersistedOwnerReputationState = false;
    bool hasPersistedFanTendencyState = false;
    bool upgradedExistingSave = false;

    QSqlQuery saveQuery(database_);
    if (!saveQuery.exec(QStringLiteral(
            "SELECT id, display_name, game_date, owner_name, rng_seed "
            "FROM save_games ORDER BY id LIMIT 1"))) {
        (void)fail(errorMessage, queryError(QStringLiteral("세이브 조회 실패"), saveQuery));
        return std::nullopt;
    }

    if (saveQuery.next()) {
        state.saveId = saveQuery.value(0).toInt();
        state.saveName = toStdString(saveQuery.value(1).toString());
        state.currentDate = dateOrFallback(saveQuery.value(2).toString(), state.currentDate);
        state.ownerName = toStdString(saveQuery.value(3).toString());
        state.rngSeed = saveQuery.value(4).toUInt();
        saveQuery.finish();
    } else {
        saveQuery.finish();
        QSqlQuery insert(database_);
        insert.prepare(QStringLiteral(
            "INSERT INTO save_games "
            "(display_name, game_date, player_club_id, owner_name, rng_seed) "
            "VALUES (:name, :date, :club, :owner, :seed)"));
        insert.bindValue(QStringLiteral(":name"), toQString(state.saveName));
        insert.bindValue(QStringLiteral(":date"), toQString(state.currentDate.toIsoString()));
        insert.bindValue(QStringLiteral(":club"), toQString(state.club.id));
        insert.bindValue(QStringLiteral(":owner"), toQString(state.ownerName));
        insert.bindValue(QStringLiteral(":seed"), static_cast<qulonglong>(state.rngSeed));
        if (!insert.exec()) {
            (void)fail(errorMessage, queryError(QStringLiteral("새 세이브 생성 실패"), insert));
            return std::nullopt;
        }
        state.saveId = insert.lastInsertId().toInt();
        created = true;
    }

    QSqlQuery clubQuery(database_);
    clubQuery.prepare(QStringLiteral(
        "SELECT c.name, c.short_name, cp.name, h.cash_pence, h.transfer_budget_pence, "
        "h.weekly_wage_budget_pence, h.weekly_wage_commitment_pence, h.debt_pence, "
        "h.reputation, h.squad_strength, h.data_confidence "
        "FROM clubs c "
        "JOIN competitions cp ON cp.id = c.competition_id "
        "JOIN historical_club_snapshots h ON h.club_id = c.id "
        "WHERE c.id = :club AND h.snapshot_date = '1992-07-01'"));
    clubQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!clubQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("역사 구단 스냅샷 조회 실패"), clubQuery));
        return std::nullopt;
    }
    if (clubQuery.next()) {
        state.club.name = toStdString(clubQuery.value(0).toString());
        state.club.shortName = toStdString(clubQuery.value(1).toString());
        state.club.leagueName = toStdString(clubQuery.value(2).toString());
        state.club.finance.cashPence = clubQuery.value(3).toLongLong();
        state.club.finance.transferBudgetPence = clubQuery.value(4).toLongLong();
        state.club.finance.weeklyWageBudgetPence = clubQuery.value(5).toLongLong();
        state.club.finance.weeklyWageCommitmentPence = clubQuery.value(6).toLongLong();
        state.club.finance.debtPence = clubQuery.value(7).toLongLong();
        state.club.reputation = clubQuery.value(8).toInt();
        state.club.squadStrength = clubQuery.value(9).toInt();
        state.club.finance.confidence = toStdString(clubQuery.value(10).toString());
    }
    clubQuery.finish();

    QSqlQuery eraRuleQuery(database_);
    eraRuleQuery.prepare(QStringLiteral(
        "SELECT rule_profile_id, transfer_windows_active, "
        "registration_cutoff_month, registration_cutoff_day, "
        "free_movement_after_contract, domestic_loan_model, data_confidence "
        "FROM transfer_rule_profiles "
        "WHERE competition_id = 'eng-premier-league' "
        "AND effective_from <= :date "
        "AND (effective_to IS NULL OR effective_to >= :date) "
        "ORDER BY effective_from DESC LIMIT 1"));
    eraRuleQuery.bindValue(
        QStringLiteral(":date"),
        toQString(state.currentDate.toIsoString()));
    if (!eraRuleQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(QStringLiteral("시대별 이적 규정 조회 실패"), eraRuleQuery));
        return std::nullopt;
    }
    if (eraRuleQuery.next()) {
        state.eraRules = {
            toStdString(eraRuleQuery.value(0).toString()),
            eraRuleQuery.value(1).toBool(),
            eraRuleQuery.value(2).toInt(),
            eraRuleQuery.value(3).toInt(),
            eraRuleQuery.value(4).toBool(),
            toStdString(eraRuleQuery.value(5).toString()),
            toStdString(eraRuleQuery.value(6).toString())
        };
    }
    eraRuleQuery.finish();

    QSqlQuery clubDelta(database_);
    clubDelta.prepare(QStringLiteral(
        "SELECT cash_pence, transfer_budget_pence, weekly_wage_budget_pence, "
        "weekly_wage_commitment_pence, debt_pence, manager_board_confidence "
        "FROM save_club_deltas WHERE save_id = :save AND club_id = :club"));
    clubDelta.bindValue(QStringLiteral(":save"), state.saveId);
    clubDelta.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!clubDelta.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("구단 세이브 델타 조회 실패"), clubDelta));
        return std::nullopt;
    }
    bool hasClubDelta = false;
    if (clubDelta.next()) {
        hasClubDelta = true;
        state.club.finance.cashPence = clubDelta.value(0).toLongLong();
        state.club.finance.transferBudgetPence = clubDelta.value(1).toLongLong();
        state.club.finance.weeklyWageBudgetPence = clubDelta.value(2).toLongLong();
        state.club.finance.weeklyWageCommitmentPence = clubDelta.value(3).toLongLong();
        state.club.finance.debtPence = clubDelta.value(4).toLongLong();
        state.club.manager.boardConfidence = clubDelta.value(5).toInt();
    }
    clubDelta.finish();

    QSqlQuery managerQuery(database_);
    managerQuery.prepare(QStringLiteral(
        "SELECT p.full_name, s.role, s.board_confidence, s.tactical_identity, s.current_assessment "
        "FROM historical_staff_roles s "
        "JOIN people p ON p.id = s.person_id "
        "WHERE s.club_id = :club AND s.snapshot_date = '1992-07-01' AND s.role = 'Manager' "
        "LIMIT 1"));
    managerQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!managerQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("감독 스냅샷 조회 실패"), managerQuery));
        return std::nullopt;
    }
    if (managerQuery.next()) {
        state.club.manager.name = toStdString(managerQuery.value(0).toString());
        state.club.manager.role = toStdString(managerQuery.value(1).toString());
        if (!hasClubDelta) {
            state.club.manager.boardConfidence = managerQuery.value(2).toInt();
        }
        state.club.manager.tacticalIdentity = toStdString(managerQuery.value(3).toString());
        state.club.manager.currentAssessment = toStdString(managerQuery.value(4).toString());
    }
    managerQuery.finish();

    QSqlQuery playerQuery(database_);
    playerQuery.prepare(QStringLiteral(
        "SELECT p.id, p.full_name, p.birth_date, p.nationality, pp.primary_position, "
        "r.squad_group, r.registration_status, r.data_confidence, "
        "hp.joined_date, hp.departed_date "
        "FROM historical_registrations r "
        "JOIN people p ON p.id = r.person_id "
        "JOIN player_profiles pp ON pp.person_id = p.id "
        "LEFT JOIN historical_registration_periods hp "
        "ON hp.person_id = p.id AND hp.club_id = r.club_id "
        "WHERE r.club_id = :club AND r.snapshot_date = '1992-07-01' "
        "ORDER BY CASE r.squad_group "
        "WHEN 'First Team' THEN 1 WHEN 'Reserve' THEN 2 WHEN 'Youth' THEN 3 ELSE 4 END, "
        "CASE pp.primary_position "
        "WHEN 'GK' THEN 1 WHEN 'RB' THEN 2 WHEN 'CB' THEN 3 WHEN 'LB' THEN 4 "
        "WHEN 'DF/MF' THEN 5 WHEN 'RB/CM' THEN 6 WHEN 'CM' THEN 7 WHEN 'MF' THEN 8 "
        "WHEN 'LM' THEN 9 WHEN 'RM' THEN 10 WHEN 'FW' THEN 11 WHEN 'ST' THEN 12 ELSE 13 END, "
        "p.full_name"));
    playerQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!playerQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("선수단 조회 실패"), playerQuery));
        return std::nullopt;
    }
    std::vector<Player> loadedPlayers;
    while (playerQuery.next()) {
        loadedPlayers.push_back({
            toStdString(playerQuery.value(0).toString()),
            toStdString(playerQuery.value(1).toString()),
            optionalDate(playerQuery.value(2).toString()),
            toStdString(playerQuery.value(3).toString()),
            toStdString(playerQuery.value(4).toString()),
            toStdString(playerQuery.value(5).toString()),
            toStdString(playerQuery.value(6).toString()),
            toStdString(playerQuery.value(7).toString()),
            optionalDate(playerQuery.value(8).toString()),
            optionalDate(playerQuery.value(9).toString())
        });
    }
    playerQuery.finish();
    if (!loadedPlayers.empty()) {
        state.players = std::move(loadedPlayers);
    }

    QSqlQuery savedPlayerQuery(database_);
    savedPlayerQuery.prepare(QStringLiteral(
        "SELECT p.id, p.full_name, p.birth_date, p.nationality, pp.primary_position, "
        "r.squad_group, r.registration_status, r.data_confidence, r.joined_date "
        "FROM save_player_registrations r "
        "JOIN people p ON p.id = r.person_id "
        "JOIN player_profiles pp ON pp.person_id = p.id "
        "WHERE r.save_id = :save AND r.club_id = :club "
        "ORDER BY r.joined_date, p.full_name"));
    savedPlayerQuery.bindValue(QStringLiteral(":save"), state.saveId);
    savedPlayerQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!savedPlayerQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("영입 선수 등록 조회 실패"), savedPlayerQuery));
        return std::nullopt;
    }
    while (savedPlayerQuery.next()) {
        const std::string personId = toStdString(savedPlayerQuery.value(0).toString());
        const auto duplicate = std::find_if(
            state.players.begin(),
            state.players.end(),
            [&personId](const Player& player) {
                return player.id == personId;
            });
        if (duplicate != state.players.end()) {
            continue;
        }
        state.players.push_back({
            personId,
            toStdString(savedPlayerQuery.value(1).toString()),
            optionalDate(savedPlayerQuery.value(2).toString()),
            toStdString(savedPlayerQuery.value(3).toString()),
            toStdString(savedPlayerQuery.value(4).toString()),
            toStdString(savedPlayerQuery.value(5).toString()),
            toStdString(savedPlayerQuery.value(6).toString()),
            toStdString(savedPlayerQuery.value(7).toString()),
            optionalDate(savedPlayerQuery.value(8).toString()),
            std::nullopt
        });
    }
    savedPlayerQuery.finish();

    QSqlQuery departureQuery(database_);
    departureQuery.prepare(QStringLiteral(
        "SELECT person_id FROM save_player_departures "
        "WHERE save_id = :save AND from_club_id = :club"));
    departureQuery.bindValue(QStringLiteral(":save"), state.saveId);
    departureQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!departureQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("판매 선수 이탈 기록 조회 실패"), departureQuery));
        return std::nullopt;
    }
    while (departureQuery.next()) {
        const std::string departedPlayerId = toStdString(departureQuery.value(0).toString());
        state.players.erase(
            std::remove_if(
                state.players.begin(),
                state.players.end(),
                [&departedPlayerId](const Player& player) {
                    return player.id == departedPlayerId;
                }),
            state.players.end());
    }
    departureQuery.finish();

    QSqlQuery candidateQuery(database_);
    if (!candidateQuery.exec(QStringLiteral(
            "SELECT p.id, p.full_name, p.birth_date, p.nationality, pp.primary_position, "
            "p.data_confidence, c.id, c.name, m.recommendation_rank, "
            "m.current_ability_low, m.current_ability_high, m.potential_low, "
            "m.potential_high, m.manager_priority, m.player_interest, "
            "m.market_value_pence, m.opening_offer_pence, m.seller_minimum_pence, "
            "m.asking_price_pence, m.expected_weekly_wage_pence, "
            "m.expected_contract_years, m.role_fit, m.scout_summary, m.availability, "
            "COALESCE(rc.id, ''), COALESCE(rc.name, ''), m.rival_bid_pence, "
            "m.data_confidence "
            "FROM transfer_market_profiles m "
            "JOIN people p ON p.id = m.person_id "
            "JOIN player_profiles pp ON pp.person_id = p.id "
            "JOIN clubs c ON c.id = m.current_club_id "
            "LEFT JOIN clubs rc ON rc.id = m.rival_club_id "
            "WHERE m.snapshot_date = '1992-07-01' "
            "ORDER BY m.recommendation_rank, p.full_name"))) {
        (void)fail(errorMessage, queryError(QStringLiteral("이적시장 후보 조회 실패"), candidateQuery));
        return std::nullopt;
    }
    std::vector<TransferCandidate> loadedCandidates;
    while (candidateQuery.next()) {
        loadedCandidates.push_back({
            {
                toStdString(candidateQuery.value(0).toString()),
                toStdString(candidateQuery.value(1).toString()),
                optionalDate(candidateQuery.value(2).toString()),
                toStdString(candidateQuery.value(3).toString()),
                toStdString(candidateQuery.value(4).toString()),
                "Transfer Target",
                "Available",
                toStdString(candidateQuery.value(5).toString())
            },
            toStdString(candidateQuery.value(6).toString()),
            toStdString(candidateQuery.value(7).toString()),
            candidateQuery.value(8).toInt(),
            candidateQuery.value(9).toInt(),
            candidateQuery.value(10).toInt(),
            candidateQuery.value(11).toInt(),
            candidateQuery.value(12).toInt(),
            candidateQuery.value(13).toInt(),
            candidateQuery.value(14).toInt(),
            candidateQuery.value(15).toLongLong(),
            candidateQuery.value(16).toLongLong(),
            candidateQuery.value(17).toLongLong(),
            candidateQuery.value(18).toLongLong(),
            candidateQuery.value(19).toLongLong(),
            candidateQuery.value(20).toInt(),
            toStdString(candidateQuery.value(21).toString()),
            toStdString(candidateQuery.value(22).toString()),
            toStdString(candidateQuery.value(23).toString()),
            toStdString(candidateQuery.value(24).toString()),
            toStdString(candidateQuery.value(25).toString()),
            candidateQuery.value(26).toLongLong(),
            toStdString(candidateQuery.value(27).toString())
        });
    }
    candidateQuery.finish();
    if (!loadedCandidates.empty()) {
        state.transferCandidates = std::move(loadedCandidates);
    }

    QSqlQuery scoutQuery(database_);
    scoutQuery.prepare(QStringLiteral(
        "SELECT scout_id, full_name, role, region_specialism, "
        "judging_current_ability, judging_potential, adaptability, "
        "assignment_capacity, data_confidence "
        "FROM scouting_staff_profiles "
        "WHERE club_id = :club AND snapshot_date = '1992-07-01' "
        "ORDER BY role, full_name"));
    scoutQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!scoutQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("스카우트 조직 조회 실패"), scoutQuery));
        return std::nullopt;
    }
    std::vector<ScoutProfile> loadedScouts;
    while (scoutQuery.next()) {
        loadedScouts.push_back({
            toStdString(scoutQuery.value(0).toString()),
            toStdString(scoutQuery.value(1).toString()),
            toStdString(scoutQuery.value(2).toString()),
            toStdString(scoutQuery.value(3).toString()),
            scoutQuery.value(4).toInt(),
            scoutQuery.value(5).toInt(),
            scoutQuery.value(6).toInt(),
            scoutQuery.value(7).toInt(),
            toStdString(scoutQuery.value(8).toString())
        });
    }
    scoutQuery.finish();
    if (!loadedScouts.empty()) {
        state.scouts = std::move(loadedScouts);
    }

    QSqlQuery scoutFamiliarityQuery(database_);
    scoutFamiliarityQuery.prepare(QStringLiteral(
        "SELECT scout_id, category, familiarity_key, level, "
        "updated_date, data_confidence "
        "FROM save_scout_familiarities WHERE save_id = :save "
        "ORDER BY scout_id, category, familiarity_key"));
    scoutFamiliarityQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!scoutFamiliarityQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("스카우터 친숙도 조회 실패"),
                scoutFamiliarityQuery));
        return std::nullopt;
    }
    state.scoutFamiliarities.clear();
    while (scoutFamiliarityQuery.next()) {
        hasPersistedScoutFamiliarities = true;
        state.scoutFamiliarities.push_back({
            toStdString(scoutFamiliarityQuery.value(0).toString()),
            toStdString(scoutFamiliarityQuery.value(1).toString()),
            toStdString(scoutFamiliarityQuery.value(2).toString()),
            scoutFamiliarityQuery.value(3).toInt(),
            dateOrFallback(
                scoutFamiliarityQuery.value(4).toString(),
                state.currentDate),
            toStdString(scoutFamiliarityQuery.value(5).toString())
        });
    }
    scoutFamiliarityQuery.finish();
    if (!hasPersistedScoutFamiliarities) {
        ensureDefaultScoutFamiliarities(state);
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    QSqlQuery transferQuery(database_);
    transferQuery.prepare(QStringLiteral(
        "SELECT t.proposal_id, p.id, p.full_name, p.birth_date, p.nationality, "
        "pp.primary_position, p.data_confidence, c.id, c.name, "
        "t.fee_pence, t.weekly_wage_pence, t.contract_years, t.requested_by, "
        "t.rationale, t.proposed_date, t.decision_deadline, t.decision_date, "
        "t.join_date, t.status, t.data_confidence "
        "FROM save_transfer_proposals t "
        "JOIN people p ON p.id = t.target_person_id "
        "JOIN player_profiles pp ON pp.person_id = p.id "
        "JOIN clubs c ON c.id = t.from_club_id "
        "WHERE t.save_id = :save ORDER BY t.proposed_date, t.proposal_id"));
    transferQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!transferQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("영입 승인 안건 조회 실패"), transferQuery));
        return std::nullopt;
    }
    std::vector<TransferProposal> loadedTransferProposals;
    while (transferQuery.next()) {
        loadedTransferProposals.push_back({
            toStdString(transferQuery.value(0).toString()),
            {
                toStdString(transferQuery.value(1).toString()),
                toStdString(transferQuery.value(2).toString()),
                optionalDate(transferQuery.value(3).toString()),
                toStdString(transferQuery.value(4).toString()),
                toStdString(transferQuery.value(5).toString()),
                "Transfer Target",
                "Available",
                toStdString(transferQuery.value(6).toString())
            },
            toStdString(transferQuery.value(7).toString()),
            toStdString(transferQuery.value(8).toString()),
            transferQuery.value(9).toLongLong(),
            transferQuery.value(10).toLongLong(),
            transferQuery.value(11).toInt(),
            toStdString(transferQuery.value(12).toString()),
            toStdString(transferQuery.value(13).toString()),
            dateOrFallback(transferQuery.value(14).toString(), state.currentDate),
            dateOrFallback(transferQuery.value(15).toString(), state.currentDate),
            optionalDate(transferQuery.value(16).toString()),
            optionalDate(transferQuery.value(17).toString()),
            transferStatusFromKey(toStdString(transferQuery.value(18).toString())),
            toStdString(transferQuery.value(19).toString()),
            {},
            {},
            "not_checked",
            "not_checked"
        });
    }
    transferQuery.finish();
    if (!loadedTransferProposals.empty()) {
        state.transferProposals = std::move(loadedTransferProposals);
        hasPersistedTransferProposals = true;
    }
    for (auto& proposal : state.transferProposals) {
        proposal.feeTerms = effectiveFeeTerms(proposal.feePence, proposal.feeTerms);
        proposal.contractTerms = effectiveContractTerms(
            proposal.weeklyWagePence,
            proposal.contractYears,
            proposal.contractTerms);
    }

    QSqlQuery proposalDetailsQuery(database_);
    proposalDetailsQuery.prepare(QStringLiteral(
        "SELECT proposal_id, upfront_fee_pence, installment_total_pence, "
        "installment_count, appearance_bonus_pence, appearance_threshold, "
        "sell_on_percentage, signing_bonus_pence, loyalty_bonus_pence, "
        "appearance_fee_pence, goal_bonus_pence, agent_fee_pence, "
        "release_clause_pence, squad_role, medical_status, registration_status "
        "FROM save_transfer_proposal_details WHERE save_id = :save"));
    proposalDetailsQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!proposalDetailsQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("상세 영입 승인 조건 조회 실패"), proposalDetailsQuery));
        return std::nullopt;
    }
    while (proposalDetailsQuery.next()) {
        hasPersistedDetailedTransferData = true;
        const std::string proposalId =
            toStdString(proposalDetailsQuery.value(0).toString());
        const auto proposal = std::find_if(
            state.transferProposals.begin(),
            state.transferProposals.end(),
            [&proposalId](const TransferProposal& item) {
                return item.id == proposalId;
            });
        if (proposal == state.transferProposals.end()) {
            continue;
        }
        proposal->feeTerms = {
            proposalDetailsQuery.value(1).toLongLong(),
            proposalDetailsQuery.value(2).toLongLong(),
            proposalDetailsQuery.value(3).toInt(),
            proposalDetailsQuery.value(4).toLongLong(),
            proposalDetailsQuery.value(5).toInt(),
            proposalDetailsQuery.value(6).toInt()
        };
        proposal->contractTerms = {
            proposal->weeklyWagePence,
            proposal->contractYears,
            proposalDetailsQuery.value(7).toLongLong(),
            proposalDetailsQuery.value(8).toLongLong(),
            proposalDetailsQuery.value(9).toLongLong(),
            proposalDetailsQuery.value(10).toLongLong(),
            proposalDetailsQuery.value(11).toLongLong(),
            proposalDetailsQuery.value(12).toLongLong(),
            toStdString(proposalDetailsQuery.value(13).toString())
        };
        proposal->medicalStatus =
            toStdString(proposalDetailsQuery.value(14).toString());
        proposal->registrationStatus =
            toStdString(proposalDetailsQuery.value(15).toString());
        proposal->feePence = guaranteedTransferFee(proposal->feeTerms);
    }
    proposalDetailsQuery.finish();

    QSqlQuery pursuitQuery(database_);
    pursuitQuery.prepare(QStringLiteral(
        "SELECT p.pursuit_id, p.target_person_id, p.stage, p.current_offer_pence, "
        "p.club_counter_pence, p.negotiation_round, p.started_date, "
        "p.next_action_date, p.decision_deadline, p.last_update_date, "
        "p.response_note, p.rival_bid_active, COALESCE(rc.id, ''), "
        "COALESCE(rc.name, ''), p.rival_bid_pence, COALESCE(p.linked_proposal_id, '') "
        "FROM save_transfer_pursuits p "
        "LEFT JOIN clubs rc ON rc.id = p.rival_club_id "
        "WHERE p.save_id = :save ORDER BY p.started_date, p.pursuit_id"));
    pursuitQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!pursuitQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("영입 협상 진행 상태 조회 실패"), pursuitQuery));
        return std::nullopt;
    }
    std::vector<TransferPursuit> loadedPursuits;
    while (pursuitQuery.next()) {
        loadedPursuits.push_back({
            toStdString(pursuitQuery.value(0).toString()),
            toStdString(pursuitQuery.value(1).toString()),
            transferPursuitStageFromKey(toStdString(pursuitQuery.value(2).toString())),
            pursuitQuery.value(3).toLongLong(),
            pursuitQuery.value(4).toLongLong(),
            pursuitQuery.value(5).toInt(),
            dateOrFallback(pursuitQuery.value(6).toString(), state.currentDate),
            dateOrFallback(pursuitQuery.value(7).toString(), state.currentDate),
            dateOrFallback(pursuitQuery.value(8).toString(), state.currentDate),
            optionalDate(pursuitQuery.value(9).toString()),
            toStdString(pursuitQuery.value(10).toString()),
            pursuitQuery.value(11).toBool(),
            toStdString(pursuitQuery.value(12).toString()),
            toStdString(pursuitQuery.value(13).toString()),
            pursuitQuery.value(14).toLongLong(),
            toStdString(pursuitQuery.value(15).toString()),
            {},
            {},
            {},
            {},
            0,
            false,
            0,
            "not_started",
            "not_checked"
        });
    }
    pursuitQuery.finish();
    if (!loadedPursuits.empty()) {
        state.transferPursuits = std::move(loadedPursuits);
        hasPersistedTransferPursuits = true;
    }
    for (auto& pursuit : state.transferPursuits) {
        pursuit.offerTerms = effectiveFeeTerms(
            pursuit.currentOfferPence,
            pursuit.offerTerms);
        if (pursuit.clubCounterPence > 0) {
            pursuit.clubCounterTerms = effectiveFeeTerms(
                pursuit.clubCounterPence,
                pursuit.clubCounterTerms);
        }
        const auto candidate = std::find_if(
            state.transferCandidates.begin(),
            state.transferCandidates.end(),
            [&pursuit](const TransferCandidate& item) {
                return item.player.id == pursuit.candidatePlayerId;
            });
        if (candidate != state.transferCandidates.end()) {
            pursuit.playerOfferTerms = effectiveContractTerms(
                candidate->expectedWeeklyWagePence,
                candidate->expectedContractYears,
                pursuit.playerOfferTerms);
        }
    }

    QSqlQuery pursuitDetailsQuery(database_);
    pursuitDetailsQuery.prepare(QStringLiteral(
        "SELECT pursuit_id, detailed_stage, "
        "offer_upfront_pence, offer_installment_total_pence, offer_installment_count, "
        "offer_appearance_bonus_pence, offer_appearance_threshold, offer_sell_on_percentage, "
        "counter_upfront_pence, counter_installment_total_pence, counter_installment_count, "
        "counter_appearance_bonus_pence, counter_appearance_threshold, counter_sell_on_percentage, "
        "player_offer_weekly_wage_pence, player_offer_contract_years, "
        "player_offer_signing_bonus_pence, player_offer_loyalty_bonus_pence, "
        "player_offer_appearance_fee_pence, player_offer_goal_bonus_pence, "
        "player_offer_agent_fee_pence, player_offer_release_clause_pence, "
        "player_offer_squad_role, "
        "player_counter_weekly_wage_pence, player_counter_contract_years, "
        "player_counter_signing_bonus_pence, player_counter_loyalty_bonus_pence, "
        "player_counter_appearance_fee_pence, player_counter_goal_bonus_pence, "
        "player_counter_agent_fee_pence, player_counter_release_clause_pence, "
        "player_counter_squad_role, player_negotiation_round, player_offer_submitted, "
        "medical_risk_percent, medical_status, registration_status "
        "FROM save_transfer_pursuit_details WHERE save_id = :save"));
    pursuitDetailsQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!pursuitDetailsQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("상세 영입 협상 상태 조회 실패"), pursuitDetailsQuery));
        return std::nullopt;
    }
    while (pursuitDetailsQuery.next()) {
        const std::string pursuitId =
            toStdString(pursuitDetailsQuery.value(0).toString());
        const auto pursuit = std::find_if(
            state.transferPursuits.begin(),
            state.transferPursuits.end(),
            [&pursuitId](const TransferPursuit& item) {
                return item.id == pursuitId;
            });
        if (pursuit == state.transferPursuits.end()) {
            continue;
        }
        pursuit->stage = transferPursuitStageFromKey(
            toStdString(pursuitDetailsQuery.value(1).toString()));
        pursuit->offerTerms = {
            pursuitDetailsQuery.value(2).toLongLong(),
            pursuitDetailsQuery.value(3).toLongLong(),
            pursuitDetailsQuery.value(4).toInt(),
            pursuitDetailsQuery.value(5).toLongLong(),
            pursuitDetailsQuery.value(6).toInt(),
            pursuitDetailsQuery.value(7).toInt()
        };
        pursuit->clubCounterTerms = {
            pursuitDetailsQuery.value(8).toLongLong(),
            pursuitDetailsQuery.value(9).toLongLong(),
            pursuitDetailsQuery.value(10).toInt(),
            pursuitDetailsQuery.value(11).toLongLong(),
            pursuitDetailsQuery.value(12).toInt(),
            pursuitDetailsQuery.value(13).toInt()
        };
        pursuit->playerOfferTerms = {
            pursuitDetailsQuery.value(14).toLongLong(),
            pursuitDetailsQuery.value(15).toInt(),
            pursuitDetailsQuery.value(16).toLongLong(),
            pursuitDetailsQuery.value(17).toLongLong(),
            pursuitDetailsQuery.value(18).toLongLong(),
            pursuitDetailsQuery.value(19).toLongLong(),
            pursuitDetailsQuery.value(20).toLongLong(),
            pursuitDetailsQuery.value(21).toLongLong(),
            toStdString(pursuitDetailsQuery.value(22).toString())
        };
        pursuit->playerCounterTerms = {
            pursuitDetailsQuery.value(23).toLongLong(),
            pursuitDetailsQuery.value(24).toInt(),
            pursuitDetailsQuery.value(25).toLongLong(),
            pursuitDetailsQuery.value(26).toLongLong(),
            pursuitDetailsQuery.value(27).toLongLong(),
            pursuitDetailsQuery.value(28).toLongLong(),
            pursuitDetailsQuery.value(29).toLongLong(),
            pursuitDetailsQuery.value(30).toLongLong(),
            toStdString(pursuitDetailsQuery.value(31).toString())
        };
        pursuit->playerNegotiationRound = pursuitDetailsQuery.value(32).toInt();
        pursuit->playerOfferSubmitted = pursuitDetailsQuery.value(33).toBool();
        pursuit->medicalRiskPercent = pursuitDetailsQuery.value(34).toInt();
        pursuit->medicalStatus =
            toStdString(pursuitDetailsQuery.value(35).toString());
        pursuit->registrationStatus =
            toStdString(pursuitDetailsQuery.value(36).toString());
        pursuit->currentOfferPence = guaranteedTransferFee(pursuit->offerTerms);
        pursuit->clubCounterPence =
            guaranteedTransferFee(pursuit->clubCounterTerms);
    }
    pursuitDetailsQuery.finish();

    for (auto& pursuit : state.transferPursuits) {
        if (pursuit.linkedProposalId.empty()) {
            continue;
        }
        const auto proposal = std::find_if(
            state.transferProposals.begin(),
            state.transferProposals.end(),
            [&pursuit](const TransferProposal& item) {
                return item.id == pursuit.linkedProposalId;
            });
        if (proposal == state.transferProposals.end()) {
            continue;
        }
        switch (proposal->status) {
        case TransferStatus::Pending:
            pursuit.stage = TransferPursuitStage::AwaitingOwnerApproval;
            break;
        case TransferStatus::Approved:
            pursuit.stage = TransferPursuitStage::AwaitingCompletion;
            break;
        case TransferStatus::Rejected:
            pursuit.stage = TransferPursuitStage::OwnerRejected;
            break;
        case TransferStatus::Completed:
            pursuit.stage = TransferPursuitStage::Completed;
            break;
        case TransferStatus::Expired:
            pursuit.stage = TransferPursuitStage::Expired;
            break;
        }
    }

    QSqlQuery outgoingQuery(database_);
    outgoingQuery.prepare(QStringLiteral(
        "SELECT o.offer_id, p.id, p.full_name, c.id, c.name, o.fee_pence, "
        "o.weekly_wage_relief_pence, o.proposed_date, o.decision_deadline, "
        "o.decision_date, o.completion_date, "
        "o.status, o.requested_role, o.rationale, o.data_confidence "
        "FROM save_outgoing_transfer_offers o "
        "JOIN people p ON p.id = o.player_person_id "
        "JOIN clubs c ON c.id = o.buyer_club_id "
        "WHERE o.save_id = :save ORDER BY o.proposed_date, o.offer_id"));
    outgoingQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!outgoingQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("선수 판매 제안 조회 실패"), outgoingQuery));
        return std::nullopt;
    }
    std::vector<OutgoingTransferOffer> loadedOutgoingOffers;
    while (outgoingQuery.next()) {
        loadedOutgoingOffers.push_back({
            toStdString(outgoingQuery.value(0).toString()),
            toStdString(outgoingQuery.value(1).toString()),
            toStdString(outgoingQuery.value(2).toString()),
            toStdString(outgoingQuery.value(3).toString()),
            toStdString(outgoingQuery.value(4).toString()),
            outgoingQuery.value(5).toLongLong(),
            outgoingQuery.value(6).toLongLong(),
            dateOrFallback(outgoingQuery.value(7).toString(), state.currentDate),
            dateOrFallback(outgoingQuery.value(8).toString(), state.currentDate),
            optionalDate(outgoingQuery.value(9).toString()),
            optionalDate(outgoingQuery.value(10).toString()),
            outgoingTransferStatusFromKey(toStdString(outgoingQuery.value(11).toString())),
            toStdString(outgoingQuery.value(12).toString()),
            toStdString(outgoingQuery.value(13).toString()),
            toStdString(outgoingQuery.value(14).toString()),
            {},
            {},
            OutgoingNegotiationStage::DecisionPending,
            0,
            std::nullopt,
            0
        });
    }
    outgoingQuery.finish();
    if (!loadedOutgoingOffers.empty()) {
        state.outgoingTransferOffers = std::move(loadedOutgoingOffers);
        hasPersistedOutgoingOffers = true;
    }
    for (auto& offer : state.outgoingTransferOffers) {
        offer.feeTerms = effectiveFeeTerms(offer.feePence, offer.feeTerms);
        if (offer.buyerMaximumValuePence == 0) {
            offer.buyerMaximumValuePence = offer.feePence * 5 / 4;
        }
    }

    QSqlQuery outgoingDetailsQuery(database_);
    outgoingDetailsQuery.prepare(QStringLiteral(
        "SELECT offer_id, upfront_fee_pence, installment_total_pence, "
        "installment_count, appearance_bonus_pence, appearance_threshold, "
        "sell_on_percentage, buyer_counter_upfront_pence, "
        "buyer_counter_installment_total_pence, buyer_counter_installment_count, "
        "buyer_counter_appearance_bonus_pence, buyer_counter_appearance_threshold, "
        "buyer_counter_sell_on_percentage, negotiation_stage, negotiation_round, "
        "next_action_date, buyer_maximum_value_pence "
        "FROM save_outgoing_transfer_details WHERE save_id = :save"));
    outgoingDetailsQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!outgoingDetailsQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("상세 선수 판매 조건 조회 실패"), outgoingDetailsQuery));
        return std::nullopt;
    }
    while (outgoingDetailsQuery.next()) {
        const std::string offerId =
            toStdString(outgoingDetailsQuery.value(0).toString());
        const auto offer = std::find_if(
            state.outgoingTransferOffers.begin(),
            state.outgoingTransferOffers.end(),
            [&offerId](const OutgoingTransferOffer& item) {
                return item.id == offerId;
            });
        if (offer == state.outgoingTransferOffers.end()) {
            continue;
        }
        offer->feeTerms = {
            outgoingDetailsQuery.value(1).toLongLong(),
            outgoingDetailsQuery.value(2).toLongLong(),
            outgoingDetailsQuery.value(3).toInt(),
            outgoingDetailsQuery.value(4).toLongLong(),
            outgoingDetailsQuery.value(5).toInt(),
            outgoingDetailsQuery.value(6).toInt()
        };
        offer->buyerCounterTerms = {
            outgoingDetailsQuery.value(7).toLongLong(),
            outgoingDetailsQuery.value(8).toLongLong(),
            outgoingDetailsQuery.value(9).toInt(),
            outgoingDetailsQuery.value(10).toLongLong(),
            outgoingDetailsQuery.value(11).toInt(),
            outgoingDetailsQuery.value(12).toInt()
        };
        offer->negotiationStage = outgoingNegotiationStageFromKey(
            toStdString(outgoingDetailsQuery.value(13).toString()));
        offer->negotiationRound = outgoingDetailsQuery.value(14).toInt();
        offer->nextActionDate =
            optionalDate(outgoingDetailsQuery.value(15).toString());
        offer->buyerMaximumValuePence =
            outgoingDetailsQuery.value(16).toLongLong();
        offer->feePence = guaranteedTransferFee(offer->feeTerms);
    }
    outgoingDetailsQuery.finish();

    QSqlQuery contractQuery(database_);
    contractQuery.prepare(QStringLiteral(
        "SELECT person_id, club_id, contract_start_date, contract_end_date, "
        "weekly_wage_pence, squad_role, COALESCE(source_proposal_id, ''), data_confidence "
        "FROM save_player_contracts WHERE save_id = :save ORDER BY contract_start_date, person_id"));
    contractQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!contractQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("선수 계약 조회 실패"), contractQuery));
        return std::nullopt;
    }
    std::vector<PlayerContract> loadedContracts;
    while (contractQuery.next()) {
        loadedContracts.push_back({
            toStdString(contractQuery.value(0).toString()),
            toStdString(contractQuery.value(1).toString()),
            dateOrFallback(contractQuery.value(2).toString(), state.currentDate),
            dateOrFallback(contractQuery.value(3).toString(), state.currentDate),
            contractQuery.value(4).toLongLong(),
            toStdString(contractQuery.value(5).toString()),
            toStdString(contractQuery.value(6).toString()),
            toStdString(contractQuery.value(7).toString()),
            {}
        });
    }
    contractQuery.finish();
    state.playerContracts = std::move(loadedContracts);

    QSqlQuery historicalContractQuery(database_);
    historicalContractQuery.prepare(QStringLiteral(
        "SELECT person_id, club_id, contract_start_date, contract_end_date, "
        "weekly_wage_pence, squad_role, data_confidence "
        "FROM historical_player_contracts "
        "WHERE club_id = :club AND snapshot_date = '1992-07-01' "
        "AND contract_start_date IS NOT NULL AND contract_end_date IS NOT NULL "
        "AND weekly_wage_pence IS NOT NULL"));
    historicalContractQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!historicalContractQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("기준 선수 계약 조회 실패"), historicalContractQuery));
        return std::nullopt;
    }
    while (historicalContractQuery.next()) {
        const std::string playerId = toStdString(historicalContractQuery.value(0).toString());
        const bool currentlyRegistered = std::any_of(
            state.players.begin(),
            state.players.end(),
            [&playerId](const Player& player) {
                return player.id == playerId;
            });
        const bool alreadyLoaded = std::any_of(
            state.playerContracts.begin(),
            state.playerContracts.end(),
            [&playerId, &state](const PlayerContract& contract) {
                return contract.playerId == playerId
                    && contract.clubId == state.club.id;
            });
        if (!currentlyRegistered || alreadyLoaded) {
            continue;
        }
        state.playerContracts.push_back({
            playerId,
            toStdString(historicalContractQuery.value(1).toString()),
            dateOrFallback(historicalContractQuery.value(2).toString(), state.currentDate),
            dateOrFallback(historicalContractQuery.value(3).toString(), state.currentDate),
            historicalContractQuery.value(4).toLongLong(),
            toStdString(historicalContractQuery.value(5).toString()),
            {},
            toStdString(historicalContractQuery.value(6).toString()),
            {}
        });
    }
    historicalContractQuery.finish();
    for (auto& contract : state.playerContracts) {
        const int estimatedYears = std::clamp(
            contract.endDate.year - contract.startDate.year,
            1,
            5);
        contract.terms = effectiveContractTerms(
            contract.weeklyWagePence,
            estimatedYears,
            contract.terms);
        contract.terms.squadRole = contract.squadRole;
    }

    QSqlQuery contractDetailsQuery(database_);
    contractDetailsQuery.prepare(QStringLiteral(
        "SELECT person_id, club_id, contract_years, signing_bonus_pence, "
        "loyalty_bonus_pence, appearance_fee_pence, goal_bonus_pence, "
        "agent_fee_pence, release_clause_pence "
        "FROM save_player_contract_details WHERE save_id = :save"));
    contractDetailsQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!contractDetailsQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("선수 상세 계약 조회 실패"), contractDetailsQuery));
        return std::nullopt;
    }
    while (contractDetailsQuery.next()) {
        const std::string playerId =
            toStdString(contractDetailsQuery.value(0).toString());
        const std::string clubId =
            toStdString(contractDetailsQuery.value(1).toString());
        const auto contract = std::find_if(
            state.playerContracts.begin(),
            state.playerContracts.end(),
            [&playerId, &clubId](const PlayerContract& item) {
                return item.playerId == playerId && item.clubId == clubId;
            });
        if (contract == state.playerContracts.end()) {
            continue;
        }
        contract->terms = {
            contract->weeklyWagePence,
            contractDetailsQuery.value(2).toInt(),
            contractDetailsQuery.value(3).toLongLong(),
            contractDetailsQuery.value(4).toLongLong(),
            contractDetailsQuery.value(5).toLongLong(),
            contractDetailsQuery.value(6).toLongLong(),
            contractDetailsQuery.value(7).toLongLong(),
            contractDetailsQuery.value(8).toLongLong(),
            contract->squadRole
        };
    }
    contractDetailsQuery.finish();

    QSqlQuery contractLifecycleQuery(database_);
    contractLifecycleQuery.prepare(QStringLiteral(
        "SELECT person_id, club_id, lifecycle_status, expiry_notified "
        "FROM save_player_contract_lifecycle WHERE save_id = :save"));
    contractLifecycleQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!contractLifecycleQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("선수 계약 생명주기 조회 실패"),
                contractLifecycleQuery));
        return std::nullopt;
    }
    while (contractLifecycleQuery.next()) {
        const std::string playerId =
            toStdString(contractLifecycleQuery.value(0).toString());
        const std::string clubId =
            toStdString(contractLifecycleQuery.value(1).toString());
        const auto contract = std::find_if(
            state.playerContracts.begin(),
            state.playerContracts.end(),
            [&playerId, &clubId](const PlayerContract& item) {
                return item.playerId == playerId
                    && item.clubId == clubId;
            });
        if (contract == state.playerContracts.end()) {
            continue;
        }
        contract->lifecycleStatus =
            toStdString(contractLifecycleQuery.value(2).toString());
        contract->expiryNotified =
            contractLifecycleQuery.value(3).toBool();
    }
    contractLifecycleQuery.finish();

    QSqlQuery loanQuery(database_);
    loanQuery.prepare(QStringLiteral(
        "SELECT l.loan_id, p.id, p.full_name, p.birth_date, p.nationality, "
        "pp.primary_position, p.data_confidence, l.direction, "
        "pc.id, pc.name, lc.id, lc.name, l.purpose, l.proposed_date, "
        "l.decision_deadline, l.planned_start_date, l.stage, "
        "l.loan_fee_pence, l.borrowing_wage_percentage, l.promised_role, "
        "l.end_date, l.can_recall, l.recall_notice_days, l.future_fee_type, "
        "l.future_fee_pence, l.parent_match_restricted, "
        "l.unused_player_fee_pence, l.treatment_responsibility, "
        "l.counter_loan_fee_pence, l.counter_borrowing_wage_percentage, "
        "l.counter_promised_role, l.counter_end_date, l.counter_can_recall, "
        "l.counter_recall_notice_days, l.counter_future_fee_type, "
        "l.counter_future_fee_pence, l.counter_parent_match_restricted, "
        "l.counter_unused_player_fee_pence, "
        "l.counter_treatment_responsibility, l.next_action_date, "
        "l.actual_start_date, l.actual_end_date, l.last_report_date, "
        "l.parent_squad_group, l.negotiation_round, l.appearances, l.starts, "
        "l.response_note, l.data_confidence "
        "FROM save_loan_deals l "
        "JOIN people p ON p.id = l.person_id "
        "JOIN player_profiles pp ON pp.person_id = p.id "
        "JOIN clubs pc ON pc.id = l.parent_club_id "
        "JOIN clubs lc ON lc.id = l.loan_club_id "
        "WHERE l.save_id = :save ORDER BY l.proposed_date, l.loan_id"));
    loanQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!loanQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(QStringLiteral("임대 안건 조회 실패"), loanQuery));
        return std::nullopt;
    }
    std::vector<LoanDeal> loadedLoanDeals;
    while (loanQuery.next()) {
        hasPersistedLoanDeals = true;
        loadedLoanDeals.push_back({
            toStdString(loanQuery.value(0).toString()),
            {
                toStdString(loanQuery.value(1).toString()),
                toStdString(loanQuery.value(2).toString()),
                optionalDate(loanQuery.value(3).toString()),
                toStdString(loanQuery.value(4).toString()),
                toStdString(loanQuery.value(5).toString()),
                "Loan",
                "Available",
                toStdString(loanQuery.value(6).toString())
            },
            loanDirectionFromKey(
                toStdString(loanQuery.value(7).toString())),
            toStdString(loanQuery.value(8).toString()),
            toStdString(loanQuery.value(9).toString()),
            toStdString(loanQuery.value(10).toString()),
            toStdString(loanQuery.value(11).toString()),
            toStdString(loanQuery.value(12).toString()),
            dateOrFallback(loanQuery.value(13).toString(), state.currentDate),
            dateOrFallback(loanQuery.value(14).toString(), state.currentDate),
            dateOrFallback(loanQuery.value(15).toString(), state.currentDate),
            loanStageFromKey(toStdString(loanQuery.value(16).toString())),
            loanTermsFromQuery(loanQuery, 17, state.currentDate),
            loanTermsFromQuery(loanQuery, 28, state.currentDate),
            optionalDate(loanQuery.value(39).toString()),
            optionalDate(loanQuery.value(40).toString()),
            optionalDate(loanQuery.value(41).toString()),
            optionalDate(loanQuery.value(42).toString()),
            toStdString(loanQuery.value(43).toString()),
            loanQuery.value(44).toInt(),
            loanQuery.value(45).toInt(),
            loanQuery.value(46).toInt(),
            toStdString(loanQuery.value(47).toString()),
            toStdString(loanQuery.value(48).toString())
        });
    }
    loanQuery.finish();
    if (hasPersistedLoanDeals) {
        state.loanDeals = std::move(loadedLoanDeals);
    }

    QSqlQuery renewalQuery(database_);
    renewalQuery.prepare(QStringLiteral(
        "SELECT renewal_id, person_id, player_name, requested_by, rationale, "
        "current_contract_end_date, decision_deadline, stage, "
        "recommended_weekly_wage_pence, recommended_contract_years, "
        "recommended_signing_bonus_pence, recommended_loyalty_bonus_pence, "
        "recommended_appearance_fee_pence, recommended_goal_bonus_pence, "
        "recommended_agent_fee_pence, recommended_release_clause_pence, "
        "recommended_squad_role, offer_weekly_wage_pence, "
        "offer_contract_years, offer_signing_bonus_pence, "
        "offer_loyalty_bonus_pence, offer_appearance_fee_pence, "
        "offer_goal_bonus_pence, offer_agent_fee_pence, "
        "offer_release_clause_pence, offer_squad_role, "
        "counter_weekly_wage_pence, counter_contract_years, "
        "counter_signing_bonus_pence, counter_loyalty_bonus_pence, "
        "counter_appearance_fee_pence, counter_goal_bonus_pence, "
        "counter_agent_fee_pence, counter_release_clause_pence, "
        "counter_squad_role, next_action_date, last_update_date, "
        "signed_date, player_interest, agent_patience, negotiation_round, "
        "response_note, data_confidence "
        "FROM save_contract_renewals WHERE save_id = :save "
        "ORDER BY current_contract_end_date, renewal_id"));
    renewalQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!renewalQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(QStringLiteral("재계약 안건 조회 실패"), renewalQuery));
        return std::nullopt;
    }
    std::vector<ContractRenewalCase> loadedRenewals;
    while (renewalQuery.next()) {
        hasPersistedContractRenewals = true;
        loadedRenewals.push_back({
            toStdString(renewalQuery.value(0).toString()),
            toStdString(renewalQuery.value(1).toString()),
            toStdString(renewalQuery.value(2).toString()),
            toStdString(renewalQuery.value(3).toString()),
            toStdString(renewalQuery.value(4).toString()),
            dateOrFallback(renewalQuery.value(5).toString(), state.currentDate),
            dateOrFallback(renewalQuery.value(6).toString(), state.currentDate),
            contractRenewalStageFromKey(
                toStdString(renewalQuery.value(7).toString())),
            contractTermsFromQuery(renewalQuery, 8),
            contractTermsFromQuery(renewalQuery, 17),
            contractTermsFromQuery(renewalQuery, 26),
            optionalDate(renewalQuery.value(35).toString()),
            optionalDate(renewalQuery.value(36).toString()),
            optionalDate(renewalQuery.value(37).toString()),
            renewalQuery.value(38).toInt(),
            renewalQuery.value(39).toInt(),
            renewalQuery.value(40).toInt(),
            toStdString(renewalQuery.value(41).toString()),
            toStdString(renewalQuery.value(42).toString())
        });
    }
    renewalQuery.finish();
    if (hasPersistedContractRenewals) {
        state.contractRenewals = std::move(loadedRenewals);
    }

    QSqlQuery paymentQuery(database_);
    paymentQuery.prepare(QStringLiteral(
        "SELECT payment_id, source_id, counterparty_club_id, counterparty_name, "
        "direction, category, due_date, amount_pence, status, settled_date, description "
        "FROM save_transfer_payments WHERE save_id = :save "
        "ORDER BY due_date, payment_id"));
    paymentQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!paymentQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("이적료 지급 일정 조회 실패"), paymentQuery));
        return std::nullopt;
    }
    state.transferPayments.clear();
    while (paymentQuery.next()) {
        state.transferPayments.push_back({
            toStdString(paymentQuery.value(0).toString()),
            toStdString(paymentQuery.value(1).toString()),
            toStdString(paymentQuery.value(2).toString()),
            toStdString(paymentQuery.value(3).toString()),
            transferPaymentDirectionFromKey(
                toStdString(paymentQuery.value(4).toString())),
            toStdString(paymentQuery.value(5).toString()),
            dateOrFallback(paymentQuery.value(6).toString(), state.currentDate),
            paymentQuery.value(7).toLongLong(),
            transferPaymentStatusFromKey(
                toStdString(paymentQuery.value(8).toString())),
            optionalDate(paymentQuery.value(9).toString()),
            toStdString(paymentQuery.value(10).toString())
        });
    }
    paymentQuery.finish();

    QSqlQuery aiClubQuery(database_);
    aiClubQuery.prepare(QStringLiteral(
        "SELECT club_id, club_name, cash_pence, transfer_budget_pence, "
        "weekly_wage_budget_pence, weekly_wage_commitment_pence, debt_pence, "
        "finance_confidence, risk_tolerance, transfer_need, "
        "last_evaluation_date, next_evaluation_date, market_seed, data_confidence "
        "FROM save_ai_club_market_states WHERE save_id = :save "
        "ORDER BY club_name, club_id"));
    aiClubQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!aiClubQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(QStringLiteral("AI 구단 시장 상태 조회 실패"), aiClubQuery));
        return std::nullopt;
    }
    std::vector<AiClubMarketState> loadedAiClubStates;
    while (aiClubQuery.next()) {
        hasPersistedAiMarketState = true;
        FinanceState finance;
        finance.cashPence = aiClubQuery.value(2).toLongLong();
        finance.transferBudgetPence = aiClubQuery.value(3).toLongLong();
        finance.weeklyWageBudgetPence = aiClubQuery.value(4).toLongLong();
        finance.weeklyWageCommitmentPence =
            aiClubQuery.value(5).toLongLong();
        finance.debtPence = aiClubQuery.value(6).toLongLong();
        finance.confidence = toStdString(aiClubQuery.value(7).toString());
        loadedAiClubStates.push_back({
            toStdString(aiClubQuery.value(0).toString()),
            toStdString(aiClubQuery.value(1).toString()),
            finance,
            aiClubQuery.value(8).toInt(),
            toStdString(aiClubQuery.value(9).toString()),
            optionalDate(aiClubQuery.value(10).toString()),
            dateOrFallback(aiClubQuery.value(11).toString(), state.currentDate),
            aiClubQuery.value(12).toUInt(),
            toStdString(aiClubQuery.value(13).toString())
        });
    }
    aiClubQuery.finish();
    if (hasPersistedAiMarketState) {
        state.aiClubMarketStates = std::move(loadedAiClubStates);
    }

    QSqlQuery aiDealQuery(database_);
    aiDealQuery.prepare(QStringLiteral(
        "SELECT d.deal_id, d.deal_type, d.stage, p.id, p.full_name, "
        "p.birth_date, p.nationality, pp.primary_position, p.data_confidence, "
        "d.selling_club_id, d.selling_club_name, d.buying_club_id, "
        "d.buying_club_name, d.proposed_date, d.decision_deadline, "
        "d.next_action_date, d.completion_date, d.completed_date, "
        "d.upfront_fee_pence, d.installment_total_pence, d.installment_count, "
        "d.appearance_bonus_pence, d.appearance_threshold, "
        "d.sell_on_percentage, d.weekly_wage_pence, d.contract_years, "
        "d.signing_bonus_pence, d.loyalty_bonus_pence, "
        "d.appearance_fee_pence, d.goal_bonus_pence, d.agent_fee_pence, "
        "d.release_clause_pence, d.squad_role, d.loan_fee_pence, "
        "d.borrowing_wage_percentage, d.promised_role, d.loan_end_date, "
        "d.can_recall, d.recall_notice_days, d.future_fee_type, "
        "d.future_fee_pence, d.parent_match_restricted, "
        "d.unused_player_fee_pence, d.treatment_responsibility, "
        "d.source_loan_id, d.source_contract_id, d.rationale, "
        "d.player_club_involved, d.budget_reserved, d.completion_applied, "
        "d.data_confidence "
        "FROM save_ai_market_deals d "
        "JOIN people p ON p.id = d.person_id "
        "JOIN player_profiles pp ON pp.person_id = p.id "
        "WHERE d.save_id = :save ORDER BY d.proposed_date, d.deal_id"));
    aiDealQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!aiDealQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(QStringLiteral("AI 이적시장 거래 조회 실패"), aiDealQuery));
        return std::nullopt;
    }
    state.aiMarketDeals.clear();
    while (aiDealQuery.next()) {
        AiMarketDeal deal;
        deal.id = toStdString(aiDealQuery.value(0).toString());
        deal.type = aiMarketDealTypeFromKey(
            toStdString(aiDealQuery.value(1).toString()));
        deal.stage = aiMarketDealStageFromKey(
            toStdString(aiDealQuery.value(2).toString()));
        deal.player = {
            toStdString(aiDealQuery.value(3).toString()),
            toStdString(aiDealQuery.value(4).toString()),
            optionalDate(aiDealQuery.value(5).toString()),
            toStdString(aiDealQuery.value(6).toString()),
            toStdString(aiDealQuery.value(7).toString()),
            "AI Market",
            "Transfer Target",
            toStdString(aiDealQuery.value(8).toString())
        };
        deal.sellingClubId = toStdString(aiDealQuery.value(9).toString());
        deal.sellingClubName = toStdString(aiDealQuery.value(10).toString());
        deal.buyingClubId = toStdString(aiDealQuery.value(11).toString());
        deal.buyingClubName = toStdString(aiDealQuery.value(12).toString());
        deal.proposedDate =
            dateOrFallback(aiDealQuery.value(13).toString(), state.currentDate);
        deal.decisionDeadline =
            dateOrFallback(aiDealQuery.value(14).toString(), state.currentDate);
        deal.nextActionDate = optionalDate(aiDealQuery.value(15).toString());
        deal.completionDate = optionalDate(aiDealQuery.value(16).toString());
        deal.completedDate = optionalDate(aiDealQuery.value(17).toString());
        deal.feeTerms = {
            aiDealQuery.value(18).toLongLong(),
            aiDealQuery.value(19).toLongLong(),
            aiDealQuery.value(20).toInt(),
            aiDealQuery.value(21).toLongLong(),
            aiDealQuery.value(22).toInt(),
            aiDealQuery.value(23).toInt()
        };
        deal.contractTerms = contractTermsFromQuery(aiDealQuery, 24);
        deal.loanTerms = loanTermsFromQuery(aiDealQuery, 33, state.currentDate);
        deal.sourceLoanId = toStdString(aiDealQuery.value(44).toString());
        deal.sourceContractId = toStdString(aiDealQuery.value(45).toString());
        deal.rationale = toStdString(aiDealQuery.value(46).toString());
        deal.playerClubInvolved = aiDealQuery.value(47).toBool();
        deal.budgetReserved = aiDealQuery.value(48).toBool();
        deal.completionApplied = aiDealQuery.value(49).toBool();
        deal.dataConfidence = toStdString(aiDealQuery.value(50).toString());
        state.aiMarketDeals.push_back(std::move(deal));
    }
    aiDealQuery.finish();

    const auto aiClubStateCount = state.aiClubMarketStates.size();
    ensureDefaultAiMarketState(state);
    if (!hasPersistedAiMarketState && !state.aiClubMarketStates.empty()) {
        QSqlQuery aiFinanceBaselineQuery(database_);
        aiFinanceBaselineQuery.prepare(QStringLiteral(
            "SELECT cash_pence, transfer_budget_pence, weekly_wage_budget_pence, "
            "weekly_wage_commitment_pence, debt_pence, data_confidence "
            "FROM historical_club_snapshots "
            "WHERE club_id = :club AND snapshot_date = '1992-07-01'"));
        for (auto& aiClub : state.aiClubMarketStates) {
            aiFinanceBaselineQuery.bindValue(QStringLiteral(":club"), toQString(aiClub.clubId));
            if (!aiFinanceBaselineQuery.exec()) {
                (void)fail(
                    errorMessage,
                    queryError(
                        QStringLiteral("AI 구단 재정 기준선 조회 실패"),
                        aiFinanceBaselineQuery));
                return std::nullopt;
            }
            if (aiFinanceBaselineQuery.next()) {
                aiClub.finance.cashPence = aiFinanceBaselineQuery.value(0).toLongLong();
                aiClub.finance.transferBudgetPence = aiFinanceBaselineQuery.value(1).toLongLong();
                aiClub.finance.weeklyWageBudgetPence = aiFinanceBaselineQuery.value(2).toLongLong();
                aiClub.finance.weeklyWageCommitmentPence =
                    aiFinanceBaselineQuery.value(3).toLongLong();
                aiClub.finance.debtPence = aiFinanceBaselineQuery.value(4).toLongLong();
                aiClub.finance.confidence = toStdString(aiFinanceBaselineQuery.value(5).toString());
            }
            aiFinanceBaselineQuery.finish();
        }
    }
    if (!created
        && (!hasPersistedAiMarketState
            || state.aiClubMarketStates.size() != aiClubStateCount)) {
        upgradedExistingSave = true;
    }

    QSqlQuery scoutingReportQuery(database_);
    scoutingReportQuery.prepare(QStringLiteral(
        "SELECT person_id, knowledge_level, current_ability_low, "
        "current_ability_high, potential_low, potential_high, "
        "market_value_low_pence, market_value_high_pence, "
        "asking_price_low_pence, asking_price_high_pence, "
        "expected_wage_low_pence, expected_wage_high_pence, "
        "role_fit, summary, availability, last_observed_date, valid_until, "
        "scout_id, focus, report_version, stale_notified "
        "FROM save_scouting_reports WHERE save_id = :save "
        "ORDER BY person_id"));
    scoutingReportQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!scoutingReportQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("스카우팅 보고서 조회 실패"), scoutingReportQuery));
        return std::nullopt;
    }
    std::vector<ScoutingReport> loadedScoutingReports;
    while (scoutingReportQuery.next()) {
        hasPersistedScoutingReports = true;
        loadedScoutingReports.push_back({
            toStdString(scoutingReportQuery.value(0).toString()),
            scoutingReportQuery.value(1).toInt(),
            scoutingReportQuery.value(2).toInt(),
            scoutingReportQuery.value(3).toInt(),
            scoutingReportQuery.value(4).toInt(),
            scoutingReportQuery.value(5).toInt(),
            scoutingReportQuery.value(6).toLongLong(),
            scoutingReportQuery.value(7).toLongLong(),
            scoutingReportQuery.value(8).toLongLong(),
            scoutingReportQuery.value(9).toLongLong(),
            scoutingReportQuery.value(10).toLongLong(),
            scoutingReportQuery.value(11).toLongLong(),
            toStdString(scoutingReportQuery.value(12).toString()),
            toStdString(scoutingReportQuery.value(13).toString()),
            toStdString(scoutingReportQuery.value(14).toString()),
            dateOrFallback(scoutingReportQuery.value(15).toString(), state.currentDate),
            dateOrFallback(scoutingReportQuery.value(16).toString(), state.currentDate),
            toStdString(scoutingReportQuery.value(17).toString()),
            toStdString(scoutingReportQuery.value(18).toString()),
            scoutingReportQuery.value(19).toInt(),
            scoutingReportQuery.value(20).toBool()
        });
    }
    scoutingReportQuery.finish();
    if (hasPersistedScoutingReports) {
        state.scoutingReports = std::move(loadedScoutingReports);
    } else {
        state.scoutingReports.clear();
        QSqlQuery initialKnowledgeQuery(database_);
        initialKnowledgeQuery.prepare(QStringLiteral(
            "SELECT person_id, scout_id, knowledge_level, focus, last_observed_date "
            "FROM initial_scouting_knowledge "
            "WHERE club_id = :club AND snapshot_date = '1992-07-01' "
            "ORDER BY person_id"));
        initialKnowledgeQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
        if (!initialKnowledgeQuery.exec()) {
            (void)fail(errorMessage, queryError(QStringLiteral("초기 스카우팅 지식 조회 실패"), initialKnowledgeQuery));
            return std::nullopt;
        }
        while (initialKnowledgeQuery.next()) {
            const std::string playerId =
                toStdString(initialKnowledgeQuery.value(0).toString());
            const std::string scoutId =
                toStdString(initialKnowledgeQuery.value(1).toString());
            const auto candidate = std::find_if(
                state.transferCandidates.begin(),
                state.transferCandidates.end(),
                [&playerId](const TransferCandidate& item) {
                    return item.player.id == playerId;
                });
            const auto scout = std::find_if(
                state.scouts.begin(),
                state.scouts.end(),
                [&scoutId](const ScoutProfile& item) {
                    return item.id == scoutId;
                });
            if (candidate == state.transferCandidates.end()
                || scout == state.scouts.end()) {
                continue;
            }
            state.scoutingReports.push_back(makeScoutingReport(
                *candidate,
                *scout,
                initialKnowledgeQuery.value(2).toInt(),
                toStdString(initialKnowledgeQuery.value(3).toString()),
                dateOrFallback(
                    initialKnowledgeQuery.value(4).toString(),
                    state.currentDate)));
        }
        initialKnowledgeQuery.finish();
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    QSqlQuery scoutingAssignmentQuery(database_);
    scoutingAssignmentQuery.prepare(QStringLiteral(
        "SELECT assignment_id, person_id, scout_id, depth, focus, "
        "started_date, next_update_date, due_date, status, "
        "starting_knowledge, target_knowledge, last_milestone, completed_date "
        "FROM save_scouting_assignments WHERE save_id = :save "
        "ORDER BY started_date, assignment_id"));
    scoutingAssignmentQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!scoutingAssignmentQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("스카우팅 배정 조회 실패"), scoutingAssignmentQuery));
        return std::nullopt;
    }
    state.scoutingAssignments.clear();
    while (scoutingAssignmentQuery.next()) {
        state.scoutingAssignments.push_back({
            toStdString(scoutingAssignmentQuery.value(0).toString()),
            toStdString(scoutingAssignmentQuery.value(1).toString()),
            toStdString(scoutingAssignmentQuery.value(2).toString()),
            scoutingDepthFromKey(
                toStdString(scoutingAssignmentQuery.value(3).toString())),
            toStdString(scoutingAssignmentQuery.value(4).toString()),
            dateOrFallback(scoutingAssignmentQuery.value(5).toString(), state.currentDate),
            dateOrFallback(scoutingAssignmentQuery.value(6).toString(), state.currentDate),
            dateOrFallback(scoutingAssignmentQuery.value(7).toString(), state.currentDate),
            scoutingAssignmentStatusFromKey(
                toStdString(scoutingAssignmentQuery.value(8).toString())),
            scoutingAssignmentQuery.value(9).toInt(),
            scoutingAssignmentQuery.value(10).toInt(),
            scoutingAssignmentQuery.value(11).toInt(),
            optionalDate(scoutingAssignmentQuery.value(12).toString())
        });
    }
    scoutingAssignmentQuery.finish();

    QSqlQuery scoutingDirectiveQuery(database_);
    scoutingDirectiveQuery.prepare(QStringLiteral(
        "SELECT directive_id, directive_type, status, scout_id, depth, focus, "
        "target_person_id, generated_assignment_id, region, country, league, "
        "position, style, min_age, max_age, homegrown_only, max_budget_pence, "
        "matched_candidate_count, matched_player_ids, recommendation_score, "
        "estimated_duration_days, accuracy_modifier, created_date, due_date, "
        "completed_date, summary, data_confidence "
        "FROM save_scouting_directives WHERE save_id = :save "
        "ORDER BY created_date DESC, directive_id"));
    scoutingDirectiveQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!scoutingDirectiveQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("스카우팅 지시 조회 실패"),
                scoutingDirectiveQuery));
        return std::nullopt;
    }
    state.scoutingDirectives.clear();
    while (scoutingDirectiveQuery.next()) {
        ScoutingDirective directive;
        directive.id = toStdString(scoutingDirectiveQuery.value(0).toString());
        directive.type = scoutingDirectiveTypeFromKey(
            toStdString(scoutingDirectiveQuery.value(1).toString()));
        directive.status = scoutingDirectiveStatusFromKey(
            toStdString(scoutingDirectiveQuery.value(2).toString()));
        directive.scoutId =
            toStdString(scoutingDirectiveQuery.value(3).toString());
        directive.depth = scoutingDepthFromKey(
            toStdString(scoutingDirectiveQuery.value(4).toString()));
        directive.focus = toStdString(scoutingDirectiveQuery.value(5).toString());
        if (!scoutingDirectiveQuery.value(6).isNull()) {
            directive.targetPlayerId =
                toStdString(scoutingDirectiveQuery.value(6).toString());
        }
        directive.generatedAssignmentId =
            toStdString(scoutingDirectiveQuery.value(7).toString());
        directive.region = toStdString(scoutingDirectiveQuery.value(8).toString());
        directive.country =
            toStdString(scoutingDirectiveQuery.value(9).toString());
        directive.league =
            toStdString(scoutingDirectiveQuery.value(10).toString());
        directive.position =
            toStdString(scoutingDirectiveQuery.value(11).toString());
        directive.style =
            toStdString(scoutingDirectiveQuery.value(12).toString());
        directive.minAge = scoutingDirectiveQuery.value(13).toInt();
        directive.maxAge = scoutingDirectiveQuery.value(14).toInt();
        directive.homegrownOnly =
            scoutingDirectiveQuery.value(15).toBool();
        directive.maxBudgetPence =
            scoutingDirectiveQuery.value(16).toLongLong();
        directive.matchedCandidateCount =
            scoutingDirectiveQuery.value(17).toInt();
        directive.matchedPlayerIds =
            toStdString(scoutingDirectiveQuery.value(18).toString());
        directive.recommendationScore =
            scoutingDirectiveQuery.value(19).toInt();
        directive.estimatedDurationDays =
            scoutingDirectiveQuery.value(20).toInt();
        directive.accuracyModifier =
            scoutingDirectiveQuery.value(21).toInt();
        directive.createdDate = dateOrFallback(
            scoutingDirectiveQuery.value(22).toString(),
            state.currentDate);
        directive.dueDate = dateOrFallback(
            scoutingDirectiveQuery.value(23).toString(),
            directive.createdDate);
        directive.completedDate =
            optionalDate(scoutingDirectiveQuery.value(24).toString());
        directive.summary =
            toStdString(scoutingDirectiveQuery.value(25).toString());
        directive.dataConfidence =
            toStdString(scoutingDirectiveQuery.value(26).toString());
        state.scoutingDirectives.push_back(std::move(directive));
    }
    scoutingDirectiveQuery.finish();

    QSqlQuery recruitmentPlanQuery(database_);
    recruitmentPlanQuery.prepare(QStringLiteral(
        "SELECT plan_id, title, requested_by, description, start_date, "
        "evaluation_date, target_signings, minimum_acceptable_signings, "
        "status, confidence_delta, evaluated_date "
        "FROM save_recruitment_plans WHERE save_id = :save "
        "ORDER BY start_date, plan_id"));
    recruitmentPlanQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!recruitmentPlanQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("영입 계획 조회 실패"), recruitmentPlanQuery));
        return std::nullopt;
    }
    std::vector<RecruitmentPlan> loadedRecruitmentPlans;
    while (recruitmentPlanQuery.next()) {
        hasPersistedRecruitmentPlans = true;
        loadedRecruitmentPlans.push_back({
            toStdString(recruitmentPlanQuery.value(0).toString()),
            toStdString(recruitmentPlanQuery.value(1).toString()),
            toStdString(recruitmentPlanQuery.value(2).toString()),
            toStdString(recruitmentPlanQuery.value(3).toString()),
            dateOrFallback(recruitmentPlanQuery.value(4).toString(), state.currentDate),
            dateOrFallback(recruitmentPlanQuery.value(5).toString(), state.currentDate),
            recruitmentPlanQuery.value(6).toInt(),
            recruitmentPlanQuery.value(7).toInt(),
            recruitmentPlanStatusFromKey(
                toStdString(recruitmentPlanQuery.value(8).toString())),
            recruitmentPlanQuery.value(9).toInt(),
            optionalDate(recruitmentPlanQuery.value(10).toString())
        });
    }
    recruitmentPlanQuery.finish();
    if (hasPersistedRecruitmentPlans) {
        state.recruitmentPlans = std::move(loadedRecruitmentPlans);
    } else {
        if (!created) {
            int legacyTransferConfidenceDelta = 0;
            for (const auto& proposal : state.transferProposals) {
                switch (proposal.status) {
                case TransferStatus::Approved:
                case TransferStatus::Completed:
                    legacyTransferConfidenceDelta += 2;
                    break;
                case TransferStatus::Rejected:
                    legacyTransferConfidenceDelta -= 2;
                    break;
                case TransferStatus::Expired:
                    legacyTransferConfidenceDelta -= 3;
                    break;
                case TransferStatus::Pending:
                    break;
                }
            }
            state.club.manager.boardConfidence = std::clamp(
                state.club.manager.boardConfidence
                    - legacyTransferConfidenceDelta,
                0,
                100);
        }
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    QSqlQuery squadPlanningOpinionQuery(database_);
    squadPlanningOpinionQuery.prepare(QStringLiteral(
        "SELECT opinion_id, season_id, club_id, player_person_id, "
        "position_group, source_role, opinion_type, priority, reason, "
        "target_window, status, created_date, updated_date, data_confidence "
        "FROM save_squad_planning_opinions WHERE save_id = :save "
        "ORDER BY updated_date DESC, opinion_id"));
    squadPlanningOpinionQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!squadPlanningOpinionQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("선수단 계획 의견 조회 실패"),
                squadPlanningOpinionQuery));
        return std::nullopt;
    }
    state.squadPlanningOpinions.clear();
    while (squadPlanningOpinionQuery.next()) {
        SquadPlanningOpinion opinion;
        opinion.id = toStdString(squadPlanningOpinionQuery.value(0).toString());
        opinion.seasonId =
            toStdString(squadPlanningOpinionQuery.value(1).toString());
        opinion.clubId =
            toStdString(squadPlanningOpinionQuery.value(2).toString());
        if (!squadPlanningOpinionQuery.value(3).isNull()) {
            opinion.playerId =
                toStdString(squadPlanningOpinionQuery.value(3).toString());
        }
        if (!squadPlanningOpinionQuery.value(4).isNull()) {
            opinion.positionGroup =
                toStdString(squadPlanningOpinionQuery.value(4).toString());
        }
        opinion.source = squadPlanningOpinionSourceFromKey(
            toStdString(squadPlanningOpinionQuery.value(5).toString()));
        opinion.type = squadPlanningOpinionTypeFromKey(
            toStdString(squadPlanningOpinionQuery.value(6).toString()));
        opinion.priority = squadPlanningOpinionQuery.value(7).toInt();
        opinion.reason =
            toStdString(squadPlanningOpinionQuery.value(8).toString());
        opinion.targetWindow =
            toStdString(squadPlanningOpinionQuery.value(9).toString());
        opinion.status = squadPlanningOpinionStatusFromKey(
            toStdString(squadPlanningOpinionQuery.value(10).toString()));
        opinion.createdDate = dateOrFallback(
            squadPlanningOpinionQuery.value(11).toString(),
            state.currentDate);
        opinion.updatedDate = dateOrFallback(
            squadPlanningOpinionQuery.value(12).toString(),
            opinion.createdDate);
        opinion.dataConfidence =
            toStdString(squadPlanningOpinionQuery.value(13).toString());
        state.squadPlanningOpinions.push_back(std::move(opinion));
    }
    squadPlanningOpinionQuery.finish();

    QSqlQuery meetingSettingsQuery(database_);
    meetingSettingsQuery.prepare(QStringLiteral(
        "SELECT cadence, attendee_mode, include_manager, include_director, "
        "include_finance, include_scouting, include_medical, "
        "next_meeting_date, updated_date, data_confidence "
        "FROM save_operating_meeting_settings WHERE save_id = :save"));
    meetingSettingsQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!meetingSettingsQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("운영 회의 설정 조회 실패"),
                meetingSettingsQuery));
        return std::nullopt;
    }
    if (meetingSettingsQuery.next()) {
        state.operatingMeetingSettings.cadence = operatingMeetingCadenceFromKey(
            toStdString(meetingSettingsQuery.value(0).toString()));
        state.operatingMeetingSettings.attendeeMode =
            operatingMeetingAttendeeModeFromKey(
                toStdString(meetingSettingsQuery.value(1).toString()));
        state.operatingMeetingSettings.includeManager =
            meetingSettingsQuery.value(2).toBool();
        state.operatingMeetingSettings.includeDirector =
            meetingSettingsQuery.value(3).toBool();
        state.operatingMeetingSettings.includeFinance =
            meetingSettingsQuery.value(4).toBool();
        state.operatingMeetingSettings.includeScouting =
            meetingSettingsQuery.value(5).toBool();
        state.operatingMeetingSettings.includeMedical =
            meetingSettingsQuery.value(6).toBool();
        state.operatingMeetingSettings.nextMeetingDate =
            optionalDate(meetingSettingsQuery.value(7).toString());
        state.operatingMeetingSettings.updatedDate = dateOrFallback(
            meetingSettingsQuery.value(8).toString(),
            state.currentDate);
        state.operatingMeetingSettings.dataConfidence =
            toStdString(meetingSettingsQuery.value(9).toString());
    } else {
        state.operatingMeetingSettings.updatedDate = state.currentDate;
    }
    meetingSettingsQuery.finish();

    QSqlQuery meetingQuery(database_);
    meetingQuery.prepare(QStringLiteral(
        "SELECT meeting_id, meeting_date, agenda_type, status, "
        "attendee_mode, summary, outcome_inbox_event_key, created_date, "
        "data_confidence "
        "FROM save_operating_meetings WHERE save_id = :save "
        "ORDER BY meeting_date DESC, meeting_id DESC"));
    meetingQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!meetingQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(QStringLiteral("운영 회의 조회 실패"), meetingQuery));
        return std::nullopt;
    }
    state.operatingMeetings.clear();
    while (meetingQuery.next()) {
        OperatingMeeting meeting;
        meeting.id = toStdString(meetingQuery.value(0).toString());
        meeting.meetingDate = dateOrFallback(
            meetingQuery.value(1).toString(),
            state.currentDate);
        meeting.agenda = operatingMeetingAgendaTypeFromKey(
            toStdString(meetingQuery.value(2).toString()));
        meeting.status = operatingMeetingStatusFromKey(
            toStdString(meetingQuery.value(3).toString()));
        meeting.attendeeMode = operatingMeetingAttendeeModeFromKey(
            toStdString(meetingQuery.value(4).toString()));
        meeting.summary = toStdString(meetingQuery.value(5).toString());
        meeting.outcomeInboxEventKey =
            toStdString(meetingQuery.value(6).toString());
        meeting.createdDate = dateOrFallback(
            meetingQuery.value(7).toString(),
            meeting.meetingDate);
        meeting.dataConfidence =
            toStdString(meetingQuery.value(8).toString());
        state.operatingMeetings.push_back(std::move(meeting));
    }
    meetingQuery.finish();

    QSqlQuery attendeeQuery(database_);
    attendeeQuery.prepare(QStringLiteral(
        "SELECT meeting_id, attendee_role, display_name, auto_assigned, notes "
        "FROM save_operating_meeting_attendees WHERE save_id = :save "
        "ORDER BY meeting_id, attendee_role"));
    attendeeQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!attendeeQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("운영 회의 참석자 조회 실패"),
                attendeeQuery));
        return std::nullopt;
    }
    while (attendeeQuery.next()) {
        const std::string meetingId =
            toStdString(attendeeQuery.value(0).toString());
        const auto meeting = std::find_if(
            state.operatingMeetings.begin(),
            state.operatingMeetings.end(),
            [&meetingId](const OperatingMeeting& item) {
                return item.id == meetingId;
            });
        if (meeting == state.operatingMeetings.end()) {
            continue;
        }
        meeting->attendees.push_back({
            toStdString(attendeeQuery.value(1).toString()),
            toStdString(attendeeQuery.value(2).toString()),
            attendeeQuery.value(3).toBool(),
            toStdString(attendeeQuery.value(4).toString())
        });
    }
    attendeeQuery.finish();

    QSqlQuery ownerReputationQuery(database_);
    ownerReputationQuery.prepare(QStringLiteral(
        "SELECT overall_score, sporting_ambition, financial_prudence, "
        "squad_loyalty, autonomy_respect, headline, updated_date, "
        "data_confidence "
        "FROM save_owner_reputation_state WHERE save_id = :save"));
    ownerReputationQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!ownerReputationQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("구단주 평판 상태 조회 실패"),
                ownerReputationQuery));
        return std::nullopt;
    }
    if (ownerReputationQuery.next()) {
        hasPersistedOwnerReputationState = true;
        state.ownerReputation.overallScore =
            ownerReputationQuery.value(0).toInt();
        state.ownerReputation.sportingAmbition =
            ownerReputationQuery.value(1).toInt();
        state.ownerReputation.financialPrudence =
            ownerReputationQuery.value(2).toInt();
        state.ownerReputation.squadLoyalty =
            ownerReputationQuery.value(3).toInt();
        state.ownerReputation.autonomyRespect =
            ownerReputationQuery.value(4).toInt();
        state.ownerReputation.headline =
            toStdString(ownerReputationQuery.value(5).toString());
        state.ownerReputation.updatedDate = dateOrFallback(
            ownerReputationQuery.value(6).toString(),
            state.currentDate);
        state.ownerReputation.dataConfidence =
            toStdString(ownerReputationQuery.value(7).toString());
    }
    ownerReputationQuery.finish();

    QSqlQuery fanTendencyQuery(database_);
    fanTendencyQuery.prepare(QStringLiteral(
        "SELECT trust_score, patience_score, skepticism_score, "
        "star_signing_preference, homegrown_preference, "
        "youth_development_preference, balanced_squad_preference, "
        "mood, updated_date, data_confidence "
        "FROM save_fan_tendency_state WHERE save_id = :save"));
    fanTendencyQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!fanTendencyQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("팬 성향 상태 조회 실패"),
                fanTendencyQuery));
        return std::nullopt;
    }
    if (fanTendencyQuery.next()) {
        hasPersistedFanTendencyState = true;
        state.fanTendency.trustScore = fanTendencyQuery.value(0).toInt();
        state.fanTendency.patienceScore = fanTendencyQuery.value(1).toInt();
        state.fanTendency.skepticismScore = fanTendencyQuery.value(2).toInt();
        state.fanTendency.starSigningPreference =
            fanTendencyQuery.value(3).toInt();
        state.fanTendency.homegrownPreference =
            fanTendencyQuery.value(4).toInt();
        state.fanTendency.youthDevelopmentPreference =
            fanTendencyQuery.value(5).toInt();
        state.fanTendency.balancedSquadPreference =
            fanTendencyQuery.value(6).toInt();
        state.fanTendency.mood =
            toStdString(fanTendencyQuery.value(7).toString());
        state.fanTendency.updatedDate = dateOrFallback(
            fanTendencyQuery.value(8).toString(),
            state.currentDate);
        state.fanTendency.dataConfidence =
            toStdString(fanTendencyQuery.value(9).toString());
    }
    fanTendencyQuery.finish();

    QSqlQuery ownerActionQuery(database_);
    ownerActionQuery.prepare(QStringLiteral(
        "SELECT action_id, action_date, action_type, category, subject_id, "
        "subject_name, summary, reputation_delta, fan_trust_delta, "
        "ambition_delta, prudence_delta, loyalty_delta, autonomy_delta, "
        "star_preference_delta, homegrown_preference_delta, "
        "youth_preference_delta, balanced_preference_delta, "
        "skepticism_delta, data_confidence "
        "FROM save_owner_action_log WHERE save_id = :save "
        "ORDER BY action_date, action_id"));
    ownerActionQuery.bindValue(QStringLiteral(":save"), state.saveId);
    if (!ownerActionQuery.exec()) {
        (void)fail(
            errorMessage,
            queryError(
                QStringLiteral("구단주 행동 로그 조회 실패"),
                ownerActionQuery));
        return std::nullopt;
    }
    state.ownerActionLog.clear();
    while (ownerActionQuery.next()) {
        state.ownerActionLog.push_back({
            toStdString(ownerActionQuery.value(0).toString()),
            dateOrFallback(ownerActionQuery.value(1).toString(), state.currentDate),
            toStdString(ownerActionQuery.value(2).toString()),
            toStdString(ownerActionQuery.value(3).toString()),
            toStdString(ownerActionQuery.value(4).toString()),
            toStdString(ownerActionQuery.value(5).toString()),
            toStdString(ownerActionQuery.value(6).toString()),
            ownerActionQuery.value(7).toInt(),
            ownerActionQuery.value(8).toInt(),
            ownerActionQuery.value(9).toInt(),
            ownerActionQuery.value(10).toInt(),
            ownerActionQuery.value(11).toInt(),
            ownerActionQuery.value(12).toInt(),
            ownerActionQuery.value(13).toInt(),
            ownerActionQuery.value(14).toInt(),
            ownerActionQuery.value(15).toInt(),
            ownerActionQuery.value(16).toInt(),
            ownerActionQuery.value(17).toInt(),
            toStdString(ownerActionQuery.value(18).toString())
        });
    }
    ownerActionQuery.finish();

    ensureDefaultOwnerReputation(state);
    if (!hasPersistedOwnerReputationState || !hasPersistedFanTendencyState) {
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    if (!created && !hasPersistedDetailedTransferData) {
        upgradedExistingSave = true;
    }

    QSqlQuery fixtureQuery(database_);
    fixtureQuery.prepare(QStringLiteral(
        "SELECT f.id, f.fixture_date, cp.name, hc.name, ac.name, "
        "f.home_strength_estimate, f.away_strength_estimate, "
        "COALESCE(d.played, 0), COALESCE(d.home_goals, 0), COALESCE(d.away_goals, 0) "
        "FROM fixtures f "
        "JOIN competitions cp ON cp.id = f.competition_id "
        "JOIN clubs hc ON hc.id = f.home_club_id "
        "JOIN clubs ac ON ac.id = f.away_club_id "
        "LEFT JOIN save_fixture_deltas d ON d.fixture_id = f.id AND d.save_id = :save "
        "WHERE f.competition_id = (SELECT competition_id FROM clubs WHERE id = :club) "
        "ORDER BY f.fixture_date"));
    fixtureQuery.bindValue(QStringLiteral(":save"), state.saveId);
    fixtureQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!fixtureQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("경기 일정 조회 실패"), fixtureQuery));
        return std::nullopt;
    }
    std::vector<Fixture> loadedFixtures;
    while (fixtureQuery.next()) {
        loadedFixtures.push_back({
            toStdString(fixtureQuery.value(0).toString()),
            dateOrFallback(fixtureQuery.value(1).toString(), state.currentDate),
            toStdString(fixtureQuery.value(2).toString()),
            toStdString(fixtureQuery.value(3).toString()),
            toStdString(fixtureQuery.value(4).toString()),
            fixtureQuery.value(5).toInt(),
            fixtureQuery.value(6).toInt(),
            fixtureQuery.value(7).toBool(),
            fixtureQuery.value(8).toInt(),
            fixtureQuery.value(9).toInt()
        });
    }
    fixtureQuery.finish();
    if (!loadedFixtures.empty()) {
        state.fixtures = std::move(loadedFixtures);
    }

    QSqlQuery seasonTeamQuery(database_);
    seasonTeamQuery.prepare(QStringLiteral(
        "SELECT s.id, cp.name, c.id, c.name, t.source_locked_club_id, t.seed_order "
        "FROM competition_season_teams t "
        "JOIN competition_seasons s ON s.id = t.season_id "
        "JOIN competitions cp ON cp.id = s.competition_id "
        "JOIN clubs c ON c.id = t.club_id "
        "WHERE s.competition_id = (SELECT competition_id FROM clubs WHERE id = :club) "
        "ORDER BY t.seed_order"));
    seasonTeamQuery.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    if (!seasonTeamQuery.exec()) {
        (void)fail(errorMessage, queryError(QStringLiteral("시즌 참가팀 조회 실패"), seasonTeamQuery));
        return std::nullopt;
    }
    std::vector<CompetitionSeasonTeam> loadedSeasonTeams;
    while (seasonTeamQuery.next()) {
        loadedSeasonTeams.push_back({
            toStdString(seasonTeamQuery.value(0).toString()),
            toStdString(seasonTeamQuery.value(1).toString()),
            toStdString(seasonTeamQuery.value(2).toString()),
            toStdString(seasonTeamQuery.value(3).toString()),
            toStdString(seasonTeamQuery.value(4).toString()),
            seasonTeamQuery.value(5).toInt()
        });
    }
    seasonTeamQuery.finish();
    if (!loadedSeasonTeams.empty()) {
        state.competitionSeasonTeams = std::move(loadedSeasonTeams);
    }
    recalculateLeagueStandings(state);

    if (!created) {
        QSqlQuery inboxQuery(database_);
        inboxQuery.prepare(QStringLiteral(
            "SELECT item_id, event_key, item_date, category, title, body, unread, requires_decision "
            "FROM save_inbox_items WHERE save_id = :save ORDER BY item_id"));
        inboxQuery.bindValue(QStringLiteral(":save"), state.saveId);
        if (!inboxQuery.exec()) {
            (void)fail(errorMessage, queryError(QStringLiteral("받은 편지함 조회 실패"), inboxQuery));
            return std::nullopt;
        }
        std::vector<InboxItem> loadedInbox;
        while (inboxQuery.next()) {
            loadedInbox.push_back({
                inboxQuery.value(0).toInt(),
                toStdString(inboxQuery.value(1).toString()),
                dateOrFallback(inboxQuery.value(2).toString(), state.currentDate),
                inboxCategoryFromKey(toStdString(inboxQuery.value(3).toString())),
                toStdString(inboxQuery.value(4).toString()),
                toStdString(inboxQuery.value(5).toString()),
                inboxQuery.value(6).toBool(),
                inboxQuery.value(7).toBool()
            });
        }
        inboxQuery.finish();
        if (!loadedInbox.empty()) {
            state.inbox = std::move(loadedInbox);
        }

        QSqlQuery eventQuery(database_);
        eventQuery.prepare(QStringLiteral(
            "SELECT event_id, event_key, due_date, category, title, body, requires_decision, processed "
            "FROM save_event_queue WHERE save_id = :save ORDER BY event_id"));
        eventQuery.bindValue(QStringLiteral(":save"), state.saveId);
        if (!eventQuery.exec()) {
            (void)fail(errorMessage, queryError(QStringLiteral("이벤트 큐 조회 실패"), eventQuery));
            return std::nullopt;
        }
        std::vector<ScheduledEvent> loadedEvents;
        while (eventQuery.next()) {
            loadedEvents.push_back({
                eventQuery.value(0).toInt(),
                toStdString(eventQuery.value(1).toString()),
                dateOrFallback(eventQuery.value(2).toString(), state.currentDate),
                inboxCategoryFromKey(toStdString(eventQuery.value(3).toString())),
                toStdString(eventQuery.value(4).toString()),
                toStdString(eventQuery.value(5).toString()),
                eventQuery.value(6).toBool(),
                eventQuery.value(7).toBool()
            });
        }
        eventQuery.finish();
        if (!loadedEvents.empty()) {
            state.eventQueue = std::move(loadedEvents);
        }
    }

    if (!hasPersistedTransferProposals) {
        for (auto& proposal : state.transferProposals) {
            if (proposal.status == TransferStatus::Pending
                && proposal.decisionDeadline < state.currentDate) {
                proposal.proposedDate = state.currentDate;
                proposal.decisionDeadline = state.currentDate.addDays(7);
                proposal.decisionDate.reset();
                proposal.joinDate.reset();
            }

            const std::string requestKey = "transfer-proposal-" + proposal.id;
            const auto request = std::find_if(
                state.inbox.begin(),
                state.inbox.end(),
                [&requestKey](const InboxItem& item) {
                    return item.eventKey == requestKey;
                });
            if (request != state.inbox.end()) {
                request->requiresDecision = proposal.status == TransferStatus::Pending;
                continue;
            }

            int nextId = 1;
            for (const auto& item : state.inbox) {
                nextId = std::max(nextId, item.id + 1);
            }
            state.inbox.push_back({
                nextId,
                requestKey,
                state.currentDate,
                InboxCategory::Football,
                proposal.requestedBy + " 감독이 " + proposal.target.fullName + " 영입 승인을 요청했습니다",
                proposal.fromClubName + "과의 주요 거래입니다. 제안 이적료 "
                    + formatMoney(proposal.feePence) + ", 주급 "
                    + formatMoney(proposal.weeklyWagePence)
                    + "는 기능 검증용 추정 조건입니다.",
                true,
                proposal.status == TransferStatus::Pending
            });
        }
        upgradedExistingSave = !created;
    }

    const auto nextInboxId = [&state] {
        int nextId = 1;
        for (const auto& item : state.inbox) {
            nextId = std::max(nextId, item.id + 1);
        }
        return nextId;
    };
    const auto hasInboxEvent = [&state](const std::string& eventKey) {
        return std::any_of(
            state.inbox.begin(),
            state.inbox.end(),
            [&eventKey](const InboxItem& item) {
                return item.eventKey == eventKey;
            });
    };

    if (!hasPersistedTransferPursuits) {
        for (auto& pursuit : state.transferPursuits) {
            if (!pursuit.linkedProposalId.empty()) {
                const auto proposal = std::find_if(
                    state.transferProposals.begin(),
                    state.transferProposals.end(),
                    [&pursuit](const TransferProposal& item) {
                        return item.id == pursuit.linkedProposalId;
                    });
                if (proposal != state.transferProposals.end()) {
                    pursuit.decisionDeadline = proposal->decisionDeadline;
                    switch (proposal->status) {
                    case TransferStatus::Pending:
                        pursuit.stage = TransferPursuitStage::AwaitingOwnerApproval;
                        break;
                    case TransferStatus::Approved:
                        pursuit.stage = TransferPursuitStage::AwaitingCompletion;
                        break;
                    case TransferStatus::Rejected:
                        pursuit.stage = TransferPursuitStage::OwnerRejected;
                        break;
                    case TransferStatus::Completed:
                        pursuit.stage = TransferPursuitStage::Completed;
                        break;
                    case TransferStatus::Expired:
                        pursuit.stage = TransferPursuitStage::Expired;
                        break;
                    }
                }
            }
        }
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    if (!hasInboxEvent("transfer-market-opening-report")) {
        state.inbox.push_back({
            nextInboxId(),
            "transfer-market-opening-report",
            state.currentDate,
            InboxCategory::Football,
            "여름 이적시장 1차 후보 보고가 도착했습니다",
            "감독과 스카우트 조직이 8명의 후보를 우선순위와 정보 신뢰도별로 정리했습니다. 이적 화면에서 추가 협상을 지시할 수 있습니다.",
            true,
            false
        });
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    if (!hasPersistedOutgoingOffers) {
        for (auto& offer : state.outgoingTransferOffers) {
            if (offer.status == OutgoingTransferStatus::Pending
                && offer.decisionDeadline < state.currentDate) {
                offer.proposedDate = state.currentDate;
                offer.decisionDeadline = state.currentDate.addDays(7);
                offer.decisionDate.reset();
                offer.completionDate.reset();
            }

            const std::string eventKey = "outgoing-offer-" + offer.id;
            if (hasInboxEvent(eventKey)) {
                continue;
            }
            state.inbox.push_back({
                nextInboxId(),
                eventKey,
                state.currentDate,
                InboxCategory::Football,
                offer.buyerClubName + "가 " + offer.playerName + " 영입을 제안했습니다",
                "제안 이적료는 " + formatMoney(offer.feePence)
                    + "이며 " + offer.decisionDeadline.toIsoString()
                    + "까지 구단주 결정을 요청합니다.",
                true,
                offer.status == OutgoingTransferStatus::Pending
            });
        }
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    if (!hasPersistedLoanDeals) {
        for (auto& loan : state.loanDeals) {
            if ((loan.stage == LoanStage::DecisionPending
                    || loan.stage == LoanStage::AwaitingClubResponse
                    || loan.stage == LoanStage::ClubCountered)
                && loan.decisionDeadline < state.currentDate) {
                loan.proposedDate = state.currentDate;
                loan.decisionDeadline = state.currentDate.addDays(7);
                loan.plannedStartDate = state.currentDate.addDays(3);
                loan.nextActionDate.reset();
                if (loan.terms.endDate.daysUntil(state.currentDate) >= 0
                    || state.currentDate.daysUntil(loan.terms.endDate) < 28) {
                    loan.terms.endDate =
                        state.currentDate.addMonths(10);
                    loan.counterTerms.endDate = loan.terms.endDate;
                }
            }

            const std::string eventKey = "loan-offer-" + loan.id;
            if (loan.stage == LoanStage::DecisionPending
                && !hasInboxEvent(eventKey)) {
                state.inbox.push_back({
                    nextInboxId(),
                    eventKey,
                    state.currentDate,
                    InboxCategory::Football,
                    loan.loanClubName + "가 " + loan.player.fullName
                        + " 임대를 제안했습니다",
                    "임대료 " + formatMoney(loan.terms.loanFeePence)
                        + ", 차입 구단 주급 부담 "
                        + std::to_string(
                            loan.terms.borrowingClubWagePercentage)
                        + "% 조건입니다.",
                    true,
                    true
                });
            }
        }
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    if (!hasPersistedContractRenewals) {
        for (auto& renewal : state.contractRenewals) {
            if (renewal.decisionDeadline < state.currentDate) {
                renewal.decisionDeadline =
                    state.currentDate.addDays(45);
            }
            const std::string eventKey =
                "contract-renewal-recommendation-" + renewal.id;
            if (renewal.stage == ContractRenewalStage::Recommended
                && !hasInboxEvent(eventKey)) {
                state.inbox.push_back({
                    nextInboxId(),
                    eventKey,
                    state.currentDate,
                    InboxCategory::Football,
                    renewal.playerName + " 재계약 검토가 필요합니다",
                    renewal.rationale,
                    true,
                    false
                });
            }
        }
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    for (const auto& loan : state.loanDeals) {
        const auto registeredPlayer = std::find_if(
            state.players.begin(),
            state.players.end(),
            [&loan](const Player& player) {
                return player.id == loan.player.id;
            });
        if (loan.direction == LoanDirection::Outgoing
            && (loan.stage == LoanStage::Active
                || loan.stage == LoanStage::RecallPending)
            && registeredPlayer != state.players.end()) {
            registeredPlayer->squadGroup = "Loan";
            registeredPlayer->status = "On Loan";
        } else if (loan.direction == LoanDirection::Outgoing
            && (loan.stage == LoanStage::Completed
                || loan.stage == LoanStage::Recalled)
            && registeredPlayer != state.players.end()) {
            registeredPlayer->squadGroup =
                loan.parentSquadGroup.empty()
                ? "Reserve"
                : loan.parentSquadGroup;
            registeredPlayer->status = "Available";
        } else if (loan.direction == LoanDirection::Incoming
            && (loan.stage == LoanStage::Active
                || loan.stage == LoanStage::RecallPending)
            && registeredPlayer == state.players.end()) {
            Player incoming = loan.player;
            incoming.squadGroup =
                loan.terms.promisedRole == "Regular Starter"
                ? "First Team"
                : "Reserve";
            incoming.status = "Loaned In";
            incoming.joinedDate = loan.actualStartDate;
            state.players.push_back(std::move(incoming));
        } else if (loan.direction == LoanDirection::Incoming
            && (loan.stage == LoanStage::Completed
                || loan.stage == LoanStage::Recalled)
            && registeredPlayer != state.players.end()) {
            state.players.erase(registeredPlayer);
        }
    }

    for (const auto& contract : state.playerContracts) {
        if (contract.lifecycleStatus == "active") {
            continue;
        }
        const auto player = std::find_if(
            state.players.begin(),
            state.players.end(),
            [&contract](const Player& item) {
                return item.id == contract.playerId;
            });
        if (player != state.players.end()) {
            player->status =
                contract.lifecycleStatus == "expired_retained"
                ? "Contract Expired · Registration Retained"
                : "Contract Expired";
        }
    }

    for (const auto& proposal : state.transferProposals) {
        if (proposal.status != TransferStatus::Completed
            || !proposal.joinDate.has_value()) {
            continue;
        }
        const auto contract = std::find_if(
            state.playerContracts.begin(),
            state.playerContracts.end(),
            [&proposal, &state](const PlayerContract& item) {
                return item.playerId == proposal.target.id
                    && item.clubId == state.club.id;
            });
        if (contract != state.playerContracts.end()) {
            continue;
        }
        const GameDate startDate = *proposal.joinDate;
        const PlayerContractTerms terms = effectiveContractTerms(
            proposal.weeklyWagePence,
            proposal.contractYears,
            proposal.contractTerms);
        state.playerContracts.push_back({
            proposal.target.id,
            state.club.id,
            startDate,
            startDate.addMonths(terms.contractYears * 12),
            terms.weeklyWagePence,
            terms.squadRole,
            proposal.id,
            proposal.dataConfidence,
            terms
        });
        upgradedExistingSave = upgradedExistingSave || !created;
    }

    if ((created || upgradedExistingSave) && !save(state, errorMessage)) {
        return std::nullopt;
    }

    return state;
}

bool SqliteGameRepository::save(const GameState& state, QString* errorMessage)
{
    if (state.saveId <= 0) {
        return fail(errorMessage, QStringLiteral("유효한 saveId 없이 저장할 수 없습니다."));
    }
    if (!initialized_ && !initialize(errorMessage)) {
        return false;
    }
    if (!database_.transaction()) {
        return fail(errorMessage, QStringLiteral("저장 트랜잭션을 시작할 수 없습니다: ") + database_.lastError().text());
    }

    const auto rollback = [&](const QString& message) {
        (void)database_.rollback();
        return fail(errorMessage, message);
    };

    QSqlQuery updateSave(database_);
    updateSave.prepare(QStringLiteral(
        "UPDATE save_games SET display_name = :name, game_date = :date, owner_name = :owner, "
        "rng_seed = :seed, updated_at = CURRENT_TIMESTAMP WHERE id = :id"));
    updateSave.bindValue(QStringLiteral(":name"), toQString(state.saveName));
    updateSave.bindValue(QStringLiteral(":date"), toQString(state.currentDate.toIsoString()));
    updateSave.bindValue(QStringLiteral(":owner"), toQString(state.ownerName));
    updateSave.bindValue(QStringLiteral(":seed"), static_cast<qulonglong>(state.rngSeed));
    updateSave.bindValue(QStringLiteral(":id"), state.saveId);
    if (!updateSave.exec()) {
        return rollback(queryError(QStringLiteral("세이브 메타데이터 저장 실패"), updateSave));
    }

    QSqlQuery clubDelta(database_);
    clubDelta.prepare(QStringLiteral(
        "INSERT INTO save_club_deltas "
        "(save_id, club_id, cash_pence, transfer_budget_pence, weekly_wage_budget_pence, "
        "weekly_wage_commitment_pence, debt_pence, manager_board_confidence) "
        "VALUES (:save, :club, :cash, :transfer, :wage_budget, :wage, :debt, :confidence) "
        "ON CONFLICT(save_id, club_id) DO UPDATE SET "
        "cash_pence = excluded.cash_pence, transfer_budget_pence = excluded.transfer_budget_pence, "
        "weekly_wage_budget_pence = excluded.weekly_wage_budget_pence, "
        "weekly_wage_commitment_pence = excluded.weekly_wage_commitment_pence, "
        "debt_pence = excluded.debt_pence, manager_board_confidence = excluded.manager_board_confidence"));
    clubDelta.bindValue(QStringLiteral(":save"), state.saveId);
    clubDelta.bindValue(QStringLiteral(":club"), toQString(state.club.id));
    clubDelta.bindValue(QStringLiteral(":cash"), state.club.finance.cashPence);
    clubDelta.bindValue(QStringLiteral(":transfer"), state.club.finance.transferBudgetPence);
    clubDelta.bindValue(QStringLiteral(":wage_budget"), state.club.finance.weeklyWageBudgetPence);
    clubDelta.bindValue(QStringLiteral(":wage"), state.club.finance.weeklyWageCommitmentPence);
    clubDelta.bindValue(QStringLiteral(":debt"), state.club.finance.debtPence);
    clubDelta.bindValue(QStringLiteral(":confidence"), state.club.manager.boardConfidence);
    if (!clubDelta.exec()) {
        return rollback(queryError(QStringLiteral("구단 변화량 저장 실패"), clubDelta));
    }

    QSqlQuery deleteFixtures(database_);
    deleteFixtures.prepare(QStringLiteral("DELETE FROM save_fixture_deltas WHERE save_id = :save"));
    deleteFixtures.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteFixtures.exec()) {
        return rollback(queryError(QStringLiteral("기존 경기 변화량 정리 실패"), deleteFixtures));
    }
    for (const auto& fixture : state.fixtures) {
        if (!fixture.played) {
            continue;
        }
        QSqlQuery fixtureDelta(database_);
        fixtureDelta.prepare(QStringLiteral(
            "INSERT INTO save_fixture_deltas "
            "(save_id, fixture_id, played, home_goals, away_goals) "
            "VALUES (:save, :fixture, 1, :home, :away)"));
        fixtureDelta.bindValue(QStringLiteral(":save"), state.saveId);
        fixtureDelta.bindValue(QStringLiteral(":fixture"), toQString(fixture.id));
        fixtureDelta.bindValue(QStringLiteral(":home"), fixture.homeGoals);
        fixtureDelta.bindValue(QStringLiteral(":away"), fixture.awayGoals);
        if (!fixtureDelta.exec()) {
            return rollback(queryError(QStringLiteral("경기 변화량 저장 실패"), fixtureDelta));
        }
    }

    QSqlQuery deleteInbox(database_);
    deleteInbox.prepare(QStringLiteral("DELETE FROM save_inbox_items WHERE save_id = :save"));
    deleteInbox.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteInbox.exec()) {
        return rollback(queryError(QStringLiteral("기존 받은 편지함 정리 실패"), deleteInbox));
    }
    for (const auto& item : state.inbox) {
        QSqlQuery insertInbox(database_);
        insertInbox.prepare(QStringLiteral(
            "INSERT INTO save_inbox_items "
            "(save_id, item_id, event_key, item_date, category, title, body, unread, requires_decision) "
            "VALUES (:save, :id, :key, :date, :category, :title, :body, :unread, :decision)"));
        insertInbox.bindValue(QStringLiteral(":save"), state.saveId);
        insertInbox.bindValue(QStringLiteral(":id"), item.id);
        insertInbox.bindValue(QStringLiteral(":key"), toQString(item.eventKey));
        insertInbox.bindValue(QStringLiteral(":date"), toQString(item.date.toIsoString()));
        insertInbox.bindValue(QStringLiteral(":category"), toQString(inboxCategoryKey(item.category)));
        insertInbox.bindValue(QStringLiteral(":title"), toQString(item.title));
        insertInbox.bindValue(QStringLiteral(":body"), toQString(item.body));
        insertInbox.bindValue(QStringLiteral(":unread"), item.unread ? 1 : 0);
        insertInbox.bindValue(QStringLiteral(":decision"), item.requiresDecision ? 1 : 0);
        if (!insertInbox.exec()) {
            return rollback(queryError(QStringLiteral("받은 편지함 저장 실패"), insertInbox));
        }
    }

    QSqlQuery deleteEvents(database_);
    deleteEvents.prepare(QStringLiteral("DELETE FROM save_event_queue WHERE save_id = :save"));
    deleteEvents.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteEvents.exec()) {
        return rollback(queryError(QStringLiteral("기존 이벤트 큐 정리 실패"), deleteEvents));
    }
    for (const auto& event : state.eventQueue) {
        QSqlQuery insertEvent(database_);
        insertEvent.prepare(QStringLiteral(
            "INSERT INTO save_event_queue "
            "(save_id, event_id, event_key, due_date, category, title, body, requires_decision, processed) "
            "VALUES (:save, :id, :key, :date, :category, :title, :body, :decision, :processed)"));
        insertEvent.bindValue(QStringLiteral(":save"), state.saveId);
        insertEvent.bindValue(QStringLiteral(":id"), event.id);
        insertEvent.bindValue(QStringLiteral(":key"), toQString(event.key));
        insertEvent.bindValue(QStringLiteral(":date"), toQString(event.dueDate.toIsoString()));
        insertEvent.bindValue(QStringLiteral(":category"), toQString(inboxCategoryKey(event.category)));
        insertEvent.bindValue(QStringLiteral(":title"), toQString(event.title));
        insertEvent.bindValue(QStringLiteral(":body"), toQString(event.body));
        insertEvent.bindValue(QStringLiteral(":decision"), event.requiresDecision ? 1 : 0);
        insertEvent.bindValue(QStringLiteral(":processed"), event.processed ? 1 : 0);
        if (!insertEvent.exec()) {
            return rollback(queryError(QStringLiteral("이벤트 큐 저장 실패"), insertEvent));
        }
    }

    QSqlQuery deleteTransfers(database_);
    deleteTransfers.prepare(QStringLiteral("DELETE FROM save_transfer_proposals WHERE save_id = :save"));
    deleteTransfers.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteTransfers.exec()) {
        return rollback(queryError(QStringLiteral("기존 영입 승인 안건 정리 실패"), deleteTransfers));
    }
    for (const auto& proposal : state.transferProposals) {
        QSqlQuery insertTransfer(database_);
        insertTransfer.prepare(QStringLiteral(
            "INSERT INTO save_transfer_proposals "
            "(save_id, proposal_id, target_person_id, from_club_id, fee_pence, "
            "weekly_wage_pence, contract_years, requested_by, rationale, proposed_date, "
            "decision_deadline, decision_date, join_date, status, data_confidence) "
            "VALUES (:save, :proposal, :target, :from_club, :fee, :wage, :years, "
            ":requester, :rationale, :proposed, :deadline, :decision, :join_date, "
            ":status, :confidence)"));
        insertTransfer.bindValue(QStringLiteral(":save"), state.saveId);
        insertTransfer.bindValue(QStringLiteral(":proposal"), toQString(proposal.id));
        insertTransfer.bindValue(QStringLiteral(":target"), toQString(proposal.target.id));
        insertTransfer.bindValue(QStringLiteral(":from_club"), toQString(proposal.fromClubId));
        insertTransfer.bindValue(QStringLiteral(":fee"), proposal.feePence);
        insertTransfer.bindValue(QStringLiteral(":wage"), proposal.weeklyWagePence);
        insertTransfer.bindValue(QStringLiteral(":years"), proposal.contractYears);
        insertTransfer.bindValue(QStringLiteral(":requester"), toQString(proposal.requestedBy));
        insertTransfer.bindValue(QStringLiteral(":rationale"), toQString(proposal.rationale));
        insertTransfer.bindValue(QStringLiteral(":proposed"), toQString(proposal.proposedDate.toIsoString()));
        insertTransfer.bindValue(QStringLiteral(":deadline"), toQString(proposal.decisionDeadline.toIsoString()));
        insertTransfer.bindValue(QStringLiteral(":decision"), optionalDateValue(proposal.decisionDate));
        insertTransfer.bindValue(QStringLiteral(":join_date"), optionalDateValue(proposal.joinDate));
        insertTransfer.bindValue(QStringLiteral(":status"), toQString(transferStatusKey(proposal.status)));
        insertTransfer.bindValue(QStringLiteral(":confidence"), toQString(proposal.dataConfidence));
        if (!insertTransfer.exec()) {
            return rollback(queryError(QStringLiteral("영입 승인 안건 저장 실패"), insertTransfer));
        }
    }
    for (const auto& proposal : state.transferProposals) {
        const TransferFeeTerms feeTerms =
            effectiveFeeTerms(proposal.feePence, proposal.feeTerms);
        const PlayerContractTerms contractTerms = effectiveContractTerms(
            proposal.weeklyWagePence,
            proposal.contractYears,
            proposal.contractTerms);
        QSqlQuery insertDetails(database_);
        insertDetails.prepare(QStringLiteral(
            "INSERT INTO save_transfer_proposal_details "
            "(save_id, proposal_id, upfront_fee_pence, installment_total_pence, "
            "installment_count, appearance_bonus_pence, appearance_threshold, "
            "sell_on_percentage, signing_bonus_pence, loyalty_bonus_pence, "
            "appearance_fee_pence, goal_bonus_pence, agent_fee_pence, "
            "release_clause_pence, squad_role, medical_status, registration_status) "
            "VALUES (:save, :proposal, :upfront, :installment_total, "
            ":installment_count, :appearance_bonus, :appearance_threshold, "
            ":sell_on, :signing, :loyalty, :appearance_fee, :goal_bonus, "
            ":agent, :release_clause, :role, :medical, :registration)"));
        insertDetails.bindValue(QStringLiteral(":save"), state.saveId);
        insertDetails.bindValue(QStringLiteral(":proposal"), toQString(proposal.id));
        insertDetails.bindValue(QStringLiteral(":upfront"), feeTerms.upfrontFeePence);
        insertDetails.bindValue(QStringLiteral(":installment_total"), feeTerms.installmentTotalPence);
        insertDetails.bindValue(QStringLiteral(":installment_count"), feeTerms.installmentCount);
        insertDetails.bindValue(QStringLiteral(":appearance_bonus"), feeTerms.appearanceBonusPence);
        insertDetails.bindValue(QStringLiteral(":appearance_threshold"), feeTerms.appearanceThreshold);
        insertDetails.bindValue(QStringLiteral(":sell_on"), feeTerms.sellOnPercentage);
        insertDetails.bindValue(QStringLiteral(":signing"), contractTerms.signingBonusPence);
        insertDetails.bindValue(QStringLiteral(":loyalty"), contractTerms.loyaltyBonusPence);
        insertDetails.bindValue(QStringLiteral(":appearance_fee"), contractTerms.appearanceFeePence);
        insertDetails.bindValue(QStringLiteral(":goal_bonus"), contractTerms.goalBonusPence);
        insertDetails.bindValue(QStringLiteral(":agent"), contractTerms.agentFeePence);
        insertDetails.bindValue(QStringLiteral(":release_clause"), contractTerms.releaseClausePence);
        insertDetails.bindValue(QStringLiteral(":role"), toQString(contractTerms.squadRole));
        insertDetails.bindValue(QStringLiteral(":medical"), toQString(proposal.medicalStatus));
        insertDetails.bindValue(QStringLiteral(":registration"), toQString(proposal.registrationStatus));
        if (!insertDetails.exec()) {
            return rollback(queryError(QStringLiteral("상세 영입 승인 조건 저장 실패"), insertDetails));
        }
    }

    QSqlQuery deletePursuits(database_);
    deletePursuits.prepare(QStringLiteral("DELETE FROM save_transfer_pursuits WHERE save_id = :save"));
    deletePursuits.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deletePursuits.exec()) {
        return rollback(queryError(QStringLiteral("기존 영입 협상 상태 정리 실패"), deletePursuits));
    }
    for (const auto& pursuit : state.transferPursuits) {
        QSqlQuery insertPursuit(database_);
        insertPursuit.prepare(QStringLiteral(
            "INSERT INTO save_transfer_pursuits "
            "(save_id, pursuit_id, target_person_id, stage, current_offer_pence, "
            "club_counter_pence, negotiation_round, started_date, next_action_date, "
            "decision_deadline, last_update_date, response_note, rival_bid_active, "
            "rival_club_id, rival_bid_pence, linked_proposal_id) "
            "VALUES (:save, :pursuit, :target, :stage, :offer, :counter, :round, "
            ":started, :next_action, :deadline, :updated, :note, :rival_active, "
            ":rival_club, :rival_bid, :proposal)"));
        insertPursuit.bindValue(QStringLiteral(":save"), state.saveId);
        insertPursuit.bindValue(QStringLiteral(":pursuit"), toQString(pursuit.id));
        insertPursuit.bindValue(QStringLiteral(":target"), toQString(pursuit.candidatePlayerId));
        insertPursuit.bindValue(
            QStringLiteral(":stage"),
            toQString(legacyTransferPursuitStageKey(pursuit.stage)));
        insertPursuit.bindValue(QStringLiteral(":offer"), pursuit.currentOfferPence);
        insertPursuit.bindValue(QStringLiteral(":counter"), pursuit.clubCounterPence);
        insertPursuit.bindValue(QStringLiteral(":round"), pursuit.negotiationRound);
        insertPursuit.bindValue(QStringLiteral(":started"), toQString(pursuit.startedDate.toIsoString()));
        insertPursuit.bindValue(QStringLiteral(":next_action"), toQString(pursuit.nextActionDate.toIsoString()));
        insertPursuit.bindValue(QStringLiteral(":deadline"), toQString(pursuit.decisionDeadline.toIsoString()));
        insertPursuit.bindValue(QStringLiteral(":updated"), optionalDateValue(pursuit.lastUpdateDate));
        insertPursuit.bindValue(QStringLiteral(":note"), toQString(pursuit.responseNote));
        insertPursuit.bindValue(QStringLiteral(":rival_active"), pursuit.rivalBidActive ? 1 : 0);
        if (pursuit.rivalClubId.empty()) {
            insertPursuit.bindValue(QStringLiteral(":rival_club"), QVariant {});
        } else {
            insertPursuit.bindValue(QStringLiteral(":rival_club"), toQString(pursuit.rivalClubId));
        }
        insertPursuit.bindValue(QStringLiteral(":rival_bid"), pursuit.rivalBidPence);
        if (pursuit.linkedProposalId.empty()) {
            insertPursuit.bindValue(QStringLiteral(":proposal"), QVariant {});
        } else {
            insertPursuit.bindValue(QStringLiteral(":proposal"), toQString(pursuit.linkedProposalId));
        }
        if (!insertPursuit.exec()) {
            return rollback(queryError(QStringLiteral("영입 협상 상태 저장 실패"), insertPursuit));
        }
    }
    for (const auto& pursuit : state.transferPursuits) {
        const TransferFeeTerms offerTerms = effectiveFeeTerms(
            pursuit.currentOfferPence,
            pursuit.offerTerms);
        const TransferFeeTerms counterTerms =
            pursuit.clubCounterPence > 0
            ? effectiveFeeTerms(pursuit.clubCounterPence, pursuit.clubCounterTerms)
            : pursuit.clubCounterTerms;
        QSqlQuery insertDetails(database_);
        insertDetails.prepare(QStringLiteral(
            "INSERT INTO save_transfer_pursuit_details "
            "(save_id, pursuit_id, detailed_stage, "
            "offer_upfront_pence, offer_installment_total_pence, "
            "offer_installment_count, offer_appearance_bonus_pence, "
            "offer_appearance_threshold, offer_sell_on_percentage, "
            "counter_upfront_pence, counter_installment_total_pence, "
            "counter_installment_count, counter_appearance_bonus_pence, "
            "counter_appearance_threshold, counter_sell_on_percentage, "
            "player_offer_weekly_wage_pence, player_offer_contract_years, "
            "player_offer_signing_bonus_pence, player_offer_loyalty_bonus_pence, "
            "player_offer_appearance_fee_pence, player_offer_goal_bonus_pence, "
            "player_offer_agent_fee_pence, player_offer_release_clause_pence, "
            "player_offer_squad_role, player_counter_weekly_wage_pence, "
            "player_counter_contract_years, player_counter_signing_bonus_pence, "
            "player_counter_loyalty_bonus_pence, player_counter_appearance_fee_pence, "
            "player_counter_goal_bonus_pence, player_counter_agent_fee_pence, "
            "player_counter_release_clause_pence, player_counter_squad_role, "
            "player_negotiation_round, player_offer_submitted, medical_risk_percent, "
            "medical_status, registration_status) "
            "VALUES (:save, :pursuit, :stage, :offer_upfront, "
            ":offer_installment_total, :offer_installment_count, "
            ":offer_appearance_bonus, :offer_appearance_threshold, :offer_sell_on, "
            ":counter_upfront, :counter_installment_total, :counter_installment_count, "
            ":counter_appearance_bonus, :counter_appearance_threshold, :counter_sell_on, "
            ":player_wage, :player_years, :player_signing, :player_loyalty, "
            ":player_appearance, :player_goal, :player_agent, :player_release, :player_role, "
            ":counter_wage, :counter_years, :counter_signing, :counter_loyalty, "
            ":counter_appearance, :counter_goal, :counter_agent, :counter_release, "
            ":counter_role, :player_round, :player_submitted, :medical_risk, "
            ":medical_status, :registration_status)"));
        insertDetails.bindValue(QStringLiteral(":save"), state.saveId);
        insertDetails.bindValue(QStringLiteral(":pursuit"), toQString(pursuit.id));
        insertDetails.bindValue(QStringLiteral(":stage"), toQString(transferPursuitStageKey(pursuit.stage)));
        insertDetails.bindValue(QStringLiteral(":offer_upfront"), offerTerms.upfrontFeePence);
        insertDetails.bindValue(QStringLiteral(":offer_installment_total"), offerTerms.installmentTotalPence);
        insertDetails.bindValue(QStringLiteral(":offer_installment_count"), offerTerms.installmentCount);
        insertDetails.bindValue(QStringLiteral(":offer_appearance_bonus"), offerTerms.appearanceBonusPence);
        insertDetails.bindValue(QStringLiteral(":offer_appearance_threshold"), offerTerms.appearanceThreshold);
        insertDetails.bindValue(QStringLiteral(":offer_sell_on"), offerTerms.sellOnPercentage);
        insertDetails.bindValue(QStringLiteral(":counter_upfront"), counterTerms.upfrontFeePence);
        insertDetails.bindValue(QStringLiteral(":counter_installment_total"), counterTerms.installmentTotalPence);
        insertDetails.bindValue(QStringLiteral(":counter_installment_count"), counterTerms.installmentCount);
        insertDetails.bindValue(QStringLiteral(":counter_appearance_bonus"), counterTerms.appearanceBonusPence);
        insertDetails.bindValue(QStringLiteral(":counter_appearance_threshold"), counterTerms.appearanceThreshold);
        insertDetails.bindValue(QStringLiteral(":counter_sell_on"), counterTerms.sellOnPercentage);
        insertDetails.bindValue(QStringLiteral(":player_wage"), pursuit.playerOfferTerms.weeklyWagePence);
        insertDetails.bindValue(QStringLiteral(":player_years"), pursuit.playerOfferTerms.contractYears);
        insertDetails.bindValue(QStringLiteral(":player_signing"), pursuit.playerOfferTerms.signingBonusPence);
        insertDetails.bindValue(QStringLiteral(":player_loyalty"), pursuit.playerOfferTerms.loyaltyBonusPence);
        insertDetails.bindValue(QStringLiteral(":player_appearance"), pursuit.playerOfferTerms.appearanceFeePence);
        insertDetails.bindValue(QStringLiteral(":player_goal"), pursuit.playerOfferTerms.goalBonusPence);
        insertDetails.bindValue(QStringLiteral(":player_agent"), pursuit.playerOfferTerms.agentFeePence);
        insertDetails.bindValue(QStringLiteral(":player_release"), pursuit.playerOfferTerms.releaseClausePence);
        insertDetails.bindValue(QStringLiteral(":player_role"), toQString(pursuit.playerOfferTerms.squadRole));
        insertDetails.bindValue(QStringLiteral(":counter_wage"), pursuit.playerCounterTerms.weeklyWagePence);
        insertDetails.bindValue(QStringLiteral(":counter_years"), pursuit.playerCounterTerms.contractYears);
        insertDetails.bindValue(QStringLiteral(":counter_signing"), pursuit.playerCounterTerms.signingBonusPence);
        insertDetails.bindValue(QStringLiteral(":counter_loyalty"), pursuit.playerCounterTerms.loyaltyBonusPence);
        insertDetails.bindValue(QStringLiteral(":counter_appearance"), pursuit.playerCounterTerms.appearanceFeePence);
        insertDetails.bindValue(QStringLiteral(":counter_goal"), pursuit.playerCounterTerms.goalBonusPence);
        insertDetails.bindValue(QStringLiteral(":counter_agent"), pursuit.playerCounterTerms.agentFeePence);
        insertDetails.bindValue(QStringLiteral(":counter_release"), pursuit.playerCounterTerms.releaseClausePence);
        insertDetails.bindValue(QStringLiteral(":counter_role"), toQString(pursuit.playerCounterTerms.squadRole));
        insertDetails.bindValue(QStringLiteral(":player_round"), pursuit.playerNegotiationRound);
        insertDetails.bindValue(QStringLiteral(":player_submitted"), pursuit.playerOfferSubmitted ? 1 : 0);
        insertDetails.bindValue(QStringLiteral(":medical_risk"), pursuit.medicalRiskPercent);
        insertDetails.bindValue(QStringLiteral(":medical_status"), toQString(pursuit.medicalStatus));
        insertDetails.bindValue(QStringLiteral(":registration_status"), toQString(pursuit.registrationStatus));
        if (!insertDetails.exec()) {
            return rollback(queryError(QStringLiteral("상세 영입 협상 상태 저장 실패"), insertDetails));
        }
    }

    QSqlQuery deleteOutgoingOffers(database_);
    deleteOutgoingOffers.prepare(QStringLiteral(
        "DELETE FROM save_outgoing_transfer_offers WHERE save_id = :save"));
    deleteOutgoingOffers.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteOutgoingOffers.exec()) {
        return rollback(queryError(QStringLiteral("기존 선수 판매 제안 정리 실패"), deleteOutgoingOffers));
    }
    for (const auto& offer : state.outgoingTransferOffers) {
        QSqlQuery insertOutgoingOffer(database_);
        insertOutgoingOffer.prepare(QStringLiteral(
            "INSERT INTO save_outgoing_transfer_offers "
            "(save_id, offer_id, player_person_id, buyer_club_id, fee_pence, "
            "weekly_wage_relief_pence, "
            "proposed_date, decision_deadline, decision_date, completion_date, "
            "status, requested_role, rationale, data_confidence) "
            "VALUES (:save, :offer, :player, :buyer, :fee, :wage_relief, :proposed, :deadline, "
            ":decision, :completion, :status, :role, :rationale, :confidence)"));
        insertOutgoingOffer.bindValue(QStringLiteral(":save"), state.saveId);
        insertOutgoingOffer.bindValue(QStringLiteral(":offer"), toQString(offer.id));
        insertOutgoingOffer.bindValue(QStringLiteral(":player"), toQString(offer.playerId));
        insertOutgoingOffer.bindValue(QStringLiteral(":buyer"), toQString(offer.buyerClubId));
        insertOutgoingOffer.bindValue(QStringLiteral(":fee"), offer.feePence);
        insertOutgoingOffer.bindValue(QStringLiteral(":wage_relief"), offer.weeklyWageReliefPence);
        insertOutgoingOffer.bindValue(QStringLiteral(":proposed"), toQString(offer.proposedDate.toIsoString()));
        insertOutgoingOffer.bindValue(QStringLiteral(":deadline"), toQString(offer.decisionDeadline.toIsoString()));
        insertOutgoingOffer.bindValue(QStringLiteral(":decision"), optionalDateValue(offer.decisionDate));
        insertOutgoingOffer.bindValue(QStringLiteral(":completion"), optionalDateValue(offer.completionDate));
        insertOutgoingOffer.bindValue(QStringLiteral(":status"), toQString(outgoingTransferStatusKey(offer.status)));
        insertOutgoingOffer.bindValue(QStringLiteral(":role"), toQString(offer.requestedRole));
        insertOutgoingOffer.bindValue(QStringLiteral(":rationale"), toQString(offer.rationale));
        insertOutgoingOffer.bindValue(QStringLiteral(":confidence"), toQString(offer.dataConfidence));
        if (!insertOutgoingOffer.exec()) {
            return rollback(queryError(QStringLiteral("선수 판매 제안 저장 실패"), insertOutgoingOffer));
        }
    }
    for (const auto& offer : state.outgoingTransferOffers) {
        const TransferFeeTerms feeTerms =
            effectiveFeeTerms(offer.feePence, offer.feeTerms);
        QSqlQuery insertDetails(database_);
        insertDetails.prepare(QStringLiteral(
            "INSERT INTO save_outgoing_transfer_details "
            "(save_id, offer_id, upfront_fee_pence, installment_total_pence, "
            "installment_count, appearance_bonus_pence, appearance_threshold, "
            "sell_on_percentage, buyer_counter_upfront_pence, "
            "buyer_counter_installment_total_pence, buyer_counter_installment_count, "
            "buyer_counter_appearance_bonus_pence, buyer_counter_appearance_threshold, "
            "buyer_counter_sell_on_percentage, negotiation_stage, negotiation_round, "
            "next_action_date, buyer_maximum_value_pence) "
            "VALUES (:save, :offer, :upfront, :installment_total, :installment_count, "
            ":appearance_bonus, :appearance_threshold, :sell_on, :counter_upfront, "
            ":counter_installment_total, :counter_installment_count, "
            ":counter_appearance_bonus, :counter_appearance_threshold, :counter_sell_on, "
            ":stage, :round, :next_action, :buyer_maximum)"));
        insertDetails.bindValue(QStringLiteral(":save"), state.saveId);
        insertDetails.bindValue(QStringLiteral(":offer"), toQString(offer.id));
        insertDetails.bindValue(QStringLiteral(":upfront"), feeTerms.upfrontFeePence);
        insertDetails.bindValue(QStringLiteral(":installment_total"), feeTerms.installmentTotalPence);
        insertDetails.bindValue(QStringLiteral(":installment_count"), feeTerms.installmentCount);
        insertDetails.bindValue(QStringLiteral(":appearance_bonus"), feeTerms.appearanceBonusPence);
        insertDetails.bindValue(QStringLiteral(":appearance_threshold"), feeTerms.appearanceThreshold);
        insertDetails.bindValue(QStringLiteral(":sell_on"), feeTerms.sellOnPercentage);
        insertDetails.bindValue(QStringLiteral(":counter_upfront"), offer.buyerCounterTerms.upfrontFeePence);
        insertDetails.bindValue(QStringLiteral(":counter_installment_total"), offer.buyerCounterTerms.installmentTotalPence);
        insertDetails.bindValue(QStringLiteral(":counter_installment_count"), offer.buyerCounterTerms.installmentCount);
        insertDetails.bindValue(QStringLiteral(":counter_appearance_bonus"), offer.buyerCounterTerms.appearanceBonusPence);
        insertDetails.bindValue(QStringLiteral(":counter_appearance_threshold"), offer.buyerCounterTerms.appearanceThreshold);
        insertDetails.bindValue(QStringLiteral(":counter_sell_on"), offer.buyerCounterTerms.sellOnPercentage);
        insertDetails.bindValue(QStringLiteral(":stage"), toQString(outgoingNegotiationStageKey(offer.negotiationStage)));
        insertDetails.bindValue(QStringLiteral(":round"), offer.negotiationRound);
        insertDetails.bindValue(QStringLiteral(":next_action"), optionalDateValue(offer.nextActionDate));
        insertDetails.bindValue(QStringLiteral(":buyer_maximum"), offer.buyerMaximumValuePence);
        if (!insertDetails.exec()) {
            return rollback(queryError(QStringLiteral("상세 선수 판매 조건 저장 실패"), insertDetails));
        }
    }

    QSqlQuery deleteContracts(database_);
    deleteContracts.prepare(QStringLiteral("DELETE FROM save_player_contracts WHERE save_id = :save"));
    deleteContracts.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteContracts.exec()) {
        return rollback(queryError(QStringLiteral("기존 선수 계약 정리 실패"), deleteContracts));
    }
    for (const auto& contract : state.playerContracts) {
        QSqlQuery insertContract(database_);
        insertContract.prepare(QStringLiteral(
            "INSERT INTO save_player_contracts "
            "(save_id, person_id, club_id, contract_start_date, contract_end_date, "
            "weekly_wage_pence, squad_role, source_proposal_id, data_confidence) "
            "VALUES (:save, :person, :club, :start, :end, :wage, :role, "
            ":proposal, :confidence)"));
        insertContract.bindValue(QStringLiteral(":save"), state.saveId);
        insertContract.bindValue(QStringLiteral(":person"), toQString(contract.playerId));
        insertContract.bindValue(QStringLiteral(":club"), toQString(contract.clubId));
        insertContract.bindValue(QStringLiteral(":start"), toQString(contract.startDate.toIsoString()));
        insertContract.bindValue(QStringLiteral(":end"), toQString(contract.endDate.toIsoString()));
        insertContract.bindValue(QStringLiteral(":wage"), contract.weeklyWagePence);
        insertContract.bindValue(QStringLiteral(":role"), toQString(contract.squadRole));
        if (contract.sourceProposalId.empty()) {
            insertContract.bindValue(QStringLiteral(":proposal"), QVariant {});
        } else {
            insertContract.bindValue(QStringLiteral(":proposal"), toQString(contract.sourceProposalId));
        }
        insertContract.bindValue(QStringLiteral(":confidence"), toQString(contract.dataConfidence));
        if (!insertContract.exec()) {
            return rollback(queryError(QStringLiteral("선수 계약 저장 실패"), insertContract));
        }
    }
    for (const auto& contract : state.playerContracts) {
        const int estimatedYears = std::clamp(
            contract.endDate.year - contract.startDate.year,
            1,
            5);
        const PlayerContractTerms terms = effectiveContractTerms(
            contract.weeklyWagePence,
            estimatedYears,
            contract.terms);
        QSqlQuery insertDetails(database_);
        insertDetails.prepare(QStringLiteral(
            "INSERT INTO save_player_contract_details "
            "(save_id, person_id, club_id, contract_years, signing_bonus_pence, "
            "loyalty_bonus_pence, appearance_fee_pence, goal_bonus_pence, "
            "agent_fee_pence, release_clause_pence) "
            "VALUES (:save, :person, :club, :years, :signing, :loyalty, "
            ":appearance, :goal, :agent, :release_clause)"));
        insertDetails.bindValue(QStringLiteral(":save"), state.saveId);
        insertDetails.bindValue(QStringLiteral(":person"), toQString(contract.playerId));
        insertDetails.bindValue(QStringLiteral(":club"), toQString(contract.clubId));
        insertDetails.bindValue(QStringLiteral(":years"), terms.contractYears);
        insertDetails.bindValue(QStringLiteral(":signing"), terms.signingBonusPence);
        insertDetails.bindValue(QStringLiteral(":loyalty"), terms.loyaltyBonusPence);
        insertDetails.bindValue(QStringLiteral(":appearance"), terms.appearanceFeePence);
        insertDetails.bindValue(QStringLiteral(":goal"), terms.goalBonusPence);
        insertDetails.bindValue(QStringLiteral(":agent"), terms.agentFeePence);
        insertDetails.bindValue(QStringLiteral(":release_clause"), terms.releaseClausePence);
        if (!insertDetails.exec()) {
            return rollback(queryError(QStringLiteral("선수 상세 계약 저장 실패"), insertDetails));
        }
    }

    QSqlQuery deleteContractLifecycle(database_);
    deleteContractLifecycle.prepare(QStringLiteral(
        "DELETE FROM save_player_contract_lifecycle WHERE save_id = :save"));
    deleteContractLifecycle.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteContractLifecycle.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 선수 계약 생명주기 정리 실패"),
            deleteContractLifecycle));
    }
    for (const auto& contract : state.playerContracts) {
        QSqlQuery insertLifecycle(database_);
        insertLifecycle.prepare(QStringLiteral(
            "INSERT INTO save_player_contract_lifecycle "
            "(save_id, person_id, club_id, lifecycle_status, expiry_notified) "
            "VALUES (:save, :person, :club, :status, :notified)"));
        insertLifecycle.bindValue(QStringLiteral(":save"), state.saveId);
        insertLifecycle.bindValue(
            QStringLiteral(":person"),
            toQString(contract.playerId));
        insertLifecycle.bindValue(
            QStringLiteral(":club"),
            toQString(contract.clubId));
        insertLifecycle.bindValue(
            QStringLiteral(":status"),
            toQString(contract.lifecycleStatus));
        insertLifecycle.bindValue(
            QStringLiteral(":notified"),
            contract.expiryNotified ? 1 : 0);
        if (!insertLifecycle.exec()) {
            return rollback(queryError(
                QStringLiteral("선수 계약 생명주기 저장 실패"),
                insertLifecycle));
        }
    }

    const auto bindLoanTerms = [](
                                   QSqlQuery& query,
                                   const QString& prefix,
                                   const LoanTerms& terms) {
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_fee"),
            terms.loanFeePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_wage_percent"),
            terms.borrowingClubWagePercentage);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_role"),
            toQString(terms.promisedRole));
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_end"),
            toQString(terms.endDate.toIsoString()));
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_can_recall"),
            terms.canRecall ? 1 : 0);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_recall_days"),
            terms.recallNoticeDays);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_future_type"),
            toQString(terms.futureFeeType));
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_future_fee"),
            terms.futureFeePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_parent_restricted"),
            terms.parentClubMatchRestricted ? 1 : 0);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_unused_fee"),
            terms.unusedPlayerFeePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_treatment"),
            toQString(terms.treatmentResponsibility));
    };

    QSqlQuery deleteLoanDeals(database_);
    deleteLoanDeals.prepare(QStringLiteral(
        "DELETE FROM save_loan_deals WHERE save_id = :save"));
    deleteLoanDeals.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteLoanDeals.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 임대 안건 정리 실패"),
            deleteLoanDeals));
    }
    for (const auto& loan : state.loanDeals) {
        QSqlQuery insertLoan(database_);
        insertLoan.prepare(QStringLiteral(
            "INSERT INTO save_loan_deals ("
            "save_id, loan_id, person_id, direction, parent_club_id, "
            "loan_club_id, purpose, proposed_date, decision_deadline, "
            "planned_start_date, stage, loan_fee_pence, "
            "borrowing_wage_percentage, promised_role, end_date, can_recall, "
            "recall_notice_days, future_fee_type, future_fee_pence, "
            "parent_match_restricted, unused_player_fee_pence, "
            "treatment_responsibility, counter_loan_fee_pence, "
            "counter_borrowing_wage_percentage, counter_promised_role, "
            "counter_end_date, counter_can_recall, counter_recall_notice_days, "
            "counter_future_fee_type, counter_future_fee_pence, "
            "counter_parent_match_restricted, counter_unused_player_fee_pence, "
            "counter_treatment_responsibility, next_action_date, "
            "actual_start_date, actual_end_date, last_report_date, "
            "parent_squad_group, negotiation_round, appearances, starts, "
            "response_note, data_confidence"
            ") VALUES ("
            ":save, :loan, :person, :direction, :parent, :loan_club, "
            ":purpose, :proposed, :deadline, :planned, :stage, "
            ":terms_fee, :terms_wage_percent, :terms_role, :terms_end, "
            ":terms_can_recall, :terms_recall_days, :terms_future_type, "
            ":terms_future_fee, :terms_parent_restricted, :terms_unused_fee, "
            ":terms_treatment, :counter_fee, :counter_wage_percent, "
            ":counter_role, :counter_end, :counter_can_recall, "
            ":counter_recall_days, :counter_future_type, :counter_future_fee, "
            ":counter_parent_restricted, :counter_unused_fee, "
            ":counter_treatment, :next_action, :actual_start, :actual_end, "
            ":last_report, :parent_group, :round, :appearances, :starts, "
            ":response, :confidence)"));
        insertLoan.bindValue(QStringLiteral(":save"), state.saveId);
        insertLoan.bindValue(QStringLiteral(":loan"), toQString(loan.id));
        insertLoan.bindValue(
            QStringLiteral(":person"),
            toQString(loan.player.id));
        insertLoan.bindValue(
            QStringLiteral(":direction"),
            toQString(loanDirectionKey(loan.direction)));
        insertLoan.bindValue(
            QStringLiteral(":parent"),
            toQString(loan.parentClubId));
        insertLoan.bindValue(
            QStringLiteral(":loan_club"),
            toQString(loan.loanClubId));
        insertLoan.bindValue(
            QStringLiteral(":purpose"),
            toQString(loan.purpose));
        insertLoan.bindValue(
            QStringLiteral(":proposed"),
            toQString(loan.proposedDate.toIsoString()));
        insertLoan.bindValue(
            QStringLiteral(":deadline"),
            toQString(loan.decisionDeadline.toIsoString()));
        insertLoan.bindValue(
            QStringLiteral(":planned"),
            toQString(loan.plannedStartDate.toIsoString()));
        insertLoan.bindValue(
            QStringLiteral(":stage"),
            toQString(loanStageKey(loan.stage)));
        bindLoanTerms(insertLoan, QStringLiteral("terms"), loan.terms);
        bindLoanTerms(
            insertLoan,
            QStringLiteral("counter"),
            loan.counterTerms);
        insertLoan.bindValue(
            QStringLiteral(":next_action"),
            optionalDateValue(loan.nextActionDate));
        insertLoan.bindValue(
            QStringLiteral(":actual_start"),
            optionalDateValue(loan.actualStartDate));
        insertLoan.bindValue(
            QStringLiteral(":actual_end"),
            optionalDateValue(loan.actualEndDate));
        insertLoan.bindValue(
            QStringLiteral(":last_report"),
            optionalDateValue(loan.lastReportDate));
        insertLoan.bindValue(
            QStringLiteral(":parent_group"),
            toQString(loan.parentSquadGroup));
        insertLoan.bindValue(
            QStringLiteral(":round"),
            loan.negotiationRound);
        insertLoan.bindValue(
            QStringLiteral(":appearances"),
            loan.appearances);
        insertLoan.bindValue(QStringLiteral(":starts"), loan.starts);
        insertLoan.bindValue(
            QStringLiteral(":response"),
            toQString(loan.responseNote));
        insertLoan.bindValue(
            QStringLiteral(":confidence"),
            toQString(loan.dataConfidence));
        if (!insertLoan.exec()) {
            return rollback(queryError(
                QStringLiteral("임대 안건 저장 실패"),
                insertLoan));
        }
    }

    const auto bindContractTerms = [](
                                       QSqlQuery& query,
                                       const QString& prefix,
                                       const PlayerContractTerms& terms) {
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_wage"),
            terms.weeklyWagePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_years"),
            terms.contractYears);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_signing"),
            terms.signingBonusPence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_loyalty"),
            terms.loyaltyBonusPence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_appearance"),
            terms.appearanceFeePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_goal"),
            terms.goalBonusPence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_agent"),
            terms.agentFeePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_release"),
            terms.releaseClausePence);
        query.bindValue(
            QStringLiteral(":") + prefix + QStringLiteral("_role"),
            toQString(terms.squadRole));
    };

    QSqlQuery deleteRenewals(database_);
    deleteRenewals.prepare(QStringLiteral(
        "DELETE FROM save_contract_renewals WHERE save_id = :save"));
    deleteRenewals.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteRenewals.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 재계약 안건 정리 실패"),
            deleteRenewals));
    }
    for (const auto& renewal : state.contractRenewals) {
        QSqlQuery insertRenewal(database_);
        insertRenewal.prepare(QStringLiteral(
            "INSERT INTO save_contract_renewals ("
            "save_id, renewal_id, person_id, player_name, requested_by, "
            "rationale, current_contract_end_date, decision_deadline, stage, "
            "recommended_weekly_wage_pence, recommended_contract_years, "
            "recommended_signing_bonus_pence, recommended_loyalty_bonus_pence, "
            "recommended_appearance_fee_pence, recommended_goal_bonus_pence, "
            "recommended_agent_fee_pence, recommended_release_clause_pence, "
            "recommended_squad_role, offer_weekly_wage_pence, "
            "offer_contract_years, offer_signing_bonus_pence, "
            "offer_loyalty_bonus_pence, offer_appearance_fee_pence, "
            "offer_goal_bonus_pence, offer_agent_fee_pence, "
            "offer_release_clause_pence, offer_squad_role, "
            "counter_weekly_wage_pence, counter_contract_years, "
            "counter_signing_bonus_pence, counter_loyalty_bonus_pence, "
            "counter_appearance_fee_pence, counter_goal_bonus_pence, "
            "counter_agent_fee_pence, counter_release_clause_pence, "
            "counter_squad_role, next_action_date, last_update_date, "
            "signed_date, player_interest, agent_patience, negotiation_round, "
            "response_note, data_confidence"
            ") VALUES ("
            ":save, :renewal, :person, :player_name, :requested_by, "
            ":rationale, :current_end, :deadline, :stage, "
            ":recommended_wage, :recommended_years, :recommended_signing, "
            ":recommended_loyalty, :recommended_appearance, :recommended_goal, "
            ":recommended_agent, :recommended_release, :recommended_role, "
            ":offer_wage, :offer_years, :offer_signing, :offer_loyalty, "
            ":offer_appearance, :offer_goal, :offer_agent, :offer_release, "
            ":offer_role, :counter_wage, :counter_years, :counter_signing, "
            ":counter_loyalty, :counter_appearance, :counter_goal, "
            ":counter_agent, :counter_release, :counter_role, :next_action, "
            ":last_update, :signed, :interest, :patience, :round, "
            ":response, :confidence)"));
        insertRenewal.bindValue(QStringLiteral(":save"), state.saveId);
        insertRenewal.bindValue(
            QStringLiteral(":renewal"),
            toQString(renewal.id));
        insertRenewal.bindValue(
            QStringLiteral(":person"),
            toQString(renewal.playerId));
        insertRenewal.bindValue(
            QStringLiteral(":player_name"),
            toQString(renewal.playerName));
        insertRenewal.bindValue(
            QStringLiteral(":requested_by"),
            toQString(renewal.requestedBy));
        insertRenewal.bindValue(
            QStringLiteral(":rationale"),
            toQString(renewal.rationale));
        insertRenewal.bindValue(
            QStringLiteral(":current_end"),
            toQString(renewal.currentContractEndDate.toIsoString()));
        insertRenewal.bindValue(
            QStringLiteral(":deadline"),
            toQString(renewal.decisionDeadline.toIsoString()));
        insertRenewal.bindValue(
            QStringLiteral(":stage"),
            toQString(contractRenewalStageKey(renewal.stage)));
        bindContractTerms(
            insertRenewal,
            QStringLiteral("recommended"),
            renewal.recommendedTerms);
        bindContractTerms(
            insertRenewal,
            QStringLiteral("offer"),
            renewal.offerTerms);
        bindContractTerms(
            insertRenewal,
            QStringLiteral("counter"),
            renewal.counterTerms);
        insertRenewal.bindValue(
            QStringLiteral(":next_action"),
            optionalDateValue(renewal.nextActionDate));
        insertRenewal.bindValue(
            QStringLiteral(":last_update"),
            optionalDateValue(renewal.lastUpdateDate));
        insertRenewal.bindValue(
            QStringLiteral(":signed"),
            optionalDateValue(renewal.signedDate));
        insertRenewal.bindValue(
            QStringLiteral(":interest"),
            renewal.playerInterest);
        insertRenewal.bindValue(
            QStringLiteral(":patience"),
            renewal.agentPatience);
        insertRenewal.bindValue(
            QStringLiteral(":round"),
            renewal.negotiationRound);
        insertRenewal.bindValue(
            QStringLiteral(":response"),
            toQString(renewal.responseNote));
        insertRenewal.bindValue(
            QStringLiteral(":confidence"),
            toQString(renewal.dataConfidence));
        if (!insertRenewal.exec()) {
            return rollback(queryError(
                QStringLiteral("재계약 안건 저장 실패"),
                insertRenewal));
        }
    }

    QSqlQuery deleteAiDeals(database_);
    deleteAiDeals.prepare(QStringLiteral(
        "DELETE FROM save_ai_market_deals WHERE save_id = :save"));
    deleteAiDeals.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteAiDeals.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 AI 이적시장 거래 정리 실패"),
            deleteAiDeals));
    }

    QSqlQuery deleteAiClubs(database_);
    deleteAiClubs.prepare(QStringLiteral(
        "DELETE FROM save_ai_club_market_states WHERE save_id = :save"));
    deleteAiClubs.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteAiClubs.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 AI 구단 시장 상태 정리 실패"),
            deleteAiClubs));
    }

    for (const auto& club : state.aiClubMarketStates) {
        QSqlQuery insertAiClub(database_);
        insertAiClub.prepare(QStringLiteral(
            "INSERT INTO save_ai_club_market_states "
            "(save_id, club_id, club_name, cash_pence, "
            "transfer_budget_pence, weekly_wage_budget_pence, "
            "weekly_wage_commitment_pence, debt_pence, finance_confidence, "
            "risk_tolerance, transfer_need, last_evaluation_date, "
            "next_evaluation_date, market_seed, data_confidence) "
            "VALUES (:save, :club, :name, :cash, :transfer, :wage_budget, "
            ":wage, :debt, :finance_confidence, :risk, :need, "
            ":last_eval, :next_eval, :seed, :confidence)"));
        insertAiClub.bindValue(QStringLiteral(":save"), state.saveId);
        insertAiClub.bindValue(QStringLiteral(":club"), toQString(club.clubId));
        insertAiClub.bindValue(QStringLiteral(":name"), toQString(club.clubName));
        insertAiClub.bindValue(QStringLiteral(":cash"), club.finance.cashPence);
        insertAiClub.bindValue(
            QStringLiteral(":transfer"),
            club.finance.transferBudgetPence);
        insertAiClub.bindValue(
            QStringLiteral(":wage_budget"),
            club.finance.weeklyWageBudgetPence);
        insertAiClub.bindValue(
            QStringLiteral(":wage"),
            club.finance.weeklyWageCommitmentPence);
        insertAiClub.bindValue(QStringLiteral(":debt"), club.finance.debtPence);
        insertAiClub.bindValue(
            QStringLiteral(":finance_confidence"),
            toQString(club.finance.confidence));
        insertAiClub.bindValue(QStringLiteral(":risk"), club.riskTolerance);
        insertAiClub.bindValue(QStringLiteral(":need"), toQString(club.transferNeed));
        insertAiClub.bindValue(
            QStringLiteral(":last_eval"),
            optionalDateValue(club.lastEvaluationDate));
        insertAiClub.bindValue(
            QStringLiteral(":next_eval"),
            toQString(club.nextEvaluationDate.toIsoString()));
        insertAiClub.bindValue(
            QStringLiteral(":seed"),
            static_cast<qulonglong>(club.marketSeed));
        insertAiClub.bindValue(
            QStringLiteral(":confidence"),
            toQString(club.dataConfidence));
        if (!insertAiClub.exec()) {
            return rollback(queryError(
                QStringLiteral("AI 구단 시장 상태 저장 실패"),
                insertAiClub));
        }
    }

    for (const auto& deal : state.aiMarketDeals) {
        QSqlQuery insertAiDeal(database_);
        insertAiDeal.prepare(QStringLiteral(
            "INSERT INTO save_ai_market_deals "
            "(save_id, deal_id, deal_type, stage, person_id, "
            "selling_club_id, selling_club_name, buying_club_id, "
            "buying_club_name, proposed_date, decision_deadline, "
            "next_action_date, completion_date, completed_date, "
            "upfront_fee_pence, installment_total_pence, installment_count, "
            "appearance_bonus_pence, appearance_threshold, "
            "sell_on_percentage, weekly_wage_pence, contract_years, "
            "signing_bonus_pence, loyalty_bonus_pence, appearance_fee_pence, "
            "goal_bonus_pence, agent_fee_pence, release_clause_pence, "
            "squad_role, loan_fee_pence, borrowing_wage_percentage, "
            "promised_role, loan_end_date, can_recall, recall_notice_days, "
            "future_fee_type, future_fee_pence, parent_match_restricted, "
            "unused_player_fee_pence, treatment_responsibility, "
            "source_loan_id, source_contract_id, rationale, "
            "player_club_involved, budget_reserved, completion_applied, "
            "data_confidence) "
            "VALUES (:save, :deal, :type, :stage, :person, :selling_id, "
            ":selling_name, :buying_id, :buying_name, :proposed, "
            ":deadline, :next_action, :completion, :completed, "
            ":upfront, :installment_total, :installment_count, "
            ":appearance_bonus, :appearance_threshold, :sell_on, "
            ":contract_wage, :contract_years, :contract_signing, "
            ":contract_loyalty, :contract_appearance, :contract_goal, "
            ":contract_agent, :contract_release, :contract_role, "
            ":loan_fee, :loan_wage_percent, :loan_role, :loan_end, "
            ":loan_can_recall, :loan_recall_days, :loan_future_type, "
            ":loan_future_fee, :loan_parent_restricted, :loan_unused_fee, "
            ":loan_treatment, :source_loan, :source_contract, :rationale, "
            ":player_club, :reserved, :applied, :confidence)"));
        insertAiDeal.bindValue(QStringLiteral(":save"), state.saveId);
        insertAiDeal.bindValue(QStringLiteral(":deal"), toQString(deal.id));
        insertAiDeal.bindValue(
            QStringLiteral(":type"),
            toQString(aiMarketDealTypeKey(deal.type)));
        insertAiDeal.bindValue(
            QStringLiteral(":stage"),
            toQString(aiMarketDealStageKey(deal.stage)));
        insertAiDeal.bindValue(QStringLiteral(":person"), toQString(deal.player.id));
        insertAiDeal.bindValue(
            QStringLiteral(":selling_id"),
            toQString(deal.sellingClubId));
        insertAiDeal.bindValue(
            QStringLiteral(":selling_name"),
            toQString(deal.sellingClubName));
        insertAiDeal.bindValue(
            QStringLiteral(":buying_id"),
            toQString(deal.buyingClubId));
        insertAiDeal.bindValue(
            QStringLiteral(":buying_name"),
            toQString(deal.buyingClubName));
        insertAiDeal.bindValue(
            QStringLiteral(":proposed"),
            toQString(deal.proposedDate.toIsoString()));
        insertAiDeal.bindValue(
            QStringLiteral(":deadline"),
            toQString(deal.decisionDeadline.toIsoString()));
        insertAiDeal.bindValue(
            QStringLiteral(":next_action"),
            optionalDateValue(deal.nextActionDate));
        insertAiDeal.bindValue(
            QStringLiteral(":completion"),
            optionalDateValue(deal.completionDate));
        insertAiDeal.bindValue(
            QStringLiteral(":completed"),
            optionalDateValue(deal.completedDate));
        insertAiDeal.bindValue(
            QStringLiteral(":upfront"),
            deal.feeTerms.upfrontFeePence);
        insertAiDeal.bindValue(
            QStringLiteral(":installment_total"),
            deal.feeTerms.installmentTotalPence);
        insertAiDeal.bindValue(
            QStringLiteral(":installment_count"),
            deal.feeTerms.installmentCount);
        insertAiDeal.bindValue(
            QStringLiteral(":appearance_bonus"),
            deal.feeTerms.appearanceBonusPence);
        insertAiDeal.bindValue(
            QStringLiteral(":appearance_threshold"),
            deal.feeTerms.appearanceThreshold);
        insertAiDeal.bindValue(
            QStringLiteral(":sell_on"),
            deal.feeTerms.sellOnPercentage);
        bindContractTerms(
            insertAiDeal,
            QStringLiteral("contract"),
            deal.contractTerms);
        bindLoanTerms(
            insertAiDeal,
            QStringLiteral("loan"),
            deal.loanTerms);
        insertAiDeal.bindValue(
            QStringLiteral(":source_loan"),
            toQString(deal.sourceLoanId));
        insertAiDeal.bindValue(
            QStringLiteral(":source_contract"),
            toQString(deal.sourceContractId));
        insertAiDeal.bindValue(
            QStringLiteral(":rationale"),
            toQString(deal.rationale));
        insertAiDeal.bindValue(
            QStringLiteral(":player_club"),
            deal.playerClubInvolved ? 1 : 0);
        insertAiDeal.bindValue(
            QStringLiteral(":reserved"),
            deal.budgetReserved ? 1 : 0);
        insertAiDeal.bindValue(
            QStringLiteral(":applied"),
            deal.completionApplied ? 1 : 0);
        insertAiDeal.bindValue(
            QStringLiteral(":confidence"),
            toQString(deal.dataConfidence));
        if (!insertAiDeal.exec()) {
            return rollback(queryError(
                QStringLiteral("AI 이적시장 거래 저장 실패"),
                insertAiDeal));
        }
    }

    QSqlQuery deletePayments(database_);
    deletePayments.prepare(QStringLiteral("DELETE FROM save_transfer_payments WHERE save_id = :save"));
    deletePayments.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deletePayments.exec()) {
        return rollback(queryError(QStringLiteral("기존 이적료 지급 일정 정리 실패"), deletePayments));
    }
    for (const auto& payment : state.transferPayments) {
        if (payment.amountPence <= 0) {
            continue;
        }
        QSqlQuery insertPayment(database_);
        insertPayment.prepare(QStringLiteral(
            "INSERT INTO save_transfer_payments "
            "(save_id, payment_id, source_id, counterparty_club_id, "
            "counterparty_name, direction, category, due_date, amount_pence, "
            "status, settled_date, description) "
            "VALUES (:save, :payment, :source, :counterparty_id, "
            ":counterparty_name, :direction, :category, :due, :amount, "
            ":status, :settled, :description)"));
        insertPayment.bindValue(QStringLiteral(":save"), state.saveId);
        insertPayment.bindValue(QStringLiteral(":payment"), toQString(payment.id));
        insertPayment.bindValue(QStringLiteral(":source"), toQString(payment.sourceId));
        insertPayment.bindValue(QStringLiteral(":counterparty_id"), toQString(payment.counterpartyClubId));
        insertPayment.bindValue(QStringLiteral(":counterparty_name"), toQString(payment.counterpartyName));
        insertPayment.bindValue(QStringLiteral(":direction"), toQString(transferPaymentDirectionKey(payment.direction)));
        insertPayment.bindValue(QStringLiteral(":category"), toQString(payment.category));
        insertPayment.bindValue(QStringLiteral(":due"), toQString(payment.dueDate.toIsoString()));
        insertPayment.bindValue(QStringLiteral(":amount"), payment.amountPence);
        insertPayment.bindValue(QStringLiteral(":status"), toQString(transferPaymentStatusKey(payment.status)));
        insertPayment.bindValue(QStringLiteral(":settled"), optionalDateValue(payment.settledDate));
        insertPayment.bindValue(QStringLiteral(":description"), toQString(payment.description));
        if (!insertPayment.exec()) {
            return rollback(queryError(QStringLiteral("이적료 지급 일정 저장 실패"), insertPayment));
        }
    }

    QSqlQuery deleteScoutingReports(database_);
    deleteScoutingReports.prepare(QStringLiteral(
        "DELETE FROM save_scouting_reports WHERE save_id = :save"));
    deleteScoutingReports.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteScoutingReports.exec()) {
        return rollback(queryError(QStringLiteral("기존 스카우팅 보고서 정리 실패"), deleteScoutingReports));
    }
    for (const auto& report : state.scoutingReports) {
        QSqlQuery insertReport(database_);
        insertReport.prepare(QStringLiteral(
            "INSERT INTO save_scouting_reports "
            "(save_id, person_id, knowledge_level, current_ability_low, "
            "current_ability_high, potential_low, potential_high, "
            "market_value_low_pence, market_value_high_pence, "
            "asking_price_low_pence, asking_price_high_pence, "
            "expected_wage_low_pence, expected_wage_high_pence, role_fit, "
            "summary, availability, last_observed_date, valid_until, "
            "scout_id, focus, report_version, stale_notified) "
            "VALUES (:save, :person, :knowledge, :current_low, :current_high, "
            ":potential_low, :potential_high, :market_low, :market_high, "
            ":asking_low, :asking_high, :wage_low, :wage_high, :role_fit, "
            ":summary, :availability, :observed, :valid_until, :scout, "
            ":focus, :version, :stale_notified)"));
        insertReport.bindValue(QStringLiteral(":save"), state.saveId);
        insertReport.bindValue(QStringLiteral(":person"), toQString(report.playerId));
        insertReport.bindValue(QStringLiteral(":knowledge"), report.knowledgeLevel);
        insertReport.bindValue(QStringLiteral(":current_low"), report.currentAbilityLow);
        insertReport.bindValue(QStringLiteral(":current_high"), report.currentAbilityHigh);
        insertReport.bindValue(QStringLiteral(":potential_low"), report.potentialLow);
        insertReport.bindValue(QStringLiteral(":potential_high"), report.potentialHigh);
        insertReport.bindValue(QStringLiteral(":market_low"), report.marketValueLowPence);
        insertReport.bindValue(QStringLiteral(":market_high"), report.marketValueHighPence);
        insertReport.bindValue(QStringLiteral(":asking_low"), report.askingPriceLowPence);
        insertReport.bindValue(QStringLiteral(":asking_high"), report.askingPriceHighPence);
        insertReport.bindValue(QStringLiteral(":wage_low"), report.expectedWeeklyWageLowPence);
        insertReport.bindValue(QStringLiteral(":wage_high"), report.expectedWeeklyWageHighPence);
        insertReport.bindValue(QStringLiteral(":role_fit"), toQString(report.roleFit));
        insertReport.bindValue(QStringLiteral(":summary"), toQString(report.summary));
        insertReport.bindValue(QStringLiteral(":availability"), toQString(report.availability));
        insertReport.bindValue(QStringLiteral(":observed"), toQString(report.lastObservedDate.toIsoString()));
        insertReport.bindValue(QStringLiteral(":valid_until"), toQString(report.validUntil.toIsoString()));
        insertReport.bindValue(QStringLiteral(":scout"), toQString(report.scoutId));
        insertReport.bindValue(QStringLiteral(":focus"), toQString(report.focus));
        insertReport.bindValue(QStringLiteral(":version"), report.reportVersion);
        insertReport.bindValue(QStringLiteral(":stale_notified"), report.staleNotified ? 1 : 0);
        if (!insertReport.exec()) {
            return rollback(queryError(QStringLiteral("스카우팅 보고서 저장 실패"), insertReport));
        }
    }

    QSqlQuery deleteScoutingAssignments(database_);
    deleteScoutingAssignments.prepare(QStringLiteral(
        "DELETE FROM save_scouting_assignments WHERE save_id = :save"));
    deleteScoutingAssignments.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteScoutingAssignments.exec()) {
        return rollback(queryError(QStringLiteral("기존 스카우팅 배정 정리 실패"), deleteScoutingAssignments));
    }
    for (const auto& assignment : state.scoutingAssignments) {
        QSqlQuery insertAssignment(database_);
        insertAssignment.prepare(QStringLiteral(
            "INSERT INTO save_scouting_assignments "
            "(save_id, assignment_id, person_id, scout_id, depth, focus, "
            "started_date, next_update_date, due_date, status, "
            "starting_knowledge, target_knowledge, last_milestone, completed_date) "
            "VALUES (:save, :assignment, :person, :scout, :depth, :focus, "
            ":started, :next_update, :due, :status, :starting_knowledge, "
            ":target_knowledge, :milestone, :completed)"));
        insertAssignment.bindValue(QStringLiteral(":save"), state.saveId);
        insertAssignment.bindValue(QStringLiteral(":assignment"), toQString(assignment.id));
        insertAssignment.bindValue(QStringLiteral(":person"), toQString(assignment.playerId));
        insertAssignment.bindValue(QStringLiteral(":scout"), toQString(assignment.scoutId));
        insertAssignment.bindValue(QStringLiteral(":depth"), toQString(scoutingDepthKey(assignment.depth)));
        insertAssignment.bindValue(QStringLiteral(":focus"), toQString(assignment.focus));
        insertAssignment.bindValue(QStringLiteral(":started"), toQString(assignment.startedDate.toIsoString()));
        insertAssignment.bindValue(QStringLiteral(":next_update"), toQString(assignment.nextUpdateDate.toIsoString()));
        insertAssignment.bindValue(QStringLiteral(":due"), toQString(assignment.dueDate.toIsoString()));
        insertAssignment.bindValue(QStringLiteral(":status"), toQString(scoutingAssignmentStatusKey(assignment.status)));
        insertAssignment.bindValue(QStringLiteral(":starting_knowledge"), assignment.startingKnowledge);
        insertAssignment.bindValue(QStringLiteral(":target_knowledge"), assignment.targetKnowledge);
        insertAssignment.bindValue(QStringLiteral(":milestone"), assignment.lastMilestone);
        insertAssignment.bindValue(QStringLiteral(":completed"), optionalDateValue(assignment.completedDate));
        if (!insertAssignment.exec()) {
            return rollback(queryError(QStringLiteral("스카우팅 배정 저장 실패"), insertAssignment));
        }
    }

    QSqlQuery deleteScoutFamiliarities(database_);
    deleteScoutFamiliarities.prepare(QStringLiteral(
        "DELETE FROM save_scout_familiarities WHERE save_id = :save"));
    deleteScoutFamiliarities.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteScoutFamiliarities.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 스카우터 친숙도 정리 실패"),
            deleteScoutFamiliarities));
    }
    for (const auto& familiarity : state.scoutFamiliarities) {
        QSqlQuery insertFamiliarity(database_);
        insertFamiliarity.prepare(QStringLiteral(
            "INSERT INTO save_scout_familiarities "
            "(save_id, scout_id, category, familiarity_key, level, "
            "updated_date, data_confidence) "
            "VALUES (:save, :scout, :category, :key, :level, "
            ":updated, :confidence)"));
        insertFamiliarity.bindValue(QStringLiteral(":save"), state.saveId);
        insertFamiliarity.bindValue(
            QStringLiteral(":scout"),
            toQString(familiarity.scoutId));
        insertFamiliarity.bindValue(
            QStringLiteral(":category"),
            toQString(familiarity.category));
        insertFamiliarity.bindValue(
            QStringLiteral(":key"),
            toQString(familiarity.key));
        insertFamiliarity.bindValue(QStringLiteral(":level"), familiarity.level);
        insertFamiliarity.bindValue(
            QStringLiteral(":updated"),
            toQString(familiarity.updatedDate.toIsoString()));
        insertFamiliarity.bindValue(
            QStringLiteral(":confidence"),
            toQString(familiarity.dataConfidence));
        if (!insertFamiliarity.exec()) {
            return rollback(queryError(
                QStringLiteral("스카우터 친숙도 저장 실패"),
                insertFamiliarity));
        }
    }

    QSqlQuery deleteScoutingDirectives(database_);
    deleteScoutingDirectives.prepare(QStringLiteral(
        "DELETE FROM save_scouting_directives WHERE save_id = :save"));
    deleteScoutingDirectives.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteScoutingDirectives.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 스카우팅 지시 정리 실패"),
            deleteScoutingDirectives));
    }
    for (const auto& directive : state.scoutingDirectives) {
        QSqlQuery insertDirective(database_);
        insertDirective.prepare(QStringLiteral(
            "INSERT INTO save_scouting_directives "
            "(save_id, directive_id, directive_type, status, scout_id, "
            "depth, focus, target_person_id, generated_assignment_id, "
            "region, country, league, position, style, min_age, max_age, "
            "homegrown_only, max_budget_pence, matched_candidate_count, "
            "matched_player_ids, recommendation_score, estimated_duration_days, "
            "accuracy_modifier, created_date, due_date, completed_date, "
            "summary, data_confidence) "
            "VALUES (:save, :directive, :type, :status, :scout, "
            ":depth, :focus, :target, :assignment, :region, :country, "
            ":league, :position, :style, :min_age, :max_age, :homegrown, "
            ":budget, :matched_count, :matched_ids, :score, :duration, "
            ":accuracy, :created, :due, :completed, :summary, :confidence)"));
        insertDirective.bindValue(QStringLiteral(":save"), state.saveId);
        insertDirective.bindValue(
            QStringLiteral(":directive"),
            toQString(directive.id));
        insertDirective.bindValue(
            QStringLiteral(":type"),
            toQString(scoutingDirectiveTypeKey(directive.type)));
        insertDirective.bindValue(
            QStringLiteral(":status"),
            toQString(scoutingDirectiveStatusKey(directive.status)));
        insertDirective.bindValue(
            QStringLiteral(":scout"),
            toQString(directive.scoutId));
        insertDirective.bindValue(
            QStringLiteral(":depth"),
            toQString(scoutingDepthKey(directive.depth)));
        insertDirective.bindValue(
            QStringLiteral(":focus"),
            toQString(directive.focus));
        insertDirective.bindValue(
            QStringLiteral(":target"),
            directive.targetPlayerId.empty()
                ? QVariant {}
                : QVariant {toQString(directive.targetPlayerId)});
        insertDirective.bindValue(
            QStringLiteral(":assignment"),
            toQString(directive.generatedAssignmentId));
        insertDirective.bindValue(
            QStringLiteral(":region"),
            toQString(directive.region));
        insertDirective.bindValue(
            QStringLiteral(":country"),
            toQString(directive.country));
        insertDirective.bindValue(
            QStringLiteral(":league"),
            toQString(directive.league));
        insertDirective.bindValue(
            QStringLiteral(":position"),
            toQString(directive.position));
        insertDirective.bindValue(
            QStringLiteral(":style"),
            toQString(directive.style));
        insertDirective.bindValue(QStringLiteral(":min_age"), directive.minAge);
        insertDirective.bindValue(QStringLiteral(":max_age"), directive.maxAge);
        insertDirective.bindValue(
            QStringLiteral(":homegrown"),
            directive.homegrownOnly ? 1 : 0);
        insertDirective.bindValue(
            QStringLiteral(":budget"),
            directive.maxBudgetPence);
        insertDirective.bindValue(
            QStringLiteral(":matched_count"),
            directive.matchedCandidateCount);
        insertDirective.bindValue(
            QStringLiteral(":matched_ids"),
            toQString(directive.matchedPlayerIds));
        insertDirective.bindValue(
            QStringLiteral(":score"),
            directive.recommendationScore);
        insertDirective.bindValue(
            QStringLiteral(":duration"),
            directive.estimatedDurationDays);
        insertDirective.bindValue(
            QStringLiteral(":accuracy"),
            directive.accuracyModifier);
        insertDirective.bindValue(
            QStringLiteral(":created"),
            toQString(directive.createdDate.toIsoString()));
        insertDirective.bindValue(
            QStringLiteral(":due"),
            toQString(directive.dueDate.toIsoString()));
        insertDirective.bindValue(
            QStringLiteral(":completed"),
            optionalDateValue(directive.completedDate));
        insertDirective.bindValue(
            QStringLiteral(":summary"),
            toQString(directive.summary));
        insertDirective.bindValue(
            QStringLiteral(":confidence"),
            toQString(directive.dataConfidence));
        if (!insertDirective.exec()) {
            return rollback(queryError(
                QStringLiteral("스카우팅 지시 저장 실패"),
                insertDirective));
        }
    }

    QSqlQuery deleteRecruitmentPlans(database_);
    deleteRecruitmentPlans.prepare(QStringLiteral(
        "DELETE FROM save_recruitment_plans WHERE save_id = :save"));
    deleteRecruitmentPlans.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteRecruitmentPlans.exec()) {
        return rollback(queryError(QStringLiteral("기존 영입 계획 정리 실패"), deleteRecruitmentPlans));
    }
    for (const auto& plan : state.recruitmentPlans) {
        QSqlQuery insertPlan(database_);
        insertPlan.prepare(QStringLiteral(
            "INSERT INTO save_recruitment_plans "
            "(save_id, plan_id, title, requested_by, description, start_date, "
            "evaluation_date, target_signings, minimum_acceptable_signings, "
            "status, confidence_delta, evaluated_date) "
            "VALUES (:save, :plan, :title, :requested_by, :description, :start, "
            ":evaluation, :target, :minimum, :status, :delta, :evaluated)"));
        insertPlan.bindValue(QStringLiteral(":save"), state.saveId);
        insertPlan.bindValue(QStringLiteral(":plan"), toQString(plan.id));
        insertPlan.bindValue(QStringLiteral(":title"), toQString(plan.title));
        insertPlan.bindValue(QStringLiteral(":requested_by"), toQString(plan.requestedBy));
        insertPlan.bindValue(QStringLiteral(":description"), toQString(plan.description));
        insertPlan.bindValue(QStringLiteral(":start"), toQString(plan.startDate.toIsoString()));
        insertPlan.bindValue(QStringLiteral(":evaluation"), toQString(plan.evaluationDate.toIsoString()));
        insertPlan.bindValue(QStringLiteral(":target"), plan.targetSignings);
        insertPlan.bindValue(QStringLiteral(":minimum"), plan.minimumAcceptableSignings);
        insertPlan.bindValue(QStringLiteral(":status"), toQString(recruitmentPlanStatusKey(plan.status)));
        insertPlan.bindValue(QStringLiteral(":delta"), plan.confidenceDelta);
        insertPlan.bindValue(QStringLiteral(":evaluated"), optionalDateValue(plan.evaluatedDate));
        if (!insertPlan.exec()) {
            return rollback(queryError(QStringLiteral("영입 계획 저장 실패"), insertPlan));
        }
    }

    QSqlQuery deleteSquadPlanningOpinions(database_);
    deleteSquadPlanningOpinions.prepare(QStringLiteral(
        "DELETE FROM save_squad_planning_opinions WHERE save_id = :save"));
    deleteSquadPlanningOpinions.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteSquadPlanningOpinions.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 선수단 계획 의견 정리 실패"),
            deleteSquadPlanningOpinions));
    }
    for (const auto& opinion : state.squadPlanningOpinions) {
        if (!opinion.playerId.has_value()
            && (!opinion.positionGroup.has_value()
                || opinion.positionGroup->empty())) {
            continue;
        }

        QSqlQuery insertOpinion(database_);
        insertOpinion.prepare(QStringLiteral(
            "INSERT INTO save_squad_planning_opinions "
            "(save_id, opinion_id, season_id, club_id, player_person_id, "
            "position_group, source_role, opinion_type, priority, reason, "
            "target_window, status, created_date, updated_date, "
            "data_confidence) "
            "VALUES (:save, :opinion, :season, :club, :player, "
            ":position_group, :source, :type, :priority, :reason, "
            ":target_window, :status, :created, :updated, :confidence)"));
        insertOpinion.bindValue(QStringLiteral(":save"), state.saveId);
        insertOpinion.bindValue(QStringLiteral(":opinion"), toQString(opinion.id));
        insertOpinion.bindValue(QStringLiteral(":season"), toQString(opinion.seasonId));
        insertOpinion.bindValue(QStringLiteral(":club"), toQString(opinion.clubId));
        if (opinion.playerId.has_value()) {
            insertOpinion.bindValue(
                QStringLiteral(":player"),
                toQString(*opinion.playerId));
        } else {
            insertOpinion.bindValue(QStringLiteral(":player"), QVariant {});
        }
        if (opinion.positionGroup.has_value()
            && !opinion.positionGroup->empty()) {
            insertOpinion.bindValue(
                QStringLiteral(":position_group"),
                toQString(*opinion.positionGroup));
        } else {
            insertOpinion.bindValue(
                QStringLiteral(":position_group"),
                QVariant {});
        }
        insertOpinion.bindValue(
            QStringLiteral(":source"),
            toQString(squadPlanningOpinionSourceKey(opinion.source)));
        insertOpinion.bindValue(
            QStringLiteral(":type"),
            toQString(squadPlanningOpinionTypeKey(opinion.type)));
        insertOpinion.bindValue(
            QStringLiteral(":priority"),
            std::clamp(opinion.priority, 1, 5));
        insertOpinion.bindValue(
            QStringLiteral(":reason"),
            toQString(opinion.reason));
        insertOpinion.bindValue(
            QStringLiteral(":target_window"),
            toQString(opinion.targetWindow));
        insertOpinion.bindValue(
            QStringLiteral(":status"),
            toQString(squadPlanningOpinionStatusKey(opinion.status)));
        insertOpinion.bindValue(
            QStringLiteral(":created"),
            toQString(opinion.createdDate.toIsoString()));
        insertOpinion.bindValue(
            QStringLiteral(":updated"),
            toQString(opinion.updatedDate.toIsoString()));
        insertOpinion.bindValue(
            QStringLiteral(":confidence"),
            toQString(opinion.dataConfidence));
        if (!insertOpinion.exec()) {
            return rollback(queryError(
                QStringLiteral("선수단 계획 의견 저장 실패"),
                insertOpinion));
        }
    }

    QSqlQuery deleteMeetingSettings(database_);
    deleteMeetingSettings.prepare(QStringLiteral(
        "DELETE FROM save_operating_meeting_settings WHERE save_id = :save"));
    deleteMeetingSettings.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteMeetingSettings.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 운영 회의 설정 정리 실패"),
            deleteMeetingSettings));
    }
    QSqlQuery insertMeetingSettings(database_);
    insertMeetingSettings.prepare(QStringLiteral(
        "INSERT INTO save_operating_meeting_settings "
        "(save_id, cadence, attendee_mode, include_manager, "
        "include_director, include_finance, include_scouting, "
        "include_medical, next_meeting_date, updated_date, data_confidence) "
        "VALUES (:save, :cadence, :mode, :manager, :director, "
        ":finance, :scouting, :medical, :next, :updated, :confidence)"));
    insertMeetingSettings.bindValue(QStringLiteral(":save"), state.saveId);
    insertMeetingSettings.bindValue(
        QStringLiteral(":cadence"),
        toQString(operatingMeetingCadenceKey(
            state.operatingMeetingSettings.cadence)));
    insertMeetingSettings.bindValue(
        QStringLiteral(":mode"),
        toQString(operatingMeetingAttendeeModeKey(
            state.operatingMeetingSettings.attendeeMode)));
    insertMeetingSettings.bindValue(
        QStringLiteral(":manager"),
        state.operatingMeetingSettings.includeManager ? 1 : 0);
    insertMeetingSettings.bindValue(
        QStringLiteral(":director"),
        state.operatingMeetingSettings.includeDirector ? 1 : 0);
    insertMeetingSettings.bindValue(
        QStringLiteral(":finance"),
        state.operatingMeetingSettings.includeFinance ? 1 : 0);
    insertMeetingSettings.bindValue(
        QStringLiteral(":scouting"),
        state.operatingMeetingSettings.includeScouting ? 1 : 0);
    insertMeetingSettings.bindValue(
        QStringLiteral(":medical"),
        state.operatingMeetingSettings.includeMedical ? 1 : 0);
    insertMeetingSettings.bindValue(
        QStringLiteral(":next"),
        optionalDateValue(state.operatingMeetingSettings.nextMeetingDate));
    insertMeetingSettings.bindValue(
        QStringLiteral(":updated"),
        toQString(state.operatingMeetingSettings.updatedDate.toIsoString()));
    insertMeetingSettings.bindValue(
        QStringLiteral(":confidence"),
        toQString(state.operatingMeetingSettings.dataConfidence));
    if (!insertMeetingSettings.exec()) {
        return rollback(queryError(
            QStringLiteral("운영 회의 설정 저장 실패"),
            insertMeetingSettings));
    }

    QSqlQuery deleteMeetings(database_);
    deleteMeetings.prepare(QStringLiteral(
        "DELETE FROM save_operating_meetings WHERE save_id = :save"));
    deleteMeetings.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteMeetings.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 운영 회의 정리 실패"),
            deleteMeetings));
    }
    for (const auto& meeting : state.operatingMeetings) {
        QSqlQuery insertMeeting(database_);
        insertMeeting.prepare(QStringLiteral(
            "INSERT INTO save_operating_meetings "
            "(save_id, meeting_id, meeting_date, agenda_type, status, "
            "attendee_mode, summary, outcome_inbox_event_key, "
            "created_date, data_confidence) "
            "VALUES (:save, :meeting, :date, :agenda, :status, :mode, "
            ":summary, :event_key, :created, :confidence)"));
        insertMeeting.bindValue(QStringLiteral(":save"), state.saveId);
        insertMeeting.bindValue(QStringLiteral(":meeting"), toQString(meeting.id));
        insertMeeting.bindValue(
            QStringLiteral(":date"),
            toQString(meeting.meetingDate.toIsoString()));
        insertMeeting.bindValue(
            QStringLiteral(":agenda"),
            toQString(operatingMeetingAgendaTypeKey(meeting.agenda)));
        insertMeeting.bindValue(
            QStringLiteral(":status"),
            toQString(operatingMeetingStatusKey(meeting.status)));
        insertMeeting.bindValue(
            QStringLiteral(":mode"),
            toQString(operatingMeetingAttendeeModeKey(meeting.attendeeMode)));
        insertMeeting.bindValue(
            QStringLiteral(":summary"),
            toQString(meeting.summary));
        insertMeeting.bindValue(
            QStringLiteral(":event_key"),
            toQString(meeting.outcomeInboxEventKey));
        insertMeeting.bindValue(
            QStringLiteral(":created"),
            toQString(meeting.createdDate.toIsoString()));
        insertMeeting.bindValue(
            QStringLiteral(":confidence"),
            toQString(meeting.dataConfidence));
        if (!insertMeeting.exec()) {
            return rollback(queryError(
                QStringLiteral("운영 회의 저장 실패"),
                insertMeeting));
        }

        for (const auto& attendee : meeting.attendees) {
            QSqlQuery insertAttendee(database_);
            insertAttendee.prepare(QStringLiteral(
                "INSERT INTO save_operating_meeting_attendees "
                "(save_id, meeting_id, attendee_role, display_name, "
                "auto_assigned, notes) "
                "VALUES (:save, :meeting, :role, :name, :auto, :notes)"));
            insertAttendee.bindValue(QStringLiteral(":save"), state.saveId);
            insertAttendee.bindValue(
                QStringLiteral(":meeting"),
                toQString(meeting.id));
            insertAttendee.bindValue(
                QStringLiteral(":role"),
                toQString(attendee.role));
            insertAttendee.bindValue(
                QStringLiteral(":name"),
                toQString(attendee.displayName));
            insertAttendee.bindValue(
                QStringLiteral(":auto"),
                attendee.autoAssigned ? 1 : 0);
            insertAttendee.bindValue(
                QStringLiteral(":notes"),
                toQString(attendee.notes));
            if (!insertAttendee.exec()) {
                return rollback(queryError(
                    QStringLiteral("운영 회의 참석자 저장 실패"),
                    insertAttendee));
            }
        }
    }

    QSqlQuery deleteOwnerReputation(database_);
    deleteOwnerReputation.prepare(QStringLiteral(
        "DELETE FROM save_owner_reputation_state WHERE save_id = :save"));
    deleteOwnerReputation.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteOwnerReputation.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 구단주 평판 상태 정리 실패"),
            deleteOwnerReputation));
    }
    QSqlQuery insertOwnerReputation(database_);
    insertOwnerReputation.prepare(QStringLiteral(
        "INSERT INTO save_owner_reputation_state "
        "(save_id, overall_score, sporting_ambition, financial_prudence, "
        "squad_loyalty, autonomy_respect, headline, updated_date, "
        "data_confidence) "
        "VALUES (:save, :overall, :ambition, :prudence, :loyalty, "
        ":autonomy, :headline, :updated, :confidence)"));
    insertOwnerReputation.bindValue(QStringLiteral(":save"), state.saveId);
    insertOwnerReputation.bindValue(
        QStringLiteral(":overall"),
        std::clamp(state.ownerReputation.overallScore, 0, 100));
    insertOwnerReputation.bindValue(
        QStringLiteral(":ambition"),
        std::clamp(state.ownerReputation.sportingAmbition, 0, 100));
    insertOwnerReputation.bindValue(
        QStringLiteral(":prudence"),
        std::clamp(state.ownerReputation.financialPrudence, 0, 100));
    insertOwnerReputation.bindValue(
        QStringLiteral(":loyalty"),
        std::clamp(state.ownerReputation.squadLoyalty, 0, 100));
    insertOwnerReputation.bindValue(
        QStringLiteral(":autonomy"),
        std::clamp(state.ownerReputation.autonomyRespect, 0, 100));
    insertOwnerReputation.bindValue(
        QStringLiteral(":headline"),
        toQString(ownerReputationHeadline(state.ownerReputation.overallScore)));
    insertOwnerReputation.bindValue(
        QStringLiteral(":updated"),
        toQString(state.ownerReputation.updatedDate.toIsoString()));
    insertOwnerReputation.bindValue(
        QStringLiteral(":confidence"),
        toQString(state.ownerReputation.dataConfidence));
    if (!insertOwnerReputation.exec()) {
        return rollback(queryError(
            QStringLiteral("구단주 평판 상태 저장 실패"),
            insertOwnerReputation));
    }

    QSqlQuery deleteFanTendency(database_);
    deleteFanTendency.prepare(QStringLiteral(
        "DELETE FROM save_fan_tendency_state WHERE save_id = :save"));
    deleteFanTendency.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteFanTendency.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 팬 성향 상태 정리 실패"),
            deleteFanTendency));
    }
    QSqlQuery insertFanTendency(database_);
    insertFanTendency.prepare(QStringLiteral(
        "INSERT INTO save_fan_tendency_state "
        "(save_id, trust_score, patience_score, skepticism_score, "
        "star_signing_preference, homegrown_preference, "
        "youth_development_preference, balanced_squad_preference, "
        "mood, updated_date, data_confidence) "
        "VALUES (:save, :trust, :patience, :skepticism, :star, "
        ":homegrown, :youth, :balanced, :mood, :updated, :confidence)"));
    insertFanTendency.bindValue(QStringLiteral(":save"), state.saveId);
    insertFanTendency.bindValue(
        QStringLiteral(":trust"),
        std::clamp(state.fanTendency.trustScore, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":patience"),
        std::clamp(state.fanTendency.patienceScore, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":skepticism"),
        std::clamp(state.fanTendency.skepticismScore, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":star"),
        std::clamp(state.fanTendency.starSigningPreference, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":homegrown"),
        std::clamp(state.fanTendency.homegrownPreference, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":youth"),
        std::clamp(state.fanTendency.youthDevelopmentPreference, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":balanced"),
        std::clamp(state.fanTendency.balancedSquadPreference, 0, 100));
    insertFanTendency.bindValue(
        QStringLiteral(":mood"),
        toQString(fanMoodKey(state.fanTendency)));
    insertFanTendency.bindValue(
        QStringLiteral(":updated"),
        toQString(state.fanTendency.updatedDate.toIsoString()));
    insertFanTendency.bindValue(
        QStringLiteral(":confidence"),
        toQString(state.fanTendency.dataConfidence));
    if (!insertFanTendency.exec()) {
        return rollback(queryError(
            QStringLiteral("팬 성향 상태 저장 실패"),
            insertFanTendency));
    }

    QSqlQuery deleteOwnerActionLog(database_);
    deleteOwnerActionLog.prepare(QStringLiteral(
        "DELETE FROM save_owner_action_log WHERE save_id = :save"));
    deleteOwnerActionLog.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteOwnerActionLog.exec()) {
        return rollback(queryError(
            QStringLiteral("기존 구단주 행동 로그 정리 실패"),
            deleteOwnerActionLog));
    }
    for (const auto& action : state.ownerActionLog) {
        QSqlQuery insertOwnerAction(database_);
        insertOwnerAction.prepare(QStringLiteral(
            "INSERT INTO save_owner_action_log "
            "(save_id, action_id, action_date, action_type, category, "
            "subject_id, subject_name, summary, reputation_delta, "
            "fan_trust_delta, ambition_delta, prudence_delta, "
            "loyalty_delta, autonomy_delta, star_preference_delta, "
            "homegrown_preference_delta, youth_preference_delta, "
            "balanced_preference_delta, skepticism_delta, data_confidence) "
            "VALUES (:save, :action, :date, :type, :category, "
            ":subject_id, :subject_name, :summary, :reputation, "
            ":fan_trust, :ambition, :prudence, :loyalty, :autonomy, "
            ":star, :homegrown, :youth, :balanced, :skepticism, "
            ":confidence)"));
        insertOwnerAction.bindValue(QStringLiteral(":save"), state.saveId);
        insertOwnerAction.bindValue(
            QStringLiteral(":action"),
            toQString(action.id));
        insertOwnerAction.bindValue(
            QStringLiteral(":date"),
            toQString(action.actionDate.toIsoString()));
        insertOwnerAction.bindValue(
            QStringLiteral(":type"),
            toQString(action.actionType));
        insertOwnerAction.bindValue(
            QStringLiteral(":category"),
            toQString(action.category));
        insertOwnerAction.bindValue(
            QStringLiteral(":subject_id"),
            toQString(action.subjectId));
        insertOwnerAction.bindValue(
            QStringLiteral(":subject_name"),
            toQString(action.subjectName));
        insertOwnerAction.bindValue(
            QStringLiteral(":summary"),
            toQString(action.summary));
        insertOwnerAction.bindValue(
            QStringLiteral(":reputation"),
            std::clamp(action.reputationDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":fan_trust"),
            std::clamp(action.fanTrustDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":ambition"),
            std::clamp(action.ambitionDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":prudence"),
            std::clamp(action.prudenceDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":loyalty"),
            std::clamp(action.loyaltyDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":autonomy"),
            std::clamp(action.autonomyDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":star"),
            std::clamp(action.starPreferenceDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":homegrown"),
            std::clamp(action.homegrownPreferenceDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":youth"),
            std::clamp(action.youthPreferenceDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":balanced"),
            std::clamp(action.balancedPreferenceDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":skepticism"),
            std::clamp(action.skepticismDelta, -100, 100));
        insertOwnerAction.bindValue(
            QStringLiteral(":confidence"),
            toQString(action.dataConfidence));
        if (!insertOwnerAction.exec()) {
            return rollback(queryError(
                QStringLiteral("구단주 행동 로그 저장 실패"),
                insertOwnerAction));
        }
    }

    QSqlQuery deleteRegistrations(database_);
    deleteRegistrations.prepare(QStringLiteral("DELETE FROM save_player_registrations WHERE save_id = :save"));
    deleteRegistrations.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteRegistrations.exec()) {
        return rollback(queryError(QStringLiteral("기존 영입 선수 등록 정리 실패"), deleteRegistrations));
    }
    for (const auto& proposal : state.transferProposals) {
        if (proposal.status != TransferStatus::Completed || !proposal.joinDate.has_value()) {
            continue;
        }

        const auto player = std::find_if(
            state.players.begin(),
            state.players.end(),
            [&proposal](const Player& candidate) {
                return candidate.id == proposal.target.id;
            });
        if (player == state.players.end()) {
            continue;
        }

        QSqlQuery insertRegistration(database_);
        insertRegistration.prepare(QStringLiteral(
            "INSERT INTO save_player_registrations "
            "(save_id, person_id, club_id, squad_group, registration_status, "
            "joined_date, data_confidence, source_proposal_id) "
            "VALUES (:save, :person, :club, :group, :status, :joined, :confidence, :proposal)"));
        insertRegistration.bindValue(QStringLiteral(":save"), state.saveId);
        insertRegistration.bindValue(QStringLiteral(":person"), toQString(player->id));
        insertRegistration.bindValue(QStringLiteral(":club"), toQString(state.club.id));
        insertRegistration.bindValue(QStringLiteral(":group"), toQString(player->squadGroup));
        insertRegistration.bindValue(QStringLiteral(":status"), toQString(player->status));
        insertRegistration.bindValue(QStringLiteral(":joined"), toQString(proposal.joinDate->toIsoString()));
        insertRegistration.bindValue(QStringLiteral(":confidence"), toQString(player->dataConfidence));
        insertRegistration.bindValue(QStringLiteral(":proposal"), toQString(proposal.id));
        if (!insertRegistration.exec()) {
            return rollback(queryError(QStringLiteral("영입 선수 등록 저장 실패"), insertRegistration));
        }
    }

    QSqlQuery deleteDepartures(database_);
    deleteDepartures.prepare(QStringLiteral("DELETE FROM save_player_departures WHERE save_id = :save"));
    deleteDepartures.bindValue(QStringLiteral(":save"), state.saveId);
    if (!deleteDepartures.exec()) {
        return rollback(queryError(QStringLiteral("기존 판매 선수 이탈 기록 정리 실패"), deleteDepartures));
    }
    for (const auto& offer : state.outgoingTransferOffers) {
        if (offer.status != OutgoingTransferStatus::Completed
            || !offer.completionDate.has_value()) {
            continue;
        }
        QSqlQuery insertDeparture(database_);
        insertDeparture.prepare(QStringLiteral(
            "INSERT INTO save_player_departures "
            "(save_id, person_id, from_club_id, to_club_id, departure_date, source_offer_id) "
            "VALUES (:save, :person, :from_club, :to_club, :date, :offer)"));
        insertDeparture.bindValue(QStringLiteral(":save"), state.saveId);
        insertDeparture.bindValue(QStringLiteral(":person"), toQString(offer.playerId));
        insertDeparture.bindValue(QStringLiteral(":from_club"), toQString(state.club.id));
        insertDeparture.bindValue(QStringLiteral(":to_club"), toQString(offer.buyerClubId));
        insertDeparture.bindValue(QStringLiteral(":date"), toQString(offer.completionDate->toIsoString()));
        insertDeparture.bindValue(QStringLiteral(":offer"), toQString(offer.id));
        if (!insertDeparture.exec()) {
            return rollback(queryError(QStringLiteral("판매 선수 이탈 기록 저장 실패"), insertDeparture));
        }
    }

    if (!database_.commit()) {
        (void)database_.rollback();
        return fail(errorMessage, QStringLiteral("저장 커밋 실패: ") + database_.lastError().text());
    }
    return true;
}

bool SqliteGameRepository::writeFileMetadata(
    const int saveId,
    const SaveFileMetadata& metadata,
    QString* errorMessage)
{
    if (saveId <= 0) {
        return fail(errorMessage, QStringLiteral("유효한 saveId 없이 저장 파일 메타데이터를 기록할 수 없습니다."));
    }
    if (!initialized_ && !initialize(errorMessage)) {
        return false;
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO save_file_metadata "
        "(save_id, career_id, save_kind, parent_career_id, autosave_slot, "
        "play_time_seconds, saved_at) "
        "VALUES (:save, :career, :kind, :parent, :slot, :play_time, :saved_at) "
        "ON CONFLICT(save_id) DO UPDATE SET "
        "career_id = excluded.career_id, save_kind = excluded.save_kind, "
        "parent_career_id = excluded.parent_career_id, "
        "autosave_slot = excluded.autosave_slot, "
        "play_time_seconds = excluded.play_time_seconds, "
        "saved_at = excluded.saved_at"));
    query.bindValue(QStringLiteral(":save"), saveId);
    query.bindValue(QStringLiteral(":career"), metadata.careerId);
    query.bindValue(QStringLiteral(":kind"), metadata.saveKind);
    if (metadata.parentCareerId.isEmpty()) {
        query.bindValue(QStringLiteral(":parent"), QVariant {});
    } else {
        query.bindValue(QStringLiteral(":parent"), metadata.parentCareerId);
    }
    query.bindValue(QStringLiteral(":slot"), metadata.autosaveSlot);
    query.bindValue(QStringLiteral(":play_time"), metadata.playTimeSeconds);
    query.bindValue(QStringLiteral(":saved_at"), metadata.savedAt);
    if (!query.exec()) {
        return fail(errorMessage, queryError(QStringLiteral("저장 파일 메타데이터 기록 실패"), query));
    }
    return true;
}

std::optional<SaveFileMetadata> SqliteGameRepository::readFileMetadata(QString* errorMessage)
{
    if (!initialized_ && !initialize(errorMessage)) {
        return std::nullopt;
    }

    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral(
            "SELECT career_id, save_kind, COALESCE(parent_career_id, ''), "
            "autosave_slot, play_time_seconds, saved_at "
            "FROM save_file_metadata ORDER BY save_id LIMIT 1"))) {
        (void)fail(errorMessage, queryError(QStringLiteral("저장 파일 메타데이터 조회 실패"), query));
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }

    return SaveFileMetadata {
        query.value(0).toString(),
        query.value(1).toString(),
        query.value(2).toString(),
        query.value(3).toInt(),
        query.value(4).toLongLong(),
        query.value(5).toString()
    };
}

bool SqliteGameRepository::prepareForFileReplacement(QString* errorMessage)
{
    if (!initialized_ && !initialize(errorMessage)) {
        return false;
    }

    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"))) {
        return fail(errorMessage, queryError(QStringLiteral("SQLite WAL 정리 실패"), query));
    }
    query.finish();
    if (!query.exec(QStringLiteral("PRAGMA journal_mode = DELETE"))) {
        return fail(errorMessage, queryError(QStringLiteral("SQLite 파일 교체 모드 전환 실패"), query));
    }
    return true;
}

QString SqliteGameRepository::databasePath() const
{
    return databasePath_;
}

bool SqliteGameRepository::importLockedWorldBaseline(QString* errorMessage)
{
    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral(
        "SELECT status, people FROM world_baseline_imports "
        "WHERE dataset_id = :dataset_id AND version = :version"));
    existing.bindValue(QStringLiteral(":dataset_id"), QString::fromLatin1(kLockedWorldDatasetId));
    existing.bindValue(QStringLiteral(":version"), QString::fromLatin1(kLockedWorldVersion));
    if (!existing.exec()) {
        return fail(errorMessage, queryError(QStringLiteral("LOCKED v11 import 이력 조회 실패"), existing));
    }
    if (existing.next()
        && existing.value(0).toString() == QStringLiteral("imported")
        && existing.value(1).toLongLong() == 2076) {
        existing.finish();

        QSqlQuery sanityCheck(database_);
        if (sanityCheck.exec(QStringLiteral("SELECT COUNT(*) FROM fo92_locked_people"))
            && sanityCheck.next()
            && sanityCheck.value(0).toLongLong() == 2076) {
            return true;
        }
        sanityCheck.finish();
    }
    existing.finish();

    QFile certificate(QString::fromLatin1(kLockedWorldCertificateResourcePath));
    if (!certificate.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(
            errorMessage,
            QStringLiteral("LOCKED v11 검증 인증 리소스를 열 수 없습니다: ")
                + QString::fromLatin1(kLockedWorldCertificateResourcePath));
    }
    certificate.close();

    QFile source(QString::fromLatin1(kLockedWorldResourcePath));
    if (!source.open(QIODevice::ReadOnly)) {
        return fail(
            errorMessage,
            QStringLiteral("LOCKED v11 SQLite 리소스를 열 수 없습니다: ")
                + QString::fromLatin1(kLockedWorldResourcePath));
    }

    QTemporaryFile temporaryDatabase;
    if (!temporaryDatabase.open()) {
        return fail(
            errorMessage,
            QStringLiteral("LOCKED v11 임시 SQLite 파일을 만들 수 없습니다: ")
                + temporaryDatabase.errorString());
    }
    const QByteArray sourceBytes = source.readAll();
    source.close();
    if (temporaryDatabase.write(sourceBytes) != sourceBytes.size()) {
        return fail(
            errorMessage,
            QStringLiteral("LOCKED v11 임시 SQLite 파일 쓰기 실패: ")
                + temporaryDatabase.errorString());
    }
    if (!temporaryDatabase.flush()) {
        return fail(
            errorMessage,
            QStringLiteral("LOCKED v11 임시 SQLite 파일 flush 실패: ")
                + temporaryDatabase.errorString());
    }
    const QString temporaryDatabasePath = temporaryDatabase.fileName();
    temporaryDatabase.close();

    QSqlQuery attach(database_);
    attach.prepare(QStringLiteral("ATTACH DATABASE :path AS locked_world"));
    attach.bindValue(QStringLiteral(":path"), temporaryDatabasePath);
    if (!attach.exec()) {
        return fail(errorMessage, queryError(QStringLiteral("LOCKED v11 SQLite attach 실패"), attach));
    }
    attach.finish();

    bool attached = true;
    const auto detachLockedWorld = [&]() -> bool {
        if (!attached) {
            return true;
        }
        QSqlQuery detach(database_);
        if (!detach.exec(QStringLiteral("DETACH DATABASE locked_world"))) {
            return false;
        }
        detach.finish();
        attached = false;
        return true;
    };

    QString failure;
    const auto executeSql = [&](const QString& sql, const QString& context) -> bool {
        QSqlQuery query(database_);
        if (!query.exec(sql)) {
            failure = queryError(context, query);
            return false;
        }
        query.finish();
        return true;
    };

    const auto countRows = [&](const QString& sql, qint64* count, const QString& context) -> bool {
        QSqlQuery query(database_);
        if (!query.exec(sql)) {
            failure = queryError(context, query);
            return false;
        }
        if (!query.next()) {
            failure = context + QStringLiteral(": 결과가 없습니다");
            return false;
        }
        *count = query.value(0).toLongLong();
        query.finish();
        return true;
    };

    const auto rollbackDetachAndFail = [&](const QString& message) -> bool {
        (void)database_.rollback();
        if (!detachLockedWorld()) {
            return fail(
                errorMessage,
                message + QStringLiteral("; LOCKED v11 SQLite detach도 실패했습니다: ")
                    + database_.lastError().text());
        }
        return fail(errorMessage, message);
    };

    if (!database_.transaction()) {
        const QString message =
            QStringLiteral("LOCKED v11 import 트랜잭션 시작 실패: ") + database_.lastError().text();
        (void)detachLockedWorld();
        return fail(errorMessage, message);
    }

    for (const LockedWorldTable& table : kLockedWorldTables) {
        const QString target = lockedWorldTargetTable(table.sourceName);
        if (!executeSql(
                QStringLiteral("DROP TABLE IF EXISTS ") + target,
                QStringLiteral("LOCKED v11 기존 미러 테이블 제거 실패: ") + target)) {
            return rollbackDetachAndFail(failure);
        }
        if (!executeSql(
                QStringLiteral("CREATE TABLE ") + target + QStringLiteral(" AS SELECT * FROM locked_world.")
                    + QString::fromLatin1(table.sourceName),
                QStringLiteral("LOCKED v11 미러 테이블 복사 실패: ") + target)) {
            return rollbackDetachAndFail(failure);
        }
    }

    const std::array<QString, 11> indexes {{
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_clubs_club_id ON fo92_locked_clubs(club_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_clubs_playable ON fo92_locked_clubs(playable_status)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_people_person_id ON fo92_locked_people(person_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_people_position ON fo92_locked_people(primary_position)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_attributes_person_id ON fo92_locked_player_attributes(person_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_registrations_person_id ON fo92_locked_start_registrations(person_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_registrations_club_id ON fo92_locked_start_registrations(club_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_contracts_person_id ON fo92_locked_contracts_and_academy_terms(person_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_external_registered_person_id ON fo92_locked_external_registered_players(person_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_staff_club_id ON fo92_locked_club_staff(club_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_fo92_locked_future_events_person_id ON fo92_locked_future_transfer_events(person_id)"),
    }};
    for (const QString& indexSql : indexes) {
        if (!executeSql(indexSql, QStringLiteral("LOCKED v11 미러 인덱스 생성 실패"))) {
            return rollbackDetachAndFail(failure);
        }
    }

    qint64 clubs = 0;
    qint64 playableClubs = 0;
    qint64 people = 0;
    qint64 activeStartRegistrations = 0;
    qint64 openingContractStates = 0;
    qint64 externalRegisteredPlayers = 0;
    qint64 staffRecords = 0;
    qint64 futureTransferEvents = 0;
    qint64 provenanceRows = 0;
    qint64 inactiveRegistrationViolations = 0;
    qint64 externalOverlapViolations = 0;
    qint64 outfieldGoalkeeperPolicyViolations = 0;

    if (!countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_clubs"),
            &clubs,
            QStringLiteral("LOCKED v11 clubs count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_clubs WHERE playable_status = 'playable'"),
            &playableClubs,
            QStringLiteral("LOCKED v11 playable clubs count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_people"),
            &people,
            QStringLiteral("LOCKED v11 people count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_start_registrations"),
            &activeStartRegistrations,
            QStringLiteral("LOCKED v11 start registrations count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_contracts_and_academy_terms"),
            &openingContractStates,
            QStringLiteral("LOCKED v11 contracts count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_external_registered_players"),
            &externalRegisteredPlayers,
            QStringLiteral("LOCKED v11 external registered players count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_club_staff"),
            &staffRecords,
            QStringLiteral("LOCKED v11 staff count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_future_transfer_events"),
            &futureTransferEvents,
            QStringLiteral("LOCKED v11 future transfer events count 실패"))
        || !countRows(
            QStringLiteral("SELECT COUNT(*) FROM fo92_locked_data_provenance"),
            &provenanceRows,
            QStringLiteral("LOCKED v11 provenance count 실패"))
        || !countRows(
            QStringLiteral(
                "SELECT COUNT(*) "
                "FROM fo92_locked_start_registrations AS registration "
                "JOIN fo92_locked_clubs AS club ON club.club_id = registration.club_id "
                "WHERE club.playable_status <> 'playable'"),
            &inactiveRegistrationViolations,
            QStringLiteral("LOCKED v11 playable registration scope 검수 실패"))
        || !countRows(
            QStringLiteral(
                "SELECT COUNT(*) "
                "FROM fo92_locked_external_registered_players AS external_player "
                "JOIN fo92_locked_start_registrations AS registration "
                "ON registration.person_id = external_player.person_id"),
            &externalOverlapViolations,
            QStringLiteral("LOCKED v11 external player overlap 검수 실패"))
        || !countRows(
            QStringLiteral(
                "SELECT COUNT(*) "
                "FROM fo92_locked_people AS person "
                "JOIN fo92_locked_player_attributes AS attributes "
                "ON attributes.person_id = person.person_id "
                "WHERE person.primary_position <> 'GK' "
                "AND attributes.goalkeeper_attribute_policy <> 'ignore_handling_and_reflexes'"),
            &outfieldGoalkeeperPolicyViolations,
            QStringLiteral("LOCKED v11 non-GK goalkeeper attribute policy 검수 실패"))) {
        return rollbackDetachAndFail(failure);
    }

    const auto requireCount = [&](const qint64 actual, const qint64 expected, const QString& label) -> bool {
        if (actual == expected) {
            return true;
        }
        failure = QStringLiteral("LOCKED v11 ") + label
            + QStringLiteral(" count mismatch: expected ")
            + QString::number(expected)
            + QStringLiteral(", actual ")
            + QString::number(actual);
        return false;
    };

    if (!requireCount(clubs, 60, QStringLiteral("clubs"))
        || !requireCount(playableClubs, 46, QStringLiteral("playable clubs"))
        || !requireCount(people, 2076, QStringLiteral("people"))
        || !requireCount(activeStartRegistrations, 1486, QStringLiteral("active start registrations"))
        || !requireCount(openingContractStates, 1490, QStringLiteral("opening contract states"))
        || !requireCount(externalRegisteredPlayers, 6, QStringLiteral("external registered players"))
        || !requireCount(staffRecords, 345, QStringLiteral("staff records"))
        || !requireCount(futureTransferEvents, 54, QStringLiteral("future transfer events"))
        || !requireCount(provenanceRows, 5463, QStringLiteral("provenance rows"))
        || !requireCount(inactiveRegistrationViolations, 0, QStringLiteral("inactive registration violations"))
        || !requireCount(externalOverlapViolations, 0, QStringLiteral("external overlap violations"))
        || !requireCount(outfieldGoalkeeperPolicyViolations, 0, QStringLiteral("outfield goalkeeper policy violations"))) {
        return rollbackDetachAndFail(failure);
    }

    QSqlQuery import(database_);
    import.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO world_baseline_imports ("
        "dataset_id, version, source_resource, logical_digest, imported_at, status, "
        "clubs, playable_clubs, people, active_start_registrations, "
        "opening_contract_states, external_registered_players, staff_records, "
        "future_transfer_events, provenance_rows, notes"
        ") VALUES ("
        ":dataset_id, :version, :source_resource, :logical_digest, CURRENT_TIMESTAMP, :status, "
        ":clubs, :playable_clubs, :people, :active_start_registrations, "
        ":opening_contract_states, :external_registered_players, :staff_records, "
        ":future_transfer_events, :provenance_rows, :notes"
        ")"));
    import.bindValue(QStringLiteral(":dataset_id"), QString::fromLatin1(kLockedWorldDatasetId));
    import.bindValue(QStringLiteral(":version"), QString::fromLatin1(kLockedWorldVersion));
    import.bindValue(QStringLiteral(":source_resource"), QString::fromLatin1(kLockedWorldResourcePath));
    import.bindValue(QStringLiteral(":logical_digest"), QString::fromLatin1(kLockedWorldSqliteSha256));
    import.bindValue(QStringLiteral(":status"), QStringLiteral("imported"));
    import.bindValue(QStringLiteral(":clubs"), clubs);
    import.bindValue(QStringLiteral(":playable_clubs"), playableClubs);
    import.bindValue(QStringLiteral(":people"), people);
    import.bindValue(QStringLiteral(":active_start_registrations"), activeStartRegistrations);
    import.bindValue(QStringLiteral(":opening_contract_states"), openingContractStates);
    import.bindValue(QStringLiteral(":external_registered_players"), externalRegisteredPlayers);
    import.bindValue(QStringLiteral(":staff_records"), staffRecords);
    import.bindValue(QStringLiteral(":future_transfer_events"), futureTransferEvents);
    import.bindValue(QStringLiteral(":provenance_rows"), provenanceRows);
    import.bindValue(
        QStringLiteral(":notes"),
        QStringLiteral("SEALED_IMPLEMENTATION_BASELINE; certificate=")
            + QString::fromLatin1(kLockedWorldCertificateResourcePath)
            + QStringLiteral("; sqlite_sha256=")
            + QString::fromLatin1(kLockedWorldSqliteSha256)
            + QStringLiteral("; zip_sha256=")
            + QString::fromLatin1(kLockedWorldZipSha256)
            + QStringLiteral("; mirrored as fo92_locked_* raw source tables"));
    if (!import.exec()) {
        return rollbackDetachAndFail(queryError(QStringLiteral("LOCKED v11 import 이력 저장 실패"), import));
    }
    import.finish();

    if (!database_.commit()) {
        (void)database_.rollback();
        const QString message =
            QStringLiteral("LOCKED v11 import 커밋 실패: ") + database_.lastError().text();
        if (!detachLockedWorld()) {
            return fail(
                errorMessage,
                message + QStringLiteral("; LOCKED v11 SQLite detach도 실패했습니다: ")
                    + database_.lastError().text());
        }
        return fail(errorMessage, message);
    }

    if (!detachLockedWorld()) {
        return fail(
            errorMessage,
            QStringLiteral("LOCKED v11 SQLite detach 실패: ") + database_.lastError().text());
    }

    return true;
}

bool SqliteGameRepository::importFactSafeHandoffV545(QString* errorMessage)
{
    const QString datasetId = QString::fromLatin1(kFactSafeHandoffDatasetId);
    const QString version = QString::fromLatin1(kFactSafeHandoffVersion);

    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral(
        "SELECT status, manifest_file_count, runtime_file_count, runtime_row_count, "
        "competition_runtime_file_count, competition_runtime_row_count, clubs, playable_clubs, "
        "people, opening_registrations, opening_relationship_records, scheduled_future_generations, "
        "future_family_relationships, premier_league_schedule_fixtures, "
        "charity_shield_schedule_fixtures, competition_participants "
        "FROM fact_safe_handoff_imports "
        "WHERE dataset_id = :dataset_id AND version = :version"));
    existing.bindValue(QStringLiteral(":dataset_id"), datasetId);
    existing.bindValue(QStringLiteral(":version"), version);
    if (!existing.exec()) {
        return fail(errorMessage, queryError(QStringLiteral("v5.4.5 handoff import 이력 조회 실패"), existing));
    }

    if (existing.next()
        && existing.value(0).toString() == QStringLiteral("imported")
        && existing.value(1).toLongLong() == kFactSafeManifestRows
        && existing.value(2).toLongLong() == kFactSafeRuntimeFiles
        && existing.value(3).toLongLong() == kFactSafeRuntimeRows
        && existing.value(4).toLongLong() == kFactSafeCompetitionRuntimeFiles
        && existing.value(5).toLongLong() == kFactSafeCompetitionRuntimeRows
        && existing.value(6).toLongLong() == kFactSafeClubs
        && existing.value(7).toLongLong() == kFactSafePlayableClubs
        && existing.value(8).toLongLong() == kFactSafePeople
        && existing.value(9).toLongLong() == kFactSafeOpeningRegistrations
        && existing.value(10).toLongLong() == kFactSafeOpeningRelationshipRecords
        && existing.value(11).toLongLong() == kFactSafeScheduledFutureGenerations
        && existing.value(12).toLongLong() == kFactSafeFutureFamilyRelationships
        && existing.value(13).toLongLong() == kFactSafePremierLeagueScheduleFixtures
        && existing.value(14).toLongLong() == kFactSafeCharityShieldScheduleFixtures
        && existing.value(15).toLongLong() == kFactSafeCompetitionParticipants) {
        existing.finish();

        const auto expectExistingScalar = [&](const QString& sql, const qint64 expected) -> bool {
            QSqlQuery query(database_);
            query.prepare(sql);
            query.bindValue(QStringLiteral(":dataset_id"), datasetId);
            if (!query.exec() || !query.next()) {
                return false;
            }
            const bool matched = query.value(0).toLongLong() == expected;
            query.finish();
            return matched;
        };

        if (expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_files "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeRuntimeFiles + kFactSafeCompetitionRuntimeFiles)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fo92_fact_safe_rows "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeRuntimeRows + kFactSafeCompetitionRuntimeRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_handoff_files "
                    "WHERE dataset_id = :dataset_id "
                    "AND (file_path LIKE 'audit_archive_do_not_import/%' "
                    "OR file_path LIKE 'competitions/historical_reference/%')"),
                0)) {
            return true;
        }
    }
    existing.finish();

    QString failure;
    ParsedFactSafeFile manifest;
    if (!loadFactSafeCsvFile(
            FactSafeFileCandidate {
                QStringLiteral("IMPORT_MANIFEST.csv"),
                QStringLiteral("root"),
                QStringLiteral("manifest"),
                false,
                version,
                0,
                {}
            },
            &manifest,
            &failure)) {
        return fail(errorMessage, failure);
    }

    const auto requireExact = [&](const qint64 actual, const qint64 expected, const QString& label) -> bool {
        if (actual == expected) {
            return true;
        }
        failure = QStringLiteral("v5.4.5 handoff ")
            + label
            + QStringLiteral(" count mismatch: expected ")
            + QString::number(expected)
            + QStringLiteral(", actual ")
            + QString::number(actual);
        return false;
    };

    if (!requireExact(
            static_cast<qint64>(manifest.rows.size()),
            kFactSafeManifestRows,
            QStringLiteral("manifest rows"))) {
        return fail(errorMessage, failure);
    }

    const int manifestFilePathColumn = csvColumnIndex(manifest.headers, QStringLiteral("file_path"));
    const int manifestShaColumn = csvColumnIndex(manifest.headers, QStringLiteral("sha256"));
    const int manifestImportRoleColumn = csvColumnIndex(manifest.headers, QStringLiteral("import_role"));
    const int manifestRuntimeAllowedColumn = csvColumnIndex(manifest.headers, QStringLiteral("runtime_import_allowed"));
    const int manifestAuthoritativeVersionColumn =
        csvColumnIndex(manifest.headers, QStringLiteral("authoritative_version"));
    const int manifestLoadOrderColumn = csvColumnIndex(manifest.headers, QStringLiteral("load_order"));

    if (manifestFilePathColumn < 0
        || manifestShaColumn < 0
        || manifestImportRoleColumn < 0
        || manifestRuntimeAllowedColumn < 0
        || manifestAuthoritativeVersionColumn < 0
        || manifestLoadOrderColumn < 0) {
        return fail(errorMessage, QStringLiteral("v5.4.5 IMPORT_MANIFEST.csv 필수 컬럼이 없습니다."));
    }

    std::vector<FactSafeFileCandidate> runtimeCandidates;
    runtimeCandidates.reserve(kFactSafeRuntimeFiles);
    for (const QStringList& row : manifest.rows) {
        if (csvValueAt(row, manifestRuntimeAllowedColumn).compare(QStringLiteral("true"), Qt::CaseInsensitive) != 0) {
            continue;
        }

        FactSafeFileCandidate candidate;
        candidate.filePath = normalizedFactSafePath(csvValueAt(row, manifestFilePathColumn));
        candidate.sourceArea = factSafeSourceArea(candidate.filePath);
        candidate.importRole = csvValueAt(row, manifestImportRoleColumn);
        candidate.runtimeImportAllowed = true;
        candidate.authoritativeVersion = csvValueAt(row, manifestAuthoritativeVersionColumn);
        if (candidate.authoritativeVersion.isEmpty()) {
            candidate.authoritativeVersion = version;
        }
        candidate.expectedSha256 = csvValueAt(row, manifestShaColumn).toLower();
        bool loadOrderOk = false;
        candidate.loadOrder = csvValueAt(row, manifestLoadOrderColumn).toInt(&loadOrderOk);
        if (!loadOrderOk) {
            candidate.loadOrder = 0;
        }

        if (candidate.filePath.startsWith(QStringLiteral("audit_archive_do_not_import/"))
            || candidate.filePath.startsWith(QStringLiteral("competitions/historical_reference/"))) {
            return fail(
                errorMessage,
                QStringLiteral("v5.4.5 runtime manifest에 import 금지 경로가 포함되어 있습니다: ")
                    + candidate.filePath);
        }
        runtimeCandidates.push_back(candidate);
    }

    if (!requireExact(
            static_cast<qint64>(runtimeCandidates.size()),
            kFactSafeRuntimeFiles,
            QStringLiteral("runtime manifest files"))) {
        return fail(errorMessage, failure);
    }

    std::vector<ParsedFactSafeFile> parsedRuntimeFiles;
    parsedRuntimeFiles.reserve(runtimeCandidates.size());
    qint64 runtimeRowCount = 0;
    for (const FactSafeFileCandidate& candidate : runtimeCandidates) {
        ParsedFactSafeFile parsed;
        if (!loadFactSafeCsvFile(candidate, &parsed, &failure)) {
            return fail(errorMessage, failure);
        }
        runtimeRowCount += static_cast<qint64>(parsed.rows.size());
        parsedRuntimeFiles.push_back(std::move(parsed));
    }

    std::vector<ParsedFactSafeFile> parsedCompetitionFiles;
    parsedCompetitionFiles.reserve(kFactSafeCompetitionRuntimeFileList.size());
    qint64 competitionRuntimeRowCount = 0;
    for (const FactSafeCompetitionRuntimeFile& source : kFactSafeCompetitionRuntimeFileList) {
        FactSafeFileCandidate candidate;
        candidate.filePath = QString::fromLatin1(source.filePath);
        candidate.sourceArea = factSafeSourceArea(candidate.filePath);
        candidate.importRole = QString::fromLatin1(source.importRole);
        candidate.runtimeImportAllowed = true;
        candidate.authoritativeVersion = version;
        candidate.loadOrder = source.loadOrder;

        ParsedFactSafeFile parsed;
        if (!loadFactSafeCsvFile(candidate, &parsed, &failure)) {
            return fail(errorMessage, failure);
        }
        competitionRuntimeRowCount += static_cast<qint64>(parsed.rows.size());
        parsedCompetitionFiles.push_back(std::move(parsed));
    }

    const auto findParsedFile = [&](const QString& path) -> const ParsedFactSafeFile* {
        for (const ParsedFactSafeFile& parsed : parsedRuntimeFiles) {
            if (parsed.candidate.filePath == path) {
                return &parsed;
            }
        }
        for (const ParsedFactSafeFile& parsed : parsedCompetitionFiles) {
            if (parsed.candidate.filePath == path) {
                return &parsed;
            }
        }
        return nullptr;
    };
    const auto rowCountFor = [&](const QString& path) -> qint64 {
        const ParsedFactSafeFile* parsed = findParsedFile(path);
        if (parsed == nullptr) {
            failure = QStringLiteral("v5.4.5 필수 파일이 import 대상에 없습니다: ") + path;
            return -1;
        }
        return static_cast<qint64>(parsed->rows.size());
    };
    const auto countWhere = [&](const QString& path, const QString& columnName, const QString& value) -> qint64 {
        const ParsedFactSafeFile* parsed = findParsedFile(path);
        if (parsed == nullptr) {
            failure = QStringLiteral("v5.4.5 필수 파일이 import 대상에 없습니다: ") + path;
            return -1;
        }
        const int column = csvColumnIndex(parsed->headers, columnName);
        if (column < 0) {
            failure = QStringLiteral("v5.4.5 필수 컬럼이 없습니다: ")
                + path
                + QStringLiteral(".")
                + columnName;
            return -1;
        }
        qint64 count = 0;
        for (const QStringList& row : parsed->rows) {
            if (csvValueAt(row, column) == value) {
                ++count;
            }
        }
        return count;
    };

    const qint64 clubs =
        rowCountFor(QStringLiteral("core/clubs_corrected_candidates_and_model_fields.csv"));
    const qint64 playableClubs =
        countWhere(
            QStringLiteral("core/clubs_corrected_candidates_and_model_fields.csv"),
            QStringLiteral("playable_status"),
            QStringLiteral("playable"));
    const qint64 people =
        rowCountFor(QStringLiteral("core/people_master_corrected_candidates.csv"));
    const qint64 openingRegistrations =
        rowCountFor(QStringLiteral("core/opening_registrations_corrected_candidate_set.csv"));
    const qint64 openingRelationshipRecords =
        rowCountFor(QStringLiteral("gameplay_model/contracts_and_academy_terms_model.csv"));
    const qint64 scheduledFutureGenerations =
        rowCountFor(QStringLiteral("candidates/scheduled_future_generations.csv"));
    const qint64 futureFamilyRelationships =
        rowCountFor(QStringLiteral("provenance/future_generation_family_relationships_v5.4.5.csv"));
    const qint64 premierLeagueScheduleFixtures =
        countWhere(
            QStringLiteral("competitions/runtime_safe/fixtures_schedule_seed.csv"),
            QStringLiteral("competition_id"),
            QStringLiteral("ENG-PL-1992"));
    const qint64 charityShieldScheduleFixtures =
        countWhere(
            QStringLiteral("competitions/runtime_safe/fixtures_schedule_seed.csv"),
            QStringLiteral("competition_id"),
            QStringLiteral("ENG-CHARITY-1992"));
    const qint64 competitionParticipants =
        rowCountFor(QStringLiteral("competitions/runtime_safe/participants_known_at_start.csv"));

    if (!failure.isEmpty()) {
        return fail(errorMessage, failure);
    }
    if (!requireExact(runtimeRowCount, kFactSafeRuntimeRows, QStringLiteral("runtime rows"))
        || !requireExact(
            static_cast<qint64>(parsedCompetitionFiles.size()),
            kFactSafeCompetitionRuntimeFiles,
            QStringLiteral("competition runtime files"))
        || !requireExact(
            competitionRuntimeRowCount,
            kFactSafeCompetitionRuntimeRows,
            QStringLiteral("competition runtime rows"))
        || !requireExact(clubs, kFactSafeClubs, QStringLiteral("clubs"))
        || !requireExact(playableClubs, kFactSafePlayableClubs, QStringLiteral("playable clubs"))
        || !requireExact(people, kFactSafePeople, QStringLiteral("people"))
        || !requireExact(openingRegistrations, kFactSafeOpeningRegistrations, QStringLiteral("opening registrations"))
        || !requireExact(
            openingRelationshipRecords,
            kFactSafeOpeningRelationshipRecords,
            QStringLiteral("opening relationship records"))
        || !requireExact(
            scheduledFutureGenerations,
            kFactSafeScheduledFutureGenerations,
            QStringLiteral("scheduled future generations"))
        || !requireExact(
            futureFamilyRelationships,
            kFactSafeFutureFamilyRelationships,
            QStringLiteral("future family relationships"))
        || !requireExact(
            premierLeagueScheduleFixtures,
            kFactSafePremierLeagueScheduleFixtures,
            QStringLiteral("Premier League schedule fixtures"))
        || !requireExact(
            charityShieldScheduleFixtures,
            kFactSafeCharityShieldScheduleFixtures,
            QStringLiteral("Charity Shield schedule fixtures"))
        || !requireExact(
            competitionParticipants,
            kFactSafeCompetitionParticipants,
            QStringLiteral("competition participants"))) {
        return fail(errorMessage, failure);
    }

    const auto rollbackAndFail = [&](const QString& message) -> bool {
        (void)database_.rollback();
        return fail(errorMessage, message);
    };

    if (!database_.transaction()) {
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 handoff import 트랜잭션 시작 실패: ") + database_.lastError().text());
    }

    const auto executeDatasetDelete = [&](const QString& sql, const QString& context) -> bool {
        QSqlQuery query(database_);
        query.prepare(sql);
        query.bindValue(QStringLiteral(":dataset_id"), datasetId);
        if (!query.exec()) {
            failure = queryError(context, query);
            return false;
        }
        query.finish();
        return true;
    };

    if (!executeDatasetDelete(
            QStringLiteral("DELETE FROM fo92_fact_safe_rows WHERE dataset_id = :dataset_id"),
            QStringLiteral("v5.4.5 기존 raw row 제거 실패"))
        || !executeDatasetDelete(
            QStringLiteral("DELETE FROM fact_safe_handoff_files WHERE dataset_id = :dataset_id"),
            QStringLiteral("v5.4.5 기존 파일 메타 제거 실패"))
        || !executeDatasetDelete(
            QStringLiteral("DELETE FROM fact_safe_handoff_imports WHERE dataset_id = :dataset_id"),
            QStringLiteral("v5.4.5 기존 import 메타 제거 실패"))) {
        return rollbackAndFail(failure);
    }

    QSqlQuery import(database_);
    import.prepare(QStringLiteral(
        "INSERT INTO fact_safe_handoff_imports ("
        "dataset_id, version, source_resource_root, imported_at, status, "
        "manifest_file_count, runtime_file_count, runtime_row_count, "
        "competition_runtime_file_count, competition_runtime_row_count, clubs, playable_clubs, "
        "people, opening_registrations, opening_relationship_records, scheduled_future_generations, "
        "future_family_relationships, premier_league_schedule_fixtures, "
        "charity_shield_schedule_fixtures, competition_participants, notes"
        ") VALUES ("
        ":dataset_id, :version, :source_resource_root, CURRENT_TIMESTAMP, :status, "
        ":manifest_file_count, :runtime_file_count, :runtime_row_count, "
        ":competition_runtime_file_count, :competition_runtime_row_count, :clubs, :playable_clubs, "
        ":people, :opening_registrations, :opening_relationship_records, :scheduled_future_generations, "
        ":future_family_relationships, :premier_league_schedule_fixtures, "
        ":charity_shield_schedule_fixtures, :competition_participants, :notes"
        ")"));
    import.bindValue(QStringLiteral(":dataset_id"), datasetId);
    import.bindValue(QStringLiteral(":version"), version);
    import.bindValue(QStringLiteral(":source_resource_root"), QString::fromLatin1(kFactSafeHandoffResourceRoot));
    import.bindValue(QStringLiteral(":status"), QStringLiteral("imported"));
    import.bindValue(QStringLiteral(":manifest_file_count"), kFactSafeManifestRows);
    import.bindValue(QStringLiteral(":runtime_file_count"), kFactSafeRuntimeFiles);
    import.bindValue(QStringLiteral(":runtime_row_count"), runtimeRowCount);
    import.bindValue(QStringLiteral(":competition_runtime_file_count"), kFactSafeCompetitionRuntimeFiles);
    import.bindValue(QStringLiteral(":competition_runtime_row_count"), competitionRuntimeRowCount);
    import.bindValue(QStringLiteral(":clubs"), clubs);
    import.bindValue(QStringLiteral(":playable_clubs"), playableClubs);
    import.bindValue(QStringLiteral(":people"), people);
    import.bindValue(QStringLiteral(":opening_registrations"), openingRegistrations);
    import.bindValue(QStringLiteral(":opening_relationship_records"), openingRelationshipRecords);
    import.bindValue(QStringLiteral(":scheduled_future_generations"), scheduledFutureGenerations);
    import.bindValue(QStringLiteral(":future_family_relationships"), futureFamilyRelationships);
    import.bindValue(QStringLiteral(":premier_league_schedule_fixtures"), premierLeagueScheduleFixtures);
    import.bindValue(QStringLiteral(":charity_shield_schedule_fixtures"), charityShieldScheduleFixtures);
    import.bindValue(QStringLiteral(":competition_participants"), competitionParticipants);
    import.bindValue(
        QStringLiteral(":notes"),
        QStringLiteral(
            "FACT_SAFE_DATA_HANDOFF_v5.4.5 raw mirror only; root runtime files are SHA-256 checked "
            "from IMPORT_MANIFEST.csv; competitions/runtime_safe result-free seeds are included; "
            "audit_archive_do_not_import and competitions/historical_reference are excluded from live save DB."));
    if (!import.exec()) {
        return rollbackAndFail(queryError(QStringLiteral("v5.4.5 import 메타 저장 실패"), import));
    }
    import.finish();

    QSqlQuery fileInsert(database_);
    fileInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_handoff_files ("
        "dataset_id, file_path, source_area, import_role, runtime_import_allowed, "
        "authoritative_version, load_order, sha256, row_count, column_names, status, notes"
        ") VALUES ("
        ":dataset_id, :file_path, :source_area, :import_role, :runtime_import_allowed, "
        ":authoritative_version, :load_order, :sha256, :row_count, :column_names, :status, :notes"
        ")"));

    const auto insertFileMetadata = [&](const ParsedFactSafeFile& parsed) -> bool {
        fileInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        fileInsert.bindValue(QStringLiteral(":file_path"), parsed.candidate.filePath);
        fileInsert.bindValue(QStringLiteral(":source_area"), parsed.candidate.sourceArea);
        fileInsert.bindValue(QStringLiteral(":import_role"), parsed.candidate.importRole);
        fileInsert.bindValue(QStringLiteral(":runtime_import_allowed"), parsed.candidate.runtimeImportAllowed ? 1 : 0);
        fileInsert.bindValue(QStringLiteral(":authoritative_version"), parsed.candidate.authoritativeVersion);
        fileInsert.bindValue(QStringLiteral(":load_order"), parsed.candidate.loadOrder);
        fileInsert.bindValue(QStringLiteral(":sha256"), parsed.sha256);
        fileInsert.bindValue(QStringLiteral(":row_count"), static_cast<qint64>(parsed.rows.size()));
        fileInsert.bindValue(QStringLiteral(":column_names"), parsed.headers.join(QLatin1Char('|')));
        fileInsert.bindValue(QStringLiteral(":status"), QStringLiteral("imported"));
        fileInsert.bindValue(QStringLiteral(":notes"), QStringLiteral("CSV mirrored as JSON rows; not yet activated into gameplay tables."));
        if (!fileInsert.exec()) {
            failure = queryError(QStringLiteral("v5.4.5 파일 메타 저장 실패: ") + parsed.candidate.filePath, fileInsert);
            return false;
        }
        return true;
    };

    for (const ParsedFactSafeFile& parsed : parsedRuntimeFiles) {
        if (!insertFileMetadata(parsed)) {
            return rollbackAndFail(failure);
        }
    }
    for (const ParsedFactSafeFile& parsed : parsedCompetitionFiles) {
        if (!insertFileMetadata(parsed)) {
            return rollbackAndFail(failure);
        }
    }

    const auto insertRows = [&](const ParsedFactSafeFile& parsed) -> bool {
        if (parsed.rows.empty()) {
            return true;
        }

        QVariantList datasetIds;
        QVariantList filePaths;
        QVariantList rowNumbers;
        QVariantList rowJsonValues;
        const qsizetype rowCount = static_cast<qsizetype>(parsed.rows.size());
        datasetIds.reserve(rowCount);
        filePaths.reserve(rowCount);
        rowNumbers.reserve(rowCount);
        rowJsonValues.reserve(rowCount);

        for (std::size_t rowIndex = 0; rowIndex < parsed.rows.size(); ++rowIndex) {
            datasetIds.append(datasetId);
            filePaths.append(parsed.candidate.filePath);
            rowNumbers.append(static_cast<qint64>(rowIndex + 1));
            rowJsonValues.append(rowToJson(parsed.headers, parsed.rows.at(rowIndex)));
        }

        QSqlQuery rowInsert(database_);
        rowInsert.prepare(QStringLiteral(
            "INSERT INTO fo92_fact_safe_rows (dataset_id, file_path, row_number, row_json) "
            "VALUES (?, ?, ?, ?)"));
        rowInsert.addBindValue(datasetIds);
        rowInsert.addBindValue(filePaths);
        rowInsert.addBindValue(rowNumbers);
        rowInsert.addBindValue(rowJsonValues);
        if (!rowInsert.execBatch()) {
            failure = queryError(QStringLiteral("v5.4.5 raw row batch 저장 실패: ") + parsed.candidate.filePath, rowInsert);
            return false;
        }
        return true;
    };

    for (const ParsedFactSafeFile& parsed : parsedRuntimeFiles) {
        if (!insertRows(parsed)) {
            return rollbackAndFail(failure);
        }
    }
    for (const ParsedFactSafeFile& parsed : parsedCompetitionFiles) {
        if (!insertRows(parsed)) {
            return rollbackAndFail(failure);
        }
    }

    if (!database_.commit()) {
        (void)database_.rollback();
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 handoff import 커밋 실패: ") + database_.lastError().text());
    }

    return true;
}

bool SqliteGameRepository::importFactSafeCompetitionScheduleBridgeV545(QString* errorMessage)
{
    const QString datasetId = QString::fromLatin1(kFactSafeCompetitionBridgeDatasetId);
    const QString version = QString::fromLatin1(kFactSafeHandoffVersion);
    const QString sourceDatasetId = QString::fromLatin1(kFactSafeHandoffDatasetId);

    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral(
        "SELECT status, competition_definitions, competition_id_bridges, competition_seasons, "
        "competition_participants, fixture_seed_rows, active_fixture_rows, "
        "premier_league_fixture_rows, charity_shield_fixture_rows, result_seeded_rows "
        "FROM fact_safe_competition_bridge_imports "
        "WHERE dataset_id = :dataset_id AND version = :version"));
    existing.bindValue(QStringLiteral(":dataset_id"), datasetId);
    existing.bindValue(QStringLiteral(":version"), version);
    if (!existing.exec()) {
        return fail(errorMessage, queryError(QStringLiteral("v5.4.5 competition bridge import 이력 조회 실패"), existing));
    }
    if (existing.next()
        && existing.value(0).toString() == QStringLiteral("imported")
        && existing.value(1).toLongLong() == kFactSafeCompetitionDefinitions
        && existing.value(2).toLongLong() == kFactSafeCompetitionIdBridges
        && existing.value(3).toLongLong() == kFactSafeCompetitionSeasons
        && existing.value(4).toLongLong() == kFactSafeCompetitionParticipants
        && existing.value(5).toLongLong() == kFactSafeFixtureSeedRows
        && existing.value(6).toLongLong() == kFactSafeActiveFixtureRows
        && existing.value(7).toLongLong() == kFactSafePremierLeagueScheduleFixtures
        && existing.value(8).toLongLong() == kFactSafeCharityShieldScheduleFixtures
        && existing.value(9).toLongLong() == 0) {
        existing.finish();

        const auto expectExistingScalar = [&](const QString& sql, const qint64 expected) -> bool {
            QSqlQuery query(database_);
            query.prepare(sql);
            query.bindValue(QStringLiteral(":dataset_id"), datasetId);
            if (!query.exec() || !query.next()) {
                return false;
            }
            const bool matched = query.value(0).toLongLong() == expected;
            query.finish();
            return matched;
        };

        if (expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_competition_id_bridge "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeCompetitionIdBridges)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_fixture_id_bridge "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeActiveFixtureRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fixtures "
                    "WHERE id IN ("
                    "SELECT gameplay_fixture_id FROM fact_safe_fixture_id_bridge "
                    "WHERE dataset_id = :dataset_id)"),
                kFactSafeActiveFixtureRows)) {
            return true;
        }
    }
    existing.finish();

    QString failure;
    const auto loadRuntimeFile = [&](const QString& filePath, ParsedFactSafeFile* parsed) -> bool {
        FactSafeFileCandidate candidate;
        candidate.filePath = filePath;
        candidate.sourceArea = factSafeSourceArea(filePath);
        candidate.importRole = QStringLiteral("runtime_competition_bridge_source");
        candidate.runtimeImportAllowed = true;
        candidate.authoritativeVersion = version;
        candidate.loadOrder = 50;
        return loadFactSafeCsvFile(candidate, parsed, &failure);
    };

    ParsedFactSafeFile competitionDefinitions;
    ParsedFactSafeFile participants;
    ParsedFactSafeFile fixtures;
    ParsedFactSafeFile clubs;
    if (!loadRuntimeFile(QStringLiteral("competitions/runtime_safe/competition_definitions.csv"), &competitionDefinitions)
        || !loadRuntimeFile(QStringLiteral("competitions/runtime_safe/participants_known_at_start.csv"), &participants)
        || !loadRuntimeFile(QStringLiteral("competitions/runtime_safe/fixtures_schedule_seed.csv"), &fixtures)
        || !loadRuntimeFile(QStringLiteral("core/clubs_corrected_candidates_and_model_fields.csv"), &clubs)) {
        return fail(errorMessage, failure);
    }

    const auto requireExact = [&](const qint64 actual, const qint64 expected, const QString& label) -> bool {
        if (actual == expected) {
            return true;
        }
        failure = QStringLiteral("v5.4.5 competition bridge ")
            + label
            + QStringLiteral(" count mismatch: expected ")
            + QString::number(expected)
            + QStringLiteral(", actual ")
            + QString::number(actual);
        return false;
    };

    if (!requireExact(
            static_cast<qint64>(competitionDefinitions.rows.size()),
            kFactSafeCompetitionDefinitions,
            QStringLiteral("competition definitions"))
        || !requireExact(
            static_cast<qint64>(participants.rows.size()),
            kFactSafeCompetitionParticipants,
            QStringLiteral("competition participants"))
        || !requireExact(
            static_cast<qint64>(fixtures.rows.size()),
            kFactSafeFixtureSeedRows,
            QStringLiteral("fixture seed rows"))) {
        return fail(errorMessage, failure);
    }

    const int definitionCompetitionIdColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("competition_id"));
    const int definitionSeasonIdColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("season_id"));
    const int definitionNameColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("competition_name"));
    const int definitionTypeColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("competition_type"));
    const int definitionCountryColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("country"));
    const int definitionTierColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("tier"));
    const int definitionStartDateColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("start_date"));
    const int definitionEndDateColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("end_date"));
    const int definitionPolicyColumn =
        csvColumnIndex(competitionDefinitions.headers, QStringLiteral("runtime_seed_policy"));

    const int participantCompetitionIdColumn =
        csvColumnIndex(participants.headers, QStringLiteral("competition_id"));
    const int participantClubIdColumn =
        csvColumnIndex(participants.headers, QStringLiteral("club_id"));
    const int participantClubNameColumn =
        csvColumnIndex(participants.headers, QStringLiteral("club_name"));
    const int participantPolicyColumn =
        csvColumnIndex(participants.headers, QStringLiteral("runtime_seed_policy"));

    const int fixtureIdColumn = csvColumnIndex(fixtures.headers, QStringLiteral("fixture_id"));
    const int fixtureCompetitionIdColumn =
        csvColumnIndex(fixtures.headers, QStringLiteral("competition_id"));
    const int fixtureDateColumn = csvColumnIndex(fixtures.headers, QStringLiteral("match_date"));
    const int fixtureHomeClubIdColumn =
        csvColumnIndex(fixtures.headers, QStringLiteral("home_club_id"));
    const int fixtureAwayClubIdColumn =
        csvColumnIndex(fixtures.headers, QStringLiteral("away_club_id"));
    const int fixturePolicyColumn =
        csvColumnIndex(fixtures.headers, QStringLiteral("runtime_result_policy"));
    const int fixtureConfidenceColumn =
        csvColumnIndex(fixtures.headers, QStringLiteral("confidence"));

    const int clubIdColumn = csvColumnIndex(clubs.headers, QStringLiteral("club_id"));
    const int clubReputationColumn = csvColumnIndex(clubs.headers, QStringLiteral("reputation"));

    if (definitionCompetitionIdColumn < 0
        || definitionSeasonIdColumn < 0
        || definitionNameColumn < 0
        || definitionTypeColumn < 0
        || definitionCountryColumn < 0
        || definitionTierColumn < 0
        || definitionStartDateColumn < 0
        || definitionEndDateColumn < 0
        || definitionPolicyColumn < 0
        || participantCompetitionIdColumn < 0
        || participantClubIdColumn < 0
        || participantClubNameColumn < 0
        || participantPolicyColumn < 0
        || fixtureIdColumn < 0
        || fixtureCompetitionIdColumn < 0
        || fixtureDateColumn < 0
        || fixtureHomeClubIdColumn < 0
        || fixtureAwayClubIdColumn < 0
        || fixturePolicyColumn < 0
        || fixtureConfidenceColumn < 0
        || clubIdColumn < 0
        || clubReputationColumn < 0) {
        return fail(errorMessage, QStringLiteral("v5.4.5 competition bridge 필수 CSV 컬럼이 없습니다."));
    }

    QHash<QString, int> reputationByLockedClubId;
    for (const QStringList& row : clubs.rows) {
        reputationByLockedClubId.insert(
            csvValueAt(row, clubIdColumn),
            strengthFromReputation(csvValueAt(row, clubReputationColumn)));
    }

    QHash<QString, QString> gameplayClubIdByLockedClubId;
    QSqlQuery clubBridgeQuery(database_);
    if (!clubBridgeQuery.exec(QStringLiteral(
            "SELECT locked_club_id, gameplay_club_id FROM world_club_id_bridge"))) {
        return fail(errorMessage, queryError(QStringLiteral("v5.4.5 club bridge 조회 실패"), clubBridgeQuery));
    }
    while (clubBridgeQuery.next()) {
        gameplayClubIdByLockedClubId.insert(
            clubBridgeQuery.value(0).toString(),
            clubBridgeQuery.value(1).toString());
    }
    clubBridgeQuery.finish();

    qint64 premierLeagueFixtureRows = 0;
    qint64 charityShieldFixtureRows = 0;
    qint64 resultSeededRows = 0;
    for (const QStringList& row : fixtures.rows) {
        const QString factSafeCompetitionId = csvValueAt(row, fixtureCompetitionIdColumn);
        if (factSafeCompetitionId == QStringLiteral("ENG-PL-1992")) {
            ++premierLeagueFixtureRows;
        }
        if (factSafeCompetitionId == QStringLiteral("ENG-CHARITY-1992")) {
            ++charityShieldFixtureRows;
        }
        if (csvValueAt(row, fixturePolicyColumn) != QStringLiteral("result_not_seeded")) {
            ++resultSeededRows;
        }
    }

    if (!requireExact(
            premierLeagueFixtureRows,
            kFactSafePremierLeagueScheduleFixtures,
            QStringLiteral("Premier League fixture rows"))
        || !requireExact(
            charityShieldFixtureRows,
            kFactSafeCharityShieldScheduleFixtures,
            QStringLiteral("Charity Shield fixture rows"))
        || !requireExact(resultSeededRows, 0, QStringLiteral("result seeded rows"))) {
        return fail(errorMessage, failure);
    }

    const auto rollbackAndFail = [&](const QString& message) -> bool {
        (void)database_.rollback();
        return fail(errorMessage, message);
    };

    if (!database_.transaction()) {
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 competition bridge 트랜잭션 시작 실패: ") + database_.lastError().text());
    }

    QSqlQuery deleteExisting(database_);
    deleteExisting.prepare(QStringLiteral(
        "DELETE FROM fact_safe_competition_bridge_imports WHERE dataset_id = :dataset_id"));
    deleteExisting.bindValue(QStringLiteral(":dataset_id"), datasetId);
    if (!deleteExisting.exec()) {
        return rollbackAndFail(queryError(QStringLiteral("v5.4.5 competition bridge 기존 메타 제거 실패"), deleteExisting));
    }
    deleteExisting.finish();

    QSqlQuery import(database_);
    import.prepare(QStringLiteral(
        "INSERT INTO fact_safe_competition_bridge_imports ("
        "dataset_id, version, source_dataset_id, imported_at, status, "
        "competition_definitions, competition_id_bridges, competition_seasons, "
        "competition_participants, fixture_seed_rows, active_fixture_rows, "
        "premier_league_fixture_rows, charity_shield_fixture_rows, result_seeded_rows, notes"
        ") VALUES ("
        ":dataset_id, :version, :source_dataset_id, CURRENT_TIMESTAMP, :status, "
        ":competition_definitions, :competition_id_bridges, :competition_seasons, "
        ":competition_participants, :fixture_seed_rows, :active_fixture_rows, "
        ":premier_league_fixture_rows, :charity_shield_fixture_rows, :result_seeded_rows, :notes"
        ")"));
    import.bindValue(QStringLiteral(":dataset_id"), datasetId);
    import.bindValue(QStringLiteral(":version"), version);
    import.bindValue(QStringLiteral(":source_dataset_id"), sourceDatasetId);
    import.bindValue(QStringLiteral(":status"), QStringLiteral("imported"));
    import.bindValue(QStringLiteral(":competition_definitions"), kFactSafeCompetitionDefinitions);
    import.bindValue(QStringLiteral(":competition_id_bridges"), kFactSafeCompetitionIdBridges);
    import.bindValue(QStringLiteral(":competition_seasons"), kFactSafeCompetitionSeasons);
    import.bindValue(QStringLiteral(":competition_participants"), kFactSafeCompetitionParticipants);
    import.bindValue(QStringLiteral(":fixture_seed_rows"), kFactSafeFixtureSeedRows);
    import.bindValue(QStringLiteral(":active_fixture_rows"), kFactSafeActiveFixtureRows);
    import.bindValue(QStringLiteral(":premier_league_fixture_rows"), premierLeagueFixtureRows);
    import.bindValue(QStringLiteral(":charity_shield_fixture_rows"), charityShieldFixtureRows);
    import.bindValue(QStringLiteral(":result_seeded_rows"), resultSeededRows);
    import.bindValue(
        QStringLiteral(":notes"),
        QStringLiteral(
            "v5.4.5 competitions/runtime_safe bridged into gameplay competitions, seasons, participants, "
            "fixture_generation_profiles, and scheduled fixtures. Fixture results are not seeded."));
    if (!import.exec()) {
        return rollbackAndFail(queryError(QStringLiteral("v5.4.5 competition bridge import 메타 저장 실패"), import));
    }
    import.finish();

    QSqlQuery competitionInsert(database_);
    competitionInsert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO competitions (id, name, country_code, level, active_detail_level) "
        "VALUES (:id, :name, :country, :level, :detail)"));
    QSqlQuery seasonInsert(database_);
    seasonInsert.prepare(QStringLiteral(
        "INSERT INTO competition_seasons ("
        "id, competition_id, season_name, start_date, end_date, status, source_status, notes"
        ") VALUES ("
        ":id, :competition, :season_name, :start, :end, :status, :source_status, :notes"
        ") ON CONFLICT(id) DO UPDATE SET "
        "source_status = excluded.source_status, notes = excluded.notes"));
    QSqlQuery idBridgeInsert(database_);
    idBridgeInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_competition_id_bridge ("
        "dataset_id, fact_safe_competition_id, fact_safe_season_id, gameplay_competition_id, "
        "gameplay_season_id, competition_name, competition_type, runtime_seed_policy, bridge_status, notes"
        ") VALUES ("
        ":dataset_id, :fact_safe_competition, :fact_safe_season, :gameplay_competition, "
        ":gameplay_season, :name, :type, :policy, :status, :notes"
        ")"));

    for (const QStringList& row : competitionDefinitions.rows) {
        const QString factSafeCompetitionId = csvValueAt(row, definitionCompetitionIdColumn);
        const QString gameplayCompetitionId = gameplayCompetitionIdForFactSafe(factSafeCompetitionId);
        const QString gameplaySeasonId = gameplaySeasonIdForFactSafe(factSafeCompetitionId);
        const QString competitionName = csvValueAt(row, definitionNameColumn);
        const QString competitionType = csvValueAt(row, definitionTypeColumn);
        const int level = competitionLevelForFactSafe(csvValueAt(row, definitionTierColumn));
        const QString countryCode =
            csvValueAt(row, definitionCountryColumn) == QStringLiteral("England")
            ? QStringLiteral("ENG")
            : csvValueAt(row, definitionCountryColumn).left(3).toUpper();

        competitionInsert.bindValue(QStringLiteral(":id"), gameplayCompetitionId);
        competitionInsert.bindValue(QStringLiteral(":name"), competitionName);
        competitionInsert.bindValue(QStringLiteral(":country"), countryCode);
        competitionInsert.bindValue(QStringLiteral(":level"), level);
        competitionInsert.bindValue(
            QStringLiteral(":detail"),
            competitionDetailLevelForFactSafe(factSafeCompetitionId, level));
        if (!competitionInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 competition 저장 실패: ") + gameplayCompetitionId, competitionInsert));
        }

        seasonInsert.bindValue(QStringLiteral(":id"), gameplaySeasonId);
        seasonInsert.bindValue(QStringLiteral(":competition"), gameplayCompetitionId);
        seasonInsert.bindValue(QStringLiteral(":season_name"), QStringLiteral("1992-93"));
        seasonInsert.bindValue(
            QStringLiteral(":start"),
            competitionSeasonStartForFactSafe(csvValueAt(row, definitionStartDateColumn)));
        seasonInsert.bindValue(
            QStringLiteral(":end"),
            competitionSeasonEndForFactSafe(csvValueAt(row, definitionEndDateColumn)));
        seasonInsert.bindValue(QStringLiteral(":status"), QStringLiteral("active"));
        seasonInsert.bindValue(QStringLiteral(":source_status"), QStringLiteral("fact_safe_v545_runtime_safe_definition"));
        seasonInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("Definition bridged from FACT SAFE v5.4.5 competitions/runtime_safe without historical results."));
        if (!seasonInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 season 저장 실패: ") + gameplaySeasonId, seasonInsert));
        }

        idBridgeInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        idBridgeInsert.bindValue(QStringLiteral(":fact_safe_competition"), factSafeCompetitionId);
        idBridgeInsert.bindValue(QStringLiteral(":fact_safe_season"), csvValueAt(row, definitionSeasonIdColumn));
        idBridgeInsert.bindValue(QStringLiteral(":gameplay_competition"), gameplayCompetitionId);
        idBridgeInsert.bindValue(QStringLiteral(":gameplay_season"), gameplaySeasonId);
        idBridgeInsert.bindValue(QStringLiteral(":name"), competitionName);
        idBridgeInsert.bindValue(QStringLiteral(":type"), competitionType);
        idBridgeInsert.bindValue(QStringLiteral(":policy"), csvValueAt(row, definitionPolicyColumn));
        idBridgeInsert.bindValue(QStringLiteral(":status"), QStringLiteral("mapped_to_gameplay_seed"));
        idBridgeInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("FACT SAFE v5.4.5 competition id mapped to stable gameplay competition/season id."));
        if (!idBridgeInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 competition id bridge 저장 실패: ") + factSafeCompetitionId, idBridgeInsert));
        }
    }

    QSqlQuery participantInsert(database_);
    participantInsert.prepare(QStringLiteral(
        "INSERT INTO competition_season_teams ("
        "season_id, club_id, source_locked_club_id, seed_order, starting_points, source_status, notes"
        ") VALUES ("
        ":season, :club, :locked, :seed_order, 0, :source_status, :notes"
        ") ON CONFLICT(season_id, club_id) DO UPDATE SET "
        "source_locked_club_id = excluded.source_locked_club_id, "
        "seed_order = excluded.seed_order, source_status = excluded.source_status, notes = excluded.notes"));
    qint64 insertedParticipants = 0;
    for (const QStringList& row : participants.rows) {
        const QString factSafeCompetitionId = csvValueAt(row, participantCompetitionIdColumn);
        const QString lockedClubId = csvValueAt(row, participantClubIdColumn);
        const QString gameplayClubId = gameplayClubIdByLockedClubId.value(lockedClubId);
        if (gameplayClubId.isEmpty()) {
            return rollbackAndFail(
                QStringLiteral("v5.4.5 participant club bridge 누락: ")
                + lockedClubId
                + QStringLiteral(" / ")
                + csvValueAt(row, participantClubNameColumn));
        }
        bool seedOrderOk = false;
        int seedOrder = lockedClubId.mid(3).toInt(&seedOrderOk);
        if (!seedOrderOk) {
            seedOrder = static_cast<int>(insertedParticipants + 1);
        }

        participantInsert.bindValue(QStringLiteral(":season"), gameplaySeasonIdForFactSafe(factSafeCompetitionId));
        participantInsert.bindValue(QStringLiteral(":club"), gameplayClubId);
        participantInsert.bindValue(QStringLiteral(":locked"), lockedClubId);
        participantInsert.bindValue(QStringLiteral(":seed_order"), seedOrder);
        participantInsert.bindValue(QStringLiteral(":source_status"), QStringLiteral("fact_safe_v545_known_at_scenario_start"));
        participantInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("Participant bridged from FACT SAFE v5.4.5 participants_known_at_start; policy=")
                + csvValueAt(row, participantPolicyColumn));
        if (!participantInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 participant 저장 실패: ") + lockedClubId, participantInsert));
        }
        ++insertedParticipants;
    }
    if (!requireExact(insertedParticipants, kFactSafeCompetitionParticipants, QStringLiteral("inserted participants"))) {
        return rollbackAndFail(failure);
    }

    QSqlQuery profileInsert(database_);
    profileInsert.prepare(QStringLiteral(
        "INSERT INTO fixture_generation_profiles ("
        "id, season_id, method, rng_seed, generated_fixture_count, source_status, notes"
        ") VALUES ("
        ":id, :season, :method, :seed, :count, :source_status, :notes"
        ") ON CONFLICT(season_id) DO UPDATE SET "
        "method = excluded.method, generated_fixture_count = excluded.generated_fixture_count, "
        "source_status = excluded.source_status, notes = excluded.notes"));
    for (const QStringList& row : competitionDefinitions.rows) {
        const QString factSafeCompetitionId = csvValueAt(row, definitionCompetitionIdColumn);
        const QString gameplaySeasonId = gameplaySeasonIdForFactSafe(factSafeCompetitionId);
        const qint64 generatedCount =
            factSafeCompetitionId == QStringLiteral("ENG-PL-1992")
            ? kFactSafePremierLeagueScheduleFixtures
            : (factSafeCompetitionId == QStringLiteral("ENG-CHARITY-1992")
                ? kFactSafeCharityShieldScheduleFixtures
                : 0);
        const QString policy = csvValueAt(row, definitionPolicyColumn);
        const QString method =
            generatedCount > 0
            ? QStringLiteral("fact_safe_v545_result_free_schedule_seed")
            : (policy.contains(QStringLiteral("generate_schedule"))
                ? QStringLiteral("definition_only_generate_schedule")
                : QStringLiteral("definition_only_generate_participants_and_draw"));

        profileInsert.bindValue(QStringLiteral(":id"), gameplaySeasonId + QStringLiteral("-fixture-source"));
        profileInsert.bindValue(QStringLiteral(":season"), gameplaySeasonId);
        profileInsert.bindValue(QStringLiteral(":method"), method);
        profileInsert.bindValue(QStringLiteral(":seed"), 1992070100 + static_cast<int>(generatedCount));
        profileInsert.bindValue(QStringLiteral(":count"), generatedCount);
        profileInsert.bindValue(
            QStringLiteral(":source_status"),
            generatedCount > 0
            ? QStringLiteral("fact_safe_v545_result_free_schedule_seed")
            : QStringLiteral("fact_safe_v545_definition_only"));
        profileInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("FACT SAFE v5.4.5 runtime seed policy=") + policy);
        if (!profileInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 fixture profile 저장 실패: ") + gameplaySeasonId, profileInsert));
        }
    }

    QSqlQuery fixtureInsert(database_);
    fixtureInsert.prepare(QStringLiteral(
        "INSERT INTO fixtures ("
        "id, competition_id, fixture_date, home_club_id, away_club_id, "
        "home_strength_estimate, away_strength_estimate, simulation_confidence, status"
        ") VALUES ("
        ":id, :competition, :date, :home, :away, :home_strength, :away_strength, :confidence, 'scheduled'"
        ") ON CONFLICT(id) DO UPDATE SET "
        "competition_id = excluded.competition_id, fixture_date = excluded.fixture_date, "
        "home_club_id = excluded.home_club_id, away_club_id = excluded.away_club_id, "
        "home_strength_estimate = excluded.home_strength_estimate, "
        "away_strength_estimate = excluded.away_strength_estimate, "
        "simulation_confidence = excluded.simulation_confidence, status = excluded.status"));
    QSqlQuery fixtureBridgeInsert(database_);
    fixtureBridgeInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_fixture_id_bridge ("
        "dataset_id, fact_safe_fixture_id, gameplay_fixture_id, fact_safe_competition_id, "
        "gameplay_competition_id, gameplay_season_id, match_date, home_locked_club_id, "
        "away_locked_club_id, runtime_result_policy, bridge_status, notes"
        ") VALUES ("
        ":dataset_id, :fact_safe_fixture, :gameplay_fixture, :fact_safe_competition, "
        ":gameplay_competition, :gameplay_season, :date, :home_locked, "
        ":away_locked, :policy, :status, :notes"
        ")"));

    qint64 insertedFixtures = 0;
    for (const QStringList& row : fixtures.rows) {
        const QString factSafeFixtureId = csvValueAt(row, fixtureIdColumn);
        const QString factSafeCompetitionId = csvValueAt(row, fixtureCompetitionIdColumn);
        const QString gameplayCompetitionId = gameplayCompetitionIdForFactSafe(factSafeCompetitionId);
        const QString gameplaySeasonId = gameplaySeasonIdForFactSafe(factSafeCompetitionId);
        const QString gameplayFixtureId = gameplayFixtureIdForFactSafe(factSafeFixtureId);
        const QString homeLockedClubId = csvValueAt(row, fixtureHomeClubIdColumn);
        const QString awayLockedClubId = csvValueAt(row, fixtureAwayClubIdColumn);
        const QString homeGameplayClubId = gameplayClubIdByLockedClubId.value(homeLockedClubId);
        const QString awayGameplayClubId = gameplayClubIdByLockedClubId.value(awayLockedClubId);
        if (homeGameplayClubId.isEmpty() || awayGameplayClubId.isEmpty()) {
            return rollbackAndFail(
                QStringLiteral("v5.4.5 fixture club bridge 누락: ")
                + factSafeFixtureId
                + QStringLiteral(" home=")
                + homeLockedClubId
                + QStringLiteral(" away=")
                + awayLockedClubId);
        }

        fixtureInsert.bindValue(QStringLiteral(":id"), gameplayFixtureId);
        fixtureInsert.bindValue(QStringLiteral(":competition"), gameplayCompetitionId);
        fixtureInsert.bindValue(QStringLiteral(":date"), csvValueAt(row, fixtureDateColumn));
        fixtureInsert.bindValue(QStringLiteral(":home"), homeGameplayClubId);
        fixtureInsert.bindValue(QStringLiteral(":away"), awayGameplayClubId);
        fixtureInsert.bindValue(
            QStringLiteral(":home_strength"),
            reputationByLockedClubId.value(homeLockedClubId, 50));
        fixtureInsert.bindValue(
            QStringLiteral(":away_strength"),
            reputationByLockedClubId.value(awayLockedClubId, 50));
        fixtureInsert.bindValue(
            QStringLiteral(":confidence"),
            QStringLiteral("fact_safe_v545_") + csvValueAt(row, fixtureConfidenceColumn));
        if (!fixtureInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 fixture 저장 실패: ") + gameplayFixtureId, fixtureInsert));
        }

        fixtureBridgeInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":fact_safe_fixture"), factSafeFixtureId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":gameplay_fixture"), gameplayFixtureId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":fact_safe_competition"), factSafeCompetitionId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":gameplay_competition"), gameplayCompetitionId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":gameplay_season"), gameplaySeasonId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":date"), csvValueAt(row, fixtureDateColumn));
        fixtureBridgeInsert.bindValue(QStringLiteral(":home_locked"), homeLockedClubId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":away_locked"), awayLockedClubId);
        fixtureBridgeInsert.bindValue(QStringLiteral(":policy"), csvValueAt(row, fixturePolicyColumn));
        fixtureBridgeInsert.bindValue(
            QStringLiteral(":status"),
            factSafeFixtureId == QStringLiteral("PL92-008")
            ? QStringLiteral("mapped_existing_seed_fixture")
            : QStringLiteral("inserted_result_free_seed_fixture"));
        fixtureBridgeInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("FACT SAFE v5.4.5 result-free schedule seed; no historical result imported."));
        if (!fixtureBridgeInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 fixture id bridge 저장 실패: ") + factSafeFixtureId, fixtureBridgeInsert));
        }
        ++insertedFixtures;
    }
    if (!requireExact(insertedFixtures, kFactSafeActiveFixtureRows, QStringLiteral("inserted fixtures"))) {
        return rollbackAndFail(failure);
    }

    if (!database_.commit()) {
        (void)database_.rollback();
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 competition bridge 커밋 실패: ") + database_.lastError().text());
    }

    return true;
}

bool SqliteGameRepository::importFactSafeRosterActivationBridgeV545(QString* errorMessage)
{
    const QString datasetId = QString::fromLatin1(kFactSafeRosterBridgeDatasetId);
    const QString version = QString::fromLatin1(kFactSafeHandoffVersion);
    const QString sourceDatasetId = QString::fromLatin1(kFactSafeHandoffDatasetId);

    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral(
        "SELECT status, people_source_rows, person_bridge_rows, "
        "opening_registration_source_rows, registration_bridge_rows, active_registration_rows, "
        "contract_source_rows, contract_bridge_rows, active_contract_rows, loan_review_rows "
        "FROM fact_safe_roster_bridge_imports "
        "WHERE dataset_id = :dataset_id AND version = :version"));
    existing.bindValue(QStringLiteral(":dataset_id"), datasetId);
    existing.bindValue(QStringLiteral(":version"), version);
    if (!existing.exec()) {
        return fail(errorMessage, queryError(QStringLiteral("v5.4.5 roster bridge import 이력 조회 실패"), existing));
    }
    if (existing.next()
        && existing.value(0).toString() == QStringLiteral("imported")
        && existing.value(1).toLongLong() == kFactSafePeople
        && existing.value(2).toLongLong() == kFactSafeRosterPersonBridgeRows
        && existing.value(3).toLongLong() == kFactSafeOpeningRegistrations
        && existing.value(4).toLongLong() == kFactSafeRosterRegistrationBridgeRows
        && existing.value(5).toLongLong() == kFactSafeRosterActiveRegistrationRows
        && existing.value(6).toLongLong() == kFactSafeOpeningRelationshipRecords
        && existing.value(7).toLongLong() == kFactSafeRosterContractBridgeRows
        && existing.value(8).toLongLong() == kFactSafeRosterActiveContractRows
        && existing.value(9).toLongLong() == kFactSafeRosterLoanReviewRows) {
        existing.finish();

        const auto expectExistingScalar = [&](const QString& sql, const qint64 expected) -> bool {
            QSqlQuery query(database_);
            query.prepare(sql);
            query.bindValue(QStringLiteral(":dataset_id"), datasetId);
            if (!query.exec() || !query.next()) {
                return false;
            }
            const bool matched = query.value(0).toLongLong() == expected;
            query.finish();
            return matched;
        };

        if (expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_person_activation_bridge "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeRosterPersonBridgeRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_registration_bridge "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeRosterRegistrationBridgeRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_roster_registration_bridge "
                    "WHERE dataset_id = :dataset_id "
                    "AND activation_status = 'active_opening_registration_insert_or_preserve_existing'"),
                kFactSafeRosterActiveRegistrationRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fact_safe_roster_registration_bridge AS bridge "
                    "JOIN historical_registrations AS registration "
                    "ON registration.person_id = bridge.gameplay_person_id "
                    "AND registration.club_id = bridge.gameplay_club_id "
                    "AND registration.snapshot_date = bridge.snapshot_date "
                    "WHERE bridge.dataset_id = :dataset_id "
                    "AND bridge.activation_status = 'active_opening_registration_insert_or_preserve_existing'"),
                kFactSafeRosterActiveRegistrationRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_contract_bridge "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeRosterContractBridgeRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_contract_bridge "
                    "WHERE dataset_id = :dataset_id "
                    "AND activation_status = 'active_contract_model_insert_or_preserve_existing'"),
                kFactSafeRosterActiveContractRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fact_safe_contract_bridge AS bridge "
                    "JOIN historical_player_contracts AS contract "
                    "ON contract.person_id = bridge.gameplay_person_id "
                    "AND contract.club_id = bridge.gameplay_club_id "
                    "AND contract.snapshot_date = '1992-07-01' "
                    "WHERE bridge.dataset_id = :dataset_id "
                    "AND bridge.activation_status = 'active_contract_model_insert_or_preserve_existing' "
                    "AND contract.contract_start_date IS NOT NULL "
                    "AND contract.contract_end_date IS NOT NULL "
                    "AND contract.weekly_wage_pence IS NOT NULL"),
                kFactSafeRosterActiveContractRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_loan_review_queue "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeRosterLoanReviewRows)) {
            return true;
        }
    }
    existing.finish();

    QString failure;
    const auto loadRuntimeFile = [&](const QString& filePath, ParsedFactSafeFile* parsed) -> bool {
        FactSafeFileCandidate candidate;
        candidate.filePath = filePath;
        candidate.sourceArea = factSafeSourceArea(filePath);
        candidate.importRole = QStringLiteral("runtime_roster_bridge_source");
        candidate.runtimeImportAllowed = true;
        candidate.authoritativeVersion = version;
        candidate.loadOrder = 60;
        return loadFactSafeCsvFile(candidate, parsed, &failure);
    };

    ParsedFactSafeFile people;
    ParsedFactSafeFile registrations;
    ParsedFactSafeFile contracts;
    if (!loadRuntimeFile(QStringLiteral("core/people_master_corrected_candidates.csv"), &people)
        || !loadRuntimeFile(QStringLiteral("core/opening_registrations_corrected_candidate_set.csv"), &registrations)
        || !loadRuntimeFile(QStringLiteral("gameplay_model/contracts_and_academy_terms_model.csv"), &contracts)) {
        return fail(errorMessage, failure);
    }

    const auto requireExact = [&](const qint64 actual, const qint64 expected, const QString& label) -> bool {
        if (actual == expected) {
            return true;
        }
        failure = QStringLiteral("v5.4.5 roster bridge ")
            + label
            + QStringLiteral(" count mismatch: expected ")
            + QString::number(expected)
            + QStringLiteral(", actual ")
            + QString::number(actual);
        return false;
    };

    if (!requireExact(static_cast<qint64>(people.rows.size()), kFactSafePeople, QStringLiteral("people"))
        || !requireExact(
            static_cast<qint64>(registrations.rows.size()),
            kFactSafeOpeningRegistrations,
            QStringLiteral("opening registrations"))
        || !requireExact(
            static_cast<qint64>(contracts.rows.size()),
            kFactSafeOpeningRelationshipRecords,
            QStringLiteral("opening relationship records"))) {
        return fail(errorMessage, failure);
    }

    const int personIdColumn = csvColumnIndex(people.headers, QStringLiteral("person_id"));
    const int personNameColumn = csvColumnIndex(people.headers, QStringLiteral("display_name"));
    const int personBirthDateColumn = csvColumnIndex(people.headers, QStringLiteral("date_of_birth"));
    const int personNationalityColumn = csvColumnIndex(people.headers, QStringLiteral("nationality"));
    const int personPrimaryPositionColumn = csvColumnIndex(people.headers, QStringLiteral("primary_position"));
    const int personPositionCodesColumn = csvColumnIndex(people.headers, QStringLiteral("position_codes"));
    const int personAvailabilityColumn = csvColumnIndex(people.headers, QStringLiteral("availability_state"));
    const int personConfidenceColumn = csvColumnIndex(people.headers, QStringLiteral("confidence"));
    const int personSourceUrlColumn = csvColumnIndex(people.headers, QStringLiteral("source_url"));
    const int personNoteColumn = csvColumnIndex(people.headers, QStringLiteral("note"));

    const int registrationIdColumn = csvColumnIndex(registrations.headers, QStringLiteral("registration_id"));
    const int registrationPersonIdColumn = csvColumnIndex(registrations.headers, QStringLiteral("person_id"));
    const int registrationNameColumn = csvColumnIndex(registrations.headers, QStringLiteral("display_name"));
    const int registrationClubIdColumn = csvColumnIndex(registrations.headers, QStringLiteral("club_id"));
    const int registrationClubNameColumn = csvColumnIndex(registrations.headers, QStringLiteral("club_name"));
    const int registrationSquadGroupColumn = csvColumnIndex(registrations.headers, QStringLiteral("squad_group"));
    const int registrationStatusColumn = csvColumnIndex(registrations.headers, QStringLiteral("registration_status"));
    const int registrationEffectiveDateColumn = csvColumnIndex(registrations.headers, QStringLiteral("effective_date"));
    const int registrationConfidenceColumn = csvColumnIndex(registrations.headers, QStringLiteral("confidence"));
    const int registrationNoteColumn = csvColumnIndex(registrations.headers, QStringLiteral("note"));
    const int registrationFactStatusColumn = csvColumnIndex(registrations.headers, QStringLiteral("fact_status"));
    const int registrationHistoricalStatusColumn =
        csvColumnIndex(registrations.headers, QStringLiteral("historical_status"));
    const int registrationPolicyColumn = csvColumnIndex(registrations.headers, QStringLiteral("codex_import_policy"));
    const int registrationOperationalStatusColumn =
        csvColumnIndex(registrations.headers, QStringLiteral("operational_status"));

    const int contractIdColumn = csvColumnIndex(contracts.headers, QStringLiteral("contract_id"));
    const int contractPersonIdColumn = csvColumnIndex(contracts.headers, QStringLiteral("person_id"));
    const int contractNameColumn = csvColumnIndex(contracts.headers, QStringLiteral("display_name"));
    const int contractClubIdColumn = csvColumnIndex(contracts.headers, QStringLiteral("club_id"));
    const int contractClubNameColumn = csvColumnIndex(contracts.headers, QStringLiteral("club_name"));
    const int contractRegistrationScopeColumn =
        csvColumnIndex(contracts.headers, QStringLiteral("registration_scope"));
    const int contractStartColumn = csvColumnIndex(contracts.headers, QStringLiteral("contract_start"));
    const int contractScenarioStartColumn =
        csvColumnIndex(contracts.headers, QStringLiteral("scenario_contract_start"));
    const int contractEndColumn = csvColumnIndex(contracts.headers, QStringLiteral("contract_end"));
    const int contractWeeklyWageColumn = csvColumnIndex(contracts.headers, QStringLiteral("weekly_wage_gbp"));
    const int contractSquadRoleColumn = csvColumnIndex(contracts.headers, QStringLiteral("squad_role"));
    const int contractStatusColumn = csvColumnIndex(contracts.headers, QStringLiteral("contract_status"));
    const int contractConfidenceColumn = csvColumnIndex(contracts.headers, QStringLiteral("confidence"));
    const int contractNoteColumn = csvColumnIndex(contracts.headers, QStringLiteral("note"));
    const int contractPolicyColumn = csvColumnIndex(contracts.headers, QStringLiteral("codex_import_policy"));
    const int contractTermValidColumn =
        csvColumnIndex(contracts.headers, QStringLiteral("term_valid_at_snapshot"));
    const int contractRelationshipTypeColumn =
        csvColumnIndex(contracts.headers, QStringLiteral("opening_relationship_type"));
    const int contractRelationshipSemanticsColumn =
        csvColumnIndex(contracts.headers, QStringLiteral("opening_relationship_semantics"));

    if (personIdColumn < 0
        || personNameColumn < 0
        || personBirthDateColumn < 0
        || personNationalityColumn < 0
        || personPrimaryPositionColumn < 0
        || personPositionCodesColumn < 0
        || personAvailabilityColumn < 0
        || personConfidenceColumn < 0
        || personSourceUrlColumn < 0
        || personNoteColumn < 0
        || registrationIdColumn < 0
        || registrationPersonIdColumn < 0
        || registrationNameColumn < 0
        || registrationClubIdColumn < 0
        || registrationClubNameColumn < 0
        || registrationSquadGroupColumn < 0
        || registrationStatusColumn < 0
        || registrationEffectiveDateColumn < 0
        || registrationConfidenceColumn < 0
        || registrationNoteColumn < 0
        || registrationFactStatusColumn < 0
        || registrationHistoricalStatusColumn < 0
        || registrationPolicyColumn < 0
        || registrationOperationalStatusColumn < 0
        || contractIdColumn < 0
        || contractPersonIdColumn < 0
        || contractNameColumn < 0
        || contractClubIdColumn < 0
        || contractClubNameColumn < 0
        || contractRegistrationScopeColumn < 0
        || contractStartColumn < 0
        || contractScenarioStartColumn < 0
        || contractEndColumn < 0
        || contractWeeklyWageColumn < 0
        || contractSquadRoleColumn < 0
        || contractStatusColumn < 0
        || contractConfidenceColumn < 0
        || contractNoteColumn < 0
        || contractPolicyColumn < 0
        || contractTermValidColumn < 0
        || contractRelationshipTypeColumn < 0
        || contractRelationshipSemanticsColumn < 0) {
        return fail(errorMessage, QStringLiteral("v5.4.5 roster bridge 필수 CSV 컬럼이 없습니다."));
    }

    QHash<QString, QString> gameplayPersonIdByFactSafePersonId;
    QSqlQuery personBridgeQuery(database_);
    if (!personBridgeQuery.exec(QStringLiteral(
            "SELECT locked_person_id, gameplay_person_id FROM world_person_id_bridge"))) {
        return fail(errorMessage, queryError(QStringLiteral("v5.4.5 person bridge 조회 실패"), personBridgeQuery));
    }
    while (personBridgeQuery.next()) {
        gameplayPersonIdByFactSafePersonId.insert(
            personBridgeQuery.value(0).toString(),
            personBridgeQuery.value(1).toString());
    }
    personBridgeQuery.finish();

    QHash<QString, QString> existingGameplayPersonIdByName;
    QHash<QString, int> existingGameplayPersonNameCounts;
    QSqlQuery existingPeopleQuery(database_);
    if (!existingPeopleQuery.exec(QStringLiteral("SELECT full_name, id FROM people"))) {
        return fail(errorMessage, queryError(QStringLiteral("기존 gameplay person 이름 조회 실패"), existingPeopleQuery));
    }
    while (existingPeopleQuery.next()) {
        const QString key = normalizedPersonName(existingPeopleQuery.value(0).toString());
        if (key.isEmpty()) {
            continue;
        }
        existingGameplayPersonIdByName.insert(key, existingPeopleQuery.value(1).toString());
        existingGameplayPersonNameCounts.insert(key, existingGameplayPersonNameCounts.value(key) + 1);
    }
    existingPeopleQuery.finish();

    const auto gameplayPersonIdForFactSafe = [&](
        const QString& factSafePersonId,
        const QString& displayName) -> QString {
        const QString mappedId = gameplayPersonIdByFactSafePersonId.value(factSafePersonId);
        const QString nameKey = normalizedPersonName(displayName);
        if ((mappedId.isEmpty() || mappedId.startsWith(QStringLiteral("fo92-")))
            && !nameKey.isEmpty()
            && existingGameplayPersonNameCounts.value(nameKey) == 1) {
            return existingGameplayPersonIdByName.value(nameKey);
        }
        return mappedId.isEmpty()
            ? factSafeGameplayPersonIdFallback(factSafePersonId)
            : mappedId;
    };

    QHash<QString, QString> activeGameplayClubIdByFactSafeClubId;
    QSqlQuery clubBridgeQuery(database_);
    if (!clubBridgeQuery.exec(QStringLiteral(
            "SELECT locked_club_id, gameplay_club_id "
            "FROM world_club_id_bridge "
            "WHERE playable_status = 'playable' "
            "AND competition_id IN ('eng-premier-league', 'eng-first-division')"))) {
        return fail(errorMessage, queryError(QStringLiteral("v5.4.5 active club bridge 조회 실패"), clubBridgeQuery));
    }
    while (clubBridgeQuery.next()) {
        activeGameplayClubIdByFactSafeClubId.insert(
            clubBridgeQuery.value(0).toString(),
            clubBridgeQuery.value(1).toString());
    }
    clubBridgeQuery.finish();

    QHash<QString, bool> activeFactSafePersonIds;
    qint64 activeRegistrationRows = 0;
    qint64 registrationLoanReviewRows = 0;
    for (const QStringList& row : registrations.rows) {
        const QString clubId = csvValueAt(row, registrationClubIdColumn);
        const QString registrationStatus = csvValueAt(row, registrationStatusColumn);
        const QString policy = csvValueAt(row, registrationPolicyColumn);
        const bool activeRegistration =
            !activeGameplayClubIdByFactSafeClubId.value(clubId).isEmpty()
            && factSafeRegistrationPolicyAllowsActiveImport(policy)
            && factSafeRegistrationStatusIsActive(registrationStatus);
        if (activeRegistration) {
            ++activeRegistrationRows;
            activeFactSafePersonIds.insert(csvValueAt(row, registrationPersonIdColumn), true);
        }
        const QString loanText =
            csvValueAt(row, registrationNoteColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, registrationHistoricalStatusColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, registrationFactStatusColumn);
        if (factSafeLoanReviewMarker(loanText)) {
            ++registrationLoanReviewRows;
        }
    }

    qint64 activeContractRows = 0;
    qint64 contractLoanReviewRows = 0;
    for (const QStringList& row : contracts.rows) {
        bool wageOk = false;
        (void)factSafeGbpToPence(csvValueAt(row, contractWeeklyWageColumn), &wageOk);
        const QString clubId = csvValueAt(row, contractClubIdColumn);
        const bool activeContract =
            !activeGameplayClubIdByFactSafeClubId.value(clubId).isEmpty()
            && factSafeTrue(csvValueAt(row, contractTermValidColumn))
            && !csvValueAt(row, contractEndColumn).isEmpty()
            && (!csvValueAt(row, contractStartColumn).isEmpty()
                || !csvValueAt(row, contractScenarioStartColumn).isEmpty())
            && wageOk;
        if (activeContract) {
            ++activeContractRows;
        }
        const QString loanText =
            csvValueAt(row, contractNoteColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, contractRelationshipTypeColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, contractRelationshipSemanticsColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, contractStatusColumn);
        if (factSafeLoanReviewMarker(loanText)) {
            ++contractLoanReviewRows;
        }
    }
    const qint64 loanReviewRows = registrationLoanReviewRows + contractLoanReviewRows;

    if (!requireExact(activeRegistrationRows, kFactSafeRosterActiveRegistrationRows, QStringLiteral("active registrations"))
        || !requireExact(activeContractRows, kFactSafeRosterActiveContractRows, QStringLiteral("active contracts"))
        || !requireExact(loanReviewRows, kFactSafeRosterLoanReviewRows, QStringLiteral("loan review rows"))) {
        return fail(errorMessage, failure);
    }

    const auto rollbackAndFail = [&](const QString& message) -> bool {
        (void)database_.rollback();
        return fail(errorMessage, message);
    };
    const auto optionalText = [](const QString& value) -> QVariant {
        return value.isEmpty() ? QVariant {} : QVariant {value};
    };

    if (!database_.transaction()) {
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 roster bridge 트랜잭션 시작 실패: ") + database_.lastError().text());
    }

    QSqlQuery deleteExisting(database_);
    deleteExisting.prepare(QStringLiteral(
        "DELETE FROM fact_safe_roster_bridge_imports WHERE dataset_id = :dataset_id"));
    deleteExisting.bindValue(QStringLiteral(":dataset_id"), datasetId);
    if (!deleteExisting.exec()) {
        return rollbackAndFail(queryError(QStringLiteral("v5.4.5 roster bridge 기존 메타 제거 실패"), deleteExisting));
    }
    deleteExisting.finish();

    QSqlQuery import(database_);
    import.prepare(QStringLiteral(
        "INSERT INTO fact_safe_roster_bridge_imports ("
        "dataset_id, version, source_dataset_id, imported_at, status, "
        "people_source_rows, person_bridge_rows, people_identity_rows_processed, "
        "player_profile_rows_processed, opening_registration_source_rows, registration_bridge_rows, "
        "active_registration_rows, skipped_registration_rows, contract_source_rows, contract_bridge_rows, "
        "active_contract_rows, skipped_contract_rows, loan_review_rows, notes"
        ") VALUES ("
        ":dataset_id, :version, :source_dataset_id, CURRENT_TIMESTAMP, :status, "
        ":people_source_rows, :person_bridge_rows, :people_identity_rows_processed, "
        ":player_profile_rows_processed, :opening_registration_source_rows, :registration_bridge_rows, "
        ":active_registration_rows, :skipped_registration_rows, :contract_source_rows, :contract_bridge_rows, "
        ":active_contract_rows, :skipped_contract_rows, :loan_review_rows, :notes"
        ")"));
    import.bindValue(QStringLiteral(":dataset_id"), datasetId);
    import.bindValue(QStringLiteral(":version"), version);
    import.bindValue(QStringLiteral(":source_dataset_id"), sourceDatasetId);
    import.bindValue(QStringLiteral(":status"), QStringLiteral("imported"));
    import.bindValue(QStringLiteral(":people_source_rows"), kFactSafePeople);
    import.bindValue(QStringLiteral(":person_bridge_rows"), kFactSafeRosterPersonBridgeRows);
    import.bindValue(QStringLiteral(":people_identity_rows_processed"), kFactSafePeople);
    import.bindValue(QStringLiteral(":player_profile_rows_processed"), kFactSafePeople);
    import.bindValue(QStringLiteral(":opening_registration_source_rows"), kFactSafeOpeningRegistrations);
    import.bindValue(QStringLiteral(":registration_bridge_rows"), kFactSafeRosterRegistrationBridgeRows);
    import.bindValue(QStringLiteral(":active_registration_rows"), activeRegistrationRows);
    import.bindValue(
        QStringLiteral(":skipped_registration_rows"),
        kFactSafeOpeningRegistrations - activeRegistrationRows);
    import.bindValue(QStringLiteral(":contract_source_rows"), kFactSafeOpeningRelationshipRecords);
    import.bindValue(QStringLiteral(":contract_bridge_rows"), kFactSafeRosterContractBridgeRows);
    import.bindValue(QStringLiteral(":active_contract_rows"), activeContractRows);
    import.bindValue(
        QStringLiteral(":skipped_contract_rows"),
        kFactSafeOpeningRelationshipRecords - activeContractRows);
    import.bindValue(QStringLiteral(":loan_review_rows"), loanReviewRows);
    import.bindValue(
        QStringLiteral(":notes"),
        QStringLiteral(
            "v5.4.5 people, opening registrations, and opening relationship records bridged into "
            "people/player_profiles/historical_registrations/historical_player_contracts. "
            "Existing people, profiles, and registrations are preserved; contract conflicts only fill missing term fields. "
            "Loan markers are review-only; no loan deals are auto-created."));
    if (!import.exec()) {
        return rollbackAndFail(queryError(QStringLiteral("v5.4.5 roster bridge import 메타 저장 실패"), import));
    }
    import.finish();

    QSqlQuery peopleInsert(database_);
    peopleInsert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO people (id, full_name, birth_date, nationality, data_confidence) "
        "VALUES (:id, :name, :birth_date, :nationality, :confidence)"));
    QSqlQuery profileInsert(database_);
    profileInsert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO player_profiles ("
        "person_id, primary_position, secondary_positions, profile_confidence"
        ") VALUES (:person_id, :primary_position, :secondary_positions, :confidence)"));
    QSqlQuery personBridgeInsert(database_);
    personBridgeInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_person_activation_bridge ("
        "dataset_id, fact_safe_person_id, gameplay_person_id, display_name, date_of_birth, "
        "nationality, primary_position, availability_state, activation_status, data_confidence, source_url, notes"
        ") VALUES ("
        ":dataset_id, :fact_safe_person_id, :gameplay_person_id, :display_name, :date_of_birth, "
        ":nationality, :primary_position, :availability_state, :activation_status, :data_confidence, :source_url, :notes"
        ")"));

    qint64 personBridgeRows = 0;
    for (const QStringList& row : people.rows) {
        const QString factSafePersonId = csvValueAt(row, personIdColumn);
        const QString displayName = csvValueAt(row, personNameColumn);
        const QString gameplayPersonId = gameplayPersonIdForFactSafe(factSafePersonId, displayName);
        const QString dateOfBirth = csvValueAt(row, personBirthDateColumn);
        const QString nationality = csvValueAt(row, personNationalityColumn);
        const QString primaryPosition = csvValueAt(row, personPrimaryPositionColumn);
        const QString positionCodes = csvValueAt(row, personPositionCodesColumn);
        const QString confidence = csvValueAt(row, personConfidenceColumn);
        const QString availability = csvValueAt(row, personAvailabilityColumn);

        peopleInsert.bindValue(QStringLiteral(":id"), gameplayPersonId);
        peopleInsert.bindValue(QStringLiteral(":name"), displayName);
        peopleInsert.bindValue(QStringLiteral(":birth_date"), dateOfBirth);
        peopleInsert.bindValue(QStringLiteral(":nationality"), nationality);
        peopleInsert.bindValue(QStringLiteral(":confidence"), factSafeDataConfidence(confidence));
        if (!peopleInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 person 저장 실패: ") + factSafePersonId, peopleInsert));
        }

        profileInsert.bindValue(QStringLiteral(":person_id"), gameplayPersonId);
        profileInsert.bindValue(
            QStringLiteral(":primary_position"),
            primaryPosition.isEmpty() ? QStringLiteral("Unknown") : primaryPosition);
        profileInsert.bindValue(
            QStringLiteral(":secondary_positions"),
            factSafeSecondaryPositions(primaryPosition, positionCodes));
        profileInsert.bindValue(QStringLiteral(":confidence"), factSafeProfileConfidence(confidence));
        if (!profileInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 player profile 저장 실패: ") + factSafePersonId, profileInsert));
        }

        const QString activationStatus =
            activeFactSafePersonIds.value(factSafePersonId)
            ? QStringLiteral("active_opening_identity")
            : (availability.contains(QStringLiteral("inactive"), Qt::CaseInsensitive)
                || availability.contains(QStringLiteral("future"), Qt::CaseInsensitive)
                ? QStringLiteral("identity_only_inactive_or_future")
                : QStringLiteral("identity_only_reference"));

        personBridgeInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        personBridgeInsert.bindValue(QStringLiteral(":fact_safe_person_id"), factSafePersonId);
        personBridgeInsert.bindValue(QStringLiteral(":gameplay_person_id"), gameplayPersonId);
        personBridgeInsert.bindValue(QStringLiteral(":display_name"), displayName);
        personBridgeInsert.bindValue(QStringLiteral(":date_of_birth"), dateOfBirth);
        personBridgeInsert.bindValue(QStringLiteral(":nationality"), nationality);
        personBridgeInsert.bindValue(
            QStringLiteral(":primary_position"),
            primaryPosition.isEmpty() ? QStringLiteral("Unknown") : primaryPosition);
        personBridgeInsert.bindValue(QStringLiteral(":availability_state"), availability);
        personBridgeInsert.bindValue(QStringLiteral(":activation_status"), activationStatus);
        personBridgeInsert.bindValue(QStringLiteral(":data_confidence"), factSafeDataConfidence(confidence));
        personBridgeInsert.bindValue(QStringLiteral(":source_url"), csvValueAt(row, personSourceUrlColumn));
        personBridgeInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("Identity pool row imported with INSERT OR IGNORE; existing gameplay seed values are preserved. Source note: ")
                + csvValueAt(row, personNoteColumn));
        if (!personBridgeInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 person bridge 저장 실패: ") + factSafePersonId, personBridgeInsert));
        }
        ++personBridgeRows;
    }
    if (!requireExact(personBridgeRows, kFactSafeRosterPersonBridgeRows, QStringLiteral("person bridge rows"))) {
        return rollbackAndFail(failure);
    }

    QSqlQuery historicalRegistrationInsert(database_);
    historicalRegistrationInsert.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO historical_registrations ("
        "person_id, club_id, snapshot_date, squad_group, registration_status, data_confidence"
        ") VALUES ("
        ":person_id, :club_id, :snapshot_date, :squad_group, :registration_status, :data_confidence"
        ")"));
    QSqlQuery registrationBridgeInsert(database_);
    registrationBridgeInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_roster_registration_bridge ("
        "dataset_id, fact_safe_registration_id, fact_safe_person_id, gameplay_person_id, "
        "fact_safe_club_id, gameplay_club_id, display_name, club_name, snapshot_date, "
        "squad_group_source, squad_group_gameplay, registration_status_source, "
        "registration_status_gameplay, codex_import_policy, operational_status, "
        "activation_status, data_confidence, notes"
        ") VALUES ("
        ":dataset_id, :fact_safe_registration_id, :fact_safe_person_id, :gameplay_person_id, "
        ":fact_safe_club_id, :gameplay_club_id, :display_name, :club_name, :snapshot_date, "
        ":squad_group_source, :squad_group_gameplay, :registration_status_source, "
        ":registration_status_gameplay, :codex_import_policy, :operational_status, "
        ":activation_status, :data_confidence, :notes"
        ")"));
    QSqlQuery loanReviewInsert(database_);
    loanReviewInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_loan_review_queue ("
        "dataset_id, source_file_path, source_row_number, fact_safe_record_id, fact_safe_person_id, "
        "gameplay_person_id, fact_safe_club_id, gameplay_club_id, display_name, review_status, "
        "evidence_summary, notes"
        ") VALUES ("
        ":dataset_id, :source_file_path, :source_row_number, :fact_safe_record_id, :fact_safe_person_id, "
        ":gameplay_person_id, :fact_safe_club_id, :gameplay_club_id, :display_name, :review_status, "
        ":evidence_summary, :notes"
        ")"));

    qint64 registrationBridgeRows = 0;
    qint64 insertedActiveRegistrations = 0;
    qint64 insertedLoanReviewRows = 0;
    for (std::size_t index = 0; index < registrations.rows.size(); ++index) {
        const QStringList& row = registrations.rows.at(index);
        const QString registrationId = csvValueAt(row, registrationIdColumn);
        const QString factSafePersonId = csvValueAt(row, registrationPersonIdColumn);
        const QString gameplayPersonId =
            gameplayPersonIdForFactSafe(factSafePersonId, csvValueAt(row, registrationNameColumn));
        const QString factSafeClubId = csvValueAt(row, registrationClubIdColumn);
        const QString gameplayClubId = activeGameplayClubIdByFactSafeClubId.value(factSafeClubId);
        const QString squadGroupSource = csvValueAt(row, registrationSquadGroupColumn);
        const QString squadGroupGameplay = factSafeSquadGroupForGameplay(squadGroupSource);
        const QString registrationStatusSource = csvValueAt(row, registrationStatusColumn);
        const QString registrationStatusGameplay =
            factSafeRegistrationStatusForGameplay(registrationStatusSource);
        const QString policy = csvValueAt(row, registrationPolicyColumn);
        const QString snapshotDate =
            csvValueAt(row, registrationEffectiveDateColumn).isEmpty()
            ? QStringLiteral("1992-07-01")
            : csvValueAt(row, registrationEffectiveDateColumn);
        const QString confidence = csvValueAt(row, registrationConfidenceColumn);
        const bool canActivate =
            !gameplayClubId.isEmpty()
            && factSafeRegistrationPolicyAllowsActiveImport(policy)
            && factSafeRegistrationStatusIsActive(registrationStatusSource);

        QString activationStatus;
        if (canActivate) {
            activationStatus = QStringLiteral("active_opening_registration_insert_or_preserve_existing");
            historicalRegistrationInsert.bindValue(QStringLiteral(":person_id"), gameplayPersonId);
            historicalRegistrationInsert.bindValue(QStringLiteral(":club_id"), gameplayClubId);
            historicalRegistrationInsert.bindValue(QStringLiteral(":snapshot_date"), snapshotDate);
            historicalRegistrationInsert.bindValue(QStringLiteral(":squad_group"), squadGroupGameplay);
            historicalRegistrationInsert.bindValue(QStringLiteral(":registration_status"), registrationStatusGameplay);
            historicalRegistrationInsert.bindValue(
                QStringLiteral(":data_confidence"),
                factSafeDataConfidence(confidence) + QStringLiteral("_opening_membership"));
            if (!historicalRegistrationInsert.exec()) {
                return rollbackAndFail(
                    queryError(QStringLiteral("v5.4.5 historical registration 저장 실패: ") + registrationId,
                        historicalRegistrationInsert));
            }
            ++insertedActiveRegistrations;
        } else if (gameplayClubId.isEmpty()) {
            activationStatus = QStringLiteral("review_only_no_active_gameplay_club_bridge");
        } else if (!factSafeRegistrationPolicyAllowsActiveImport(policy)) {
            activationStatus = QStringLiteral("review_only_import_policy");
        } else {
            activationStatus = QStringLiteral("review_only_inactive_registration_status");
        }

        registrationBridgeInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        registrationBridgeInsert.bindValue(QStringLiteral(":fact_safe_registration_id"), registrationId);
        registrationBridgeInsert.bindValue(QStringLiteral(":fact_safe_person_id"), factSafePersonId);
        registrationBridgeInsert.bindValue(QStringLiteral(":gameplay_person_id"), gameplayPersonId);
        registrationBridgeInsert.bindValue(QStringLiteral(":fact_safe_club_id"), factSafeClubId);
        registrationBridgeInsert.bindValue(QStringLiteral(":gameplay_club_id"), optionalText(gameplayClubId));
        registrationBridgeInsert.bindValue(QStringLiteral(":display_name"), csvValueAt(row, registrationNameColumn));
        registrationBridgeInsert.bindValue(QStringLiteral(":club_name"), csvValueAt(row, registrationClubNameColumn));
        registrationBridgeInsert.bindValue(QStringLiteral(":snapshot_date"), snapshotDate);
        registrationBridgeInsert.bindValue(QStringLiteral(":squad_group_source"), squadGroupSource);
        registrationBridgeInsert.bindValue(QStringLiteral(":squad_group_gameplay"), squadGroupGameplay);
        registrationBridgeInsert.bindValue(QStringLiteral(":registration_status_source"), registrationStatusSource);
        registrationBridgeInsert.bindValue(QStringLiteral(":registration_status_gameplay"), registrationStatusGameplay);
        registrationBridgeInsert.bindValue(QStringLiteral(":codex_import_policy"), policy);
        registrationBridgeInsert.bindValue(
            QStringLiteral(":operational_status"),
            csvValueAt(row, registrationOperationalStatusColumn));
        registrationBridgeInsert.bindValue(QStringLiteral(":activation_status"), activationStatus);
        registrationBridgeInsert.bindValue(QStringLiteral(":data_confidence"), factSafeDataConfidence(confidence));
        registrationBridgeInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("Opening registration disposition recorded. Source note: ")
                + csvValueAt(row, registrationNoteColumn));
        if (!registrationBridgeInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 registration bridge 저장 실패: ") + registrationId,
                    registrationBridgeInsert));
        }
        ++registrationBridgeRows;

        const QString loanText =
            csvValueAt(row, registrationNoteColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, registrationHistoricalStatusColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, registrationFactStatusColumn);
        if (factSafeLoanReviewMarker(loanText)) {
            loanReviewInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
            loanReviewInsert.bindValue(
                QStringLiteral(":source_file_path"),
                QStringLiteral("core/opening_registrations_corrected_candidate_set.csv"));
            loanReviewInsert.bindValue(
                QStringLiteral(":source_row_number"),
                static_cast<int>(index + 1));
            loanReviewInsert.bindValue(QStringLiteral(":fact_safe_record_id"), registrationId);
            loanReviewInsert.bindValue(QStringLiteral(":fact_safe_person_id"), factSafePersonId);
            loanReviewInsert.bindValue(QStringLiteral(":gameplay_person_id"), gameplayPersonId);
            loanReviewInsert.bindValue(QStringLiteral(":fact_safe_club_id"), factSafeClubId);
            loanReviewInsert.bindValue(QStringLiteral(":gameplay_club_id"), optionalText(gameplayClubId));
            loanReviewInsert.bindValue(QStringLiteral(":display_name"), csvValueAt(row, registrationNameColumn));
            loanReviewInsert.bindValue(
                QStringLiteral(":review_status"),
                QStringLiteral("review_required_not_auto_activated"));
            loanReviewInsert.bindValue(
                QStringLiteral(":evidence_summary"),
                QStringLiteral("opening registration text contains loan marker"));
            loanReviewInsert.bindValue(
                QStringLiteral(":notes"),
                QStringLiteral("Loan-like source text preserved for manual rule-backed modelling; no save_loan_deals row is created."));
            if (!loanReviewInsert.exec()) {
                return rollbackAndFail(
                    queryError(QStringLiteral("v5.4.5 registration loan review 저장 실패: ") + registrationId,
                        loanReviewInsert));
            }
            ++insertedLoanReviewRows;
        }
    }
    if (!requireExact(registrationBridgeRows, kFactSafeRosterRegistrationBridgeRows, QStringLiteral("registration bridge rows"))
        || !requireExact(
            insertedActiveRegistrations,
            kFactSafeRosterActiveRegistrationRows,
            QStringLiteral("inserted active registrations"))) {
        return rollbackAndFail(failure);
    }

    QSqlQuery historicalContractInsert(database_);
    historicalContractInsert.prepare(QStringLiteral(
        "INSERT INTO historical_player_contracts ("
        "person_id, club_id, snapshot_date, contract_start_date, contract_end_date, "
        "weekly_wage_pence, squad_role, data_confidence"
        ") VALUES ("
        ":person_id, :club_id, '1992-07-01', :contract_start_date, :contract_end_date, "
        ":weekly_wage_pence, :squad_role, :data_confidence"
        ") ON CONFLICT(person_id, club_id, snapshot_date) DO UPDATE SET "
        "contract_start_date = COALESCE(historical_player_contracts.contract_start_date, excluded.contract_start_date), "
        "contract_end_date = COALESCE(historical_player_contracts.contract_end_date, excluded.contract_end_date), "
        "weekly_wage_pence = COALESCE(historical_player_contracts.weekly_wage_pence, excluded.weekly_wage_pence), "
        "squad_role = CASE "
        "WHEN historical_player_contracts.squad_role = '' THEN excluded.squad_role "
        "ELSE historical_player_contracts.squad_role END, "
        "data_confidence = CASE "
        "WHEN historical_player_contracts.contract_start_date IS NULL "
        "OR historical_player_contracts.contract_end_date IS NULL "
        "OR historical_player_contracts.weekly_wage_pence IS NULL "
        "THEN historical_player_contracts.data_confidence || '|fact_safe_v545_missing_terms_model_filled' "
        "ELSE historical_player_contracts.data_confidence END"));
    QSqlQuery historicalContractMissingTermsUpdate(database_);
    historicalContractMissingTermsUpdate.prepare(QStringLiteral(
        "UPDATE historical_player_contracts "
        "SET data_confidence = CASE "
        "WHEN contract_start_date IS NULL OR contract_end_date IS NULL OR weekly_wage_pence IS NULL "
        "THEN data_confidence || '|fact_safe_v545_missing_terms_model_filled' "
        "ELSE data_confidence END, "
        "contract_start_date = CASE "
        "WHEN contract_start_date IS NULL THEN :contract_start_date ELSE contract_start_date END, "
        "contract_end_date = CASE "
        "WHEN contract_end_date IS NULL THEN :contract_end_date ELSE contract_end_date END, "
        "weekly_wage_pence = CASE "
        "WHEN weekly_wage_pence IS NULL THEN :weekly_wage_pence ELSE weekly_wage_pence END, "
        "squad_role = CASE WHEN squad_role = '' THEN :squad_role ELSE squad_role END "
        "WHERE person_id = :person_id "
        "AND club_id = :club_id "
        "AND snapshot_date = '1992-07-01'"));
    QSqlQuery contractBridgeInsert(database_);
    contractBridgeInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_contract_bridge ("
        "dataset_id, fact_safe_contract_id, fact_safe_person_id, gameplay_person_id, "
        "fact_safe_club_id, gameplay_club_id, display_name, club_name, registration_scope, "
        "contract_start_date, contract_end_date, weekly_wage_pence, squad_role_source, "
        "squad_role_gameplay, contract_status, codex_import_policy, activation_status, "
        "data_confidence, notes"
        ") VALUES ("
        ":dataset_id, :fact_safe_contract_id, :fact_safe_person_id, :gameplay_person_id, "
        ":fact_safe_club_id, :gameplay_club_id, :display_name, :club_name, :registration_scope, "
        ":contract_start_date, :contract_end_date, :weekly_wage_pence, :squad_role_source, "
        ":squad_role_gameplay, :contract_status, :codex_import_policy, :activation_status, "
        ":data_confidence, :notes"
        ")"));

    qint64 contractBridgeRows = 0;
    qint64 insertedActiveContracts = 0;
    for (std::size_t index = 0; index < contracts.rows.size(); ++index) {
        const QStringList& row = contracts.rows.at(index);
        const QString contractId = csvValueAt(row, contractIdColumn);
        const QString factSafePersonId = csvValueAt(row, contractPersonIdColumn);
        const QString gameplayPersonId =
            gameplayPersonIdForFactSafe(factSafePersonId, csvValueAt(row, contractNameColumn));
        const QString factSafeClubId = csvValueAt(row, contractClubIdColumn);
        const QString gameplayClubId = activeGameplayClubIdByFactSafeClubId.value(factSafeClubId);
        const QString contractStart =
            !csvValueAt(row, contractStartColumn).isEmpty()
            ? csvValueAt(row, contractStartColumn)
            : csvValueAt(row, contractScenarioStartColumn);
        const QString contractEnd = csvValueAt(row, contractEndColumn);
        bool wageOk = false;
        const qint64 weeklyWagePence =
            factSafeGbpToPence(csvValueAt(row, contractWeeklyWageColumn), &wageOk);
        const QString squadRoleSource = csvValueAt(row, contractSquadRoleColumn);
        const QString squadRoleGameplay = factSafeContractRoleForGameplay(squadRoleSource);
        const QString confidence = csvValueAt(row, contractConfidenceColumn);
        const bool canActivate =
            !gameplayClubId.isEmpty()
            && factSafeTrue(csvValueAt(row, contractTermValidColumn))
            && !contractStart.isEmpty()
            && !contractEnd.isEmpty()
            && wageOk;

        QString activationStatus;
        if (canActivate) {
            activationStatus = QStringLiteral("active_contract_model_insert_or_preserve_existing");
            historicalContractInsert.bindValue(QStringLiteral(":person_id"), gameplayPersonId);
            historicalContractInsert.bindValue(QStringLiteral(":club_id"), gameplayClubId);
            historicalContractInsert.bindValue(QStringLiteral(":contract_start_date"), contractStart);
            historicalContractInsert.bindValue(QStringLiteral(":contract_end_date"), contractEnd);
            historicalContractInsert.bindValue(QStringLiteral(":weekly_wage_pence"), weeklyWagePence);
            historicalContractInsert.bindValue(QStringLiteral(":squad_role"), squadRoleGameplay);
            historicalContractInsert.bindValue(
                QStringLiteral(":data_confidence"),
                factSafeDataConfidence(confidence) + QStringLiteral("_contract_terms_model_not_historical_payroll_fact"));
            if (!historicalContractInsert.exec()) {
                return rollbackAndFail(
                    queryError(QStringLiteral("v5.4.5 historical contract 저장 실패: ") + contractId,
                        historicalContractInsert));
            }
            historicalContractMissingTermsUpdate.bindValue(QStringLiteral(":contract_start_date"), contractStart);
            historicalContractMissingTermsUpdate.bindValue(QStringLiteral(":contract_end_date"), contractEnd);
            historicalContractMissingTermsUpdate.bindValue(QStringLiteral(":weekly_wage_pence"), weeklyWagePence);
            historicalContractMissingTermsUpdate.bindValue(QStringLiteral(":squad_role"), squadRoleGameplay);
            historicalContractMissingTermsUpdate.bindValue(QStringLiteral(":person_id"), gameplayPersonId);
            historicalContractMissingTermsUpdate.bindValue(QStringLiteral(":club_id"), gameplayClubId);
            if (!historicalContractMissingTermsUpdate.exec()) {
                return rollbackAndFail(
                    queryError(QStringLiteral("v5.4.5 historical contract NULL terms 보정 실패: ") + contractId,
                        historicalContractMissingTermsUpdate));
            }
            ++insertedActiveContracts;
        } else if (gameplayClubId.isEmpty()) {
            activationStatus = QStringLiteral("review_only_no_active_gameplay_club_bridge");
        } else if (!factSafeTrue(csvValueAt(row, contractTermValidColumn))) {
            activationStatus = QStringLiteral("review_only_term_not_valid_at_snapshot");
        } else {
            activationStatus = QStringLiteral("review_only_missing_contract_terms");
        }

        contractBridgeInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        contractBridgeInsert.bindValue(QStringLiteral(":fact_safe_contract_id"), contractId);
        contractBridgeInsert.bindValue(QStringLiteral(":fact_safe_person_id"), factSafePersonId);
        contractBridgeInsert.bindValue(QStringLiteral(":gameplay_person_id"), gameplayPersonId);
        contractBridgeInsert.bindValue(QStringLiteral(":fact_safe_club_id"), factSafeClubId);
        contractBridgeInsert.bindValue(QStringLiteral(":gameplay_club_id"), optionalText(gameplayClubId));
        contractBridgeInsert.bindValue(QStringLiteral(":display_name"), csvValueAt(row, contractNameColumn));
        contractBridgeInsert.bindValue(QStringLiteral(":club_name"), csvValueAt(row, contractClubNameColumn));
        contractBridgeInsert.bindValue(
            QStringLiteral(":registration_scope"),
            csvValueAt(row, contractRegistrationScopeColumn));
        contractBridgeInsert.bindValue(QStringLiteral(":contract_start_date"), contractStart);
        contractBridgeInsert.bindValue(QStringLiteral(":contract_end_date"), contractEnd);
        contractBridgeInsert.bindValue(QStringLiteral(":weekly_wage_pence"), wageOk ? weeklyWagePence : 0);
        contractBridgeInsert.bindValue(QStringLiteral(":squad_role_source"), squadRoleSource);
        contractBridgeInsert.bindValue(QStringLiteral(":squad_role_gameplay"), squadRoleGameplay);
        contractBridgeInsert.bindValue(QStringLiteral(":contract_status"), csvValueAt(row, contractStatusColumn));
        contractBridgeInsert.bindValue(QStringLiteral(":codex_import_policy"), csvValueAt(row, contractPolicyColumn));
        contractBridgeInsert.bindValue(QStringLiteral(":activation_status"), activationStatus);
        contractBridgeInsert.bindValue(QStringLiteral(":data_confidence"), factSafeDataConfidence(confidence));
        contractBridgeInsert.bindValue(
            QStringLiteral(":notes"),
            QStringLiteral("Opening relationship disposition recorded. Source note: ")
                + csvValueAt(row, contractNoteColumn));
        if (!contractBridgeInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 contract bridge 저장 실패: ") + contractId, contractBridgeInsert));
        }
        ++contractBridgeRows;

        const QString loanText =
            csvValueAt(row, contractNoteColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, contractRelationshipTypeColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, contractRelationshipSemanticsColumn)
            + QLatin1Char(' ')
            + csvValueAt(row, contractStatusColumn);
        if (factSafeLoanReviewMarker(loanText)) {
            loanReviewInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
            loanReviewInsert.bindValue(
                QStringLiteral(":source_file_path"),
                QStringLiteral("gameplay_model/contracts_and_academy_terms_model.csv"));
            loanReviewInsert.bindValue(
                QStringLiteral(":source_row_number"),
                static_cast<int>(index + 1));
            loanReviewInsert.bindValue(QStringLiteral(":fact_safe_record_id"), contractId);
            loanReviewInsert.bindValue(QStringLiteral(":fact_safe_person_id"), factSafePersonId);
            loanReviewInsert.bindValue(QStringLiteral(":gameplay_person_id"), gameplayPersonId);
            loanReviewInsert.bindValue(QStringLiteral(":fact_safe_club_id"), factSafeClubId);
            loanReviewInsert.bindValue(QStringLiteral(":gameplay_club_id"), optionalText(gameplayClubId));
            loanReviewInsert.bindValue(QStringLiteral(":display_name"), csvValueAt(row, contractNameColumn));
            loanReviewInsert.bindValue(
                QStringLiteral(":review_status"),
                QStringLiteral("review_required_not_auto_activated"));
            loanReviewInsert.bindValue(
                QStringLiteral(":evidence_summary"),
                QStringLiteral("opening relationship text contains loan marker"));
            loanReviewInsert.bindValue(
                QStringLiteral(":notes"),
                QStringLiteral("Loan-like source text preserved for manual rule-backed modelling; no save_loan_deals row is created."));
            if (!loanReviewInsert.exec()) {
                return rollbackAndFail(
                    queryError(QStringLiteral("v5.4.5 contract loan review 저장 실패: ") + contractId,
                        loanReviewInsert));
            }
            ++insertedLoanReviewRows;
        }
    }
    if (!requireExact(contractBridgeRows, kFactSafeRosterContractBridgeRows, QStringLiteral("contract bridge rows"))
        || !requireExact(
            insertedActiveContracts,
            kFactSafeRosterActiveContractRows,
            QStringLiteral("inserted active contracts"))
        || !requireExact(insertedLoanReviewRows, kFactSafeRosterLoanReviewRows, QStringLiteral("inserted loan review rows"))) {
        return rollbackAndFail(failure);
    }

    if (!database_.commit()) {
        (void)database_.rollback();
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 roster bridge 커밋 실패: ") + database_.lastError().text());
    }

    return true;
}

bool SqliteGameRepository::importFactSafeClubFinanceModelBridgeV545(QString* errorMessage)
{
    const QString datasetId = QString::fromLatin1(kFactSafeClubFinanceBridgeDatasetId);
    const QString version = QString::fromLatin1(kFactSafeHandoffVersion);
    const QString sourceDatasetId = QString::fromLatin1(kFactSafeHandoffDatasetId);
    const QString sourceFilePath = QStringLiteral("core/clubs_corrected_candidates_and_model_fields.csv");

    QSqlQuery existing(database_);
    existing.prepare(QStringLiteral(
        "SELECT status, club_model_rows, playable_model_rows, active_finance_rows, "
        "historical_snapshot_rows, active_gameplay_estimate_rows, historical_fact_rows "
        "FROM fact_safe_club_finance_bridge_imports "
        "WHERE dataset_id = :dataset_id AND version = :version"));
    existing.bindValue(QStringLiteral(":dataset_id"), datasetId);
    existing.bindValue(QStringLiteral(":version"), version);
    if (!existing.exec()) {
        return fail(
            errorMessage,
            queryError(QStringLiteral("v5.4.5 club finance bridge import 이력 조회 실패"), existing));
    }
    if (existing.next()
        && existing.value(0).toString() == QStringLiteral("imported")
        && existing.value(1).toLongLong() == kFactSafeClubFinanceBridgeRows
        && existing.value(2).toLongLong() == kFactSafePlayableClubs
        && existing.value(3).toLongLong() == kFactSafeClubFinanceActiveRows
        && existing.value(4).toLongLong() == kFactSafeClubFinanceActiveRows
        && existing.value(5).toLongLong() == kFactSafeClubFinanceActiveRows
        && existing.value(6).toLongLong() == kFactSafeClubFinanceHistoricalFactRows) {
        existing.finish();

        const auto expectExistingScalar = [&](const QString& sql, const qint64 expected) -> bool {
            QSqlQuery query(database_);
            query.prepare(sql);
            query.bindValue(QStringLiteral(":dataset_id"), datasetId);
            if (!query.exec() || !query.next()) {
                return false;
            }
            const bool matched = query.value(0).toLongLong() == expected;
            query.finish();
            return matched;
        };

        if (expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge "
                    "WHERE dataset_id = :dataset_id"),
                kFactSafeClubFinanceBridgeRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) FROM fact_safe_club_finance_bridge "
                    "WHERE dataset_id = :dataset_id "
                    "AND activation_status = 'activated_model_snapshot'"),
                kFactSafeClubFinanceActiveRows)
            && expectExistingScalar(
                QStringLiteral(
                    "SELECT COUNT(*) "
                    "FROM fact_safe_club_finance_bridge AS bridge "
                    "JOIN historical_club_snapshots AS snapshot "
                    "ON snapshot.club_id = bridge.gameplay_club_id "
                    "AND snapshot.snapshot_date = '1992-07-01' "
                    "WHERE bridge.dataset_id = :dataset_id "
                    "AND bridge.activation_status = 'activated_model_snapshot'"),
                kFactSafeClubFinanceActiveRows)) {
            return true;
        }
    }
    existing.finish();

    QString failure;
    FactSafeFileCandidate candidate;
    candidate.filePath = sourceFilePath;
    candidate.sourceArea = factSafeSourceArea(sourceFilePath);
    candidate.importRole = QStringLiteral("runtime_club_finance_model_source");
    candidate.runtimeImportAllowed = true;
    candidate.authoritativeVersion = version;
    candidate.loadOrder = 70;

    ParsedFactSafeFile clubs;
    if (!loadFactSafeCsvFile(candidate, &clubs, &failure)) {
        return fail(errorMessage, failure);
    }

    const auto requireExact = [&](const qint64 actual, const qint64 expected, const QString& label) -> bool {
        if (actual == expected) {
            return true;
        }
        failure = QStringLiteral("v5.4.5 club finance bridge ")
            + label
            + QStringLiteral(" count mismatch: expected ")
            + QString::number(expected)
            + QStringLiteral(", actual ")
            + QString::number(actual);
        return false;
    };

    if (!requireExact(
            static_cast<qint64>(clubs.rows.size()),
            kFactSafeClubFinanceBridgeRows,
            QStringLiteral("club model rows"))) {
        return fail(errorMessage, failure);
    }

    const int clubIdColumn = csvColumnIndex(clubs.headers, QStringLiteral("club_id"));
    const int clubNameColumn = csvColumnIndex(clubs.headers, QStringLiteral("club_name"));
    const int competitionNameColumn = csvColumnIndex(clubs.headers, QStringLiteral("competition_name"));
    const int playableStatusColumn = csvColumnIndex(clubs.headers, QStringLiteral("playable_status"));
    const int cashColumn = csvColumnIndex(clubs.headers, QStringLiteral("cash_gbp"));
    const int reputationColumn = csvColumnIndex(clubs.headers, QStringLiteral("reputation"));
    const int trainingLevelColumn = csvColumnIndex(clubs.headers, QStringLiteral("training_level"));
    const int confidenceColumn = csvColumnIndex(clubs.headers, QStringLiteral("confidence"));
    const int sourceUrlColumn = csvColumnIndex(clubs.headers, QStringLiteral("source_url"));
    const int runtimeScopeColumn = csvColumnIndex(clubs.headers, QStringLiteral("runtime_scope"));
    const int financeStatusColumn = csvColumnIndex(clubs.headers, QStringLiteral("finance_data_status"));
    const int cashFactStatusColumn = csvColumnIndex(clubs.headers, QStringLiteral("cash_gbp_fact_status"));
    const int historicalFinanceSourceColumn =
        csvColumnIndex(clubs.headers, QStringLiteral("historical_finance_source_url"));
    const int financeFallbackPolicyColumn =
        csvColumnIndex(clubs.headers, QStringLiteral("finance_model_fallback_policy"));

    if (clubIdColumn < 0
        || clubNameColumn < 0
        || competitionNameColumn < 0
        || playableStatusColumn < 0
        || cashColumn < 0
        || reputationColumn < 0
        || trainingLevelColumn < 0
        || confidenceColumn < 0
        || sourceUrlColumn < 0
        || runtimeScopeColumn < 0
        || financeStatusColumn < 0
        || cashFactStatusColumn < 0
        || historicalFinanceSourceColumn < 0
        || financeFallbackPolicyColumn < 0) {
        return fail(errorMessage, QStringLiteral("v5.4.5 club finance bridge 필수 CSV 컬럼이 없습니다."));
    }

    QHash<QString, QString> activeGameplayClubIdByFactSafeClubId;
    QSqlQuery clubBridgeQuery(database_);
    if (!clubBridgeQuery.exec(QStringLiteral(
            "SELECT locked_club_id, gameplay_club_id "
            "FROM world_club_id_bridge "
            "WHERE playable_status = 'playable' "
            "AND runtime_scope = 'active_simulation'"))) {
        return fail(
            errorMessage,
            queryError(QStringLiteral("v5.4.5 club finance active club bridge 조회 실패"), clubBridgeQuery));
    }
    while (clubBridgeQuery.next()) {
        activeGameplayClubIdByFactSafeClubId.insert(
            clubBridgeQuery.value(0).toString(),
            clubBridgeQuery.value(1).toString());
    }
    clubBridgeQuery.finish();

    QHash<QString, qint64> wageCommitmentByClubId;
    QSqlQuery wageCommitmentQuery(database_);
    if (!wageCommitmentQuery.exec(QStringLiteral(
            "SELECT club_id, COALESCE(SUM(weekly_wage_pence), 0) "
            "FROM historical_player_contracts "
            "WHERE snapshot_date = '1992-07-01' "
            "AND weekly_wage_pence IS NOT NULL "
            "GROUP BY club_id"))) {
        return fail(
            errorMessage,
            queryError(QStringLiteral("v5.4.5 club finance 주급 커밋 조회 실패"), wageCommitmentQuery));
    }
    while (wageCommitmentQuery.next()) {
        wageCommitmentByClubId.insert(
            wageCommitmentQuery.value(0).toString(),
            wageCommitmentQuery.value(1).toLongLong());
    }
    wageCommitmentQuery.finish();

    QHash<QString, int> existingSnapshotReputationByClubId;
    QHash<QString, int> existingSnapshotStrengthByClubId;
    QSqlQuery snapshotQuery(database_);
    if (!snapshotQuery.exec(QStringLiteral(
            "SELECT club_id, reputation, squad_strength "
            "FROM historical_club_snapshots "
            "WHERE snapshot_date = '1992-07-01'"))) {
        return fail(
            errorMessage,
            queryError(QStringLiteral("v5.4.5 club finance 기존 스냅샷 조회 실패"), snapshotQuery));
    }
    while (snapshotQuery.next()) {
        existingSnapshotReputationByClubId.insert(
            snapshotQuery.value(0).toString(),
            snapshotQuery.value(1).toInt());
        existingSnapshotStrengthByClubId.insert(
            snapshotQuery.value(0).toString(),
            snapshotQuery.value(2).toInt());
    }
    snapshotQuery.finish();

    qint64 playableModelRows = 0;
    qint64 activeFinanceRows = 0;
    qint64 activeGameplayEstimateRows = 0;
    qint64 historicalFactRows = 0;
    for (const QStringList& row : clubs.rows) {
        const QString playableStatus = csvValueAt(row, playableStatusColumn);
        const QString runtimeScope = csvValueAt(row, runtimeScopeColumn);
        const QString factSafeClubId = csvValueAt(row, clubIdColumn);
        const QString gameplayClubId = activeGameplayClubIdByFactSafeClubId.value(factSafeClubId);
        const bool playable = playableStatus == QStringLiteral("playable");
        const bool activeScope = runtimeScope == QStringLiteral("active_simulation");
        bool cashOk = false;
        (void)factSafeGbpToPence(csvValueAt(row, cashColumn), &cashOk);
        if (playable) {
            ++playableModelRows;
        }
        if (playable && activeScope && !gameplayClubId.isEmpty() && cashOk) {
            ++activeFinanceRows;
            if (csvValueAt(row, financeStatusColumn) == QStringLiteral("gameplay_estimate")
                && csvValueAt(row, cashFactStatusColumn)
                    == QStringLiteral("gameplay_model_no_club_specific_1992_cash_statement_loaded")) {
                ++activeGameplayEstimateRows;
            }
            if (!csvValueAt(row, historicalFinanceSourceColumn).isEmpty()
                && !csvValueAt(row, cashFactStatusColumn).startsWith(QStringLiteral("gameplay_model"))) {
                ++historicalFactRows;
            }
        }
    }
    if (!requireExact(playableModelRows, kFactSafePlayableClubs, QStringLiteral("playable model rows"))
        || !requireExact(activeFinanceRows, kFactSafeClubFinanceActiveRows, QStringLiteral("active finance rows"))
        || !requireExact(
            activeGameplayEstimateRows,
            kFactSafeClubFinanceActiveRows,
            QStringLiteral("active gameplay estimate rows"))
        || !requireExact(
            historicalFactRows,
            kFactSafeClubFinanceHistoricalFactRows,
            QStringLiteral("historical fact rows"))) {
        return fail(errorMessage, failure);
    }

    const auto rollbackAndFail = [&](const QString& message) -> bool {
        (void)database_.rollback();
        return fail(errorMessage, message);
    };
    const auto optionalText = [](const QString& value) -> QVariant {
        return value.isEmpty() ? QVariant {} : QVariant {value};
    };

    if (!database_.transaction()) {
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 club finance bridge 트랜잭션 시작 실패: ") + database_.lastError().text());
    }

    QSqlQuery deleteExisting(database_);
    deleteExisting.prepare(QStringLiteral(
        "DELETE FROM fact_safe_club_finance_bridge_imports WHERE dataset_id = :dataset_id"));
    deleteExisting.bindValue(QStringLiteral(":dataset_id"), datasetId);
    if (!deleteExisting.exec()) {
        return rollbackAndFail(
            queryError(QStringLiteral("v5.4.5 club finance bridge 기존 메타 제거 실패"), deleteExisting));
    }
    deleteExisting.finish();

    QSqlQuery import(database_);
    import.prepare(QStringLiteral(
        "INSERT INTO fact_safe_club_finance_bridge_imports ("
        "dataset_id, version, source_dataset_id, source_file_path, imported_at, status, "
        "club_model_rows, playable_model_rows, active_finance_rows, historical_snapshot_rows, "
        "active_gameplay_estimate_rows, historical_fact_rows, notes"
        ") VALUES ("
        ":dataset_id, :version, :source_dataset_id, :source_file_path, CURRENT_TIMESTAMP, :status, "
        ":club_model_rows, :playable_model_rows, :active_finance_rows, :historical_snapshot_rows, "
        ":active_gameplay_estimate_rows, :historical_fact_rows, :notes"
        ")"));
    import.bindValue(QStringLiteral(":dataset_id"), datasetId);
    import.bindValue(QStringLiteral(":version"), version);
    import.bindValue(QStringLiteral(":source_dataset_id"), sourceDatasetId);
    import.bindValue(QStringLiteral(":source_file_path"), sourceFilePath);
    import.bindValue(QStringLiteral(":status"), QStringLiteral("imported"));
    import.bindValue(QStringLiteral(":club_model_rows"), kFactSafeClubFinanceBridgeRows);
    import.bindValue(QStringLiteral(":playable_model_rows"), playableModelRows);
    import.bindValue(QStringLiteral(":active_finance_rows"), activeFinanceRows);
    import.bindValue(QStringLiteral(":historical_snapshot_rows"), activeFinanceRows);
    import.bindValue(QStringLiteral(":active_gameplay_estimate_rows"), activeGameplayEstimateRows);
    import.bindValue(QStringLiteral(":historical_fact_rows"), historicalFactRows);
    import.bindValue(
        QStringLiteral(":notes"),
        QStringLiteral(
            "v5.4.5 club cash_gbp fields are gameplay finance estimates, not audited 1992 club accounts. "
            "Playable active clubs are bridged into historical_club_snapshots with save_club_deltas still taking precedence."));
    if (!import.exec()) {
        return rollbackAndFail(queryError(QStringLiteral("v5.4.5 club finance bridge import 메타 저장 실패"), import));
    }
    import.finish();

    QSqlQuery bridgeInsert(database_);
    bridgeInsert.prepare(QStringLiteral(
        "INSERT INTO fact_safe_club_finance_bridge ("
        "dataset_id, fact_safe_club_id, gameplay_club_id, club_name, competition_name, "
        "playable_status, runtime_scope, cash_gbp, cash_pence, transfer_budget_pence, "
        "weekly_wage_budget_pence, weekly_wage_commitment_pence, debt_pence, "
        "reputation_score, training_level, finance_data_status, cash_gbp_fact_status, "
        "historical_finance_source_url, finance_model_fallback_policy, activation_status, "
        "skip_reason, data_confidence, source_url, notes"
        ") VALUES ("
        ":dataset_id, :fact_safe_club_id, :gameplay_club_id, :club_name, :competition_name, "
        ":playable_status, :runtime_scope, :cash_gbp, :cash_pence, :transfer_budget_pence, "
        ":weekly_wage_budget_pence, :weekly_wage_commitment_pence, :debt_pence, "
        ":reputation_score, :training_level, :finance_data_status, :cash_gbp_fact_status, "
        ":historical_finance_source_url, :finance_model_fallback_policy, :activation_status, "
        ":skip_reason, :data_confidence, :source_url, :notes"
        ")"));

    QSqlQuery snapshotUpsert(database_);
    snapshotUpsert.prepare(QStringLiteral(
        "INSERT INTO historical_club_snapshots ("
        "club_id, snapshot_date, cash_pence, transfer_budget_pence, weekly_wage_budget_pence, "
        "weekly_wage_commitment_pence, debt_pence, reputation, squad_strength, data_confidence, source_note"
        ") VALUES ("
        ":club_id, '1992-07-01', :cash_pence, :transfer_budget_pence, :weekly_wage_budget_pence, "
        ":weekly_wage_commitment_pence, :debt_pence, :reputation, :squad_strength, :data_confidence, :source_note"
        ") ON CONFLICT(club_id, snapshot_date) DO UPDATE SET "
        "cash_pence = excluded.cash_pence, "
        "transfer_budget_pence = excluded.transfer_budget_pence, "
        "weekly_wage_budget_pence = excluded.weekly_wage_budget_pence, "
        "weekly_wage_commitment_pence = excluded.weekly_wage_commitment_pence, "
        "debt_pence = excluded.debt_pence, "
        "data_confidence = excluded.data_confidence, "
        "source_note = excluded.source_note"));

    qint64 bridgeRows = 0;
    qint64 activatedSnapshots = 0;
    for (const QStringList& row : clubs.rows) {
        const QString factSafeClubId = csvValueAt(row, clubIdColumn);
        const QString gameplayClubId = activeGameplayClubIdByFactSafeClubId.value(factSafeClubId);
        const QString clubName = csvValueAt(row, clubNameColumn);
        const QString competitionName = csvValueAt(row, competitionNameColumn);
        const QString playableStatus = csvValueAt(row, playableStatusColumn);
        const QString runtimeScope = csvValueAt(row, runtimeScopeColumn);
        const QString financeStatus = csvValueAt(row, financeStatusColumn);
        const QString cashFactStatus = csvValueAt(row, cashFactStatusColumn);
        const QString historicalFinanceSourceUrl = csvValueAt(row, historicalFinanceSourceColumn);
        const QString fallbackPolicy = csvValueAt(row, financeFallbackPolicyColumn);
        const QString confidence = csvValueAt(row, confidenceColumn);
        const QString dataConfidence =
            factSafeDataConfidence(confidence)
            + QStringLiteral("_club_finance_model_not_historical_accounts");
        bool cashOk = false;
        const qint64 cashPence = factSafeGbpToPence(csvValueAt(row, cashColumn), &cashOk);
        bool trainingOk = false;
        const int trainingLevel = csvValueAt(row, trainingLevelColumn).toInt(&trainingOk);
        const qint64 transferBudgetPence = cashOk ? cashPence / 2 : 0;
        const qint64 weeklyWageCommitmentPence = gameplayClubId.isEmpty()
            ? 0
            : wageCommitmentByClubId.value(gameplayClubId, 0);
        const qint64 weeklyWageBudgetPence = cashOk
            ? std::max(weeklyWageCommitmentPence * 125 / 100, cashPence / 250)
            : 0;
        const qint64 debtPence = 0;
        const int reputationScore = strengthFromReputation(csvValueAt(row, reputationColumn));
        const bool playable = playableStatus == QStringLiteral("playable");
        const bool activeScope = runtimeScope == QStringLiteral("active_simulation");
        const bool canActivate = playable && activeScope && !gameplayClubId.isEmpty() && cashOk;

        QString activationStatus;
        QString skipReason;
        if (canActivate) {
            activationStatus = QStringLiteral("activated_model_snapshot");
            skipReason = QStringLiteral("none");
        } else if (!playable || !activeScope) {
            activationStatus = QStringLiteral("reference_only");
            skipReason = QStringLiteral("not_active_playable_runtime_scope");
        } else if (gameplayClubId.isEmpty()) {
            activationStatus = QStringLiteral("skipped_unmapped");
            skipReason = QStringLiteral("missing_world_club_id_bridge_mapping");
        } else {
            activationStatus = QStringLiteral("skipped_invalid_cash_model");
            skipReason = QStringLiteral("cash_gbp_missing_or_invalid");
        }

        const QString note =
            QStringLiteral("cash_gbp is imported as a gameplay estimate, not audited 1992 accounts. cash_fact_status=")
            + cashFactStatus
            + QStringLiteral("; historical_finance_source_url=")
            + historicalFinanceSourceUrl
            + QStringLiteral("; fallback_policy=")
            + fallbackPolicy;

        if (canActivate) {
            const int snapshotReputation =
                existingSnapshotReputationByClubId.value(gameplayClubId, reputationScore);
            const int snapshotStrength =
                existingSnapshotStrengthByClubId.value(gameplayClubId, reputationScore);
            snapshotUpsert.bindValue(QStringLiteral(":club_id"), gameplayClubId);
            snapshotUpsert.bindValue(QStringLiteral(":cash_pence"), cashPence);
            snapshotUpsert.bindValue(QStringLiteral(":transfer_budget_pence"), transferBudgetPence);
            snapshotUpsert.bindValue(QStringLiteral(":weekly_wage_budget_pence"), weeklyWageBudgetPence);
            snapshotUpsert.bindValue(QStringLiteral(":weekly_wage_commitment_pence"), weeklyWageCommitmentPence);
            snapshotUpsert.bindValue(QStringLiteral(":debt_pence"), debtPence);
            snapshotUpsert.bindValue(QStringLiteral(":reputation"), snapshotReputation);
            snapshotUpsert.bindValue(QStringLiteral(":squad_strength"), snapshotStrength);
            snapshotUpsert.bindValue(QStringLiteral(":data_confidence"), dataConfidence);
            snapshotUpsert.bindValue(QStringLiteral(":source_note"), note);
            if (!snapshotUpsert.exec()) {
                return rollbackAndFail(
                    queryError(QStringLiteral("v5.4.5 club finance snapshot 저장 실패: ") + gameplayClubId,
                        snapshotUpsert));
            }
            ++activatedSnapshots;
        }

        bridgeInsert.bindValue(QStringLiteral(":dataset_id"), datasetId);
        bridgeInsert.bindValue(QStringLiteral(":fact_safe_club_id"), factSafeClubId);
        bridgeInsert.bindValue(QStringLiteral(":gameplay_club_id"), optionalText(gameplayClubId));
        bridgeInsert.bindValue(QStringLiteral(":club_name"), clubName);
        bridgeInsert.bindValue(QStringLiteral(":competition_name"), competitionName);
        bridgeInsert.bindValue(QStringLiteral(":playable_status"), playableStatus);
        bridgeInsert.bindValue(QStringLiteral(":runtime_scope"), runtimeScope);
        bridgeInsert.bindValue(QStringLiteral(":cash_gbp"), csvValueAt(row, cashColumn));
        bridgeInsert.bindValue(QStringLiteral(":cash_pence"), cashOk ? cashPence : 0);
        bridgeInsert.bindValue(QStringLiteral(":transfer_budget_pence"), transferBudgetPence);
        bridgeInsert.bindValue(QStringLiteral(":weekly_wage_budget_pence"), weeklyWageBudgetPence);
        bridgeInsert.bindValue(QStringLiteral(":weekly_wage_commitment_pence"), weeklyWageCommitmentPence);
        bridgeInsert.bindValue(QStringLiteral(":debt_pence"), debtPence);
        bridgeInsert.bindValue(QStringLiteral(":reputation_score"), reputationScore);
        bridgeInsert.bindValue(QStringLiteral(":training_level"), trainingOk ? trainingLevel : 0);
        bridgeInsert.bindValue(QStringLiteral(":finance_data_status"), financeStatus);
        bridgeInsert.bindValue(QStringLiteral(":cash_gbp_fact_status"), cashFactStatus);
        bridgeInsert.bindValue(QStringLiteral(":historical_finance_source_url"), historicalFinanceSourceUrl);
        bridgeInsert.bindValue(QStringLiteral(":finance_model_fallback_policy"), fallbackPolicy);
        bridgeInsert.bindValue(QStringLiteral(":activation_status"), activationStatus);
        bridgeInsert.bindValue(QStringLiteral(":skip_reason"), skipReason);
        bridgeInsert.bindValue(QStringLiteral(":data_confidence"), dataConfidence);
        bridgeInsert.bindValue(QStringLiteral(":source_url"), csvValueAt(row, sourceUrlColumn));
        bridgeInsert.bindValue(QStringLiteral(":notes"), note);
        if (!bridgeInsert.exec()) {
            return rollbackAndFail(
                queryError(QStringLiteral("v5.4.5 club finance bridge 저장 실패: ") + factSafeClubId,
                    bridgeInsert));
        }
        ++bridgeRows;
    }

    if (!requireExact(bridgeRows, kFactSafeClubFinanceBridgeRows, QStringLiteral("bridge rows"))
        || !requireExact(
            activatedSnapshots,
            kFactSafeClubFinanceActiveRows,
            QStringLiteral("activated model snapshots"))) {
        return rollbackAndFail(failure);
    }

    if (!database_.commit()) {
        (void)database_.rollback();
        return fail(
            errorMessage,
            QStringLiteral("v5.4.5 club finance bridge 커밋 실패: ") + database_.lastError().text());
    }

    return true;
}

bool SqliteGameRepository::runResourceScript(const QString& resourcePath, QString* errorMessage)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(errorMessage, QStringLiteral("SQL 리소스를 열 수 없습니다: ") + resourcePath);
    }

    const QString script = QString::fromUtf8(file.readAll());
    QString statement;
    bool insideString = false;

    if (!database_.transaction()) {
        return fail(errorMessage, QStringLiteral("SQL 스크립트 트랜잭션 시작 실패: ") + database_.lastError().text());
    }

    const auto executeStatement = [&](const QString& sql) -> bool {
        const QString trimmed = sql.trimmed();
        if (trimmed.isEmpty()) {
            return true;
        }
        QSqlQuery query(database_);
        if (!query.exec(trimmed)) {
            (void)database_.rollback();
            return fail(errorMessage, QStringLiteral("SQL 스크립트 실행 실패: ") + query.lastError().text()
                + QStringLiteral("\n문장: ") + trimmed.left(240));
        }
        return true;
    };

    for (qsizetype index = 0; index < script.size(); ++index) {
        const QChar character = script.at(index);
        if (character == QLatin1Char('\'')) {
            statement += character;
            if (insideString && index + 1 < script.size() && script.at(index + 1) == QLatin1Char('\'')) {
                statement += script.at(++index);
                continue;
            }
            insideString = !insideString;
            continue;
        }

        if (character == QLatin1Char(';') && !insideString) {
            if (!executeStatement(statement)) {
                return false;
            }
            statement.clear();
            continue;
        }
        statement += character;
    }

    if (!executeStatement(statement)) {
        return false;
    }
    if (!database_.commit()) {
        (void)database_.rollback();
        return fail(errorMessage, QStringLiteral("SQL 스크립트 커밋 실패: ") + database_.lastError().text());
    }
    return true;
}

bool SqliteGameRepository::fail(QString* errorMessage, const QString& message) const
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return false;
}

} // namespace football_owner
