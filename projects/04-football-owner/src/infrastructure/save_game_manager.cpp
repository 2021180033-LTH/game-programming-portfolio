#include "infrastructure/save_game_manager.h"

#include "infrastructure/sqlite_game_repository.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>
#include <QUuid>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace football_owner {
namespace {

constexpr int catalogSchemaVersion = 1;
constexpr int maximumAutosaves = 3;

QString normalizedName(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

QString nowIso()
{
    static qint64 lastTimestampMs = 0;
    const qint64 clockTimestampMs = QDateTime::currentMSecsSinceEpoch();
    lastTimestampMs = std::max(clockTimestampMs, lastTimestampMs + 1);
    return QDateTime::fromMSecsSinceEpoch(lastTimestampMs, Qt::UTC)
        .toString(Qt::ISODateWithMs);
}

QString makeId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString seasonForDate(const QString& isoDate)
{
    const auto parsed = GameDate::fromIsoString(isoDate.toStdString());
    if (!parsed.has_value()) {
        return QStringLiteral("시즌 미상");
    }
    const int startYear = parsed->month >= 7 ? parsed->year : parsed->year - 1;
    const int endYear = (startYear + 1) % 100;
    return QStringLiteral("%1-%2")
        .arg(startYear)
        .arg(endYear, 2, 10, QLatin1Char('0'));
}

QJsonObject descriptorToJson(const SaveGameDescriptor& descriptor)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), descriptor.id);
    object.insert(QStringLiteral("displayName"), descriptor.displayName);
    object.insert(QStringLiteral("clubName"), descriptor.clubName);
    object.insert(QStringLiteral("clubShortName"), descriptor.clubShortName);
    object.insert(QStringLiteral("gameDate"), descriptor.gameDate);
    object.insert(QStringLiteral("seasonLabel"), descriptor.seasonLabel);
    object.insert(QStringLiteral("savedAt"), descriptor.savedAt);
    object.insert(QStringLiteral("fileName"), descriptor.fileName);
    object.insert(QStringLiteral("kind"), descriptor.kind);
    object.insert(QStringLiteral("parentId"), descriptor.parentId);
    object.insert(QStringLiteral("autosaveSlot"), descriptor.autosaveSlot);
    object.insert(QStringLiteral("playTimeSeconds"), descriptor.playTimeSeconds);
    return object;
}

std::optional<SaveGameDescriptor> descriptorFromJson(const QJsonObject& object)
{
    SaveGameDescriptor descriptor;
    descriptor.id = object.value(QStringLiteral("id")).toString();
    descriptor.displayName = object.value(QStringLiteral("displayName")).toString();
    descriptor.clubName = object.value(QStringLiteral("clubName")).toString();
    descriptor.clubShortName = object.value(QStringLiteral("clubShortName")).toString();
    descriptor.gameDate = object.value(QStringLiteral("gameDate")).toString();
    descriptor.seasonLabel = object.value(QStringLiteral("seasonLabel")).toString();
    descriptor.savedAt = object.value(QStringLiteral("savedAt")).toString();
    descriptor.fileName = object.value(QStringLiteral("fileName")).toString();
    descriptor.kind = object.value(QStringLiteral("kind")).toString(QStringLiteral("career"));
    descriptor.parentId = object.value(QStringLiteral("parentId")).toString();
    descriptor.autosaveSlot = object.value(QStringLiteral("autosaveSlot")).toInt();
    descriptor.playTimeSeconds = object.value(QStringLiteral("playTimeSeconds")).toInteger();

    const QFileInfo fileInfo(descriptor.fileName);
    if (descriptor.id.isEmpty()
        || descriptor.displayName.isEmpty()
        || descriptor.fileName.isEmpty()
        || fileInfo.fileName() != descriptor.fileName
        || !descriptor.fileName.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
        return std::nullopt;
    }
    if (descriptor.kind != QStringLiteral("career")
        && descriptor.kind != QStringLiteral("autosave")) {
        return std::nullopt;
    }
    if (descriptor.seasonLabel.isEmpty()) {
        descriptor.seasonLabel = seasonForDate(descriptor.gameDate);
    }
    return descriptor;
}

