#pragma once

#include "core/game_state.h"

#include <QSqlDatabase>
#include <QString>

#include <optional>

namespace football_owner {

struct SaveFileMetadata {
    QString careerId;
    QString saveKind;
    QString parentCareerId;
    int autosaveSlot {};
    qint64 playTimeSeconds {};
    QString savedAt;
};

class SqliteGameRepository {
public:
    explicit SqliteGameRepository(QString databasePath);
    ~SqliteGameRepository();

    SqliteGameRepository(const SqliteGameRepository&) = delete;
    SqliteGameRepository& operator=(const SqliteGameRepository&) = delete;

    [[nodiscard]] bool initialize(QString* errorMessage = nullptr);
    [[nodiscard]] std::optional<GameState> loadOrCreateCareer(QString* errorMessage = nullptr);
    [[nodiscard]] bool save(const GameState& state, QString* errorMessage = nullptr);
    [[nodiscard]] bool writeFileMetadata(
        int saveId,
        const SaveFileMetadata& metadata,
        QString* errorMessage = nullptr);
    [[nodiscard]] std::optional<SaveFileMetadata> readFileMetadata(
        QString* errorMessage = nullptr);
    [[nodiscard]] bool prepareForFileReplacement(QString* errorMessage = nullptr);

    [[nodiscard]] QString databasePath() const;

private:
    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
    bool initialized_ {false};

    [[nodiscard]] bool runResourceScript(const QString& resourcePath, QString* errorMessage);
    [[nodiscard]] bool importLockedWorldBaseline(QString* errorMessage);
    [[nodiscard]] bool importFactSafeHandoffV545(QString* errorMessage);
    [[nodiscard]] bool importFactSafeCompetitionScheduleBridgeV545(QString* errorMessage);
    [[nodiscard]] bool importFactSafeRosterActivationBridgeV545(QString* errorMessage);
    [[nodiscard]] bool importFactSafeClubFinanceModelBridgeV545(QString* errorMessage);
    [[nodiscard]] bool fail(QString* errorMessage, const QString& message) const;
};

} // namespace football_owner
