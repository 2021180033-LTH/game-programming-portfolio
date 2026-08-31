#include "DatabaseManager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <sqlext.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "GameProtocol.h"
#include "Logger.h"

#pragma comment(lib, "odbc32.lib")

namespace
{
    bool IsSqlSuccess(SQLRETURN result)
    {
        return result == SQL_SUCCESS || result == SQL_SUCCESS_WITH_INFO;
    }

    std::string EscapeSqlString(const std::string& text)
    {
        std::string result;
        result.reserve(text.size() + 8);

        for (char ch : text)
        {
            result.push_back(ch);

            if (ch == '\'')
            {
                result.push_back('\'');
            }
        }

        return result;
    }

    std::string QuoteSqlString(const std::string& text)
    {
        return "N'" + EscapeSqlString(text) + "'";
    }

    int ClampInt(int value, int minValue, int maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    int GetIntColumn(SQLHSTMT statement, SQLUSMALLINT column)
    {
        SQLINTEGER value = 0;
        SQLLEN indicator = 0;

        const SQLRETURN result = SQLGetData(
            statement,
            column,
            SQL_C_LONG,
            &value,
            sizeof(value),
            &indicator
        );

        if (!IsSqlSuccess(result) || indicator == SQL_NULL_DATA)
        {
            return 0;
        }

        return static_cast<int>(value);
    }

    std::string GetStringColumn(SQLHSTMT statement, SQLUSMALLINT column)
    {
        char buffer[256]{};
        SQLLEN indicator = 0;

        const SQLRETURN result = SQLGetData(
            statement,
            column,
            SQL_C_CHAR,
            buffer,
            sizeof(buffer),
            &indicator
        );

        if (!IsSqlSuccess(result) || indicator == SQL_NULL_DATA)
        {
            return {};
        }

        return std::string(buffer);
    }

    std::string BuildDefaultConnectionString()
    {
        const char* envConnection = std::getenv("MMORPG_DB_CONNECTION");

        if (envConnection != nullptr && envConnection[0] != '\0')
        {
            return std::string(envConnection);
        }

        return "DSN=MMORPG_DB;Trusted_Connection=yes;TrustServerCertificate=yes;MARS_Connection=Yes;";
    }

    std::string BuildSkillInsertSql(const std::string& username)
    {
        std::ostringstream sql;
        sql << "INSERT INTO dbo.PlayerSkills (Username, SkillId, SkillLevel) VALUES ";

        const int skillIds[MAX_SKILL_STATUS_COUNT] =
        {
            SKILL_POWER_STRIKE,
            SKILL_HEAVY_SLASH,
            SKILL_WHIRLWIND,
            SKILL_FIREBALL,
            SKILL_ICE_BOLT,
            SKILL_MANA_BURST,
            SKILL_VITALITY_TRAINING,
            SKILL_MANA_TRAINING,
            SKILL_LUCKY_HUNTER
        };

        for (int i = 0; i < MAX_SKILL_STATUS_COUNT; ++i)
        {
            if (i > 0)
            {
                sql << ",";
            }

            sql << "(" << QuoteSqlString(username) << "," << skillIds[i] << ",0)";
        }

        sql << ";";
        return sql.str();
    }
}

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager()
{
    Shutdown();
}

bool DatabaseManager::Initialize(const std::string& connectionString)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    Shutdown();

    SQLHENV environment = SQL_NULL_HENV;
    SQLHDBC connection = SQL_NULL_HDBC;

    SQLRETURN result = SQLAllocHandle(
        SQL_HANDLE_ENV,
        SQL_NULL_HANDLE,
        &environment
    );

    if (!IsSqlSuccess(result))
    {
        Logger::Warn("DB initialize failed. SQLAllocHandle ENV failed.");
        return false;
    }

    result = SQLSetEnvAttr(
        environment,
        SQL_ATTR_ODBC_VERSION,
        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3),
        0
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_ENV, environment, "SQLSetEnvAttr");
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        return false;
    }

    result = SQLAllocHandle(
        SQL_HANDLE_DBC,
        environment,
        &connection
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_ENV, environment, "SQLAllocHandle DBC");
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        return false;
    }

    const std::string finalConnectionString =
        connectionString.empty()
        ? BuildDefaultConnectionString()
        : connectionString;

    SQLCHAR outConnectionString[1024]{};
    SQLSMALLINT outLength = 0;

    result = SQLDriverConnectA(
        connection,
        nullptr,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(finalConnectionString.c_str())),
        SQL_NTS,
        outConnectionString,
        sizeof(outConnectionString),
        &outLength,
        SQL_DRIVER_NOPROMPT
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_DBC, connection, "SQLDriverConnect");
        SQLFreeHandle(SQL_HANDLE_DBC, connection);
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        return false;
    }

    environmentHandle_ = environment;
    connectionHandle_ = connection;
    connected_ = true;

    Logger::Info("DB connected. connection=" + finalConnectionString);
    return true;
}