SaveGameDescriptor descriptorForState(
    const QString& id,
    const QString& fileName,
    const QString& kind,
    const QString& parentId,
    const int autosaveSlot,
    const qint64 playTimeSeconds,
    const QString& savedAt,
    const GameState& state)
{
    SaveGameDescriptor descriptor;
    descriptor.id = id;
    descriptor.displayName = QString::fromStdString(state.saveName);
    descriptor.clubName = QString::fromStdString(state.club.name);
    descriptor.clubShortName = QString::fromStdString(state.club.shortName);
    descriptor.gameDate = QString::fromStdString(state.currentDate.toIsoString());
    descriptor.seasonLabel = seasonForDate(descriptor.gameDate);
    descriptor.savedAt = savedAt;
    descriptor.fileName = fileName;
    descriptor.kind = kind;
    descriptor.parentId = parentId;
    descriptor.autosaveSlot = autosaveSlot;
    descriptor.playTimeSeconds = std::max<qint64>(0, playTimeSeconds);
    return descriptor;
}

bool renameIfPresent(const QString& source, const QString& destination)
{
    if (!QFile::exists(source)) {
        return true;
    }
    return QFile::rename(source, destination);
}

} // namespace

SaveGameManager::SaveGameManager(QString dataDirectory)
    : dataDirectory_(std::move(dataDirectory))
    , savesDirectory_(QDir(dataDirectory_).filePath(QStringLiteral("saves")))
    , catalogPath_(QDir(dataDirectory_).filePath(QStringLiteral("save_catalog.json")))
    , legacyDatabasePath_(QDir(dataDirectory_).filePath(QStringLiteral("football_owner_mvp.db")))
    , legacyMarkerPath_(QDir(dataDirectory_).filePath(QStringLiteral("legacy_0_2_imported.marker")))
{
}

bool SaveGameManager::initialize(QString* errorMessage)
{
    if (initialized_) {
        return true;
    }
    if (!QDir().mkpath(dataDirectory_) || !QDir().mkpath(savesDirectory_)) {
        return fail(errorMessage, QStringLiteral("세이브 폴더를 만들 수 없습니다: ") + savesDirectory_);
    }
    if (!loadCatalog(errorMessage)) {
        return false;
    }

    QString discoveryWarning;
    if (!mergeDiscoveredFiles(&discoveryWarning)) {
        return fail(errorMessage, discoveryWarning);
    }
    if (!importLegacyCareer(errorMessage)) {
        return false;
    }
    if (!writeCatalog(errorMessage)) {
        return false;
    }

    initialized_ = true;
    return true;
}

const std::vector<SaveGameDescriptor>& SaveGameManager::descriptors() const noexcept
{
    return descriptors_;
}

const SaveGameDescriptor* SaveGameManager::descriptor(const QString& id) const noexcept
{
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&id](const SaveGameDescriptor& candidate) {
            return candidate.id == id;
        });
    return found == descriptors_.end() ? nullptr : &*found;
}

SaveGameDescriptor* SaveGameManager::mutableDescriptor(const QString& id) noexcept
{
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&id](const SaveGameDescriptor& candidate) {
            return candidate.id == id;
        });
    return found == descriptors_.end() ? nullptr : &*found;
}

bool SaveGameManager::canContinue() const
{
    return !continueSaveId().isEmpty();
}

QString SaveGameManager::continueSaveId() const
{
    const SaveGameDescriptor* last = descriptor(lastPlayedId_);
    if (last != nullptr && !last->isAutosave()) {
        return last->id;
    }

    const SaveGameDescriptor* newest = nullptr;
    for (const auto& candidate : descriptors_) {
        if (candidate.isAutosave()) {
            continue;
        }
        if (newest == nullptr || candidate.savedAt > newest->savedAt) {
            newest = &candidate;
        }
    }
    return newest != nullptr ? newest->id : QString {};
}

bool SaveGameManager::nameExists(
    const QString& displayName,
    const QString& excludedCareerId) const
{
    const QString sought = normalizedName(displayName);
    if (sought.isEmpty()) {
        return false;
    }
    return std::any_of(
        descriptors_.begin(),
        descriptors_.end(),
        [&sought, &excludedCareerId](const SaveGameDescriptor& candidate) {
            return !candidate.isAutosave()
                && candidate.id != excludedCareerId
                && normalizedName(candidate.displayName) == sought;
        });
}

QString SaveGameManager::saveIdForName(const QString& displayName) const
{
    const QString sought = normalizedName(displayName);
    const auto found = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&sought](const SaveGameDescriptor& candidate) {
            return !candidate.isAutosave()
                && normalizedName(candidate.displayName) == sought;
        });
    return found != descriptors_.end() ? found->id : QString {};
}

bool SaveGameManager::legacyImportedThisRun() const noexcept
{
    return legacyImportedThisRun_;
}

