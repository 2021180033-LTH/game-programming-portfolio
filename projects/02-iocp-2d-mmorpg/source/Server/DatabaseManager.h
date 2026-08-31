#pragma once

#include <string>
#include <vector>
#include <mutex>

struct DbPlayerData
{
    std::string username;

    int level = 1;
    int exp = 0;

    int hp = 100;
    int maxHp = 100;
    int mp = 50;
    int maxMp = 50;

    int x = 100;
    int y = 100;

    int gold = 0;
    int statPoint = 0;
    int skillPoint = 0;

    int str = 5;
    int intStat = 5;
    int dex = 5;
    int luk = 5;
};

struct DbEquipmentData
{
    int weaponItemId = 0;
    std::string weaponName;

    int armorItemId = 0;
    std::string armorName;

    int accessoryItemId = 0;
    std::string accessoryName;
};

struct DbSkillData
{
    int skillId = 0;
    int skillLevel = 0;
};

struct DbInventoryItemData
{
    int itemId = 0;
    int itemCategory = 0;
    std::string itemName;
    int itemCount = 0;
};

struct DbQuickSlotData
{
    int slotIndex = 0;
    int slotType = 0;
    int targetId = 0;
    std::string name;
};

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool Initialize(const std::string& connectionString = {});
    void Shutdown();

    bool IsConnected() const;

    bool LoadOrCreatePlayer(
        const std::string& username,
        DbPlayerData& outPlayer,
        bool& outCreated
    );

    bool SavePlayer(const DbPlayerData& player);

    bool LoadEquipment(const std::string& username, DbEquipmentData& outEquipment);
    bool SaveEquipment(const std::string& username, const DbEquipmentData& equipment);

    bool LoadSkills(const std::string& username, std::vector<DbSkillData>& outSkills);
    bool SaveSkills(const std::string& username, const std::vector<DbSkillData>& skills);

    bool LoadInventory(const std::string& username, std::vector<DbInventoryItemData>& outItems);
    bool SaveInventory(const std::string& username, const std::vector<DbInventoryItemData>& items);

    bool LoadQuickSlots(const std::string& username, std::vector<DbQuickSlotData>& outSlots);
    bool SaveQuickSlots(const std::string& username, const std::vector<DbQuickSlotData>& slots);

private:
    bool ExecuteNonQuery(const std::string& sql);
    bool PlayerExists(const std::string& username, bool& outExists);
    bool CreateDefaultPlayer(const std::string& username);

    void LogDiagnostics(short handleType, void* handle, const std::string& context) const;

private:
    mutable std::recursive_mutex dbLock_;

    void* environmentHandle_ = nullptr;
    void* connectionHandle_ = nullptr;
    bool connected_ = false;
};