void DatabaseManager::Shutdown()
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    if (connectionHandle_ != nullptr)
    {
        SQLHDBC connection = static_cast<SQLHDBC>(connectionHandle_);
        SQLDisconnect(connection);
        SQLFreeHandle(SQL_HANDLE_DBC, connection);
        connectionHandle_ = nullptr;
    }

    if (environmentHandle_ != nullptr)
    {
        SQLHENV environment = static_cast<SQLHENV>(environmentHandle_);
        SQLFreeHandle(SQL_HANDLE_ENV, environment);
        environmentHandle_ = nullptr;
    }

    connected_ = false;
}

bool DatabaseManager::IsConnected() const
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    return connected_;
}

bool DatabaseManager::ExecuteNonQuery(const std::string& sql)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    if (!connected_ || connectionHandle_ == nullptr)
    {
        return false;
    }

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(
        SQL_HANDLE_STMT,
        static_cast<SQLHDBC>(connectionHandle_),
        &statement
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_DBC, connectionHandle_, "SQLAllocHandle STMT");
        return false;
    }

    result = SQLExecDirectA(
        statement,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())),
        SQL_NTS
    );

    if (result == SQL_NO_DATA)
    {
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return true;
    }

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "SQLExecDirect: " + sql);
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return true;
}

bool DatabaseManager::PlayerExists(const std::string& username, bool& outExists)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    outExists = false;

    if (!connected_ || connectionHandle_ == nullptr)
    {
        return false;
    }

    const std::string sql =
        "SELECT COUNT(*) FROM dbo.Players WHERE Username = " +
        QuoteSqlString(username) +
        ";";

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(
        SQL_HANDLE_STMT,
        static_cast<SQLHDBC>(connectionHandle_),
        &statement
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_DBC, connectionHandle_, "SQLAllocHandle PlayerExists");
        return false;
    }

    result = SQLExecDirectA(
        statement,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())),
        SQL_NTS
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "PlayerExists");
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    result = SQLFetch(statement);

    if (IsSqlSuccess(result))
    {
        outExists = GetIntColumn(statement, 1) > 0;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return true;
}

bool DatabaseManager::CreateDefaultPlayer(const std::string& username)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    const std::string user = QuoteSqlString(username);

    std::ostringstream sql;

    sql << "INSERT INTO dbo.Players "
        << "(Username, Level, Exp, Hp, MaxHp, Mp, MaxMp, X, Y, Gold, StatPoint, SkillPoint) "
        << "VALUES (" << user << ",1,0,100,100,50,50,100,100,0,0,0);";

    sql << "INSERT INTO dbo.PlayerStats "
        << "(Username, Str, IntStat, Dex, Luk) "
        << "VALUES (" << user << ",5,5,5,5);";

    sql << "INSERT INTO dbo.PlayerEquipment "
        << "(Username, WeaponItemId, WeaponName, ArmorItemId, ArmorName, AccessoryItemId, AccessoryName) "
        << "VALUES (" << user << ",0,N'',0,N'',0,N'');";

    sql << BuildSkillInsertSql(username);

    for (int slotIndex = 0; slotIndex < MAX_QUICK_SLOT_COUNT; ++slotIndex)
    {
        sql << "INSERT INTO dbo.PlayerQuickSlots "
            << "(Username, SlotIndex, SlotType, TargetId, Name) "
            << "VALUES (" << user << "," << slotIndex << ",0,0,N'');";
    }

    return ExecuteNonQuery(sql.str());
}