std::optional<GameState> SaveGameManager::createCareer(
    const QString& displayName,
    const QString& ownerName,
    QString* createdCareerId,
    QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return std::nullopt;
    }

    const QString trimmedName = displayName.trimmed();
    if (trimmedName.isEmpty()) {
        (void)fail(errorMessage, QStringLiteral("세이브 이름을 입력하십시오."));
        return std::nullopt;
    }
    if (nameExists(trimmedName)) {
        (void)fail(errorMessage, QStringLiteral("같은 이름의 세이브가 이미 있습니다."));
        return std::nullopt;
    }

    GameState state = makeManchesterUnitedPrototype();
    state.saveName = trimmedName.toUtf8().toStdString();
    const QString trimmedOwner = ownerName.trimmed();
    if (!trimmedOwner.isEmpty()) {
        state.ownerName = trimmedOwner.toUtf8().toStdString();
    }

    const QString id = makeId();
    const QString fileName = QStringLiteral("career_%1.db").arg(id);
    SaveGameDescriptor newDescriptor = descriptorForState(
        id,
        fileName,
        QStringLiteral("career"),
        {},
        0,
        0,
        nowIso(),
        state);

    const QString targetPath = databasePath(newDescriptor);
    if (!writeStateDatabase(targetPath, state, newDescriptor, errorMessage)) {
        return std::nullopt;
    }

    const QString previousLastPlayedId = lastPlayedId_;
    descriptors_.push_back(newDescriptor);
    lastPlayedId_ = id;
    if (!writeCatalog(errorMessage)) {
        descriptors_.pop_back();
        lastPlayedId_ = previousLastPlayedId;
        (void)removeDatabaseFiles(targetPath);
        return std::nullopt;
    }

    const auto loaded = loadStateFile(newDescriptor, errorMessage);
    if (!loaded.has_value()) {
        return std::nullopt;
    }
    if (createdCareerId != nullptr) {
        *createdCareerId = id;
    }
    return loaded;
}

std::optional<GameState> SaveGameManager::loadCareer(
    const QString& saveId,
    QString* activeCareerId,
    qint64* playTimeSeconds,
    bool* restoredFromAutosave,
    QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return std::nullopt;
    }
    const SaveGameDescriptor* selected = descriptor(saveId);
    if (selected == nullptr) {
        (void)fail(errorMessage, QStringLiteral("선택한 세이브를 찾을 수 없습니다."));
        return std::nullopt;
    }

    const SaveGameDescriptor selectedCopy = *selected;
    auto state = loadStateFile(selectedCopy, errorMessage);
    if (!state.has_value()) {
        return std::nullopt;
    }

    QString resolvedCareerId = selectedCopy.id;
    bool fromAutosave = false;
    if (selectedCopy.isAutosave()) {
        const SaveGameDescriptor* parent = descriptor(selectedCopy.parentId);
        if (parent == nullptr || parent->isAutosave()) {
            (void)fail(errorMessage, QStringLiteral("자동 저장의 원본 커리어를 찾을 수 없습니다."));
            return std::nullopt;
        }
        resolvedCareerId = parent->id;
        state->saveName = parent->displayName.toUtf8().toStdString();
        fromAutosave = true;
    }

    lastPlayedId_ = resolvedCareerId;
    if (!writeCatalog(errorMessage)) {
        return std::nullopt;
    }
    if (activeCareerId != nullptr) {
        *activeCareerId = resolvedCareerId;
    }
    if (playTimeSeconds != nullptr) {
        *playTimeSeconds = selectedCopy.playTimeSeconds;
    }
    if (restoredFromAutosave != nullptr) {
        *restoredFromAutosave = fromAutosave;
    }
    return state;
}

bool SaveGameManager::saveCareer(
    const QString& careerId,
    const GameState& state,
    const qint64 playTimeSeconds,
    QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return false;
    }
    SaveGameDescriptor* current = mutableDescriptor(careerId);
    if (current == nullptr || current->isAutosave()) {
        return fail(errorMessage, QStringLiteral("현재 커리어 세이브를 찾을 수 없습니다."));
    }

    SaveGameDescriptor updated = *current;
    updated.clubName = QString::fromStdString(state.club.name);
    updated.clubShortName = QString::fromStdString(state.club.shortName);
    updated.gameDate = QString::fromStdString(state.currentDate.toIsoString());
    updated.seasonLabel = seasonForDate(updated.gameDate);
    updated.savedAt = nowIso();
    updated.playTimeSeconds = std::max<qint64>(0, playTimeSeconds);

    GameState namedState = state;
    namedState.saveName = updated.displayName.toUtf8().toStdString();
    if (!writeStateDatabase(databasePath(updated), namedState, updated, errorMessage)) {
        return false;
    }

    *current = updated;
    lastPlayedId_ = current->id;
    return writeCatalog(errorMessage);
}

