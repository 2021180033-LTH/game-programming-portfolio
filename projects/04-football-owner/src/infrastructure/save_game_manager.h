#pragma once

#include "core/game_state.h"

#include <QString>

#include <optional>
#include <vector>

namespace football_owner {

struct SaveGameDescriptor {
    QString id;
    QString displayName;
    QString clubName;
    QString clubShortName;
    QString gameDate;
    QString seasonLabel;
    QString savedAt;
    QString fileName;
    QString kind {QStringLiteral("career")};
    QString parentId;
    int autosaveSlot {};
    qint64 playTimeSeconds {};

    [[nodiscard]] bool isAutosave() const noexcept
    {
        return kind == QStringLiteral("autosave");
    }
};

class SaveGameManager {
public:
    explicit SaveGameManager(QString dataDirectory);

    [[nodiscard]] bool initialize(QString* errorMessage = nullptr);
    [[nodiscard]] const std::vector<SaveGameDescriptor>& descriptors() const noexcept;
    [[nodiscard]] const SaveGameDescriptor* descriptor(const QString& id) const noexcept;
    [[nodiscard]] bool canContinue() const;
    [[nodiscard]] QString continueSaveId() const;
    [[nodiscard]] bool nameExists(
        const QString& displayName,
        const QString& excludedCareerId = {}) const;
    [[nodiscard]] QString saveIdForName(const QString& displayName) const;
    [[nodiscard]] bool legacyImportedThisRun() const noexcept;

    [[nodiscard]] std::optional<GameState> createCareer(
        const QString& displayName,
        const QString& ownerName,
        QString* createdCareerId = nullptr,
        QString* errorMessage = nullptr);
    [[nodiscard]] std::optional<GameState> loadCareer(
        const QString& saveId,
        QString* activeCareerId = nullptr,
        qint64* playTimeSeconds = nullptr,
        bool* restoredFromAutosave = nullptr,
        QString* errorMessage = nullptr);
    [[nodiscard]] bool saveCareer(
        const QString& careerId,
        const GameState& state,
        qint64 playTimeSeconds,
        QString* errorMessage = nullptr);
    [[nodiscard]] bool saveAs(
        const QString& sourceCareerId,
        const GameState& state,
        const QString& displayName,
        const QString& overwriteCareerId,
        qint64 playTimeSeconds,
        QString* resultingCareerId = nullptr,
        QString* errorMessage = nullptr);
    [[nodiscard]] bool renameCareer(
        const QString& careerId,
        const QString& displayName,
        QString* errorMessage = nullptr);
    [[nodiscard]] bool deleteCareer(
        const QString& careerId,
        QString* errorMessage = nullptr);
    [[nodiscard]] bool createAutosave(
        const QString& parentCareerId,
        const GameState& state,
        qint64 playTimeSeconds,
        int* autosaveSlot = nullptr,
        QString* errorMessage = nullptr);

private:
    QString dataDirectory_;
    QString savesDirectory_;
    QString catalogPath_;
    QString legacyDatabasePath_;
    QString legacyMarkerPath_;
    std::vector<SaveGameDescriptor> descriptors_;
    QString lastPlayedId_;
    bool initialized_ {false};
    bool legacyImported_ {false};
    bool legacyImportedThisRun_ {false};

    [[nodiscard]] SaveGameDescriptor* mutableDescriptor(const QString& id) noexcept;
    [[nodiscard]] QString databasePath(const SaveGameDescriptor& descriptor) const;
    [[nodiscard]] QString uniqueDisplayName(const QString& preferred) const;
    [[nodiscard]] bool loadCatalog(QString* errorMessage);
    [[nodiscard]] bool writeCatalog(QString* errorMessage) const;
    [[nodiscard]] bool mergeDiscoveredFiles(QString* warningMessage);
    [[nodiscard]] bool importLegacyCareer(QString* errorMessage);
    [[nodiscard]] bool writeLegacyMarker(QString* errorMessage) const;
    [[nodiscard]] bool writeStateDatabase(
        const QString& targetPath,
        const GameState& state,
        const SaveGameDescriptor& descriptor,
        QString* errorMessage) const;
    [[nodiscard]] bool replacePreparedDatabase(
        const QString& preparedPath,
        const QString& targetPath,
        QString* errorMessage) const;
    [[nodiscard]] std::optional<GameState> loadStateFile(
        const SaveGameDescriptor& descriptor,
        QString* errorMessage) const;
    void removeAutosavesForParent(const QString& parentCareerId);
    [[nodiscard]] bool removeDatabaseFiles(const QString& databasePath) const;
    [[nodiscard]] bool fail(QString* errorMessage, const QString& message) const;
};

} // namespace football_owner