bool DatabaseManager::LoadOrCreatePlayer(
    const std::string& username,
    DbPlayerData& outPlayer,
    bool& outCreated
)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    outCreated = false;
    outPlayer = DbPlayerData{};
    outPlayer.username = username;

    bool exists = false;

    if (!PlayerExists(username, exists))
    {
        return false;
    }

    if (!exists)
    {
        if (!CreateDefaultPlayer(username))
        {
            return false;
        }

        outCreated = true;
    }

    const std::string sql =
        "SELECT "
        "p.Level, p.Exp, p.Hp, p.MaxHp, p.Mp, p.MaxMp, p.X, p.Y, "
        "p.Gold, p.StatPoint, p.SkillPoint, "
        "s.Str, s.IntStat, s.Dex, s.Luk "
        "FROM dbo.Players p "
        "LEFT JOIN dbo.PlayerStats s ON p.Username = s.Username "
        "WHERE p.Username = " + QuoteSqlString(username) + ";";

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(
        SQL_HANDLE_STMT,
        static_cast<SQLHDBC>(connectionHandle_),
        &statement
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_DBC, connectionHandle_, "SQLAllocHandle LoadPlayer");
        return false;
    }

    result = SQLExecDirectA(
        statement,
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())),
        SQL_NTS
    );

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "LoadOrCreatePlayer");
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    result = SQLFetch(statement);

    if (!IsSqlSuccess(result))
    {
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    outPlayer.level = std::max(1, GetIntColumn(statement, 1));
    outPlayer.exp = std::max(0, GetIntColumn(statement, 2));
    outPlayer.hp = std::max(1, GetIntColumn(statement, 3));
    outPlayer.maxHp = std::max(1, GetIntColumn(statement, 4));
    outPlayer.mp = std::max(0, GetIntColumn(statement, 5));
    outPlayer.maxMp = std::max(1, GetIntColumn(statement, 6));
    outPlayer.x = GetIntColumn(statement, 7);
    outPlayer.y = GetIntColumn(statement, 8);
    outPlayer.gold = std::max(0, GetIntColumn(statement, 9));
    outPlayer.statPoint = std::max(0, GetIntColumn(statement, 10));
    outPlayer.skillPoint = std::max(0, GetIntColumn(statement, 11));
    outPlayer.str = std::max(1, GetIntColumn(statement, 12));
    outPlayer.intStat = std::max(1, GetIntColumn(statement, 13));
    outPlayer.dex = std::max(1, GetIntColumn(statement, 14));
    outPlayer.luk = std::max(1, GetIntColumn(statement, 15));

    SQLFreeHandle(SQL_HANDLE_STMT, statement);

    ExecuteNonQuery(
        "UPDATE dbo.Players SET LastLoginAt = SYSDATETIME() WHERE Username = " +
        QuoteSqlString(username) +
        ";"
    );

    return true;
}

bool DatabaseManager::SavePlayer(const DbPlayerData& player)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    const std::string user = QuoteSqlString(player.username);

    std::ostringstream sql;

    sql << "UPDATE dbo.Players SET "
        << "Level=" << std::max(1, player.level) << ","
        << "Exp=" << std::max(0, player.exp) << ","
        << "Hp=" << std::max(1, player.hp) << ","
        << "MaxHp=" << std::max(1, player.maxHp) << ","
        << "Mp=" << std::max(0, player.mp) << ","
        << "MaxMp=" << std::max(1, player.maxMp) << ","
        << "X=" << player.x << ","
        << "Y=" << player.y << ","
        << "Gold=" << std::max(0, player.gold) << ","
        << "StatPoint=" << std::max(0, player.statPoint) << ","
        << "SkillPoint=" << std::max(0, player.skillPoint) << ","
        << "LastLogoutAt=SYSDATETIME() "
        << "WHERE Username=" << user << ";";

    sql << "UPDATE dbo.PlayerStats SET "
        << "Str=" << std::max(1, player.str) << ","
        << "IntStat=" << std::max(1, player.intStat) << ","
        << "Dex=" << std::max(1, player.dex) << ","
        << "Luk=" << std::max(1, player.luk) << " "
        << "WHERE Username=" << user << ";";

    return ExecuteNonQuery(sql.str());
}

bool DatabaseManager::LoadEquipment(const std::string& username, DbEquipmentData& outEquipment)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    outEquipment = DbEquipmentData{};

    const std::string sql =
        "SELECT WeaponItemId, WeaponName, ArmorItemId, ArmorName, AccessoryItemId, AccessoryName "
        "FROM dbo.PlayerEquipment WHERE Username = " + QuoteSqlString(username) + ";";

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(connectionHandle_), &statement);

    if (!IsSqlSuccess(result))
    {
        return false;
    }

    result = SQLExecDirectA(statement, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "LoadEquipment");
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    if (IsSqlSuccess(SQLFetch(statement)))
    {
        outEquipment.weaponItemId = GetIntColumn(statement, 1);
        outEquipment.weaponName = GetStringColumn(statement, 2);
        outEquipment.armorItemId = GetIntColumn(statement, 3);
        outEquipment.armorName = GetStringColumn(statement, 4);
        outEquipment.accessoryItemId = GetIntColumn(statement, 5);
        outEquipment.accessoryName = GetStringColumn(statement, 6);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return true;
}