bool SaveGameManager::saveAs(
    const QString& sourceCareerId,
    const GameState& state,
    const QString& displayName,
    const QString& overwriteCareerId,
    const qint64 playTimeSeconds,
    QString* resultingCareerId,
    QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return false;
    }

    const QString trimmedName = displayName.trimmed();
    if (trimmedName.isEmpty()) {
        return fail(errorMessage, QStringLiteral("세이브 이름을 입력하십시오."));
    }

    const QString duplicateId = saveIdForName(trimmedName);
    if (overwriteCareerId.isEmpty() && !duplicateId.isEmpty()) {
        return fail(errorMessage, QStringLiteral("같은 이름의 세이브가 이미 있습니다. 덮어쓰기를 확인하십시오."));
    }
    if (!overwriteCareerId.isEmpty() && duplicateId != overwriteCareerId) {
        return fail(errorMessage, QStringLiteral("덮어쓸 세이브 이름이 일치하지 않습니다."));
    }

    const QString savedAt = nowIso();
    GameState namedState = state;
    namedState.saveName = trimmedName.toUtf8().toStdString();

    if (!overwriteCareerId.isEmpty()) {
        SaveGameDescriptor* target = mutableDescriptor(overwriteCareerId);
        if (target == nullptr || target->isAutosave()) {
            return fail(errorMessage, QStringLiteral("덮어쓸 커리어 세이브를 찾을 수 없습니다."));
        }

        SaveGameDescriptor updated = descriptorForState(
            target->id,
            target->fileName,
            QStringLiteral("career"),
            {},
            0,
            playTimeSeconds,
            savedAt,
            namedState);
        if (!writeStateDatabase(databasePath(updated), namedState, updated, errorMessage)) {
            return false;
        }

        *target = updated;
        if (overwriteCareerId != sourceCareerId) {
            removeAutosavesForParent(overwriteCareerId);
        }
        lastPlayedId_ = overwriteCareerId;
        if (!writeCatalog(errorMessage)) {
            return false;
        }
        if (resultingCareerId != nullptr) {
            *resultingCareerId = overwriteCareerId;
        }
        return true;
    }

    const QString newId = makeId();
    const QString fileName = QStringLiteral("career_%1.db").arg(newId);
    SaveGameDescriptor created = descriptorForState(
        newId,
        fileName,
        QStringLiteral("career"),
        {},
        0,
        playTimeSeconds,
        savedAt,
        namedState);
    const QString targetPath = databasePath(created);
    if (!writeStateDatabase(targetPath, namedState, created, errorMessage)) {
        return false;
    }

    descriptors_.push_back(created);
    lastPlayedId_ = newId;
    if (!writeCatalog(errorMessage)) {
        descriptors_.pop_back();
        lastPlayedId_ = sourceCareerId;
        (void)removeDatabaseFiles(targetPath);
        return false;
    }
    if (resultingCareerId != nullptr) {
        *resultingCareerId = newId;
    }
    return true;
}

bool SaveGameManager::renameCareer(
    const QString& careerId,
    const QString& displayName,
    QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return false;
    }
    SaveGameDescriptor* target = mutableDescriptor(careerId);
    if (target == nullptr || target->isAutosave()) {
        return fail(errorMessage, QStringLiteral("이름을 바꿀 커리어 세이브를 찾을 수 없습니다."));
    }

    const QString trimmedName = displayName.trimmed();
    if (trimmedName.isEmpty()) {
        return fail(errorMessage, QStringLiteral("새 세이브 이름을 입력하십시오."));
    }
    if (nameExists(trimmedName, careerId)) {
        return fail(errorMessage, QStringLiteral("같은 이름의 세이브가 이미 있습니다."));
    }

    const SaveGameDescriptor original = *target;
    auto state = loadStateFile(original, errorMessage);
    if (!state.has_value()) {
        return false;
    }
    state->saveName = trimmedName.toUtf8().toStdString();

    SaveGameDescriptor updated = original;
    updated.displayName = trimmedName;
    updated.savedAt = nowIso();
    if (!writeStateDatabase(databasePath(updated), *state, updated, errorMessage)) {
        return false;
    }

    *target = updated;
    for (auto& candidate : descriptors_) {
        if (candidate.isAutosave() && candidate.parentId == careerId) {
            candidate.displayName = QStringLiteral("%1 · 자동 저장 %2")
                .arg(trimmedName)
                .arg(candidate.autosaveSlot);
        }
    }
    return writeCatalog(errorMessage);
}

bool SaveGameManager::deleteCareer(const QString& careerId, QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return false;
    }
    const SaveGameDescriptor* target = descriptor(careerId);
    if (target == nullptr) {
        return fail(errorMessage, QStringLiteral("삭제할 세이브를 찾을 수 없습니다."));
    }

    std::vector<QString> filesToRemove;
    filesToRemove.push_back(databasePath(*target));
    if (!target->isAutosave()) {
        for (const auto& candidate : descriptors_) {
            if (candidate.isAutosave() && candidate.parentId == careerId) {
                filesToRemove.push_back(databasePath(candidate));
            }
        }
    }

    for (const auto& file : filesToRemove) {
        if (!removeDatabaseFiles(file)) {
            return fail(
                errorMessage,
                QStringLiteral("저장 파일을 삭제하지 못했습니다: ") + file);
        }
    }

    const bool targetIsAutosave = target->isAutosave();
    descriptors_.erase(
        std::remove_if(
            descriptors_.begin(),
            descriptors_.end(),
            [&careerId, targetIsAutosave](const SaveGameDescriptor& candidate) {
                return candidate.id == careerId
                    || (!targetIsAutosave
                        && candidate.isAutosave()
                        && candidate.parentId == careerId);
            }),
        descriptors_.end());
    if (lastPlayedId_ == careerId) {
        lastPlayedId_.clear();
        lastPlayedId_ = continueSaveId();
    }
    if (!writeCatalog(errorMessage)) {
        return false;
    }

    return true;
}

bool SaveGameManager::createAutosave(
    const QString& parentCareerId,
    const GameState& state,
    const qint64 playTimeSeconds,
    int* autosaveSlot,
    QString* errorMessage)
{
    if (!initialize(errorMessage)) {
        return false;
    }
    const SaveGameDescriptor* parent = descriptor(parentCareerId);
    if (parent == nullptr || parent->isAutosave()) {
        return fail(errorMessage, QStringLiteral("자동 저장의 원본 커리어를 찾을 수 없습니다."));
    }

    std::vector<SaveGameDescriptor*> existing;
    for (auto& candidate : descriptors_) {
        if (candidate.isAutosave() && candidate.parentId == parentCareerId) {
            existing.push_back(&candidate);
        }
    }

    int slot = 1;
    SaveGameDescriptor* replaceTarget = nullptr;
    if (existing.size() < static_cast<std::size_t>(maximumAutosaves)) {
        for (; slot <= maximumAutosaves; ++slot) {
            const bool used = std::any_of(
                existing.begin(),
                existing.end(),
                [slot](const SaveGameDescriptor* candidate) {
                    return candidate->autosaveSlot == slot;
                });
            if (!used) {
                break;
            }
        }
    } else {
        replaceTarget = *std::min_element(
            existing.begin(),
            existing.end(),
            [](const SaveGameDescriptor* left, const SaveGameDescriptor* right) {
                return left->savedAt < right->savedAt;
            });
        slot = replaceTarget->autosaveSlot;
    }

    const QString id = replaceTarget != nullptr ? replaceTarget->id : makeId();
    const QString fileName = replaceTarget != nullptr
        ? replaceTarget->fileName
        : QStringLiteral("autosave_%1_%2.db").arg(parentCareerId).arg(slot);

    GameState namedState = state;
    namedState.saveName = parent->displayName.toUtf8().toStdString();
    SaveGameDescriptor snapshot = descriptorForState(
        id,
        fileName,
        QStringLiteral("autosave"),
        parentCareerId,
        slot,
        playTimeSeconds,
        nowIso(),
        namedState);
    snapshot.displayName = QStringLiteral("%1 · 자동 저장 %2")
        .arg(parent->displayName)
        .arg(slot);

    if (!writeStateDatabase(databasePath(snapshot), namedState, snapshot, errorMessage)) {
        return false;
    }
    if (replaceTarget != nullptr) {
        *replaceTarget = snapshot;
    } else {
        descriptors_.push_back(snapshot);
    }
    if (!writeCatalog(errorMessage)) {
        return false;
    }
    if (autosaveSlot != nullptr) {
        *autosaveSlot = slot;
    }
    return true;
}

QString SaveGameManager::databasePath(const SaveGameDescriptor& descriptor) const
{
    return QDir(savesDirectory_).filePath(descriptor.fileName);
}