bool DatabaseManager::SaveEquipment(const std::string& username, const DbEquipmentData& equipment)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    const std::string user = QuoteSqlString(username);

    std::ostringstream sql;
    sql << "UPDATE dbo.PlayerEquipment SET "
        << "WeaponItemId=" << equipment.weaponItemId << ","
        << "WeaponName=" << QuoteSqlString(equipment.weaponName) << ","
        << "ArmorItemId=" << equipment.armorItemId << ","
        << "ArmorName=" << QuoteSqlString(equipment.armorName) << ","
        << "AccessoryItemId=" << equipment.accessoryItemId << ","
        << "AccessoryName=" << QuoteSqlString(equipment.accessoryName) << " "
        << "WHERE Username=" << user << ";";

    sql << "IF @@ROWCOUNT = 0 "
        << "INSERT INTO dbo.PlayerEquipment "
        << "(Username, WeaponItemId, WeaponName, ArmorItemId, ArmorName, AccessoryItemId, AccessoryName) VALUES ("
        << user << ","
        << equipment.weaponItemId << "," << QuoteSqlString(equipment.weaponName) << ","
        << equipment.armorItemId << "," << QuoteSqlString(equipment.armorName) << ","
        << equipment.accessoryItemId << "," << QuoteSqlString(equipment.accessoryName) << ");";

    return ExecuteNonQuery(sql.str());
}

bool DatabaseManager::LoadSkills(const std::string& username, std::vector<DbSkillData>& outSkills)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    outSkills.clear();

    const std::string sql =
        "SELECT SkillId, SkillLevel FROM dbo.PlayerSkills WHERE Username = " +
        QuoteSqlString(username) +
        " ORDER BY SkillId;";

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(connectionHandle_), &statement);

    if (!IsSqlSuccess(result))
    {
        return false;
    }

    result = SQLExecDirectA(statement, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "LoadSkills");
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    while (IsSqlSuccess(SQLFetch(statement)))
    {
        DbSkillData skill{};
        skill.skillId = GetIntColumn(statement, 1);
        skill.skillLevel = ClampInt(GetIntColumn(statement, 2), 0, SKILL_MAX_LEVEL);
        outSkills.push_back(skill);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return true;
}

bool DatabaseManager::SaveSkills(const std::string& username, const std::vector<DbSkillData>& skills)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    const std::string user = QuoteSqlString(username);

    std::ostringstream sql;
    sql << "DELETE FROM dbo.PlayerSkills WHERE Username=" << user << ";";

    for (const DbSkillData& skill : skills)
    {
        if (skill.skillId <= 0)
        {
            continue;
        }

        sql << "INSERT INTO dbo.PlayerSkills (Username, SkillId, SkillLevel) VALUES ("
            << user << ","
            << skill.skillId << ","
            << ClampInt(skill.skillLevel, 0, SKILL_MAX_LEVEL) << ");";
    }

    return ExecuteNonQuery(sql.str());
}

bool DatabaseManager::LoadInventory(const std::string& username, std::vector<DbInventoryItemData>& outItems)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    outItems.clear();

    const std::string sql =
        "SELECT ItemId, ItemCategory, ItemName, ItemCount "
        "FROM dbo.PlayerInventory WHERE Username = " + QuoteSqlString(username) +
        " ORDER BY ItemId;";

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(connectionHandle_), &statement);

    if (!IsSqlSuccess(result))
    {
        return false;
    }

    result = SQLExecDirectA(statement, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "LoadInventory");
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    while (IsSqlSuccess(SQLFetch(statement)))
    {
        DbInventoryItemData item{};
        item.itemId = GetIntColumn(statement, 1);
        item.itemCategory = GetIntColumn(statement, 2);
        item.itemName = GetStringColumn(statement, 3);
        item.itemCount = std::max(0, GetIntColumn(statement, 4));

        if (item.itemId > 0 && item.itemCount > 0)
        {
            outItems.push_back(item);
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return true;
}

bool DatabaseManager::SaveInventory(const std::string& username, const std::vector<DbInventoryItemData>& items)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    const std::string user = QuoteSqlString(username);

    std::ostringstream sql;
    sql << "DELETE FROM dbo.PlayerInventory WHERE Username=" << user << ";";

    for (const DbInventoryItemData& item : items)
    {
        if (item.itemId <= 0 || item.itemCount <= 0)
        {
            continue;
        }

        sql << "INSERT INTO dbo.PlayerInventory "
            << "(Username, ItemId, ItemCategory, ItemName, ItemCount) VALUES ("
            << user << ","
            << item.itemId << ","
            << item.itemCategory << ","
            << QuoteSqlString(item.itemName) << ","
            << item.itemCount << ");";
    }

    return ExecuteNonQuery(sql.str());
}