QString SaveGameManager::uniqueDisplayName(const QString& preferred) const
{
    const QString base = preferred.trimmed().isEmpty()
        ? QStringLiteral("기존 세이브 - Manchester United")
        : preferred.trimmed();
    if (!nameExists(base)) {
        return base;
    }
    for (int suffix = 2; suffix < 10'000; ++suffix) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix);
        if (!nameExists(candidate)) {
            return candidate;
        }
    }
    return base + QStringLiteral(" - ") + makeId().left(8);
}

bool SaveGameManager::loadCatalog(QString* errorMessage)
{
    descriptors_.clear();
    lastPlayedId_.clear();
    legacyImported_ = QFile::exists(legacyMarkerPath_);

    QFile file(catalogPath_);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(errorMessage, QStringLiteral("세이브 목록을 열 수 없습니다: ") + file.errorString());
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        file.close();
        const QString backupPath = catalogPath_
            + QStringLiteral(".corrupt-")
            + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz"));
        (void)QFile::copy(catalogPath_, backupPath);
        descriptors_.clear();
        lastPlayedId_.clear();
        return true;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() > catalogSchemaVersion) {
        return fail(errorMessage, QStringLiteral("이 게임 버전보다 새로운 세이브 목록입니다."));
    }

    lastPlayedId_ = root.value(QStringLiteral("lastPlayedId")).toString();
    legacyImported_ = root.value(QStringLiteral("legacyImported")).toBool(legacyImported_);
    const QJsonArray entries = root.value(QStringLiteral("saves")).toArray();
    descriptors_.reserve(static_cast<std::size_t>(entries.size()));
    for (const auto& value : entries) {
        if (!value.isObject()) {
            continue;
        }
        const auto parsed = descriptorFromJson(value.toObject());
        if (parsed.has_value()) {
            descriptors_.push_back(*parsed);
        }
    }
    return true;
}

bool SaveGameManager::writeCatalog(QString* errorMessage) const
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), catalogSchemaVersion);
    root.insert(QStringLiteral("lastPlayedId"), lastPlayedId_);
    root.insert(QStringLiteral("legacyImported"), legacyImported_);

    QJsonArray entries;
    for (const auto& descriptor : descriptors_) {
        entries.push_back(descriptorToJson(descriptor));
    }
    root.insert(QStringLiteral("saves"), entries);

    QSaveFile file(catalogPath_);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(errorMessage, QStringLiteral("세이브 목록 저장을 시작할 수 없습니다: ") + file.errorString());
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return fail(errorMessage, QStringLiteral("세이브 목록을 모두 기록하지 못했습니다."));
    }
    if (!file.commit()) {
        return fail(errorMessage, QStringLiteral("세이브 목록 교체에 실패했습니다: ") + file.errorString());
    }
    return true;
}

bool SaveGameManager::mergeDiscoveredFiles(QString* warningMessage)
{
    descriptors_.erase(
        std::remove_if(
            descriptors_.begin(),
            descriptors_.end(),
            [this](const SaveGameDescriptor& candidate) {
                return !QFile::exists(databasePath(candidate));
            }),
        descriptors_.end());

    const QStringList files = QDir(savesDirectory_).entryList(
        {QStringLiteral("*.db")},
        QDir::Files,
        QDir::Name);
    QStringList skippedFiles;
    for (const QString& fileName : files) {
        const bool alreadyKnown = std::any_of(
            descriptors_.begin(),
            descriptors_.end(),
            [&fileName](const SaveGameDescriptor& candidate) {
                return candidate.fileName == fileName;
            });
        if (alreadyKnown) {
            continue;
        }

        QString loadError;
        SqliteGameRepository repository(QDir(savesDirectory_).filePath(fileName));
        const auto state = repository.loadOrCreateCareer(&loadError);
        const auto metadata = state.has_value()
            ? repository.readFileMetadata(&loadError)
            : std::optional<SaveFileMetadata> {};
        if (!state.has_value() || !metadata.has_value() || metadata->careerId.isEmpty()) {
            skippedFiles.push_back(fileName);
            continue;
        }

        descriptors_.push_back(descriptorForState(
            metadata->careerId,
            fileName,
            metadata->saveKind,
            metadata->parentCareerId,
            metadata->autosaveSlot,
            metadata->playTimeSeconds,
            metadata->savedAt,
            *state));
    }

    for (auto& candidate : descriptors_) {
        if (!candidate.isAutosave()) {
            continue;
        }
        const SaveGameDescriptor* parent = descriptor(candidate.parentId);
        if (parent != nullptr && !parent->isAutosave()) {
            candidate.displayName = QStringLiteral("%1 · 자동 저장 %2")
                .arg(parent->displayName)
                .arg(candidate.autosaveSlot);
        }
    }

    if (!skippedFiles.isEmpty() && warningMessage != nullptr) {
        *warningMessage = QStringLiteral("일부 세이브 파일의 메타데이터를 읽지 못했습니다: ")
            + skippedFiles.join(QStringLiteral(", "));
    }
    return true;
}

bool SaveGameManager::importLegacyCareer(QString* errorMessage)
{
    if (legacyImported_ || QFile::exists(legacyMarkerPath_) || !QFile::exists(legacyDatabasePath_)) {
        legacyImported_ = legacyImported_ || QFile::exists(legacyMarkerPath_);
        return true;
    }

    const QString importCopy = QDir(savesDirectory_).filePath(
        QStringLiteral(".legacy-import-%1.db").arg(makeId()));
    if (!QFile::copy(legacyDatabasePath_, importCopy)) {
        return fail(errorMessage, QStringLiteral("기존 0.2.0 세이브의 안전 복사본을 만들 수 없습니다."));
    }
    for (const QString& suffix : {
             QStringLiteral("-wal"),
             QStringLiteral("-shm"),
             QStringLiteral("-journal")}) {
        const QString legacySidecar = legacyDatabasePath_ + suffix;
        if (QFile::exists(legacySidecar)
            && !QFile::copy(legacySidecar, importCopy + suffix)) {
            (void)removeDatabaseFiles(importCopy);
            return fail(
                errorMessage,
                QStringLiteral("기존 0.2.0 세이브의 SQLite 보조 파일을 복사할 수 없습니다."));
        }
    }

    std::optional<GameState> legacyState;
    {
        SqliteGameRepository repository(importCopy);
        legacyState = repository.loadOrCreateCareer(errorMessage);
        if (legacyState.has_value() && !repository.prepareForFileReplacement(errorMessage)) {
            legacyState.reset();
        }
    }
    (void)removeDatabaseFiles(importCopy);
    if (!legacyState.has_value()) {
        return false;
    }

    legacyState->saveName = uniqueDisplayName(
        QStringLiteral("기존 세이브 - Manchester United")).toUtf8().toStdString();
    const QString id = makeId();
    SaveGameDescriptor imported = descriptorForState(
        id,
        QStringLiteral("career_%1.db").arg(id),
        QStringLiteral("career"),
        {},
        0,
        0,
        nowIso(),
        *legacyState);
    if (!writeStateDatabase(databasePath(imported), *legacyState, imported, errorMessage)) {
        return false;
    }

    const QString previousLastPlayedId = lastPlayedId_;
    descriptors_.push_back(imported);
    lastPlayedId_ = imported.id;
    legacyImported_ = true;
    legacyImportedThisRun_ = true;
    if (!writeLegacyMarker(errorMessage)) {
        descriptors_.pop_back();
        lastPlayedId_ = previousLastPlayedId;
        legacyImported_ = false;
        legacyImportedThisRun_ = false;
        (void)removeDatabaseFiles(databasePath(imported));
        return false;
    }
    return true;
}

bool SaveGameManager::writeLegacyMarker(QString* errorMessage) const
{
    QSaveFile file(legacyMarkerPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail(errorMessage, QStringLiteral("기존 세이브 승계 표시를 기록할 수 없습니다."));
    }
    const QByteArray text("Football Owner 0.2.0 legacy save imported without removing the source.\n");
    if (file.write(text) != text.size() || !file.commit()) {
        return fail(errorMessage, QStringLiteral("기존 세이브 승계 표시 저장에 실패했습니다."));
    }
    return true;
}

bool SaveGameManager::writeStateDatabase(
    const QString& targetPath,
    const GameState& state,
    const SaveGameDescriptor& descriptor,
    QString* errorMessage) const
{
    const QString preparedPath = targetPath + QStringLiteral(".tmp-") + makeId();
    (void)removeDatabaseFiles(preparedPath);

    bool prepared = false;
    {
        SqliteGameRepository repository(preparedPath);
        const auto baseState = repository.loadOrCreateCareer(errorMessage);
        if (baseState.has_value()) {
            GameState stateToWrite = state;
            stateToWrite.saveId = baseState->saveId;
            stateToWrite.saveName = descriptor.isAutosave()
                ? state.saveName
                : descriptor.displayName.toUtf8().toStdString();

            const SaveFileMetadata metadata {
                descriptor.id,
                descriptor.kind,
                descriptor.parentId,
                descriptor.autosaveSlot,
                descriptor.playTimeSeconds,
                descriptor.savedAt
            };
            prepared = repository.save(stateToWrite, errorMessage)
                && repository.writeFileMetadata(stateToWrite.saveId, metadata, errorMessage)
                && repository.prepareForFileReplacement(errorMessage);
        }
    }

    if (!prepared) {
        (void)removeDatabaseFiles(preparedPath);
        return false;
    }
    if (!replacePreparedDatabase(preparedPath, targetPath, errorMessage)) {
        (void)removeDatabaseFiles(preparedPath);
        return false;
    }
    return true;
}