bool DatabaseManager::LoadQuickSlots(const std::string& username, std::vector<DbQuickSlotData>& outSlots)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    outSlots.clear();

    const std::string sql =
        "SELECT SlotIndex, SlotType, TargetId, Name "
        "FROM dbo.PlayerQuickSlots WHERE Username = " + QuoteSqlString(username) +
        " ORDER BY SlotIndex;";

    SQLHSTMT statement = SQL_NULL_HSTMT;
    SQLRETURN result = SQLAllocHandle(SQL_HANDLE_STMT, static_cast<SQLHDBC>(connectionHandle_), &statement);

    if (!IsSqlSuccess(result))
    {
        return false;
    }

    result = SQLExecDirectA(statement, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())), SQL_NTS);

    if (!IsSqlSuccess(result))
    {
        LogDiagnostics(SQL_HANDLE_STMT, statement, "LoadQuickSlots");
        SQLFreeHandle(SQL_HANDLE_STMT, statement);
        return false;
    }

    while (IsSqlSuccess(SQLFetch(statement)))
    {
        DbQuickSlotData slot{};
        slot.slotIndex = GetIntColumn(statement, 1);
        slot.slotType = GetIntColumn(statement, 2);
        slot.targetId = GetIntColumn(statement, 3);
        slot.name = GetStringColumn(statement, 4);

        if (slot.slotIndex >= 0 && slot.slotIndex < MAX_QUICK_SLOT_COUNT)
        {
            outSlots.push_back(slot);
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return true;
}

bool DatabaseManager::SaveQuickSlots(const std::string& username, const std::vector<DbQuickSlotData>& slots)
{
    std::lock_guard<std::recursive_mutex> lock(dbLock_);
    const std::string user = QuoteSqlString(username);

    std::ostringstream sql;
    sql << "DELETE FROM dbo.PlayerQuickSlots WHERE Username=" << user << ";";

    bool used[MAX_QUICK_SLOT_COUNT]{};

    for (const DbQuickSlotData& slot : slots)
    {
        if (slot.slotIndex < 0 || slot.slotIndex >= MAX_QUICK_SLOT_COUNT)
        {
            continue;
        }

        used[slot.slotIndex] = true;

        sql << "INSERT INTO dbo.PlayerQuickSlots "
            << "(Username, SlotIndex, SlotType, TargetId, Name) VALUES ("
            << user << ","
            << slot.slotIndex << ","
            << ClampInt(slot.slotType, 0, 2) << ","
            << std::max(0, slot.targetId) << ","
            << QuoteSqlString(slot.name) << ");";
    }

    for (int i = 0; i < MAX_QUICK_SLOT_COUNT; ++i)
    {
        if (used[i])
        {
            continue;
        }

        sql << "INSERT INTO dbo.PlayerQuickSlots "
            << "(Username, SlotIndex, SlotType, TargetId, Name) VALUES ("
            << user << "," << i << ",0,0,N'');";
    }

    return ExecuteNonQuery(sql.str());
}

void DatabaseManager::LogDiagnostics(short handleType, void* handle, const std::string& context) const
{
    SQLSMALLINT record = 1;
    SQLCHAR state[6]{};
    SQLINTEGER nativeError = 0;
    SQLCHAR message[1024]{};
    SQLSMALLINT messageLength = 0;

    while (true)
    {
        const SQLRETURN result = SQLGetDiagRecA(
            handleType,
            handle,
            record,
            state,
            &nativeError,
            message,
            sizeof(message),
            &messageLength
        );

        if (!IsSqlSuccess(result))
        {
            break;
        }

        Logger::Warn(
            "DB error. context=" +
            context +
            ", state=" +
            reinterpret_cast<char*>(state) +
            ", native=" +
            std::to_string(nativeError) +
            ", message=" +
            reinterpret_cast<char*>(message)
        );

        ++record;
    }
}