bool SaveGameManager::replacePreparedDatabase(
    const QString& preparedPath,
    const QString& targetPath,
    QString* errorMessage) const
{
    if (!QFile::exists(preparedPath)) {
        return fail(errorMessage, QStringLiteral("교체할 임시 세이브 파일이 없습니다."));
    }

    const QString backupPath = targetPath + QStringLiteral(".replace-backup");
    (void)removeDatabaseFiles(backupPath);
    const bool hadTarget = QFile::exists(targetPath);

    if (hadTarget) {
        if (!QFile::rename(targetPath, backupPath)
            || !renameIfPresent(targetPath + QStringLiteral("-wal"), backupPath + QStringLiteral("-wal"))
            || !renameIfPresent(targetPath + QStringLiteral("-shm"), backupPath + QStringLiteral("-shm"))
            || !renameIfPresent(
                targetPath + QStringLiteral("-journal"),
                backupPath + QStringLiteral("-journal"))) {
            (void)renameIfPresent(backupPath, targetPath);
            (void)renameIfPresent(backupPath + QStringLiteral("-wal"), targetPath + QStringLiteral("-wal"));
            (void)renameIfPresent(backupPath + QStringLiteral("-shm"), targetPath + QStringLiteral("-shm"));
            (void)renameIfPresent(
                backupPath + QStringLiteral("-journal"),
                targetPath + QStringLiteral("-journal"));
            return fail(errorMessage, QStringLiteral("기존 세이브를 안전 교체용 위치로 이동하지 못했습니다."));
        }
    } else {
        (void)QFile::remove(targetPath + QStringLiteral("-wal"));
        (void)QFile::remove(targetPath + QStringLiteral("-shm"));
        (void)QFile::remove(targetPath + QStringLiteral("-journal"));
    }

    if (!QFile::rename(preparedPath, targetPath)) {
        if (hadTarget) {
            (void)renameIfPresent(backupPath, targetPath);
            (void)renameIfPresent(backupPath + QStringLiteral("-wal"), targetPath + QStringLiteral("-wal"));
            (void)renameIfPresent(backupPath + QStringLiteral("-shm"), targetPath + QStringLiteral("-shm"));
            (void)renameIfPresent(
                backupPath + QStringLiteral("-journal"),
                targetPath + QStringLiteral("-journal"));
        }
        return fail(errorMessage, QStringLiteral("완성된 임시 세이브를 본 파일로 교체하지 못했습니다."));
    }

    (void)removeDatabaseFiles(backupPath);
    return true;
}

std::optional<GameState> SaveGameManager::loadStateFile(
    const SaveGameDescriptor& descriptor,
    QString* errorMessage) const
{
    SqliteGameRepository repository(databasePath(descriptor));
    return repository.loadOrCreateCareer(errorMessage);
}

void SaveGameManager::removeAutosavesForParent(const QString& parentCareerId)
{
    std::vector<QString> paths;
    for (const auto& candidate : descriptors_) {
        if (candidate.isAutosave() && candidate.parentId == parentCareerId) {
            paths.push_back(databasePath(candidate));
        }
    }
    descriptors_.erase(
        std::remove_if(
            descriptors_.begin(),
            descriptors_.end(),
            [&parentCareerId](const SaveGameDescriptor& candidate) {
                return candidate.isAutosave() && candidate.parentId == parentCareerId;
            }),
        descriptors_.end());
    for (const auto& path : paths) {
        (void)removeDatabaseFiles(path);
    }
}

bool SaveGameManager::removeDatabaseFiles(const QString& databasePath) const
{
    bool removed = true;
    for (const QString& path : {
             databasePath,
             databasePath + QStringLiteral("-wal"),
             databasePath + QStringLiteral("-shm"),
             databasePath + QStringLiteral("-journal")}) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            removed = false;
        }
    }
    return removed;
}

bool SaveGameManager::fail(QString* errorMessage, const QString& message) const
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return false;
}

} // namespace football_owner
