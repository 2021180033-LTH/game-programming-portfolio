#include "GameWorld.h"

#include <fstream>
#include <sstream>
#include <cctype>

#include <lua.hpp>

namespace
{
    constexpr int VIEW_RANGE = 15;
    constexpr int VIEW_RADIUS = VIEW_RANGE / 2;
    constexpr int SECTOR_SIZE = 10;

    constexpr int MONSTER_ROAM_INTERVAL_MS = 1500;
    constexpr int WORLD_UPDATE_INTERVAL_MS = 50;
    constexpr int MONSTER_AI_UPDATE_INTERVAL_MS = 250;
    constexpr int WORLD_FAST_UPDATE_INTERVAL_MS = 250;
    constexpr int WORLD_SLOW_UPDATE_INTERVAL_MS = 1000;

    constexpr int MONSTER_AGGRO_RANGE = 11;
    constexpr int MONSTER_AGGRO_RADIUS = MONSTER_AGGRO_RANGE / 2;

    constexpr int ASTAR_SEARCH_PADDING = 12;
    constexpr int ASTAR_MAX_EXPANDED_NODES = 512;

    constexpr int SPAWN_SEARCH_RADIUS = 30;

    constexpr long long MONSTER_RESPAWN_DELAY_MS = 30000;

    constexpr long long MONSTER_ATTACK_INTERVAL_MS = 1000;

    constexpr long long PLAYER_HP_REGEN_INTERVAL_MS = 5000;
    constexpr int PLAYER_HP_REGEN_PERCENT = 10;

    constexpr int PLAYER_START_X = 100;
    constexpr int PLAYER_START_Y = 100;

    constexpr long long PLAYER_RESPAWN_DELAY_MS = 5000;
    constexpr int PLAYER_RESPAWN_COUNTDOWN_NOTICE_SECONDS = 3;

    constexpr long long DROP_EXPIRE_DELAY_MS = 30000;

    constexpr long long DIRTY_SAVE_INTERVAL_MS = 15000;

    constexpr int MP_POTION_RECOVER_AMOUNT = 50;

    void AppendForwardLineTargets(
        std::vector<std::pair<int, int>>& targets,
        int centerX,
        int centerY,
        int dx,
        int dy,
        int range
    )
    {
        for (int distance = 1; distance <= range; ++distance)
        {
            targets.push_back({
                centerX + dx * distance,
                centerY + dy * distance
                });
        }
    }

    void AppendForwardConeTargets(
        std::vector<std::pair<int, int>>& targets,
        int centerX,
        int centerY,
        int dx,
        int dy,
        int range,
        int halfWidth
    )
    {
        for (int distance = 1; distance <= range; ++distance)
        {
            for (int lateral = -halfWidth; lateral <= halfWidth; ++lateral)
            {
                int targetX = centerX + dx * distance;
                int targetY = centerY + dy * distance;

                if (dx != 0)
                {
                    targetY += lateral;
                }
                else
                {
                    targetX += lateral;
                }

                targets.push_back({ targetX, targetY });
            }
        }
    }

    void AppendDirectionalTargetPositions(
        std::vector<std::pair<int, int>>& targets,
        int centerX,
        int centerY,
        int dx,
        int dy,
        int range,
        int halfWidth
    )
    {
        AppendForwardConeTargets(targets, centerX, centerY, dx, dy, range, halfWidth);
    }

    constexpr int POWER_STRIKE_BASE_MP_COST = 10;
    constexpr int POWER_STRIKE_MP_COST_PER_LEVEL = 2;

    constexpr int HEAVY_SLASH_BASE_MP_COST = 18;
    constexpr int HEAVY_SLASH_MP_COST_PER_LEVEL = 3;

    constexpr int WHIRLWIND_BASE_MP_COST = 30;
    constexpr int WHIRLWIND_MP_COST_PER_LEVEL = 4;

    constexpr int FIREBALL_BASE_MP_COST = 12;
    constexpr int FIREBALL_MP_COST_PER_LEVEL = 2;

    constexpr int MANA_BURST_BASE_MP_COST = 35;
    constexpr int MANA_BURST_MP_COST_PER_LEVEL = 5;

    constexpr int ICE_BOLT_BASE_MP_COST = 16;
    constexpr int ICE_BOLT_MP_COST_PER_LEVEL = 3;

    void CopyStringToFixedBuffer(
        char* dest,
        std::size_t destSize,
        const std::string& source
    )
    {
        if (dest == nullptr || destSize == 0)
        {
            return;
        }

        std::memset(dest, 0, destSize);

        const std::size_t copySize =
            std::min(destSize - 1, source.size());

        if (copySize > 0)
        {
            std::memcpy(dest, source.data(), copySize);
        }
    }


    std::string ToLowerAscii(std::string text)
    {
        for (char& ch : text)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }

        return text;
    }

    bool IsDebugAccountName(const std::string& username)
    {
        const std::string lower = ToLowerAscii(username);
        return lower == "debug" || lower == "test" || lower == "test1";
    }

    bool IsEquipmentItemId(int itemId)
    {
        return
            itemId == ITEM_IRON_SWORD ||
            itemId == ITEM_STEEL_SWORD ||
            itemId == ITEM_FLAME_SWORD ||
            itemId == ITEM_MAGE_STAFF ||
            itemId == ITEM_ICE_WAND ||
            itemId == ITEM_HUNTER_BOW ||
            itemId == ITEM_IRON_ARMOR ||
            itemId == ITEM_STEEL_ARMOR ||
            itemId == ITEM_MAGIC_ROBE ||
            itemId == ITEM_LEATHER_ARMOR ||
            itemId == ITEM_MAGIC_RING ||
            itemId == ITEM_WARRIOR_RING ||
            itemId == ITEM_LUCKY_NECKLACE ||
            itemId == ITEM_SPEED_CHARM;
    }

    int GetEquipmentSlotByItemId(int itemId)
    {
        switch (itemId)
        {
        case ITEM_IRON_SWORD:
        case ITEM_STEEL_SWORD:
        case ITEM_FLAME_SWORD:
        case ITEM_MAGE_STAFF:
        case ITEM_ICE_WAND:
        case ITEM_HUNTER_BOW:
            return EQUIPMENT_SLOT_WEAPON;

        case ITEM_IRON_ARMOR:
        case ITEM_STEEL_ARMOR:
        case ITEM_MAGIC_ROBE:
        case ITEM_LEATHER_ARMOR:
            return EQUIPMENT_SLOT_ARMOR;

        case ITEM_MAGIC_RING:
        case ITEM_WARRIOR_RING:
        case ITEM_LUCKY_NECKLACE:
        case ITEM_SPEED_CHARM:
            return EQUIPMENT_SLOT_ACCESSORY;

        default:
            return 0;
        }
    }

    std::string GetEquipmentSlotName(int equipmentSlot)
    {
        switch (equipmentSlot)
        {
        case EQUIPMENT_SLOT_WEAPON:
            return "weapon";

        case EQUIPMENT_SLOT_ARMOR:
            return "armor";

        case EQUIPMENT_SLOT_ACCESSORY:
            return "accessory";

        default:
            return "equipment";
        }
    }

    std::string GetItemDisplayNameByItemId(int itemId)
    {
        switch (itemId)
        {
        case ITEM_SLIME_PART:
            return "Slime Part";
        case ITEM_GOBLIN_PART:
            return "Goblin Part";
        case ITEM_WOLF_PART:
            return "Wolf Part";
        case ITEM_ORC_PART:
            return "Orc Part";
        case ITEM_SKELETON_PART:
            return "Skeleton Part";
        case ITEM_DARK_MAGE_PART:
            return "DarkMage Part";
        case ITEM_HP_POTION:
            return "HP Potion";
        case ITEM_MP_POTION:
            return "MP Potion";
        case ITEM_HIGH_HP_POTION:
            return "High HP Potion";
        case ITEM_HIGH_MP_POTION:
            return "High MP Potion";
        case ITEM_RETURN_SCROLL:
            return "Return Scroll";
        case ITEM_IRON_SWORD:
            return "Iron Sword";
        case ITEM_STEEL_SWORD:
            return "Steel Sword";
        case ITEM_FLAME_SWORD:
            return "Flame Sword";
        case ITEM_MAGE_STAFF:
            return "Mage Staff";
        case ITEM_ICE_WAND:
            return "Ice Wand";
        case ITEM_HUNTER_BOW:
            return "Hunter Bow";
        case ITEM_IRON_ARMOR:
            return "Iron Armor";
        case ITEM_STEEL_ARMOR:
            return "Steel Armor";
        case ITEM_MAGIC_ROBE:
            return "Magic Robe";
        case ITEM_LEATHER_ARMOR:
            return "Leather Armor";
        case ITEM_MAGIC_RING:
            return "Magic Ring";
        case ITEM_WARRIOR_RING:
            return "Warrior Ring";
        case ITEM_LUCKY_NECKLACE:
            return "Lucky Necklace";
        case ITEM_SPEED_CHARM:
            return "Speed Charm";
        default:
            return "Item";
        }
    }

    int GetDefaultSellPriceByItemId(int itemId)
    {
        switch (itemId)
        {
        case ITEM_SLIME_PART:
            return 5;

        case ITEM_GOBLIN_PART:
            return 10;

        case ITEM_WOLF_PART:
            return 15;

        case ITEM_ORC_PART:
            return 20;

        case ITEM_SKELETON_PART:
            return 25;

        case ITEM_DARK_MAGE_PART:
            return 30;

        default:
            return 0;
        }
    }


    bool ExtractIntField(const std::string& line, const std::string& key, int& outValue)
    {
        const std::string token = key + "=";
        std::size_t pos = line.find(token);

        if (pos == std::string::npos)
        {
            const std::string spacedToken = key + " =";
            pos = line.find(spacedToken);
        }

        if (pos == std::string::npos)
        {
            return false;
        }

        pos = line.find('=', pos);

        if (pos == std::string::npos)
        {
            return false;
        }

        ++pos;

        while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos])))
        {
            ++pos;
        }

        try
        {
            outValue = std::stoi(line.substr(pos));
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    std::string ExtractStringField(
        const std::string& line,
        const std::string& key,
        const std::string& defaultValue
    )
    {
        const std::string token = key + "=";
        std::size_t pos = line.find(token);

        if (pos == std::string::npos)
        {
            const std::string spacedToken = key + " =";
            pos = line.find(spacedToken);
        }

        if (pos == std::string::npos)
        {
            return defaultValue;
        }

        pos = line.find('=', pos);

        if (pos == std::string::npos)
        {
            return defaultValue;
        }

        const std::size_t quoteStart = line.find('"', pos);

        if (quoteStart == std::string::npos)
        {
            return defaultValue;
        }

        const std::size_t quoteEnd = line.find('"', quoteStart + 1);

        if (quoteEnd == std::string::npos)
        {
            return defaultValue;
        }

        return line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }


    int ToAbsoluteLuaIndex(lua_State* L, int index)
    {
        if (index > 0 || index <= LUA_REGISTRYINDEX)
        {
            return index;
        }

        return lua_gettop(L) + index + 1;
    }

    bool RunLuaFileFromCandidates(
        lua_State* L,
        const char* const* candidatePaths,
        int candidatePathCount,
        std::string& outLoadedPath,
        std::string& outErrorMessage
    )
    {
        outLoadedPath.clear();
        outErrorMessage.clear();

        for (int i = 0; i < candidatePathCount; ++i)
        {
            const char* path = candidatePaths[i];

            if (luaL_dofile(L, path) == LUA_OK)
            {
                outLoadedPath = path;
                return true;
            }

            const char* luaError = lua_tostring(L, -1);

            if (luaError != nullptr)
            {
                outErrorMessage = luaError;
            }

            lua_pop(L, 1);
        }

        return false;
    }

    int GetLuaIntField(lua_State* L, int tableIndex, const char* fieldName, int defaultValue)
    {
        const int absoluteTableIndex = ToAbsoluteLuaIndex(L, tableIndex);

        lua_getfield(L, absoluteTableIndex, fieldName);

        int result = defaultValue;

        if (lua_isnumber(L, -1))
        {
            result = static_cast<int>(lua_tointeger(L, -1));
        }

        lua_pop(L, 1);
        return result;
    }

    std::string GetLuaStringField(
        lua_State* L,
        int tableIndex,
        const char* fieldName,
        const std::string& defaultValue
    )
    {
        const int absoluteTableIndex = ToAbsoluteLuaIndex(L, tableIndex);

        lua_getfield(L, absoluteTableIndex, fieldName);

        std::string result = defaultValue;

        if (lua_isstring(L, -1))
        {
            const char* text = lua_tostring(L, -1);

            if (text != nullptr)
            {
                result = text;
            }
        }

        lua_pop(L, 1);
        return result;
    }

    bool IsAdjacent4(int x1, int y1, int x2, int y2)
    {
        const int distance = std::abs(x1 - x2) + std::abs(y1 - y2);
        return distance == 1;
    }

    long long GetNowMs()
    {
        using namespace std::chrono;

        return duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }

    constexpr long long PLAYER_ATTACK_INTERVAL_MS = 1000;

    bool DirectionToDelta(DIRECTION direction, int& dx, int& dy)
    {
        dx = 0;
        dy = 0;

        switch (direction)
        {
        case UP:
            dy = -1;
            return true;

        case DOWN:
            dy = 1;
            return true;

        case LEFT:
            dx = -1;
            return true;

        case RIGHT:
            dx = 1;
            return true;

        default:
            return false;
        }
    }

    bool RollPercent(int percent)
    {
        if (percent <= 0)
        {
            return false;
        }

        if (percent >= 100)
        {
            return true;
        }

        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(1, 100);

        return dist(rng) <= percent;
    }
}
GameWorld::GameWorld(SendFunction sendFunction)
    : sendFunction_(std::move(sendFunction))
{
    databaseReady_ = databaseManager_.Initialize();

    if (!databaseReady_)
    {
        Logger::Warn("DB connection failed. Server will run with memory-only data.");
    }

    StartDbWorker();

    InitializeMap();
    InitializeMonsters();
    InitializeNpcs();
    LoadItemSellPrices();

    StartWorldLoop();
}

GameWorld::~GameWorld()
{
    StopWorldLoop();
    StopDbWorker();
}

bool GameWorld::BuildPlayerSaveSnapshot(ClientSession* player, PlayerSaveSnapshot& outSnapshot)
{
    if (player == nullptr || !databaseReady_ || !player->IsLoggedIn())
    {
        return false;
    }

    const std::string username = player->GetUsername();

    if (username.empty())
    {
        return false;
    }

    outSnapshot = PlayerSaveSnapshot{};
    outSnapshot.player.username = username;
    outSnapshot.player.level = player->GetLevel();
    outSnapshot.player.exp = player->GetExp();
    outSnapshot.player.hp = player->GetHp();
    outSnapshot.player.maxHp = player->GetMaxHp();
    outSnapshot.player.mp = player->GetMp();
    outSnapshot.player.maxMp = player->GetMaxMp();
    outSnapshot.player.x = player->GetX();
    outSnapshot.player.y = player->GetY();
    outSnapshot.player.gold = player->GetGold();
    outSnapshot.player.statPoint = player->GetStatPoint();
    outSnapshot.player.skillPoint = player->GetSkillPoint();
    outSnapshot.player.str = player->GetStr();
    outSnapshot.player.intStat = player->GetIntStat();
    outSnapshot.player.dex = player->GetDex();
    outSnapshot.player.luk = player->GetLuk();

    outSnapshot.equipment.weaponItemId = player->GetEquippedWeaponItemId();
    outSnapshot.equipment.weaponName = player->GetEquippedWeaponName();
    outSnapshot.equipment.armorItemId = player->GetEquippedArmorItemId();
    outSnapshot.equipment.armorName = player->GetEquippedArmorName();
    outSnapshot.equipment.accessoryItemId = player->GetEquippedAccessoryItemId();
    outSnapshot.equipment.accessoryName = player->GetEquippedAccessoryName();

    SkillStatusEntry skillEntries[MAX_SKILL_STATUS_COUNT]{};
    int skillCount = 0;
    player->GetSkillStatus(skillEntries, MAX_SKILL_STATUS_COUNT, skillCount);

    for (int i = 0; i < skillCount; ++i)
    {
        DbSkillData skill{};
        skill.skillId = skillEntries[i].skillId;
        skill.skillLevel = skillEntries[i].level;
        outSnapshot.skills.push_back(skill);
    }

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        auto inventoryIter = inventoriesByPlayerId_.find(player->GetPlayerId());

        if (inventoryIter != inventoriesByPlayerId_.end())
        {
            for (const auto& [itemId, item] : inventoryIter->second)
            {
                if (item.count <= 0)
                {
                    continue;
                }

                DbInventoryItemData dbItem{};
                dbItem.itemId = item.itemId;
                dbItem.itemCategory = item.itemCategory;
                dbItem.itemName = item.itemName;
                dbItem.itemCount = item.count;
                outSnapshot.inventoryItems.push_back(dbItem);
            }
        }
    }

    QuickSlotStatusEntry quickSlotEntries[MAX_QUICK_SLOT_COUNT]{};
    int quickSlotCount = 0;
    player->GetQuickSlotStatus(
        quickSlotEntries,
        MAX_QUICK_SLOT_COUNT,
        quickSlotCount
    );

    for (int i = 0; i < quickSlotCount; ++i)
    {
        DbQuickSlotData slot{};
        slot.slotIndex = quickSlotEntries[i].slotIndex;
        slot.slotType = quickSlotEntries[i].slotType;
        slot.targetId = quickSlotEntries[i].targetId;
        slot.name = quickSlotEntries[i].name;
        outSnapshot.quickSlots.push_back(slot);
    }

    return true;
}

bool GameWorld::EnqueueDbJob(DbJob&& job)
{
    if (!databaseReady_)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(dbJobLock_);

        if (dbWorkerStopRequested_.load())
        {
            return false;
        }

        job.sequence = nextDbJobSequence_++;
        dbJobs_.push(std::move(job));
    }

    dbJobCv_.notify_one();
    return true;
}

bool GameWorld::EnqueueSaveSnapshot(PlayerSaveSnapshot&& snapshot, DbSaveMode saveMode)
{
    if (snapshot.player.username.empty())
    {
        return false;
    }

    DbJob job{};
    job.type = DbJobType::Save;
    job.username = snapshot.player.username;
    job.saveMode = saveMode;
    job.saveSnapshot = std::move(snapshot);

    return EnqueueDbJob(std::move(job));
}

void GameWorld::SavePlayerDataToDatabase(ClientSession* player)
{
    PlayerSaveSnapshot snapshot{};

    if (!BuildPlayerSaveSnapshot(player, snapshot))
    {
        return;
    }

    EnqueueSaveSnapshot(std::move(snapshot), DbSaveMode::Full);
}

void GameWorld::SavePlayerCoreToDatabase(ClientSession* player)
{
    PlayerSaveSnapshot snapshot{};

    if (player == nullptr || !databaseReady_ || !player->IsLoggedIn())
    {
        return;
    }

    const std::string username = player->GetUsername();

    if (username.empty())
    {
        return;
    }

    snapshot.player.username = username;
    snapshot.player.level = player->GetLevel();
    snapshot.player.exp = player->GetExp();
    snapshot.player.hp = player->GetHp();
    snapshot.player.maxHp = player->GetMaxHp();
    snapshot.player.mp = player->GetMp();
    snapshot.player.maxMp = player->GetMaxMp();
    snapshot.player.x = player->GetX();
    snapshot.player.y = player->GetY();
    snapshot.player.gold = player->GetGold();
    snapshot.player.statPoint = player->GetStatPoint();
    snapshot.player.skillPoint = player->GetSkillPoint();
    snapshot.player.str = player->GetStr();
    snapshot.player.intStat = player->GetIntStat();
    snapshot.player.dex = player->GetDex();
    snapshot.player.luk = player->GetLuk();

    EnqueueSaveSnapshot(std::move(snapshot), DbSaveMode::PlayerOnly);
}

void GameWorld::SaveQuickSlotsToDatabase(ClientSession* player)
{
    SavePlayerDataToDatabase(player);
}

void GameWorld::MarkPlayerDirty(ClientSession* player)
{
    if (player == nullptr || !databaseReady_)
    {
        return;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return;
    }

    const int playerId = player->GetPlayerId();

    if (playerId < 0)
    {
        return;
    }

    const long long dueMs = GetNowMs() + DIRTY_SAVE_INTERVAL_MS;

    std::lock_guard<std::mutex> lock(dirtySaveLock_);

    auto iter = dirtySaveDueMsByPlayerId_.find(playerId);

    if (iter == dirtySaveDueMsByPlayerId_.end())
    {
        dirtySaveDueMsByPlayerId_[playerId] = dueMs;
    }
}

void GameWorld::FlushDirtySaves(long long nowMs)
{
    std::vector<int> readyPlayerIds;

    {
        std::lock_guard<std::mutex> lock(dirtySaveLock_);

        for (auto iter = dirtySaveDueMsByPlayerId_.begin();
            iter != dirtySaveDueMsByPlayerId_.end();)
        {
            if (iter->second > nowMs)
            {
                ++iter;
                continue;
            }

            readyPlayerIds.push_back(iter->first);
            iter = dirtySaveDueMsByPlayerId_.erase(iter);
        }
    }

    if (readyPlayerIds.empty())
    {
        return;
    }

    std::unordered_set<int> readySet(
        readyPlayerIds.begin(),
        readyPlayerIds.end()
    );

    std::vector<std::shared_ptr<ClientSession>> players =
        GetLoggedInPlayersSnapshot();

    for (const std::shared_ptr<ClientSession>& player : players)
    {
        if (player == nullptr)
        {
            continue;
        }

        if (readySet.find(player->GetPlayerId()) == readySet.end())
        {
            continue;
        }

        SavePlayerCoreToDatabase(player.get());
    }
}

void GameWorld::StartDbWorker()
{
    if (!databaseReady_)
    {
        return;
    }

    bool expected = false;

    if (!dbWorkerRunning_.compare_exchange_strong(expected, true))
    {
        return;
    }

    dbWorkerStopRequested_.store(false);
    dbWorkerThread_ = std::thread(&GameWorld::DbWorkerLoop, this);
}

void GameWorld::StopDbWorker()
{
    dbWorkerStopRequested_.store(true);
    dbJobCv_.notify_all();

    if (dbWorkerThread_.joinable())
    {
        dbWorkerThread_.join();
    }

    dbWorkerRunning_.store(false);
}

void GameWorld::DbWorkerLoop()
{
    while (true)
    {
        DbJob job{};

        {
            std::unique_lock<std::mutex> lock(dbJobLock_);

            dbJobCv_.wait(
                lock,
                [this]()
                {
                    return dbWorkerStopRequested_.load() || !dbJobs_.empty();
                }
            );

            if (dbJobs_.empty())
            {
                if (dbWorkerStopRequested_.load())
                {
                    break;
                }

                continue;
            }

            job = std::move(dbJobs_.front());
            dbJobs_.pop();
        }

        if (job.type == DbJobType::Login)
        {
            ExecuteLoginDbJob(job);
        }
        else if (job.type == DbJobType::Save)
        {
            ExecuteSaveDbJob(job);
        }
    }
}

void GameWorld::ExecuteLoginDbJob(const DbJob& job)
{
    DbLoginCompletion completion{};
    completion.sequence = job.sequence;
    completion.session = job.session;
    completion.sessionId = job.sessionId;
    completion.username = job.username;

    if (!databaseReady_)
    {
        completion.success = true;
        completion.loadedFromDatabase = false;
        PushLoginCompletion(std::move(completion));
        return;
    }

    bool created = false;

    if (!databaseManager_.LoadOrCreatePlayer(
        job.username,
        completion.loadSnapshot.player,
        created
    ))
    {
        Logger::Warn("DB async login load failed. username=" + job.username);
        completion.success = false;
        completion.failMessage = "DB load failed.";
        PushLoginCompletion(std::move(completion));
        return;
    }

    completion.loadSnapshot.created = created;

    if (!databaseManager_.LoadSkills(job.username, completion.loadSnapshot.skills))
    {
        Logger::Warn("DB async skill load failed. username=" + job.username);
    }

    if (!databaseManager_.LoadEquipment(job.username, completion.loadSnapshot.equipment))
    {
        Logger::Warn("DB async equipment load failed. username=" + job.username);
    }

    if (!databaseManager_.LoadInventory(job.username, completion.loadSnapshot.inventoryItems))
    {
        Logger::Warn("DB async inventory load failed. username=" + job.username);
    }

    if (!databaseManager_.LoadQuickSlots(job.username, completion.loadSnapshot.quickSlots))
    {
        Logger::Warn("DB async quickslot load failed. username=" + job.username);
    }

    completion.success = true;
    completion.loadedFromDatabase = true;

    PushLoginCompletion(std::move(completion));
}

void GameWorld::ExecuteSaveDbJob(const DbJob& job)
{
    const PlayerSaveSnapshot& snapshot = job.saveSnapshot;
    const std::string& username = snapshot.player.username;

    if (username.empty() || !databaseReady_)
    {
        return;
    }

    if (!databaseManager_.SavePlayer(snapshot.player))
    {
        Logger::Warn("DB async save player failed. username=" + username);
    }

    if (job.saveMode == DbSaveMode::PlayerOnly)
    {
        return;
    }

    if (!databaseManager_.SaveEquipment(username, snapshot.equipment))
    {
        Logger::Warn("DB async save equipment failed. username=" + username);
    }

    if (!databaseManager_.SaveSkills(username, snapshot.skills))
    {
        Logger::Warn("DB async save skills failed. username=" + username);
    }

    if (!databaseManager_.SaveInventory(username, snapshot.inventoryItems))
    {
        Logger::Warn("DB async save inventory failed. username=" + username);
    }

    if (!databaseManager_.SaveQuickSlots(username, snapshot.quickSlots))
    {
        Logger::Warn("DB async save quickslots failed. username=" + username);
    }
}

void GameWorld::PushLoginCompletion(DbLoginCompletion&& completion)
{
    {
        std::lock_guard<std::mutex> lock(dbCompletionLock_);
        dbLoginCompletions_.push(std::move(completion));
    }
}

void GameWorld::ProcessDbCompletions()
{
    std::queue<DbLoginCompletion> completions;

    {
        std::lock_guard<std::mutex> lock(dbCompletionLock_);
        std::swap(completions, dbLoginCompletions_);
    }

    while (!completions.empty())
    {
        CompleteLoginFromDatabase(std::move(completions.front()));
        completions.pop();
    }
}

void GameWorld::AdjustStressLoginPosition(const std::string& username, int& x, int& y) const
{
    const std::string stressPrefix = "Stress";

    if (username.size() <= stressPrefix.size() ||
        username.compare(0, stressPrefix.size(), stressPrefix) != 0)
    {
        return;
    }

    bool validStressName = true;
    int stressIndex = 0;

    for (std::size_t i = stressPrefix.size(); i < username.size(); ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(username[i]);

        if (!std::isdigit(ch))
        {
            validStressName = false;
            break;
        }

        stressIndex = stressIndex * 10 + (username[i] - '0');
    }

    if (!validStressName)
    {
        return;
    }

    const int columnCount = 100;
    const int startX = 50;
    const int startY = 50;
    const int spacingX = 19;
    const int spacingY = 19;
    const int column = stressIndex % columnCount;
    const int row = stressIndex / columnCount;

    x = startX + column * spacingX;
    y = startY + (row % columnCount) * spacingY;

    if (x >= WORLD_WIDTH)
    {
        x = WORLD_WIDTH - 2;
    }

    if (y >= WORLD_HEIGHT)
    {
        y = WORLD_HEIGHT - 2;
    }
}

void GameWorld::RemovePendingLoginName(const std::string& username, ClientSession* session)
{
    if (username.empty())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(worldLock_);

    auto iter = pendingLoginsByName_.find(username);

    if (iter == pendingLoginsByName_.end())
    {
        return;
    }

    std::shared_ptr<ClientSession> pendingSession = iter->second.lock();

    if (pendingSession == nullptr || pendingSession.get() == session)
    {
        pendingLoginsByName_.erase(iter);
    }
}

void GameWorld::ApplyLoadedSnapshotToSession(
    ClientSession* session,
    const PlayerLoadSnapshot& snapshot,
    int finalX,
    int finalY
)
{
    if (session == nullptr)
    {
        return;
    }

    const DbPlayerData& playerData = snapshot.player;

    session->LoadPersistentState(
        playerData.level,
        playerData.exp,
        playerData.hp,
        playerData.mp,
        finalX,
        finalY,
        playerData.gold,
        playerData.statPoint,
        playerData.skillPoint,
        playerData.str,
        playerData.intStat,
        playerData.dex,
        playerData.luk
    );

    for (const DbSkillData& skill : snapshot.skills)
    {
        session->SetSkillLevelForLoad(skill.skillId, skill.skillLevel);
    }

    if (snapshot.equipment.weaponItemId > 0)
    {
        session->EquipWeapon(snapshot.equipment.weaponItemId, snapshot.equipment.weaponName);
    }

    if (snapshot.equipment.armorItemId > 0)
    {
        session->EquipArmor(snapshot.equipment.armorItemId, snapshot.equipment.armorName);
    }

    if (snapshot.equipment.accessoryItemId > 0)
    {
        session->EquipAccessory(snapshot.equipment.accessoryItemId, snapshot.equipment.accessoryName);
    }

    session->ClampVitalsToCurrentMax();

    for (const DbQuickSlotData& slot : snapshot.quickSlots)
    {
        if (slot.slotType == QUICK_SLOT_TYPE_EMPTY || slot.targetId <= 0)
        {
            session->ClearQuickSlot(slot.slotIndex);
        }
        else
        {
            session->SetQuickSlot(
                slot.slotIndex,
                slot.slotType,
                slot.targetId,
                slot.name
            );
        }
    }
}

void GameWorld::SendLoginResultPacket(ClientSession* session, bool success, const std::string& message)
{
    if (session == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_LoginResult packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_LoginResult));
    packet.type = S2C_LOGIN_RESULT;
    packet.success = success;

    strncpy_s(
        packet.message,
        sizeof(packet.message),
        message.c_str(),
        _TRUNCATE
    );

    sendFunction_(
        session,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendInitialLoginPackets(ClientSession* session)
{
    if (session == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_AvatarInfo avatar{};
    avatar.size = static_cast<unsigned char>(sizeof(S2C_AvatarInfo));
    avatar.type = S2C_AVATAR_INFO;
    avatar.playerId = session->GetPlayerId();
    avatar.x = static_cast<short>(session->GetX());
    avatar.y = static_cast<short>(session->GetY());

    sendFunction_(
        session,
        reinterpret_cast<const char*>(&avatar),
        static_cast<int>(avatar.size)
    );

    SendPlayerStatus(session, 0, 0);
    SendSkillStatus(session);
    SendEquipmentStatus(session);
    SendQuickSlotStatus(session);
    GiveDefaultTestItems(session);
    SendInventorySnapshot(session);

    SendVisibleMonstersTo(session);
    SendVisibleNpcsTo(session);
    SendExistingPlayersTo(session);
    NotifyPlayerEnter(session);
}

void GameWorld::CompleteLoginFromDatabase(DbLoginCompletion&& completion)
{
    std::shared_ptr<ClientSession> sessionRef = completion.session.lock();

    if (sessionRef == nullptr)
    {
        RemovePendingLoginName(completion.username, nullptr);
        return;
    }

    ClientSession* session = sessionRef.get();
    session->EndLogin();

    RemovePendingLoginName(completion.username, session);

    if (session->GetSessionId() != completion.sessionId ||
        session->GetUsername() != completion.username ||
        !session->IsConnected())
    {
        return;
    }

    if (!completion.success)
    {
        SendLoginResultPacket(
            session,
            false,
            completion.failMessage.empty() ? "Login failed." : completion.failMessage
        );
        return;
    }

    int finalX = PLAYER_START_X;
    int finalY = PLAYER_START_Y;

    if (completion.loadedFromDatabase)
    {
        finalX = completion.loadSnapshot.player.x;
        finalY = completion.loadSnapshot.player.y;
        AdjustStressLoginPosition(completion.username, finalX, finalY);
    }

    std::unordered_map<int, InventoryItem> loadedInventory;

    if (completion.loadedFromDatabase)
    {
        for (const DbInventoryItemData& dbItem : completion.loadSnapshot.inventoryItems)
        {
            if (dbItem.itemId <= 0 || dbItem.itemCount <= 0)
            {
                continue;
            }

            InventoryItem item{};
            item.itemId = dbItem.itemId;
            item.itemCategory = dbItem.itemCategory;
            item.itemName = dbItem.itemName;
            item.count = dbItem.itemCount;
            loadedInventory[item.itemId] = item;
        }
    }

    std::string rejectMessage;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        if (session->IsLoggedIn())
        {
            rejectMessage = "Already logged in.";
        }
        else
        {
            auto nameIter = playersByName_.find(completion.username);

            if (nameIter != playersByName_.end())
            {
                std::shared_ptr<ClientSession> existingSession = nameIter->second.lock();

                if (existingSession != nullptr && existingSession->IsLoggedIn())
                {
                    rejectMessage = "This ID is already connected.";
                }
                else
                {
                    playersByName_.erase(nameIter);
                }
            }
        }

        if (rejectMessage.empty() &&
            !CanActorMoveTo(finalX, finalY, -1, completion.sessionId))
        {
            if (!FindSpawnPositionNear(PLAYER_START_X, PLAYER_START_Y, finalX, finalY))
            {
                rejectMessage = "No empty spawn position.";
            }
        }
    }

    if (!rejectMessage.empty())
    {
        SendLoginResultPacket(session, false, rejectMessage);
        return;
    }

    if (completion.loadedFromDatabase)
    {
        ApplyLoadedSnapshotToSession(
            session,
            completion.loadSnapshot,
            finalX,
            finalY
        );
    }
    else
    {
        session->SetPosition(finalX, finalY);
    }

    if (!session->IsConnected())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        if (session->IsLoggedIn())
        {
            rejectMessage = "Already logged in.";
        }
        else
        {
            auto nameIter = playersByName_.find(completion.username);

            if (nameIter != playersByName_.end())
            {
                std::shared_ptr<ClientSession> existingSession = nameIter->second.lock();

                if (existingSession != nullptr && existingSession->IsLoggedIn())
                {
                    rejectMessage = "This ID is already connected.";
                }
                else
                {
                    playersByName_.erase(nameIter);
                }
            }
        }

        if (rejectMessage.empty())
        {
            session->SetLoggedIn(true);
            playersById_[completion.sessionId] = sessionRef;
            playersByName_[completion.username] = sessionRef;
            inventoriesByPlayerId_[completion.sessionId] = std::move(loadedInventory);
            AddPlayerToSector(session);
        }
    }

    if (!rejectMessage.empty())
    {
        SendLoginResultPacket(session, false, rejectMessage);
        return;
    }

    SendLoginResultPacket(session, true, "Login Success.");
    SendInitialLoginPackets(session);

    Logger::Info(
        "World login complete. playerId=" +
        std::to_string(completion.sessionId) +
        ", username=" +
        completion.username
    );
}

bool GameWorld::BeginLoginPlayer(ClientSession* session, const std::string& username, std::string& failMessage)
{
    if (session == nullptr)
    {
        failMessage = "Invalid session.";
        return false;
    }

    if (username.empty())
    {
        failMessage = "Empty username.";
        return false;
    }

    if (!session->TryBeginLogin())
    {
        failMessage = "Login is already pending.";
        return false;
    }

    std::shared_ptr<ClientSession> sessionRef;

    try
    {
        sessionRef = session->shared_from_this();
    }
    catch (const std::bad_weak_ptr&)
    {
        session->EndLogin();
        failMessage = "Invalid session ownership.";
        return false;
    }

    const int playerId = session->GetSessionId();

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        if (session->IsLoggedIn())
        {
            session->EndLogin();
            failMessage = "Already logged in.";
            return false;
        }

        auto pendingIter = pendingLoginsByName_.find(username);

        if (pendingIter != pendingLoginsByName_.end())
        {
            std::shared_ptr<ClientSession> pendingSession = pendingIter->second.lock();

            if (pendingSession != nullptr && pendingSession->IsConnected())
            {
                session->EndLogin();
                failMessage = "This ID is already logging in.";
                return false;
            }

            pendingLoginsByName_.erase(pendingIter);
        }

        auto nameIter = playersByName_.find(username);

        if (nameIter != playersByName_.end())
        {
            std::shared_ptr<ClientSession> existingSession = nameIter->second.lock();

            if (existingSession != nullptr && existingSession->IsLoggedIn())
            {
                session->EndLogin();
                failMessage = "This ID is already connected.";
                return false;
            }

            playersByName_.erase(nameIter);
        }

        pendingLoginsByName_[username] = sessionRef;
    }

    session->SetUsername(username);
    session->SetPlayerId(playerId);
    session->SetLoggedIn(false);

    if (!databaseReady_)
    {
        DbLoginCompletion completion{};
        completion.session = sessionRef;
        completion.sessionId = playerId;
        completion.username = username;
        completion.success = true;
        completion.loadedFromDatabase = false;
        PushLoginCompletion(std::move(completion));
        failMessage.clear();
        return true;
    }

    DbJob job{};
    job.type = DbJobType::Login;
    job.username = username;
    job.session = sessionRef;
    job.sessionId = playerId;

    if (!EnqueueDbJob(std::move(job)))
    {
        RemovePendingLoginName(username, session);
        session->EndLogin();
        failMessage = "DB queue is not available.";
        return false;
    }

    failMessage.clear();
    return true;
}

bool GameWorld::LoginPlayer(ClientSession* session, const std::string& username, std::string& failMessage)
{
    return BeginLoginPlayer(session, username, failMessage);
}

void GameWorld::LogoutPlayer(ClientSession* session)
{
    if (session == nullptr)
    {
        return;
    }

    const int playerId = session->GetPlayerId();
    const std::string username = session->GetUsername();

    SavePlayerDataToDatabase(session);

    bool wasLoggedIn = false;
    std::vector<std::shared_ptr<ClientSession>> receivers;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        if (!session->IsLoggedIn())
        {
            return;
        }

        wasLoggedIn = true;

        RemovePlayerFromSector(playerId, session->GetX(), session->GetY());

        playersById_.erase(playerId);
        RemoveVisibleState(playerId);

        auto nameIter = playersByName_.find(username);

        if (nameIter != playersByName_.end())
        {
            std::shared_ptr<ClientSession> nameOwner = nameIter->second.lock();

            if (nameOwner == nullptr || nameOwner.get() == session)
            {
                playersByName_.erase(nameIter);
            }
        }

        for (auto& [id, other] : playersById_)
        {
            if (other != nullptr && other->IsConnected() && other->IsLoggedIn())
            {
                receivers.push_back(other);
            }
        }
    }

    session->SetLoggedIn(false);

    if (wasLoggedIn)
    {
        for (const std::shared_ptr<ClientSession>& receiver : receivers)
        {
            if (receiver == nullptr)
            {
                continue;
            }

            bool shouldSendRemove = false;

            auto stateIter = visibleStates_.find(receiver->GetPlayerId());

            if (stateIter != visibleStates_.end() && stateIter->second != nullptr)
            {
                std::lock_guard<std::mutex> lock(stateIter->second->lock);

                const std::size_t erasedCount =
                    stateIter->second->visiblePlayerIds.erase(playerId);

                shouldSendRemove = erasedCount > 0;
            }

            if (shouldSendRemove)
            {
                SendRemovePlayer(receiver.get(), playerId);
            }
        }

        Logger::Info(
            "World logout. playerId=" +
            std::to_string(playerId) +
            ", username=" +
            username
        );
    }
}

bool GameWorld::MovePlayer(ClientSession* session, DIRECTION direction)
{
    if (session == nullptr)
    {
        return false;
    }

    if (!session->IsConnected() || !session->IsLoggedIn())
    {
        return false;
    }

    if (!session->IsAlive())
    {
        SendBattleMessage(session, "You cannot move while dead.");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        session->SetFacingDirection(direction);

        int dx = 0;
        int dy = 0;

        switch (static_cast<int>(direction))
        {
        case 0:
            dy = -1;
            break;

        case 1:
            dy = 1;
            break;

        case 2:
            dx = -1;
            break;

        case 3:
            dx = 1;
            break;

        default:
            return false;
        }

        if (!session->TryConsumeMoveCooldown(ServerConfig::MOVE_INTERVAL_MS))
        {
            return false;
        }

        const int nextX = session->GetX() + dx;
        const int nextY = session->GetY() + dy;

        if (!CanActorMoveTo(nextX, nextY, -1, session->GetPlayerId()))
        {
            Logger::Info(
                "Move blocked. playerId=" +
                std::to_string(session->GetPlayerId()) +
                ", x=" +
                std::to_string(nextX) +
                ", y=" +
                std::to_string(nextY)
            );

            SendBattleMessage(session, "You cannot move there.");

            return false;
        }

        const int oldX = session->GetX();
        const int oldY = session->GetY();

        session->SetPosition(nextX, nextY);

        MovePlayerSectorIfNeeded(session, oldX, oldY, nextX, nextY);
    }

    TryPickupDropsAt(session);
    MarkPlayerDirty(session);

    return true;
}

void GameWorld::SendExistingPlayersTo(ClientSession* newPlayer)
{
    if (newPlayer == nullptr)
    {
        return;
    }

    if (!newPlayer->IsConnected() || !newPlayer->IsLoggedIn())
    {
        return;
    }

    std::vector<std::shared_ptr<ClientSession>> visiblePlayers;
    std::unordered_set<int> visiblePlayerIds;

    BuildVisiblePlayerSnapshot(newPlayer, visiblePlayers, visiblePlayerIds);

    std::shared_ptr<VisibleState> visibleState = GetOrCreateVisibleState(newPlayer->GetPlayerId());

    if (visibleState != nullptr)
    {
        std::lock_guard<std::mutex> lock(visibleState->lock);
        visibleState->visiblePlayerIds = std::move(visiblePlayerIds);
    }

    for (const std::shared_ptr<ClientSession>& other : visiblePlayers)
    {
        if (other == nullptr)
        {
            continue;
        }

        SendAddPlayer(newPlayer, other.get());
    }
}

void GameWorld::NotifyPlayerEnter(ClientSession* newPlayer)
{
    if (newPlayer == nullptr)
    {
        return;
    }

    if (!newPlayer->IsConnected() || !newPlayer->IsLoggedIn())
    {
        return;
    }

    std::vector<std::shared_ptr<ClientSession>> receivers;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (auto& [id, other] : playersById_)
        {
            if (other == nullptr || other.get() == newPlayer)
            {
                continue;
            }

            if (!other->IsConnected() || !other->IsLoggedIn())
            {
                continue;
            }

            if (!IsInViewRange(other.get(), newPlayer->GetX(), newPlayer->GetY()))
            {
                continue;
            }

            receivers.push_back(other);
        }
    }

    const int newPlayerId = newPlayer->GetPlayerId();

    for (const std::shared_ptr<ClientSession>& receiver : receivers)
    {
        if (receiver == nullptr)
        {
            continue;
        }

        bool shouldSend = false;

        std::shared_ptr<VisibleState> visibleState = GetOrCreateVisibleState(receiver->GetPlayerId());

        if (visibleState != nullptr)
        {
            std::lock_guard<std::mutex> lock(visibleState->lock);

            auto [iter, inserted] = visibleState->visiblePlayerIds.insert(newPlayerId);
            shouldSend = inserted;
        }

        if (shouldSend)
        {
            SendAddPlayer(receiver.get(), newPlayer);
        }
    }
}

void GameWorld::BroadcastMove(ClientSession* movedPlayer, int moveTime)
{
    if (movedPlayer == nullptr)
    {
        return;
    }

    if (!movedPlayer->IsConnected() || !movedPlayer->IsLoggedIn())
    {
        return;
    }

    std::vector<int> receiverIds;

    std::shared_ptr<VisibleState> movedVisibleState =
        GetOrCreateVisibleState(movedPlayer->GetPlayerId());

    if (movedVisibleState != nullptr)
    {
        std::lock_guard<std::mutex> lock(movedVisibleState->lock);
        receiverIds.reserve(movedVisibleState->visiblePlayerIds.size());

        for (int playerId : movedVisibleState->visiblePlayerIds)
        {
            receiverIds.push_back(playerId);
        }
    }

    std::vector<std::shared_ptr<ClientSession>> receivers;
    receivers.reserve(receiverIds.size());

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (int receiverId : receiverIds)
        {
            auto iter = playersById_.find(receiverId);

            if (iter == playersById_.end() || iter->second == nullptr)
            {
                continue;
            }

            if (!iter->second->IsConnected() || !iter->second->IsLoggedIn())
            {
                continue;
            }

            receivers.push_back(iter->second);
        }
    }

    SendMovePlayer(movedPlayer, movedPlayer, moveTime);

    for (const std::shared_ptr<ClientSession>& receiver : receivers)
    {
        SendMovePlayer(receiver.get(), movedPlayer, moveTime);
    }
}

void GameWorld::SendVisibleMonstersTo(ClientSession* receiver)
{
    if (receiver == nullptr)
    {
        return;
    }

    if (!receiver->IsConnected() || !receiver->IsLoggedIn())
    {
        return;
    }

    std::vector<Monster> visibleMonsters;
    std::unordered_set<int> visibleMonsterIds;

    BuildVisibleMonsterSnapshot(receiver, visibleMonsters, visibleMonsterIds);

    std::shared_ptr<VisibleState> visibleState = GetOrCreateVisibleState(receiver->GetPlayerId());

    if (visibleState != nullptr)
    {
        std::lock_guard<std::mutex> lock(visibleState->lock);
        visibleState->visibleMonsterIds = std::move(visibleMonsterIds);
    }

    for (const Monster& monster : visibleMonsters)
    {
        SendAddMonster(receiver, monster);
    }
}

void GameWorld::SendVisibleNpcsTo(ClientSession* receiver)
{
    if (receiver == nullptr)
    {
        return;
    }

    if (!receiver->IsConnected() || !receiver->IsLoggedIn())
    {
        return;
    }

    std::vector<Npc> npcs;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (const auto& [npcId, npc] : npcsById_)
        {
            if (IsInViewRange(receiver, npc.x, npc.y))
            {
                npcs.push_back(npc);
            }
        }
    }

    for (const Npc& npc : npcs)
    {
        SendAddNpc(receiver, npc);
    }
}

void GameWorld::UpdateMonsterVisibilityFor(ClientSession* receiver)
{
    if (receiver == nullptr)
    {
        return;
    }

    if (!receiver->IsConnected() || !receiver->IsLoggedIn())
    {
        return;
    }

    std::vector<Monster> newVisibleMonsters;
    std::unordered_set<int> newVisibleMonsterIds;

    BuildVisibleMonsterSnapshot(receiver, newVisibleMonsters, newVisibleMonsterIds);

    std::vector<Monster> monstersToAdd;
    std::vector<int> monsterIdsToRemove;

    std::shared_ptr<VisibleState> visibleState = GetOrCreateVisibleState(receiver->GetPlayerId());

    if (visibleState == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(visibleState->lock);

        for (const Monster& monster : newVisibleMonsters)
        {
            if (visibleState->visibleMonsterIds.find(monster.monsterId) == visibleState->visibleMonsterIds.end())
            {
                monstersToAdd.push_back(monster);
            }
        }

        for (int oldMonsterId : visibleState->visibleMonsterIds)
        {
            if (newVisibleMonsterIds.find(oldMonsterId) == newVisibleMonsterIds.end())
            {
                monsterIdsToRemove.push_back(oldMonsterId);
            }
        }

        visibleState->visibleMonsterIds = std::move(newVisibleMonsterIds);
    }

    for (int monsterId : monsterIdsToRemove)
    {
        SendRemoveMonster(receiver, monsterId);
    }

    for (const Monster& monster : monstersToAdd)
    {
        SendAddMonster(receiver, monster);
    }
}

void GameWorld::UpdatePlayerVisibilityFor(ClientSession* movedPlayer)
{
    if (movedPlayer == nullptr)
    {
        return;
    }

    if (!movedPlayer->IsConnected() || !movedPlayer->IsLoggedIn())
    {
        return;
    }

    const int movedPlayerId = movedPlayer->GetPlayerId();

    std::vector<std::shared_ptr<ClientSession>> newVisiblePlayers;
    std::unordered_set<int> newVisiblePlayerIds;

    BuildVisiblePlayerSnapshot(movedPlayer, newVisiblePlayers, newVisiblePlayerIds);

    std::vector<std::shared_ptr<ClientSession>> playersToAddToMoved;
    std::vector<int> playerIdsToRemoveFromMoved;
    std::unordered_set<int> oldVisiblePlayerIds;

    std::shared_ptr<VisibleState> movedVisibleState = GetOrCreateVisibleState(movedPlayerId);

    if (movedVisibleState != nullptr)
    {
        std::lock_guard<std::mutex> lock(movedVisibleState->lock);
        oldVisiblePlayerIds = movedVisibleState->visiblePlayerIds;

        for (const std::shared_ptr<ClientSession>& other : newVisiblePlayers)
        {
            if (other == nullptr)
            {
                continue;
            }

            if (oldVisiblePlayerIds.find(other->GetPlayerId()) == oldVisiblePlayerIds.end())
            {
                playersToAddToMoved.push_back(other);
            }
        }

        for (int oldPlayerId : oldVisiblePlayerIds)
        {
            if (newVisiblePlayerIds.find(oldPlayerId) == newVisiblePlayerIds.end())
            {
                playerIdsToRemoveFromMoved.push_back(oldPlayerId);
            }
        }

        movedVisibleState->visiblePlayerIds = newVisiblePlayerIds;
    }

    std::unordered_set<int> candidatePlayerIds = newVisiblePlayerIds;

    for (int oldPlayerId : oldVisiblePlayerIds)
    {
        candidatePlayerIds.insert(oldPlayerId);
    }

    std::vector<std::shared_ptr<ClientSession>> candidatePlayers;
    candidatePlayers.reserve(candidatePlayerIds.size());

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (int playerId : candidatePlayerIds)
        {
            auto iter = playersById_.find(playerId);

            if (iter == playersById_.end() || iter->second == nullptr)
            {
                continue;
            }

            if (!iter->second->IsConnected() || !iter->second->IsLoggedIn())
            {
                continue;
            }

            candidatePlayers.push_back(iter->second);
        }
    }

    std::vector<std::shared_ptr<ClientSession>> addMovedToReceivers;
    std::vector<std::shared_ptr<ClientSession>> removeMovedFromReceivers;

    for (const std::shared_ptr<ClientSession>& other : candidatePlayers)
    {
        if (other == nullptr || other.get() == movedPlayer)
        {
            continue;
        }

        std::shared_ptr<VisibleState> otherVisibleState = GetOrCreateVisibleState(other->GetPlayerId());

        if (otherVisibleState == nullptr)
        {
            continue;
        }

        const bool canSeeMovedNow =
            newVisiblePlayerIds.find(other->GetPlayerId()) != newVisiblePlayerIds.end();

        bool shouldAdd = false;
        bool shouldRemove = false;

        {
            std::lock_guard<std::mutex> lock(otherVisibleState->lock);

            const bool wasVisible =
                otherVisibleState->visiblePlayerIds.find(movedPlayerId) !=
                otherVisibleState->visiblePlayerIds.end();

            if (canSeeMovedNow && !wasVisible)
            {
                otherVisibleState->visiblePlayerIds.insert(movedPlayerId);
                shouldAdd = true;
            }
            else if (!canSeeMovedNow && wasVisible)
            {
                otherVisibleState->visiblePlayerIds.erase(movedPlayerId);
                shouldRemove = true;
            }
        }

        if (shouldAdd)
        {
            addMovedToReceivers.push_back(other);
        }

        if (shouldRemove)
        {
            removeMovedFromReceivers.push_back(other);
        }
    }

    for (int playerId : playerIdsToRemoveFromMoved)
    {
        SendRemovePlayer(movedPlayer, playerId);
    }

    for (const std::shared_ptr<ClientSession>& player : playersToAddToMoved)
    {
        if (player != nullptr)
        {
            SendAddPlayer(movedPlayer, player.get());
        }
    }

    for (const std::shared_ptr<ClientSession>& receiver : removeMovedFromReceivers)
    {
        if (receiver != nullptr)
        {
            SendRemovePlayer(receiver.get(), movedPlayerId);
        }
    }

    for (const std::shared_ptr<ClientSession>& receiver : addMovedToReceivers)
    {
        if (receiver != nullptr)
        {
            SendAddPlayer(receiver.get(), movedPlayer);
        }
    }
}

std::vector<std::shared_ptr<ClientSession>> GameWorld::GetLoggedInPlayersSnapshot()
{
    std::vector<std::shared_ptr<ClientSession>> result;

    std::lock_guard<std::mutex> lock(worldLock_);

    result.reserve(playersById_.size());

    for (auto& [id, session] : playersById_)
    {
        if (session != nullptr && session->IsConnected() && session->IsLoggedIn())
        {
            result.push_back(session);
        }
    }

    return result;
}

void GameWorld::SendAddPlayer(ClientSession* receiver, ClientSession* target)
{
    if (receiver == nullptr || target == nullptr)
    {
        return;
    }

    S2C_AddPlayer packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_AddPlayer));
    packet.type = S2C_ADD_PLAYER;
    packet.playerId = target->GetPlayerId();
    packet.x = static_cast<short>(target->GetX());
    packet.y = static_cast<short>(target->GetY());

    strncpy_s(
        packet.username,
        sizeof(packet.username),
        target->GetUsername().c_str(),
        _TRUNCATE
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendRemovePlayer(ClientSession* receiver, int removedPlayerId)
{
    if (receiver == nullptr)
    {
        return;
    }

    S2C_RemovePlayer packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_RemovePlayer));
    packet.type = S2C_REMOVE_PLAYER;
    packet.playerId = removedPlayerId;

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendMovePlayer(ClientSession* receiver, ClientSession* movedPlayer, int moveTime)
{
    if (receiver == nullptr || movedPlayer == nullptr)
    {
        return;
    }

    S2C_MovePlayer packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_MovePlayer));
    packet.type = S2C_MOVE_PLAYER;
    packet.playerId = movedPlayer->GetPlayerId();
    packet.x = static_cast<short>(movedPlayer->GetX());
    packet.y = static_cast<short>(movedPlayer->GetY());
    packet.move_time = moveTime;

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::LoadItemSellPrices()
{
    itemSellPrices_.clear();

    const char* candidatePaths[] =
    {
        "Scripts/item_prices.lua",
        "Server/Scripts/item_prices.lua",
        "../Server/Scripts/item_prices.lua"
    };

    lua_State* L = luaL_newstate();

    if (L == nullptr)
    {
        Logger::Warn("Failed to create Lua state for item_prices.lua. Using default sell prices.");
        return;
    }

    luaL_openlibs(L);

    std::string loadedPath;
    std::string errorMessage;

    const bool loaded =
        RunLuaFileFromCandidates(
            L,
            candidatePaths,
            static_cast<int>(sizeof(candidatePaths) / sizeof(candidatePaths[0])),
            loadedPath,
            errorMessage
        );

    if (!loaded)
    {
        Logger::Warn(
            "item_prices.lua load failed. Using default sell prices. error=" +
            errorMessage
        );
        lua_close(L);
        return;
    }

    if (!lua_istable(L, -1))
    {
        Logger::Warn("item_prices.lua must return a table. Using default sell prices.");
        lua_close(L);
        return;
    }

    int loadedCount = 0;

    lua_pushnil(L);

    while (lua_next(L, -2) != 0)
    {
        if (lua_isnumber(L, -2) && lua_isnumber(L, -1))
        {
            const int itemId = static_cast<int>(lua_tointeger(L, -2));
            const int price = static_cast<int>(lua_tointeger(L, -1));

            if (itemId > 0 && price > 0)
            {
                itemSellPrices_[itemId] = price;
                ++loadedCount;
            }
        }

        lua_pop(L, 1);
    }

    lua_close(L);

    Logger::Info(
        "Loaded Lua item sell prices with Lua API. path=" +
        loadedPath +
        ", count=" +
        std::to_string(loadedCount)
    );
}

int GameWorld::GetSellPriceByItemId(int itemId) const
{
    const auto iter = itemSellPrices_.find(itemId);

    if (iter != itemSellPrices_.end())
    {
        return iter->second;
    }

    return GetDefaultSellPriceByItemId(itemId);
}

void GameWorld::InitializeMap()
{
    mapWidth_ = WORLD_WIDTH;
    mapHeight_ = WORLD_HEIGHT;

    blockedTiles_.clear();
    blockedTiles_.resize(static_cast<std::size_t>(mapWidth_) * mapHeight_, 0);

    FillRect(80, 80, 130, 130, false);

    FillRect(105, 98, 105, 102, true);

    FillRect(400, 100, 430, 650, true);

    FillRect(700, 300, 1200, 330, true);

    FillRect(1000, 700, 1030, 1300, true);

    FillRect(1600, 1500, 1900, 1530, true);
    FillRect(1600, 1530, 1630, 1900, true);
    FillRect(1870, 1530, 1900, 1900, true);

    FillRect(1730, 1500, 1770, 1530, false);
}

bool GameWorld::IsInsideMap(int x, int y) const
{
    return x >= 0 && x < mapWidth_ && y >= 0 && y < mapHeight_;
}

bool GameWorld::IsBlocked(int x, int y) const
{
    if (!IsInsideMap(x, y))
    {
        return true;
    }

    return blockedTiles_[ToMapIndex(x, y)] != 0;
}

bool GameWorld::CanMoveTo(int x, int y) const
{
    if (!IsInsideMap(x, y))
    {
        return false;
    }

    if (IsBlocked(x, y))
    {
        return false;
    }

    return true;
}

void GameWorld::SetBlocked(int x, int y, bool blocked)
{
    if (!IsInsideMap(x, y))
    {
        return;
    }

    blockedTiles_[ToMapIndex(x, y)] = blocked ? 1 : 0;
}

void GameWorld::FillRect(int left, int top, int right, int bottom, bool blocked)
{
    left = std::max(0, left);
    top = std::max(0, top);
    right = std::min(mapWidth_ - 1, right);
    bottom = std::min(mapHeight_ - 1, bottom);

    if (left > right || top > bottom)
    {
        return;
    }

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
        {
            SetBlocked(x, y, blocked);
        }
    }
}

int GameWorld::ToMapIndex(int x, int y) const
{
    return y * mapWidth_ + x;
}

void GameWorld::InitializeMonsters()
{
    const long long nowMs = GetNowMs();

    if (LoadMonstersFromLua(nowMs))
    {
        return;
    }

    auto addMonster =
        [this, nowMs](
            int monsterId,
            int monsterType,
            const std::string& name,
            int level,
            int hp,
            int attackPower,
            int x,
            int y,
            MonsterAiType aiType,
            MonsterMoveType moveType
        )
        {
            Monster monster{};
            monster.monsterId = monsterId;
            monster.monsterType = monsterType;
            monster.name = name;
            monster.level = level;
            monster.hp = hp;
            monster.maxHp = hp;
            monster.attackPower = attackPower;
            monster.x = x;
            monster.y = y;
            monster.spawnX = x;
            monster.spawnY = y;
            monster.alive = true;
            monster.moveType = moveType;
            monster.aiType = aiType;
            monster.targetPlayerId = -1;
            monster.nextMoveTimeMs = nowMs + MONSTER_ROAM_INTERVAL_MS;
            monster.nextAttackTimeMs = 0;
            monster.respawnTimeMs = 0;
            monster.damageByPlayer.clear();
            AddMonsterToWorld(monster);
        };

    addMonster(NPC_ID_START + 1, MONSTER_SLIME, "Slime", 1, 30, 3, 102, 100, MonsterAiType::Peace, MonsterMoveType::Roaming);
    addMonster(NPC_ID_START + 2, MONSTER_GOBLIN, "Goblin", 3, 60, 5, 103, 101, MonsterAiType::Peace, MonsterMoveType::Roaming);
    addMonster(NPC_ID_START + 3, MONSTER_WOLF, "Wolf", 5, 90, 8, 101, 103, MonsterAiType::Agro, MonsterMoveType::Roaming);
    addMonster(NPC_ID_START + 4, MONSTER_ORC, "Orc", 8, 150, 12, 98, 100, MonsterAiType::Agro, MonsterMoveType::Roaming);
    addMonster(NPC_ID_START + 5, MONSTER_SKELETON, "Skeleton", 10, 180, 14, 100, 98, MonsterAiType::Peace, MonsterMoveType::Roaming);
    addMonster(NPC_ID_START + 6, MONSTER_DARK_MAGE, "DarkMage", 12, 120, 16, 97, 102, MonsterAiType::Agro, MonsterMoveType::Roaming);

    Logger::Info("Initialized monsters from fallback data. count=6");
}

bool GameWorld::LoadMonstersFromLua(long long nowMs)
{
    const char* candidatePaths[] =
    {
        "Scripts/monsters.lua",
        "Server/Scripts/monsters.lua",
        "../Server/Scripts/monsters.lua"
    };

    lua_State* L = luaL_newstate();

    if (L == nullptr)
    {
        Logger::Warn("Failed to create Lua state for monsters.lua. Using fallback monster data.");
        return false;
    }

    luaL_openlibs(L);

    std::string loadedPath;
    std::string errorMessage;

    const bool loaded =
        RunLuaFileFromCandidates(
            L,
            candidatePaths,
            static_cast<int>(sizeof(candidatePaths) / sizeof(candidatePaths[0])),
            loadedPath,
            errorMessage
        );

    if (!loaded)
    {
        Logger::Warn(
            "monsters.lua load failed. Using fallback monster data. error=" +
            errorMessage
        );
        lua_close(L);
        return false;
    }

    if (!lua_istable(L, -1))
    {
        Logger::Warn("monsters.lua must return a table. Using fallback monster data.");
        lua_close(L);
        return false;
    }

    int loadedCount = 0;

    lua_pushnil(L);

    while (lua_next(L, -2) != 0)
    {
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        Monster monster{};

        monster.monsterId = GetLuaIntField(L, -1, "id", -1);
        monster.monsterType = GetLuaIntField(L, -1, "type", 0);
        monster.name = GetLuaStringField(L, -1, "name", "Monster");
        monster.level = GetLuaIntField(L, -1, "level", 1);
        monster.maxHp = GetLuaIntField(L, -1, "hp", 30);
        monster.hp = monster.maxHp;
        monster.attackPower = GetLuaIntField(L, -1, "attack", 3);
        monster.x = GetLuaIntField(L, -1, "x", -1);
        monster.y = GetLuaIntField(L, -1, "y", -1);

        if (monster.monsterId <= 0 ||
            monster.monsterType <= 0 ||
            monster.x < 0 ||
            monster.y < 0)
        {
            lua_pop(L, 1);
            continue;
        }

        const std::string aiText = GetLuaStringField(L, -1, "ai", "Peace");
        const std::string moveText = GetLuaStringField(L, -1, "move", "Fixed");

        monster.aiType =
            aiText == "Agro"
            ? MonsterAiType::Agro
            : MonsterAiType::Peace;

        monster.moveType =
            moveText == "Roaming"
            ? MonsterMoveType::Roaming
            : MonsterMoveType::Fixed;

        monster.spawnX = monster.x;
        monster.spawnY = monster.y;
        monster.alive = true;
        monster.targetPlayerId = -1;
        monster.nextMoveTimeMs = nowMs + MONSTER_ROAM_INTERVAL_MS;
        monster.nextAttackTimeMs = 0;
        monster.respawnTimeMs = 0;
        monster.damageByPlayer.clear();

        AddMonsterToWorld(monster);
        ++loadedCount;

        lua_pop(L, 1);
    }

    lua_close(L);

    if (loadedCount <= 0)
    {
        Logger::Warn("monsters.lua contained no valid monsters. Using fallback monster data.");
        return false;
    }

    Logger::Info(
        "Loaded Lua monsters with Lua API. path=" +
        loadedPath +
        ", count=" +
        std::to_string(loadedCount)
    );

    return true;
}

bool GameWorld::IsInViewRange(ClientSession* viewer, int targetX, int targetY) const
{
    if (viewer == nullptr)
    {
        return false;
    }

    const int viewerX = viewer->GetX();
    const int viewerY = viewer->GetY();

    const int dx = std::abs(viewerX - targetX);
    const int dy = std::abs(viewerY - targetY);

    return dx <= VIEW_RADIUS && dy <= VIEW_RADIUS;
}

int GameWorld::GetSectorCountX() const
{
    return (mapWidth_ + SECTOR_SIZE - 1) / SECTOR_SIZE;
}

int GameWorld::GetSectorCountY() const
{
    return (mapHeight_ + SECTOR_SIZE - 1) / SECTOR_SIZE;
}

int GameWorld::GetSectorXFromPosition(int x) const
{
    if (x < 0)
    {
        return 0;
    }

    int sectorX = x / SECTOR_SIZE;
    const int maxSectorX = GetSectorCountX() - 1;

    if (sectorX > maxSectorX)
    {
        sectorX = maxSectorX;
    }

    return sectorX;
}

int GameWorld::GetSectorYFromPosition(int y) const
{
    if (y < 0)
    {
        return 0;
    }

    int sectorY = y / SECTOR_SIZE;
    const int maxSectorY = GetSectorCountY() - 1;

    if (sectorY > maxSectorY)
    {
        sectorY = maxSectorY;
    }

    return sectorY;
}

int GameWorld::GetSectorIndexFromPosition(int x, int y) const
{
    const int sectorX = GetSectorXFromPosition(x);
    const int sectorY = GetSectorYFromPosition(y);

    return GetSectorIndexFromCoord(sectorX, sectorY);
}

int GameWorld::GetSectorIndexFromCoord(int sectorX, int sectorY) const
{
    return sectorY * GetSectorCountX() + sectorX;
}

std::shared_ptr<GameWorld::Sector> GameWorld::GetOrCreateSector(int sectorIndex)
{
    auto iter = sectors_.find(sectorIndex);

    if (iter != sectors_.end())
    {
        return iter->second;
    }

    std::shared_ptr<Sector> newSector = std::make_shared<Sector>();

    sectors_.insert(std::make_pair(sectorIndex, newSector));

    iter = sectors_.find(sectorIndex);

    if (iter != sectors_.end())
    {
        return iter->second;
    }

    return newSector;
}

std::shared_ptr<GameWorld::Sector> GameWorld::FindSector(int sectorIndex)
{
    auto iter = sectors_.find(sectorIndex);

    if (iter == sectors_.end())
    {
        return nullptr;
    }

    return iter->second;
}

void GameWorld::AddPlayerToSector(ClientSession* session)
{
    if (session == nullptr)
    {
        return;
    }

    const int sectorIndex = GetSectorIndexFromPosition(session->GetX(), session->GetY());

    std::shared_ptr<Sector> sector = GetOrCreateSector(sectorIndex);

    if (sector == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sector->lock);
    sector->playerIds.insert(session->GetPlayerId());
}

void GameWorld::RemovePlayerFromSector(int playerId, int x, int y)
{
    const int sectorIndex = GetSectorIndexFromPosition(x, y);

    std::shared_ptr<Sector> sector = FindSector(sectorIndex);

    if (sector == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sector->lock);
    sector->playerIds.erase(playerId);
}

void GameWorld::MovePlayerSectorIfNeeded(ClientSession* session, int oldX, int oldY, int newX, int newY)
{
    if (session == nullptr)
    {
        return;
    }

    const int oldSectorIndex = GetSectorIndexFromPosition(oldX, oldY);
    const int newSectorIndex = GetSectorIndexFromPosition(newX, newY);

    if (oldSectorIndex == newSectorIndex)
    {
        return;
    }

    RemovePlayerFromSector(session->GetPlayerId(), oldX, oldY);
    AddPlayerToSector(session);
}

void GameWorld::InitializeNpcs()
{
    if (LoadNpcsFromLua())
    {
        return;
    }

    Npc merchant{};
    merchant.npcId = 1;
    merchant.npcType = NPC_TYPE_MERCHANT;
    merchant.name = "Merchant";
    merchant.x = 102;
    merchant.y = 100;

    npcsById_[merchant.npcId] = merchant;

    Logger::Info("Initialized NPCs from fallback data. count=1");
}

bool GameWorld::LoadNpcsFromLua()
{
    const char* candidatePaths[] =
    {
        "Scripts/npcs.lua",
        "Server/Scripts/npcs.lua",
        "../Server/Scripts/npcs.lua"
    };

    lua_State* L = luaL_newstate();

    if (L == nullptr)
    {
        Logger::Warn("Failed to create Lua state for npcs.lua. Using fallback NPC data.");
        return false;
    }

    luaL_openlibs(L);

    std::string loadedPath;
    std::string errorMessage;

    const bool loaded =
        RunLuaFileFromCandidates(
            L,
            candidatePaths,
            static_cast<int>(sizeof(candidatePaths) / sizeof(candidatePaths[0])),
            loadedPath,
            errorMessage
        );

    if (!loaded)
    {
        Logger::Warn(
            "npcs.lua load failed. Using fallback NPC data. error=" +
            errorMessage
        );
        lua_close(L);
        return false;
    }

    if (!lua_istable(L, -1))
    {
        Logger::Warn("npcs.lua must return a table. Using fallback NPC data.");
        lua_close(L);
        return false;
    }

    int loadedCount = 0;

    lua_pushnil(L);

    while (lua_next(L, -2) != 0)
    {
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        Npc npc{};

        npc.npcId = GetLuaIntField(L, -1, "id", -1);
        npc.npcType = GetLuaIntField(L, -1, "type", 0);
        npc.name = GetLuaStringField(L, -1, "name", "NPC");
        npc.x = GetLuaIntField(L, -1, "x", -1);
        npc.y = GetLuaIntField(L, -1, "y", -1);

        if (npc.npcId <= 0 ||
            npc.npcType <= 0 ||
            npc.x < 0 ||
            npc.y < 0)
        {
            lua_pop(L, 1);
            continue;
        }

        npcsById_[npc.npcId] = npc;
        ++loadedCount;

        lua_pop(L, 1);
    }

    lua_close(L);

    if (loadedCount <= 0)
    {
        Logger::Warn("npcs.lua contained no valid NPCs. Using fallback NPC data.");
        return false;
    }

    Logger::Info(
        "Loaded Lua NPCs with Lua API. path=" +
        loadedPath +
        ", count=" +
        std::to_string(loadedCount)
    );

    return true;
}

void GameWorld::AddMonsterToWorld(const Monster& monster)
{
    std::shared_ptr<Monster> monsterPtr = std::make_shared<Monster>(monster);

    monstersById_.insert(std::make_pair(monster.monsterId, monsterPtr));

    AddMonsterToSector(monster.monsterId, monster.x, monster.y);
}

void GameWorld::AddMonsterToSector(int monsterId, int x, int y)
{
    const int sectorIndex = GetSectorIndexFromPosition(x, y);

    std::shared_ptr<Sector> sector = GetOrCreateSector(sectorIndex);

    if (sector == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sector->lock);
    sector->monsterIds.insert(monsterId);
}

void GameWorld::CollectCandidateMonsterIdsInViewRange( int centerX, int centerY, std::unordered_set<int>& outMonsterIds )
{
    outMonsterIds.clear();

    const int left = std::max(0, centerX - VIEW_RADIUS);
    const int right = std::min(mapWidth_ - 1, centerX + VIEW_RADIUS);
    const int top = std::max(0, centerY - VIEW_RADIUS);
    const int bottom = std::min(mapHeight_ - 1, centerY + VIEW_RADIUS);

    const int minSectorX = GetSectorXFromPosition(left);
    const int maxSectorX = GetSectorXFromPosition(right);
    const int minSectorY = GetSectorYFromPosition(top);
    const int maxSectorY = GetSectorYFromPosition(bottom);

    for (int sectorY = minSectorY; sectorY <= maxSectorY; ++sectorY)
    {
        for (int sectorX = minSectorX; sectorX <= maxSectorX; ++sectorX)
        {
            const int sectorIndex = GetSectorIndexFromCoord(sectorX, sectorY);

            std::shared_ptr<Sector> sector = FindSector(sectorIndex);

            if (sector == nullptr)
            {
                continue;
            }

            std::lock_guard<std::mutex> lock(sector->lock);

            for (int monsterId : sector->monsterIds)
            {
                outMonsterIds.insert(monsterId);
            }
        }
    }
}

void GameWorld::CollectCandidatePlayerIdsInViewRange( int centerX, int centerY, std::unordered_set<int>& outPlayerIds )

{
    outPlayerIds.clear();

    const int left = std::max(0, centerX - VIEW_RADIUS);
    const int right = std::min(mapWidth_ - 1, centerX + VIEW_RADIUS);
    const int top = std::max(0, centerY - VIEW_RADIUS);
    const int bottom = std::min(mapHeight_ - 1, centerY + VIEW_RADIUS);

    const int minSectorX = GetSectorXFromPosition(left);
    const int maxSectorX = GetSectorXFromPosition(right);
    const int minSectorY = GetSectorYFromPosition(top);
    const int maxSectorY = GetSectorYFromPosition(bottom);

    for (int sectorY = minSectorY; sectorY <= maxSectorY; ++sectorY)
    {
        for (int sectorX = minSectorX; sectorX <= maxSectorX; ++sectorX)
        {
            const int sectorIndex = GetSectorIndexFromCoord(sectorX, sectorY);

            std::shared_ptr<Sector> sector = FindSector(sectorIndex);

            if (sector == nullptr)
            {
                continue;
            }

            std::lock_guard<std::mutex> lock(sector->lock);

            for (int playerId : sector->playerIds)
            {
                outPlayerIds.insert(playerId);
            }
        }
    }
}

std::shared_ptr<GameWorld::VisibleState> GameWorld::GetOrCreateVisibleState(int playerId)
{
    auto iter = visibleStates_.find(playerId);

    if (iter != visibleStates_.end())
    {
        return iter->second;
    }

    std::shared_ptr<VisibleState> newState = std::make_shared<VisibleState>();

    visibleStates_.insert(std::make_pair(playerId, newState));

    iter = visibleStates_.find(playerId);

    if (iter != visibleStates_.end())
    {
        return iter->second;
    }

    return newState;
}

void GameWorld::RemoveVisibleState(int playerId)
{
    visibleStates_.unsafe_erase(playerId);
}

void GameWorld::SendAddMonster(ClientSession* receiver, const Monster& monster)
{
    if (receiver == nullptr)
    {
        return;
    }

    S2C_AddMonster packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_AddMonster));
    packet.type = S2C_ADD_MONSTER;

    packet.monsterId = monster.monsterId;
    packet.monsterType = monster.monsterType;

    strncpy_s(
        packet.name,
        sizeof(packet.name),
        monster.name.c_str(),
        _TRUNCATE
    );

    packet.level = monster.level;
    packet.hp = monster.hp;
    packet.maxHp = monster.maxHp;
    packet.x = static_cast<short>(monster.x);
    packet.y = static_cast<short>(monster.y);

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendAddNpc(ClientSession* receiver, const Npc& npc)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_AddNpc packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_AddNpc));
    packet.type = S2C_ADD_NPC;

    packet.npcId = npc.npcId;
    packet.npcType = npc.npcType;

    CopyStringToFixedBuffer(
        packet.name,
        sizeof(packet.name),
        npc.name
    );

    packet.x = static_cast<short>(npc.x);
    packet.y = static_cast<short>(npc.y);

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendRemoveMonster(ClientSession* receiver, int monsterId)
{
    if (receiver == nullptr)
    {
        return;
    }

    S2C_RemoveMonster packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_RemoveMonster));
    packet.type = S2C_REMOVE_MONSTER;
    packet.monsterId = monsterId;

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendMoveMonster(ClientSession* receiver, const Monster& monster)
{
    if (receiver == nullptr)
    {
        return;
    }

    S2C_MoveMonster packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_MoveMonster));
    packet.type = S2C_MOVE_MONSTER;
    packet.monsterId = monster.monsterId;
    packet.x = static_cast<short>(monster.x);
    packet.y = static_cast<short>(monster.y);

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::BuildVisibleMonsterSnapshot( ClientSession* receiver, std::vector<Monster>& outMonsters, std::unordered_set<int>& outMonsterIds )

{
    outMonsters.clear();
    outMonsterIds.clear();

    if (receiver == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(worldLock_);

    std::unordered_set<int> candidateMonsterIds;

    CollectCandidateMonsterIdsInViewRange(
        receiver->GetX(),
        receiver->GetY(),
        candidateMonsterIds
    );

    for (int monsterId : candidateMonsterIds)
    {
        auto monsterIter = monstersById_.find(monsterId);

        if (monsterIter == monstersById_.end() || monsterIter->second == nullptr)
        {
            continue;
        }

        const Monster& monster = *(monsterIter->second);

        if (!monster.alive)
        {
            continue;
        }

        if (!IsInViewRange(receiver, monster.x, monster.y))
        {
            continue;
        }

        outMonsters.push_back(monster);
        outMonsterIds.insert(monster.monsterId);
    }
}

void GameWorld::BuildVisiblePlayerSnapshot( ClientSession* receiver, std::vector<std::shared_ptr<ClientSession>>& outPlayers, std::unordered_set<int>& outPlayerIds )
{
    outPlayers.clear();
    outPlayerIds.clear();

    if (receiver == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(worldLock_);

    std::unordered_set<int> candidatePlayerIds;

    CollectCandidatePlayerIdsInViewRange(
        receiver->GetX(),
        receiver->GetY(),
        candidatePlayerIds
    );

    for (int playerId : candidatePlayerIds)
    {
        if (playerId == receiver->GetPlayerId())
        {
            continue;
        }

        auto playerIter = playersById_.find(playerId);

        if (playerIter == playersById_.end() || playerIter->second == nullptr)
        {
            continue;
        }

        std::shared_ptr<ClientSession> other = playerIter->second;

        if (!other->IsConnected() || !other->IsLoggedIn())
        {
            continue;
        }

        if (!IsInViewRange(receiver, other->GetX(), other->GetY()))
        {
            continue;
        }

        outPlayers.push_back(other);
        outPlayerIds.insert(other->GetPlayerId());
    }
}

void GameWorld::StartWorldLoop()
{
    bool expected = false;

    if (!worldLoopRunning_.compare_exchange_strong(expected, true))
    {
        return;
    }

    worldLoopThread_ = std::thread(&GameWorld::WorldUpdateLoop, this);
}

void GameWorld::StopWorldLoop()
{
    worldLoopRunning_ = false;

    if (worldLoopThread_.joinable())
    {
        worldLoopThread_.join();
    }

    std::vector<std::shared_ptr<ClientSession>> players =
        GetLoggedInPlayersSnapshot();

    for (const std::shared_ptr<ClientSession>& player : players)
    {
        if (player != nullptr)
        {
            SavePlayerDataToDatabase(player.get());
        }
    }
}

void GameWorld::WorldUpdateLoop()
{
    long long nextMonsterAiMs = 0;
    long long nextFastUpdateMs = 0;
    long long nextSlowUpdateMs = 0;

    while (worldLoopRunning_)
    {
        const long long nowMs = GetNowMs();

        ProcessDbCompletions();

        if (nowMs >= nextMonsterAiMs)
        {
            UpdateMonsterAi(nowMs);
            nextMonsterAiMs = nowMs + MONSTER_AI_UPDATE_INTERVAL_MS;
        }

        if (nowMs >= nextFastUpdateMs)
        {
            UpdatePlayerBuffs(nowMs);
            UpdatePlayerRespawns(nowMs);
            nextFastUpdateMs = nowMs + WORLD_FAST_UPDATE_INTERVAL_MS;
        }

        if (nowMs >= nextSlowUpdateMs)
        {
            UpdateMonsterRespawns(nowMs);
            UpdatePlayerHpRegeneration(nowMs);
            UpdateDropExpirations(nowMs);
            FlushDirtySaves(nowMs);
            nextSlowUpdateMs = nowMs + WORLD_SLOW_UPDATE_INTERVAL_MS;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(WORLD_UPDATE_INTERVAL_MS)
        );
    }
}

void GameWorld::UpdateRoamingMonsters(long long nowMs)
{
    std::vector<MovedMonsterInfo> movedMonsters;

    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dirDist(0, 3);

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (auto& [monsterId, monsterPtr] : monstersById_)
        {
            if (monsterPtr == nullptr)
            {
                continue;
            }

            Monster& monster = *monsterPtr;

            if (!monster.alive)
            {
                continue;
            }

            if (monster.moveType != MonsterMoveType::Roaming)
            {
                continue;
            }

            if (nowMs < monster.nextMoveTimeMs)
            {
                continue;
            }

            monster.nextMoveTimeMs = nowMs + MONSTER_ROAM_INTERVAL_MS;

            int dx = 0;
            int dy = 0;

            const int dir = dirDist(rng);

            switch (dir)
            {
            case 0:
                dy = -1;
                break;

            case 1:
                dy = 1;
                break;

            case 2:
                dx = -1;
                break;

            case 3:
                dx = 1;
                break;

            default:
                break;
            }

            MovedMonsterInfo movedInfo{};

            if (TryMoveMonster(monster, dx, dy, movedInfo))
            {
                movedMonsters.push_back(movedInfo);
            }
        }
    }

    for (const MovedMonsterInfo& movedInfo : movedMonsters)
    {
        UpdateMonsterVisibilityAfterMove(
            movedInfo.monster,
            movedInfo.oldX,
            movedInfo.oldY
        );
    }
}

void GameWorld::CollectActiveMonsterIdsNearPlayers(std::unordered_set<int>& outMonsterIds)
{
    outMonsterIds.clear();

    std::vector<std::pair<int, int>> playerPositions;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (const auto& [playerId, player] : playersById_)
        {
            if (player == nullptr)
            {
                continue;
            }

            if (!player->IsConnected() ||
                !player->IsLoggedIn() ||
                !player->IsAlive())
            {
                continue;
            }

            playerPositions.push_back(
                std::make_pair(player->GetX(), player->GetY())
            );
        }
    }

    if (playerPositions.empty())
    {
        return;
    }

    outMonsterIds.reserve(playerPositions.size() * 24);

    std::unordered_set<int> candidateMonsterIds;
    candidateMonsterIds.reserve(64);

    for (const std::pair<int, int>& position : playerPositions)
    {
        CollectCandidateMonsterIdsInViewRange(
            position.first,
            position.second,
            candidateMonsterIds
        );

        for (int monsterId : candidateMonsterIds)
        {
            outMonsterIds.insert(monsterId);
        }
    }
}

void GameWorld::UpdateMonsterAi(long long nowMs)
{
    std::vector<MovedMonsterInfo> movedMonsters;

    struct PendingMonsterAttack
    {
        std::shared_ptr<ClientSession> targetPlayer;
        std::string firstMonsterName;
        int totalDamage = 0;
        int attackCount = 0;
        int evadedCount = 0;
    };

    std::unordered_map<int, PendingMonsterAttack> pendingMonsterAttacksByPlayerId;

    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dirDist(0, 3);

    std::unordered_set<int> activeMonsterIds;
    CollectActiveMonsterIdsNearPlayers(activeMonsterIds);

    if (activeMonsterIds.empty())
    {
        return;
    }

    pendingMonsterAttacksByPlayerId.reserve(activeMonsterIds.size() / 4 + 1);

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (int activeMonsterId : activeMonsterIds)
        {
            auto monsterIter = monstersById_.find(activeMonsterId);

            if (monsterIter == monstersById_.end() || monsterIter->second == nullptr)
            {
                continue;
            }

            Monster& monster = *monsterIter->second;

            if (!monster.alive)
            {
                continue;
            }

            if (nowMs < monster.nextMoveTimeMs)
            {
                continue;
            }

            monster.nextMoveTimeMs = nowMs + MONSTER_ROAM_INTERVAL_MS;

            MovedMonsterInfo movedInfo{};

            if (monster.aiType == MonsterAiType::Agro)
            {
                const bool hasTarget = UpdateAgroTarget(monster);

                if (hasTarget)
                {
                    std::shared_ptr<ClientSession> targetPlayer =
                        FindPlayerById(monster.targetPlayerId);

                    if (targetPlayer != nullptr)
                    {
                        if (!targetPlayer->IsAlive())
                        {
                            monster.targetPlayerId = -1;
                            continue;
                        }

                        if (IsAdjacent4(
                            monster.x,
                            monster.y,
                            targetPlayer->GetX(),
                            targetPlayer->GetY()
                        ))
                        {
                            if (nowMs >= monster.nextAttackTimeMs)
                            {
                                monster.nextAttackTimeMs =
                                    nowMs + MONSTER_ATTACK_INTERVAL_MS;

                                const int damage =
                                    CalculateMonsterAttackDamage(
                                        monster,
                                        targetPlayer.get()
                                    );

                                const int targetPlayerId = targetPlayer->GetPlayerId();
                                PendingMonsterAttack& pendingAttack =
                                    pendingMonsterAttacksByPlayerId[targetPlayerId];

                                if (pendingAttack.targetPlayer == nullptr)
                                {
                                    pendingAttack.targetPlayer = targetPlayer;
                                    pendingAttack.firstMonsterName = monster.name;
                                }

                                ++pendingAttack.attackCount;

                                if (damage <= 0)
                                {
                                    ++pendingAttack.evadedCount;
                                }
                                else
                                {
                                    pendingAttack.totalDamage += damage;
                                }
                            }

                            continue;
                        }

                        if (TryMoveMonsterTowardPlayer(
                            monster,
                            targetPlayer.get(),
                            movedInfo
                        ))
                        {
                            movedMonsters.push_back(movedInfo);
                        }

                        continue;
                    }
                }
            }

            if (monster.moveType == MonsterMoveType::Roaming)
            {
                int dx = 0;
                int dy = 0;

                const int dir = dirDist(rng);

                switch (dir)
                {
                case 0:
                    dy = -1;
                    break;

                case 1:
                    dy = 1;
                    break;

                case 2:
                    dx = -1;
                    break;

                case 3:
                    dx = 1;
                    break;

                default:
                    break;
                }

                if (TryMoveMonster(monster, dx, dy, movedInfo))
                {
                    movedMonsters.push_back(movedInfo);
                }
            }
        }
    }

    for (const MovedMonsterInfo& movedInfo : movedMonsters)
    {
        UpdateMonsterVisibilityAfterMove(
            movedInfo.monster,
            movedInfo.oldX,
            movedInfo.oldY
        );
    }

    std::unordered_set<int> deadPlayerIds;

    for (const auto& [targetPlayerId, attack] : pendingMonsterAttacksByPlayerId)
    {
        if (attack.targetPlayer == nullptr)
        {
            continue;
        }

        if (!attack.targetPlayer->IsConnected() ||
            !attack.targetPlayer->IsLoggedIn())
        {
            continue;
        }

        if (!attack.targetPlayer->IsAlive())
        {
            continue;
        }

        if (deadPlayerIds.find(targetPlayerId) != deadPlayerIds.end())
        {
            continue;
        }

        if (attack.totalDamage <= 0)
        {
            SendBattleMessage(
                attack.targetPlayer.get(),
                attack.firstMonsterName +
                (attack.evadedCount > 1 ?
                    " and other monsters missed you." :
                    " missed you.")
            );

            continue;
        }

        int hp = 0;
        int maxHp = 0;

        const bool dead =
            attack.targetPlayer->ApplyDamage(attack.totalDamage, hp, maxHp);

        SendPlayerStatus(attack.targetPlayer.get(), 0, 0);

        if (attack.attackCount <= 1)
        {
            SendBattleMessage(
                attack.targetPlayer.get(),
                attack.firstMonsterName +
                " attacked you for " +
                std::to_string(attack.totalDamage) +
                " damage. HP: " +
                std::to_string(hp) +
                "/" +
                std::to_string(maxHp)
            );
        }
        else
        {
            SendBattleMessage(
                attack.targetPlayer.get(),
                std::to_string(attack.attackCount) +
                " monsters attacked you for " +
                std::to_string(attack.totalDamage) +
                " total damage. HP: " +
                std::to_string(hp) +
                "/" +
                std::to_string(maxHp)
            );
        }

        if (dead)
        {
            deadPlayerIds.insert(targetPlayerId);
            HandlePlayerDeathAndRespawn(attack.targetPlayer.get());
        }
    }
}

bool GameWorld::UpdateAgroTarget(Monster& monster)
{
    if (monster.aiType != MonsterAiType::Agro)
    {
        monster.targetPlayerId = -1;
        return false;
    }

    if (monster.targetPlayerId != -1)
    {
        std::shared_ptr<ClientSession> currentTarget =
            FindPlayerById(monster.targetPlayerId);

        if (currentTarget != nullptr &&
            currentTarget->IsConnected() &&
            currentTarget->IsLoggedIn() &&
            currentTarget->IsAlive() &&
            IsPlayerInAggroRange(monster, currentTarget.get()))
        {
            return true;
        }

        monster.targetPlayerId = -1;
    }

    std::shared_ptr<ClientSession> newTarget =
        FindNearestPlayerInAggroRange(monster);

    if (newTarget == nullptr)
    {
        return false;
    }

    monster.targetPlayerId = newTarget->GetPlayerId();

    return true;
}

std::shared_ptr<ClientSession> GameWorld::FindNearestPlayerInAggroRange(const Monster& monster)
{
    std::unordered_set<int> candidatePlayerIds;

    CollectCandidatePlayerIdsInViewRange(
        monster.x,
        monster.y,
        candidatePlayerIds
    );

    std::shared_ptr<ClientSession> nearestPlayer;
    int nearestDistance = 999999;

    for (int playerId : candidatePlayerIds)
    {
        auto playerIter = playersById_.find(playerId);

        if (playerIter == playersById_.end() || playerIter->second == nullptr)
        {
            continue;
        }

        std::shared_ptr<ClientSession> player = playerIter->second;

        if (!player->IsConnected() || !player->IsLoggedIn())
        {
            continue;
        }

        if (!player->IsAlive())
        {
            continue;
        }

        if (!IsPlayerInAggroRange(monster, player.get()))
        {
            continue;
        }

        const int dx = std::abs(monster.x - player->GetX());
        const int dy = std::abs(monster.y - player->GetY());
        const int distance = dx + dy;

        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestPlayer = player;
        }
    }

    return nearestPlayer;
}

std::shared_ptr<ClientSession> GameWorld::FindPlayerById(int playerId)
{
    auto iter = playersById_.find(playerId);

    if (iter == playersById_.end())
    {
        return nullptr;
    }

    return iter->second;
}

bool GameWorld::IsPlayerInAggroRange(const Monster& monster, ClientSession* player) const
{
    if (player == nullptr)
    {
        return false;
    }

    const int dx = std::abs(monster.x - player->GetX());
    const int dy = std::abs(monster.y - player->GetY());

    return dx <= MONSTER_AGGRO_RADIUS && dy <= MONSTER_AGGRO_RADIUS;
}

bool GameWorld::TryMoveMonsterTowardPlayer( Monster& monster, ClientSession* targetPlayer, MovedMonsterInfo& outMovedInfo )
{
    if (targetPlayer == nullptr)
    {
        return false;
    }

    const int startX = monster.x;
    const int startY = monster.y;

    const int playerX = targetPlayer->GetX();
    const int playerY = targetPlayer->GetY();

    const int distanceToPlayer =
        std::abs(startX - playerX) + std::abs(startY - playerY);

    if (distanceToPlayer <= 1)
    {
        return false;
    }

    auto DistanceAfterMove = [playerX, playerY](int nextX, int nextY) -> int
        {
            return std::abs(nextX - playerX) + std::abs(nextY - playerY);
        };

    std::vector<std::pair<int, int>> greedyDirections;
    greedyDirections.reserve(4);

    const int diffX = playerX - startX;
    const int diffY = playerY - startY;

    const int stepX = (diffX > 0) ? 1 : ((diffX < 0) ? -1 : 0);
    const int stepY = (diffY > 0) ? 1 : ((diffY < 0) ? -1 : 0);

    if (std::abs(diffX) >= std::abs(diffY))
    {
        if (stepX != 0) greedyDirections.push_back({ stepX, 0 });
        if (stepY != 0) greedyDirections.push_back({ 0, stepY });
    }
    else
    {
        if (stepY != 0) greedyDirections.push_back({ 0, stepY });
        if (stepX != 0) greedyDirections.push_back({ stepX, 0 });
    }

    if (stepY == 0)
    {
        greedyDirections.push_back({ 0, -1 });
        greedyDirections.push_back({ 0, 1 });
    }
    else if (stepX == 0)
    {
        greedyDirections.push_back({ -1, 0 });
        greedyDirections.push_back({ 1, 0 });
    }
    else
    {
        greedyDirections.push_back({ 0, -stepY });
        greedyDirections.push_back({ -stepX, 0 });
    }

    int bestDx = 0;
    int bestDy = 0;
    int bestDistance = distanceToPlayer;

    for (const std::pair<int, int>& direction : greedyDirections)
    {
        const int nextX = startX + direction.first;
        const int nextY = startY + direction.second;

        if (!CanActorMoveTo(nextX, nextY, monster.monsterId, -1))
        {
            continue;
        }

        const int nextDistance = DistanceAfterMove(nextX, nextY);

        if (nextDistance < bestDistance)
        {
            bestDistance = nextDistance;
            bestDx = direction.first;
            bestDy = direction.second;
        }
    }

    if (bestDx != 0 || bestDy != 0)
    {
        return TryMoveMonster(monster, bestDx, bestDy, outMovedInfo);
    }

    const int goalDx[4] = { 0, 0, -1, 1 };
    const int goalDy[4] = { -1, 1, 0, 0 };

    std::vector<std::pair<int, int>> bestPath;
    int bestPathLength = 999999;

    for (int i = 0; i < 4; ++i)
    {
        const int goalX = playerX + goalDx[i];
        const int goalY = playerY + goalDy[i];

        if (!CanActorMoveTo(goalX, goalY, monster.monsterId, -1))
        {
            continue;
        }

        std::vector<std::pair<int, int>> path;

        if (!FindPathAStar(
            startX,
            startY,
            goalX,
            goalY,
            monster.monsterId,
            -1,
            path
        ))
        {
            continue;
        }

        if (path.size() < 2)
        {
            continue;
        }

        if (static_cast<int>(path.size()) < bestPathLength)
        {
            bestPathLength = static_cast<int>(path.size());
            bestPath = std::move(path);
        }
    }

    if (bestPath.size() < 2)
    {
        return false;
    }

    const int nextX = bestPath[1].first;
    const int nextY = bestPath[1].second;

    if (!CanActorMoveTo(nextX, nextY, monster.monsterId, -1))
    {
        return false;
    }

    const int dx = nextX - startX;
    const int dy = nextY - startY;

    if (std::abs(dx) + std::abs(dy) != 1)
    {
        return false;
    }

    return TryMoveMonster(monster, dx, dy, outMovedInfo);
}

bool GameWorld::FindPathAStar( int startX, int startY, int goalX, int goalY, int ignoreMonsterId, int ignorePlayerId, std::vector<std::pair<int, int>>& outPath )
{
    outPath.clear();

    if (startX == goalX && startY == goalY)
    {
        outPath.push_back({ startX, startY });
        return true;
    }

    if (!CanActorMoveTo(goalX, goalY, ignoreMonsterId, ignorePlayerId))
    {
        return false;
    }

    const int minX = std::max(
        0,
        std::min(startX, goalX) - ASTAR_SEARCH_PADDING
    );

    const int maxX = std::min(
        mapWidth_ - 1,
        std::max(startX, goalX) + ASTAR_SEARCH_PADDING
    );

    const int minY = std::max(
        0,
        std::min(startY, goalY) - ASTAR_SEARCH_PADDING
    );

    const int maxY = std::min(
        mapHeight_ - 1,
        std::max(startY, goalY) + ASTAR_SEARCH_PADDING
    );

    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    const int totalNodeCount = width * height;

    if (width <= 0 || height <= 0 || totalNodeCount <= 0)
    {
        return false;
    }

    auto ToIndex = [minX, minY, width](int x, int y) -> int
        {
            return (y - minY) * width + (x - minX);
        };

    auto IsInsideSearchArea = [minX, maxX, minY, maxY](int x, int y) -> bool
        {
            return x >= minX && x <= maxX && y >= minY && y <= maxY;
        };

    auto Heuristic = [](int x1, int y1, int x2, int y2) -> int
        {
            return std::abs(x1 - x2) + std::abs(y1 - y2);
        };

    struct OpenNode
    {
        int x = 0;
        int y = 0;
        int g = 0;
        int h = 0;
        int f = 0;
    };

    struct OpenNodeCompare
    {
        bool operator()(const OpenNode& lhs, const OpenNode& rhs) const
        {
            if (lhs.f == rhs.f)
            {
                return lhs.h > rhs.h;
            }

            return lhs.f > rhs.f;
        }
    };

    constexpr int INF = std::numeric_limits<int>::max() / 4;

    std::vector<int> gCost(totalNodeCount, INF);
    std::vector<int> parent(totalNodeCount, -1);
    std::vector<unsigned char> closed(totalNodeCount, 0);

    std::priority_queue<
        OpenNode,
        std::vector<OpenNode>,
        OpenNodeCompare
    > openList;

    const int startIndex = ToIndex(startX, startY);
    const int goalIndex = ToIndex(goalX, goalY);

    gCost[startIndex] = 0;

    const int startH = Heuristic(startX, startY, goalX, goalY);

    openList.push(OpenNode{
        startX,
        startY,
        0,
        startH,
        startH
        });

    int expandedNodeCount = 0;

    const int dx[4] = { 0, 0, -1, 1 };
    const int dy[4] = { -1, 1, 0, 0 };

    while (!openList.empty())
    {
        const OpenNode current = openList.top();
        openList.pop();

        const int currentIndex = ToIndex(current.x, current.y);

        if (closed[currentIndex] != 0)
        {
            continue;
        }

        closed[currentIndex] = 1;
        ++expandedNodeCount;

        if (expandedNodeCount > ASTAR_MAX_EXPANDED_NODES)
        {
            break;
        }

        if (currentIndex == goalIndex)
        {
            std::vector<std::pair<int, int>> reversedPath;

            int pathIndex = goalIndex;

            while (pathIndex != -1)
            {
                const int localY = pathIndex / width;
                const int localX = pathIndex % width;

                const int worldX = minX + localX;
                const int worldY = minY + localY;

                reversedPath.push_back({ worldX, worldY });

                if (pathIndex == startIndex)
                {
                    break;
                }

                pathIndex = parent[pathIndex];
            }

            if (reversedPath.empty() ||
                reversedPath.back() != std::pair<int, int>{ startX, startY })
            {
                return false;
            }

            std::reverse(reversedPath.begin(), reversedPath.end());

            outPath = std::move(reversedPath);
            return true;
        }

        for (int dir = 0; dir < 4; ++dir)
        {
            const int nextX = current.x + dx[dir];
            const int nextY = current.y + dy[dir];

            if (!IsInsideSearchArea(nextX, nextY))
            {
                continue;
            }

            if (!CanActorMoveTo(nextX, nextY, ignoreMonsterId, ignorePlayerId))
            {
                continue;
            }

            const int nextIndex = ToIndex(nextX, nextY);

            if (closed[nextIndex] != 0)
            {
                continue;
            }

            const int nextG = gCost[currentIndex] + 1;

            if (nextG >= gCost[nextIndex])
            {
                continue;
            }

            gCost[nextIndex] = nextG;
            parent[nextIndex] = currentIndex;

            const int nextH = Heuristic(nextX, nextY, goalX, goalY);
            const int nextF = nextG + nextH;

            openList.push(OpenNode{
                nextX,
                nextY,
                nextG,
                nextH,
                nextF
                });
        }
    }

    return false;
}

bool GameWorld::TryMoveMonster( Monster& monster, int dx, int dy, MovedMonsterInfo& outMovedInfo)
{
    if (dx == 0 && dy == 0)
    {
        return false;
    }

    const int oldX = monster.x;
    const int oldY = monster.y;

    const int nextX = oldX + dx;
    const int nextY = oldY + dy;

    if (!CanActorMoveTo(nextX, nextY, monster.monsterId, -1))
    {
        return false;
    }

    monster.x = nextX;
    monster.y = nextY;

    MoveMonsterSectorIfNeeded(
        monster.monsterId,
        oldX,
        oldY,
        nextX,
        nextY
    );

    outMovedInfo.monster = monster;
    outMovedInfo.oldX = oldX;
    outMovedInfo.oldY = oldY;

    return true;
}

void GameWorld::RemoveMonsterFromSector(int monsterId, int x, int y)
{
    const int sectorIndex = GetSectorIndexFromPosition(x, y);

    std::shared_ptr<Sector> sector = FindSector(sectorIndex);

    if (sector == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sector->lock);
    sector->monsterIds.erase(monsterId);
}

void GameWorld::MoveMonsterSectorIfNeeded( int monsterId, int oldX, int oldY, int newX, int newY )
{
    const int oldSectorIndex = GetSectorIndexFromPosition(oldX, oldY);
    const int newSectorIndex = GetSectorIndexFromPosition(newX, newY);

    if (oldSectorIndex == newSectorIndex)
    {
        return;
    }

    RemoveMonsterFromSector(monsterId, oldX, oldY);
    AddMonsterToSector(monsterId, newX, newY);
}

void GameWorld::UpdateMonsterVisibilityAfterMove( const Monster& monster, int oldX, int oldY )
{
    if (!monster.alive)
    {
        return;
    }

    std::vector<std::shared_ptr<ClientSession>> moveReceivers;
    std::vector<std::shared_ptr<ClientSession>> addReceivers;
    std::vector<std::shared_ptr<ClientSession>> removeReceivers;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        std::unordered_set<int> candidatePlayerIds;
        std::unordered_set<int> oldCandidatePlayerIds;
        std::unordered_set<int> newCandidatePlayerIds;

        CollectCandidatePlayerIdsInViewRange(
            oldX,
            oldY,
            oldCandidatePlayerIds
        );

        CollectCandidatePlayerIdsInViewRange(
            monster.x,
            monster.y,
            newCandidatePlayerIds
        );

        for (int playerId : oldCandidatePlayerIds)
        {
            candidatePlayerIds.insert(playerId);
        }

        for (int playerId : newCandidatePlayerIds)
        {
            candidatePlayerIds.insert(playerId);
        }

        for (int playerId : candidatePlayerIds)
        {
            auto playerIter = playersById_.find(playerId);

            if (playerIter == playersById_.end() || playerIter->second == nullptr)
            {
                continue;
            }

            std::shared_ptr<ClientSession> player = playerIter->second;

            if (!player->IsConnected() || !player->IsLoggedIn())
            {
                continue;
            }

            std::shared_ptr<VisibleState> visibleState =
                GetOrCreateVisibleState(player->GetPlayerId());

            if (visibleState == nullptr)
            {
                continue;
            }

            const bool canSeeNow = IsInViewRange(
                player.get(),
                monster.x,
                monster.y
            );

            bool shouldMove = false;
            bool shouldAdd = false;
            bool shouldRemove = false;

            {
                std::lock_guard<std::mutex> visibleLock(visibleState->lock);

                const bool wasVisible =
                    visibleState->visibleMonsterIds.find(monster.monsterId) !=
                    visibleState->visibleMonsterIds.end();

                if (canSeeNow && wasVisible)
                {
                    shouldMove = true;
                }
                else if (canSeeNow && !wasVisible)
                {
                    visibleState->visibleMonsterIds.insert(monster.monsterId);
                    shouldAdd = true;
                }
                else if (!canSeeNow && wasVisible)
                {
                    visibleState->visibleMonsterIds.erase(monster.monsterId);
                    shouldRemove = true;
                }
            }

            if (shouldMove)
            {
                moveReceivers.push_back(player);
            }

            if (shouldAdd)
            {
                addReceivers.push_back(player);
            }

            if (shouldRemove)
            {
                removeReceivers.push_back(player);
            }
        }
    }

    for (const std::shared_ptr<ClientSession>& receiver : removeReceivers)
    {
        if (receiver != nullptr)
        {
            SendRemoveMonster(receiver.get(), monster.monsterId);
        }
    }

    for (const std::shared_ptr<ClientSession>& receiver : addReceivers)
    {
        if (receiver != nullptr)
        {
            SendAddMonster(receiver.get(), monster);
        }
    }

    for (const std::shared_ptr<ClientSession>& receiver : moveReceivers)
    {
        if (receiver != nullptr)
        {
            SendMoveMonster(receiver.get(), monster);
        }
    }
}

void GameWorld::HandlePlayerAttack(ClientSession* attacker)
{
    if (attacker == nullptr)
    {
        return;
    }

    if (!attacker->IsConnected() || !attacker->IsLoggedIn())
    {
        return;
    }

    if (!attacker->IsAlive())
    {
        SendBattleMessage(attacker, "You cannot attack while dead.");
        return;
    }

    const long long nowMs = GetNowMs();

    if (!attacker->TryUseAttack(nowMs, PLAYER_ATTACK_INTERVAL_MS))
    {
        SendBattleMessage(attacker, "Attack cooldown.");
        return;
    }

    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(attacker->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(attacker, "Invalid attack direction.");
        return;
    }

    const int targetX = attacker->GetX() + dx;
    const int targetY = attacker->GetY() + dy;

    int monsterId = -1;
    int newHp = 0;
    int maxHp = 0;
    int damage = 0;
    int expGain = 0;

    bool monsterDead = false;
    bool noTarget = false;

    std::string monsterName;

    Monster monsterSnapshotForDrop{};

    std::vector<std::shared_ptr<ClientSession>> hpReceivers;
    std::vector<std::shared_ptr<ClientSession>> removeReceivers;
    std::vector<std::shared_ptr<ClientSession>> expReceivers;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        std::shared_ptr<Monster> monsterPtr = FindAliveMonsterAt(targetX, targetY);

        if (monsterPtr == nullptr)
        {
            std::vector<std::pair<int, int>> attackPositions;
            AppendDirectionalTargetPositions(
                attackPositions,
                attacker->GetX(),
                attacker->GetY(),
                dx,
                dy,
                2,
                0
            );

            for (const auto& position : attackPositions)
            {
                monsterPtr = FindAliveMonsterAt(position.first, position.second);

                if (monsterPtr != nullptr)
                {
                    break;
                }
            }
        }

        if (monsterPtr == nullptr)
        {
            noTarget = true;
        }
        else
        {
            Monster& monster = *monsterPtr;

            damage = CalculatePlayerAttackDamage(attacker, monster);

            monster.hp -= damage;

            if (monster.hp < 0)
            {
                monster.hp = 0;
            }

            monster.damageByPlayer[attacker->GetPlayerId()] += damage;

            monsterId = monster.monsterId;
            newHp = monster.hp;
            maxHp = monster.maxHp;
            monsterName = monster.name;

            for (auto& [playerId, player] : playersById_)
            {
                if (player == nullptr)
                {
                    continue;
                }

                if (!player->IsConnected() || !player->IsLoggedIn())
                {
                    continue;
                }

                if (IsInViewRange(player.get(), monster.x, monster.y))
                {
                    hpReceivers.push_back(player);
                }
            }

            if (monster.hp <= 0)
            {
                monsterDead = true;
                expGain = CalculateMonsterExperience(monster);

                monsterSnapshotForDrop = monster;

                if (monster.damageByPlayer.empty())
                {
                    monster.damageByPlayer[attacker->GetPlayerId()] = damage;
                }

                for (const auto& [contributorId, dealtDamage] : monster.damageByPlayer)
                {
                    if (dealtDamage <= 0)
                    {
                        continue;
                    }

                    auto playerIter = playersById_.find(contributorId);

                    if (playerIter == playersById_.end() || playerIter->second == nullptr)
                    {
                        continue;
                    }

                    std::shared_ptr<ClientSession> contributor = playerIter->second;

                    if (contributor->IsConnected() && contributor->IsLoggedIn())
                    {
                        expReceivers.push_back(contributor);
                    }
                }

                monster.alive = false;
                monster.targetPlayerId = -1;
                monster.respawnTimeMs = nowMs + MONSTER_RESPAWN_DELAY_MS;
                deadMonsterIds_.insert(monster.monsterId);
                monster.damageByPlayer.clear();

                RemoveMonsterFromSector(monster.monsterId, monster.x, monster.y);

                for (auto& [playerId, visibleState] : visibleStates_)
                {
                    if (visibleState == nullptr)
                    {
                        continue;
                    }

                    bool shouldRemove = false;

                    {
                        std::lock_guard<std::mutex> visibleLock(visibleState->lock);

                        const std::size_t erasedCount =
                            visibleState->visibleMonsterIds.erase(monster.monsterId);

                        shouldRemove = erasedCount > 0;
                    }

                    if (!shouldRemove)
                    {
                        continue;
                    }

                    auto playerIter = playersById_.find(playerId);

                    if (playerIter == playersById_.end() || playerIter->second == nullptr)
                    {
                        continue;
                    }

                    std::shared_ptr<ClientSession> receiver = playerIter->second;

                    if (receiver->IsConnected() && receiver->IsLoggedIn())
                    {
                        removeReceivers.push_back(receiver);
                    }
                }
            }
        }
    }

    if (noTarget)
    {
        SendBattleMessage(attacker, "There is no monster in front.");
        return;
    }

    for (const std::shared_ptr<ClientSession>& receiver : hpReceivers)
    {
        if (receiver != nullptr)
        {
            SendMonsterHp(receiver.get(), monsterId, newHp, maxHp);
        }
    }

    SendBattleMessage(
        attacker,
        "You attacked " + monsterName + " for " + std::to_string(damage) + " damage."
    );

    if (monsterDead)
    {
        for (const std::shared_ptr<ClientSession>& receiver : removeReceivers)
        {
            if (receiver != nullptr)
            {
                SendRemoveMonster(receiver.get(), monsterId);
            }
        }

        for (const std::shared_ptr<ClientSession>& receiver : expReceivers)
        {
            if (receiver == nullptr)
            {
                continue;
            }

            const int levelGained = receiver->AddExperience(expGain);

            SendPlayerStatus(receiver.get(), expGain, levelGained);

            SendBattleMessage(
                receiver.get(),
                monsterName + " died. EXP +" + std::to_string(expGain) + "."
            );

            if (levelGained > 0)
            {
                SendBattleMessage(
                    receiver.get(),
                    "Level up! Current level: " + std::to_string(receiver->GetLevel()) + "."
                );
            }

            MarkPlayerDirty(receiver.get());
        }
    }

    CreatePersonalDropsForMonster(monsterSnapshotForDrop, expReceivers);
}

std::shared_ptr<GameWorld::Monster> GameWorld::FindAliveMonsterAt(int x, int y)
{
    const int sectorIndex = GetSectorIndexFromPosition(x, y);

    std::shared_ptr<Sector> sector = FindSector(sectorIndex);

    if (sector == nullptr)
    {
        return nullptr;
    }

    std::vector<int> monsterIds;

    {
        std::lock_guard<std::mutex> lock(sector->lock);

        for (int monsterId : sector->monsterIds)
        {
            monsterIds.push_back(monsterId);
        }
    }

    for (int monsterId : monsterIds)
    {
        auto monsterIter = monstersById_.find(monsterId);

        if (monsterIter == monstersById_.end() || monsterIter->second == nullptr)
        {
            continue;
        }

        std::shared_ptr<Monster> monster = monsterIter->second;

        if (!monster->alive)
        {
            continue;
        }

        if (monster->x == x && monster->y == y)
        {
            return monster;
        }
    }

    return nullptr;
}


std::shared_ptr<GameWorld::Monster> GameWorld::FindNearestAliveMonsterInRange(
    int centerX,
    int centerY,
    int range
)
{
    std::shared_ptr<Monster> bestMonster;
    int bestDistance = 999999;

    for (int y = centerY - range; y <= centerY + range; ++y)
    {
        for (int x = centerX - range; x <= centerX + range; ++x)
        {
            const int dx = x >= centerX ? x - centerX : centerX - x;
            const int dy = y >= centerY ? y - centerY : centerY - y;
            const int distance = dx + dy;

            if (distance <= 0 || distance > range)
            {
                continue;
            }

            if (distance > bestDistance)
            {
                continue;
            }

            std::shared_ptr<Monster> monster = FindAliveMonsterAt(x, y);

            if (monster == nullptr)
            {
                continue;
            }

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestMonster = monster;
            }
        }
    }

    return bestMonster;
}

int GameWorld::CalculatePlayerAttackDamage(
    ClientSession* attacker,
    const Monster& monster
) const
{
    if (attacker == nullptr)
    {
        return 1;
    }

    if (!RollPercent(attacker->GetAccuracyRate()))
    {
        return 0;
    }

    const int playerAttack = attacker->GetPhysicalAttack();

    const int monsterDefense = monster.level * 2;

    const bool critical = RollPercent(attacker->GetCriticalRate());

    int damage = 0;

    if (critical)
    {
        damage = playerAttack * 120 / 100;
    }
    else
    {
        damage = playerAttack - monsterDefense;
    }

    if (damage < 1)
    {
        damage = 1;
    }

    return damage;
}

void GameWorld::SendMonsterHp( ClientSession* receiver, int monsterId, int hp, int maxHp )
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_MonsterHp packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_MonsterHp));
    packet.type = S2C_MONSTER_HP;
    packet.monsterId = monsterId;
    packet.hp = hp;
    packet.maxHp = maxHp;

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendBattleMessage( ClientSession* receiver, const std::string& message )
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_BattleMessage packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_BattleMessage));
    packet.type = S2C_BATTLE_MESSAGE;

    strncpy_s(
        packet.message,
        sizeof(packet.message),
        message.c_str(),
        _TRUNCATE
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendChatMessage(
    ClientSession* receiver,
    int senderPlayerId,
    const std::string& senderName,
    const std::string& message
)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_Chat packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_Chat));
    packet.type = S2C_CHAT;
    packet.playerId = senderPlayerId;

    strncpy_s(
        packet.username,
        sizeof(packet.username),
        senderName.c_str(),
        _TRUNCATE
    );

    strncpy_s(
        packet.message,
        sizeof(packet.message),
        message.c_str(),
        _TRUNCATE
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool GameWorld::Chat(ClientSession* sender, const std::string& message)
{
    if (sender == nullptr)
    {
        return false;
    }

    if (!sender->IsConnected() || !sender->IsLoggedIn())
    {
        return false;
    }

    std::string chatMessage = message;

    while (!chatMessage.empty() &&
        (chatMessage.back() == '\n' ||
            chatMessage.back() == '\r' ||
            chatMessage.back() == ' '))
    {
        chatMessage.pop_back();
    }

    if (chatMessage.empty())
    {
        return false;
    }

    if (chatMessage.size() >= CHAT_MESSAGE_LEN)
    {
        chatMessage = chatMessage.substr(0, CHAT_MESSAGE_LEN - 1);
    }

    const int senderPlayerId = sender->GetPlayerId();
    const std::string senderName = sender->GetUsername();

    const std::vector<std::shared_ptr<ClientSession>> receivers =
        GetLoggedInPlayersSnapshot();

    for (const std::shared_ptr<ClientSession>& receiver : receivers)
    {
        if (receiver == nullptr)
        {
            continue;
        }

        SendChatMessage(
            receiver.get(),
            senderPlayerId,
            senderName,
            chatMessage
        );
    }

    return true;
}

bool GameWorld::IsActorOccupied( int x, int y, int ignoreMonsterId, int ignorePlayerId )
{
    const int sectorIndex = GetSectorIndexFromPosition(x, y);

    std::shared_ptr<Sector> sector = FindSector(sectorIndex);

    if (sector == nullptr)
    {
        return false;
    }

    std::vector<int> playerIds;
    std::vector<int> monsterIds;

    {
        std::lock_guard<std::mutex> lock(sector->lock);

        for (int playerId : sector->playerIds)
        {
            playerIds.push_back(playerId);
        }

        for (int monsterId : sector->monsterIds)
        {
            monsterIds.push_back(monsterId);
        }
    }

    for (int playerId : playerIds)
    {
        if (playerId == ignorePlayerId)
        {
            continue;
        }

        auto playerIter = playersById_.find(playerId);

        if (playerIter == playersById_.end() || playerIter->second == nullptr)
        {
            continue;
        }

        std::shared_ptr<ClientSession> player = playerIter->second;

        if (!player->IsConnected() || !player->IsLoggedIn())
        {
            continue;
        }

        if (!player->IsAlive())
        {
            continue;
        }

        if (player->GetX() == x && player->GetY() == y)
        {
            return true;
        }
    }

    for (int monsterId : monsterIds)
    {
        if (monsterId == ignoreMonsterId)
        {
            continue;
        }

        auto monsterIter = monstersById_.find(monsterId);

        if (monsterIter == monstersById_.end() || monsterIter->second == nullptr)
        {
            continue;
        }

        std::shared_ptr<Monster> monster = monsterIter->second;

        if (!monster->alive)
        {
            continue;
        }

        if (monster->x == x && monster->y == y)
        {
            return true;
        }
    }

    return false;
}

bool GameWorld::CanActorMoveTo( int x, int y, int ignoreMonsterId, int ignorePlayerId )
{
    if (!CanMoveTo(x, y))
    {
        return false;
    }

    if (IsActorOccupied(x, y, ignoreMonsterId, ignorePlayerId))
    {
        return false;
    }

    return true;
}

bool GameWorld::FindSpawnPositionNear( int centerX, int centerY, int& outX, int& outY )
{
    for (int radius = 0; radius <= SPAWN_SEARCH_RADIUS; ++radius)
    {
        for (int y = centerY - radius; y <= centerY + radius; ++y)
        {
            for (int x = centerX - radius; x <= centerX + radius; ++x)
            {
                if (radius > 0)
                {
                    const bool isBorder =
                        x == centerX - radius ||
                        x == centerX + radius ||
                        y == centerY - radius ||
                        y == centerY + radius;

                    if (!isBorder)
                    {
                        continue;
                    }
                }

                if (CanActorMoveTo(x, y, -1, -1))
                {
                    outX = x;
                    outY = y;
                    return true;
                }
            }
        }
    }

    return false;
}

int GameWorld::CalculateMonsterExperience(const Monster& monster) const
{
    int exp = monster.level * monster.level * 2;

    if (monster.aiType == MonsterAiType::Agro)
    {
        exp *= 2;
    }

    if (monster.moveType == MonsterMoveType::Roaming)
    {
        exp *= 2;
    }

    return exp;
}

void GameWorld::SendPlayerStatus(
    ClientSession* receiver,
    int expGained,
    int levelGained
)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_PlayerStatus packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_PlayerStatus));
    packet.type = S2C_PLAYER_STATUS;

    packet.playerId = receiver->GetPlayerId();

    packet.hp = receiver->GetHp();
    packet.maxHp = receiver->GetMaxHp();

    packet.mp = receiver->GetMp();
    packet.maxMp = receiver->GetMaxMp();

    packet.level = receiver->GetLevel();
    packet.exp = receiver->GetExp();
    packet.requiredExp = receiver->GetRequiredExp();

    packet.statPoint = receiver->GetStatPoint();
    packet.skillPoint = receiver->GetSkillPoint();
    packet.gold = receiver->GetGold();

    packet.str = receiver->GetStr();
    packet.intStat = receiver->GetIntStat();
    packet.dex = receiver->GetDex();
    packet.luk = receiver->GetLuk();

    packet.expGained = expGained;
    packet.levelGained = levelGained;

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendSkillStatus(ClientSession* receiver)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_SkillStatus packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_SkillStatus));
    packet.type = S2C_SKILL_STATUS;

    packet.skillPoint = receiver->GetSkillPoint();
    packet.skillCount = 0;

    receiver->GetSkillStatus(
        packet.skills,
        MAX_SKILL_STATUS_COUNT,
        packet.skillCount
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}


void GameWorld::GiveDefaultTestItems(ClientSession* receiver)
{
    if (receiver == nullptr)
    {
        return;
    }

    if (!receiver->IsConnected() || !receiver->IsLoggedIn())
    {
        return;
    }

    if (!IsDebugAccountName(receiver->GetUsername()))
    {
        return;
    }

    struct DefaultItemEntry
    {
        int itemId;
        int itemCategory;
        int itemCount;
        const char* itemName;
    };

    const DefaultItemEntry defaultItems[] =
    {
        { ITEM_SLIME_PART, ITEM_CATEGORY_ETC, 99, "Slime Part" },
        { ITEM_GOBLIN_PART, ITEM_CATEGORY_ETC, 99, "Goblin Part" },
        { ITEM_WOLF_PART, ITEM_CATEGORY_ETC, 99, "Wolf Part" },
        { ITEM_ORC_PART, ITEM_CATEGORY_ETC, 99, "Orc Part" },
        { ITEM_SKELETON_PART, ITEM_CATEGORY_ETC, 99, "Skeleton Part" },
        { ITEM_DARK_MAGE_PART, ITEM_CATEGORY_ETC, 99, "DarkMage Part" },

        { ITEM_HP_POTION, ITEM_CATEGORY_CONSUMABLE, 99, "HP Potion" },
        { ITEM_MP_POTION, ITEM_CATEGORY_CONSUMABLE, 99, "MP Potion" },
        { ITEM_HIGH_HP_POTION, ITEM_CATEGORY_CONSUMABLE, 99, "High HP Potion" },
        { ITEM_HIGH_MP_POTION, ITEM_CATEGORY_CONSUMABLE, 99, "High MP Potion" },
        { ITEM_RETURN_SCROLL, ITEM_CATEGORY_CONSUMABLE, 30, "Return Scroll" },

        { ITEM_IRON_SWORD, ITEM_CATEGORY_EQUIPMENT, 1, "Iron Sword" },
        { ITEM_STEEL_SWORD, ITEM_CATEGORY_EQUIPMENT, 1, "Steel Sword" },
        { ITEM_FLAME_SWORD, ITEM_CATEGORY_EQUIPMENT, 1, "Flame Sword" },
        { ITEM_MAGE_STAFF, ITEM_CATEGORY_EQUIPMENT, 1, "Mage Staff" },
        { ITEM_ICE_WAND, ITEM_CATEGORY_EQUIPMENT, 1, "Ice Wand" },
        { ITEM_HUNTER_BOW, ITEM_CATEGORY_EQUIPMENT, 1, "Hunter Bow" },

        { ITEM_IRON_ARMOR, ITEM_CATEGORY_EQUIPMENT, 1, "Iron Armor" },
        { ITEM_STEEL_ARMOR, ITEM_CATEGORY_EQUIPMENT, 1, "Steel Armor" },
        { ITEM_MAGIC_ROBE, ITEM_CATEGORY_EQUIPMENT, 1, "Magic Robe" },
        { ITEM_LEATHER_ARMOR, ITEM_CATEGORY_EQUIPMENT, 1, "Leather Armor" },

        { ITEM_MAGIC_RING, ITEM_CATEGORY_EQUIPMENT, 1, "Magic Ring" },
        { ITEM_WARRIOR_RING, ITEM_CATEGORY_EQUIPMENT, 1, "Warrior Ring" },
        { ITEM_LUCKY_NECKLACE, ITEM_CATEGORY_EQUIPMENT, 1, "Lucky Necklace" },
        { ITEM_SPEED_CHARM, ITEM_CATEGORY_EQUIPMENT, 1, "Speed Charm" }
    };

    std::vector<InventoryItem> updatedItems;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        auto inventoryIter = inventoriesByPlayerId_.find(receiver->GetPlayerId());

        if (inventoryIter != inventoriesByPlayerId_.end() &&
            !inventoryIter->second.empty())
        {
            return;
        }

        InventoryItem updatedItem{};

        for (const DefaultItemEntry& entry : defaultItems)
        {
            AddInventoryItem(
                receiver->GetPlayerId(),
                entry.itemId,
                entry.itemCategory,
                entry.itemCount,
                entry.itemName,
                updatedItem
            );

            updatedItems.push_back(updatedItem);
        }
    }

    for (const InventoryItem& item : updatedItems)
    {
        SendInventoryItemUpdate(receiver, item);
    }

    SavePlayerDataToDatabase(receiver);
}

bool GameWorld::AddPlayerStat(ClientSession* player, int statType)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (statType != STAT_STR &&
        statType != STAT_INT &&
        statType != STAT_DEX &&
        statType != STAT_LUK)
    {
        SendBattleMessage(player, "Invalid stat.");
        return false;
    }

    int remainingStatPoint = 0;
    int newStatValue = 0;
    std::string statName;

    const bool added =
        player->TryAddStat(
            statType,
            remainingStatPoint,
            newStatValue,
            statName
        );

    if (!added)
    {
        SendBattleMessage(player, "Not enough stat point.");
        SendPlayerStatus(player, 0, 0);
        return false;
    }

    SendPlayerStatus(player, 0, 0);

    SendBattleMessage(
        player,
        statName +
        " increased to " +
        std::to_string(newStatValue) +
        ". Stat Point: " +
        std::to_string(remainingStatPoint)
    );

    SavePlayerDataToDatabase(player);

    return true;
}

bool GameWorld::AddPlayerSkillPoint(ClientSession* player, int skillId)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    int remainingSkillPoint = 0;
    int newSkillLevel = 0;
    std::string skillName;
    std::string failReason;

    const bool added =
        player->TryAddSkillPoint(
            skillId,
            remainingSkillPoint,
            newSkillLevel,
            skillName,
            failReason
        );

    if (!added)
    {
        if (failReason.empty())
        {
            failReason = "Failed to increase skill.";
        }

        SendBattleMessage(player, failReason);
        SendPlayerStatus(player, 0, 0);
        SendSkillStatus(player);

        return false;
    }

    SendPlayerStatus(player, 0, 0);
    SendSkillStatus(player);

    SendBattleMessage(
        player,
        skillName +
        " increased to Lv." +
        std::to_string(newSkillLevel) +
        ". Skill Point: " +
        std::to_string(remainingSkillPoint)
    );

    SavePlayerDataToDatabase(player);

    return true;
}

void GameWorld::UpdateMonsterRespawns(long long nowMs)
{
    std::vector<Monster> respawnedMonsters;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (auto iter = deadMonsterIds_.begin(); iter != deadMonsterIds_.end(); )
        {
            const int monsterId = *iter;
            auto monsterIter = monstersById_.find(monsterId);

            if (monsterIter == monstersById_.end() || monsterIter->second == nullptr)
            {
                iter = deadMonsterIds_.erase(iter);
                continue;
            }

            Monster& monster = *monsterIter->second;

            if (monster.alive)
            {
                iter = deadMonsterIds_.erase(iter);
                continue;
            }

            if (monster.respawnTimeMs <= 0 || nowMs < monster.respawnTimeMs)
            {
                ++iter;
                continue;
            }

            int respawnX = monster.spawnX;
            int respawnY = monster.spawnY;

            if (!CanActorMoveTo(respawnX, respawnY, monster.monsterId, -1))
            {
                if (!FindSpawnPositionNear(monster.spawnX, monster.spawnY, respawnX, respawnY))
                {
                    monster.respawnTimeMs = nowMs + 1000;
                    ++iter;
                    continue;
                }
            }

            monster.x = respawnX;
            monster.y = respawnY;
            monster.hp = monster.maxHp;
            monster.alive = true;
            monster.targetPlayerId = -1;
            monster.respawnTimeMs = 0;
            monster.nextMoveTimeMs = nowMs + MONSTER_ROAM_INTERVAL_MS;
            monster.damageByPlayer.clear();

            AddMonsterToSector(monster.monsterId, monster.x, monster.y);

            respawnedMonsters.push_back(monster);
            iter = deadMonsterIds_.erase(iter);
        }
    }

    for (const Monster& monster : respawnedMonsters)
    {
        NotifyMonsterSpawnToVisiblePlayers(monster);
    }
}

void GameWorld::NotifyMonsterSpawnToVisiblePlayers(const Monster& monster)
{
    if (!monster.alive)
    {
        return;
    }

    std::vector<std::shared_ptr<ClientSession>> receivers;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        std::unordered_set<int> candidatePlayerIds;

        CollectCandidatePlayerIdsInViewRange(
            monster.x,
            monster.y,
            candidatePlayerIds
        );

        for (int playerId : candidatePlayerIds)
        {
            auto playerIter = playersById_.find(playerId);

            if (playerIter == playersById_.end() || playerIter->second == nullptr)
            {
                continue;
            }

            std::shared_ptr<ClientSession> player = playerIter->second;

            if (!player->IsConnected() || !player->IsLoggedIn())
            {
                continue;
            }

            if (!IsInViewRange(player.get(), monster.x, monster.y))
            {
                continue;
            }

            std::shared_ptr<VisibleState> visibleState =
                GetOrCreateVisibleState(player->GetPlayerId());

            if (visibleState == nullptr)
            {
                continue;
            }

            bool shouldSendAdd = false;

            {
                std::lock_guard<std::mutex> visibleLock(visibleState->lock);

                const auto [iter, inserted] =
                    visibleState->visibleMonsterIds.insert(monster.monsterId);

                shouldSendAdd = inserted;
            }

            if (shouldSendAdd)
            {
                receivers.push_back(player);
            }
        }
    }

    for (const std::shared_ptr<ClientSession>& receiver : receivers)
    {
        if (receiver != nullptr)
        {
            SendAddMonster(receiver.get(), monster);
        }
    }
}

int GameWorld::CalculateMonsterAttackDamage(
    const Monster& monster,
    ClientSession* targetPlayer
) const
{
    if (targetPlayer == nullptr)
    {
        return 1;
    }

    if (RollPercent(targetPlayer->GetEvasionRate()))
    {
        return 0;
    }

    const int monsterAttack = monster.attackPower + monster.level;

    int playerDefense = targetPlayer->GetPhysicalDefense();

    if (monster.monsterType == MONSTER_DARK_MAGE)
    {
        playerDefense = targetPlayer->GetMagicDefense();
    }

    int damage = monsterAttack - playerDefense;

    if (damage < 1)
    {
        damage = 1;
    }

    return damage;
}

void GameWorld::UpdatePlayerHpRegeneration(long long nowMs)
{
    std::vector<std::shared_ptr<ClientSession>> players =
        GetLoggedInPlayersSnapshot();

    for (const std::shared_ptr<ClientSession>& player : players)
    {
        if (player == nullptr)
        {
            continue;
        }

        if (!player->IsConnected() || !player->IsLoggedIn())
        {
            continue;
        }

        if (!player->IsAlive())
        {
            continue;
        }

        int healed = 0;
        int hp = 0;
        int maxHp = 0;

        const bool regenerated =
            player->TryRegenerateHp(
                nowMs,
                PLAYER_HP_REGEN_INTERVAL_MS,
                PLAYER_HP_REGEN_PERCENT,
                healed,
                hp,
                maxHp
            );

        if (!regenerated)
        {
            continue;
        }

        SendPlayerStatus(player.get(), 0, 0);

        SendBattleMessage(
            player.get(),
            "HP regenerated +" +
            std::to_string(healed) +
            ". HP: " +
            std::to_string(hp) +
            "/" +
            std::to_string(maxHp)
        );

        MarkPlayerDirty(player.get());
    }
}

void GameWorld::HandlePlayerDeathAndRespawn(ClientSession* player)
{
    if (player == nullptr)
    {
        return;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return;
    }

    if (!player->IsAlive())
    {
        return;
    }

    const long long nowMs = GetNowMs();
    const long long respawnTimeMs = nowMs + PLAYER_RESPAWN_DELAY_MS;

    player->ReduceExpByHalf();
    player->MarkDead(respawnTimeMs);
    player->ResetHpRegenTimer(respawnTimeMs + PLAYER_HP_REGEN_INTERVAL_MS);

    SendPlayerStatus(player, 0, 0);

    SendBattleMessage(
        player,
        "You died. EXP was reduced by half."
    );

    SavePlayerDataToDatabase(player);
}

void GameWorld::UpdatePlayerBuffs(long long nowMs)
{
    std::vector<std::shared_ptr<ClientSession>> expiredPlayers;
    const std::vector<std::shared_ptr<ClientSession>> players =
        GetLoggedInPlayersSnapshot();

    for (const std::shared_ptr<ClientSession>& player : players)
    {
        if (player == nullptr)
        {
            continue;
        }

        if (!player->IsConnected() || !player->IsLoggedIn())
        {
            continue;
        }

        bool rageExpired = false;

        if (player->UpdateTimedBuffs(nowMs, rageExpired) && rageExpired)
        {
            expiredPlayers.push_back(player);
        }
    }

    for (const std::shared_ptr<ClientSession>& player : expiredPlayers)
    {
        if (player == nullptr)
        {
            continue;
        }

        SendPlayerStatus(player.get(), 0, 0);
        SendBattleMessage(player.get(), "Rage faded.");
    }
}

void GameWorld::UpdatePlayerRespawns(long long nowMs)
{
    std::vector<std::shared_ptr<ClientSession>> players =
        GetLoggedInPlayersSnapshot();

    for (const std::shared_ptr<ClientSession>& player : players)
    {
        if (player == nullptr)
        {
            continue;
        }

        if (!player->IsConnected() || !player->IsLoggedIn())
        {
            continue;
        }

        int remainingRespawnSeconds = 0;

        if (player->TryConsumeRespawnCountdownNotice(
            nowMs,
            remainingRespawnSeconds
        ))
        {
            SendBattleMessage(
                player.get(),
                "Respawn in " +
                std::to_string(remainingRespawnSeconds) +
                "..."
            );
        }

        if (!player->IsRespawnReady(nowMs))
        {
            continue;
        }

        int respawnX = PLAYER_START_X;
        int respawnY = PLAYER_START_Y;

        {
            std::lock_guard<std::mutex> lock(worldLock_);

            const int oldX = player->GetX();
            const int oldY = player->GetY();

            RemovePlayerFromSector(player->GetPlayerId(), oldX, oldY);

            if (!FindSpawnPositionNear(
                PLAYER_START_X,
                PLAYER_START_Y,
                respawnX,
                respawnY
            ))
            {
                respawnX = PLAYER_START_X;
                respawnY = PLAYER_START_Y;
            }

            player->SetPosition(respawnX, respawnY);

            AddPlayerToSector(player.get());
        }

        player->ReviveFull();
        player->ResetHpRegenTimer(nowMs + PLAYER_HP_REGEN_INTERVAL_MS);

        SendPlayerStatus(player.get(), 0, 0);

        SendBattleMessage(
            player.get(),
            "You respawned at the start village."
        );

        UpdatePlayerVisibilityFor(player.get());
        UpdateMonsterVisibilityFor(player.get());

        BroadcastMove(player.get(), 0);
        SavePlayerDataToDatabase(player.get());
    }
}

bool GameWorld::RollMonsterDrop(
    ClientSession* player,
    const Monster& monster,
    int& outItemId,
    int& outItemCategory,
    int& outCount,
    std::string& outItemName
)
{
    static thread_local std::mt19937 rng{ std::random_device{}() };

    int dropBonusBasisPoint = 0;

    if (player != nullptr)
    {
        dropBonusBasisPoint = player->GetDropBonusBasisPoint();
    }

    if (dropBonusBasisPoint < 0)
    {
        dropBonusBasisPoint = 0;
    }

    if (dropBonusBasisPoint > 3000)
    {
        dropBonusBasisPoint = 3000;
    }

    int noDropBasisPoint = 5000 - dropBonusBasisPoint;

    if (noDropBasisPoint < 2000)
    {
        noDropBasisPoint = 2000;
    }

    std::uniform_int_distribution<int> rollDist(1, 10000);
    const int roll = rollDist(rng);

    if (roll <= noDropBasisPoint)
    {
        return false;
    }

    const int dropArea = 10000 - noDropBasisPoint;

    const int etcArea = dropArea * 60 / 100;
    const int consumableArea = dropArea * 30 / 100;

    const int etcLimit = noDropBasisPoint + etcArea;
    const int consumableLimit = etcLimit + consumableArea;

    
    if (roll <= etcLimit)
    {
        outItemId = ITEM_MONSTER_PART_BASE + monster.monsterType;
        outItemCategory = ITEM_CATEGORY_ETC;
        outCount = 1;
        outItemName = monster.name + " Part";
        return true;
    }

    if (roll <= consumableLimit)
    {
        std::uniform_int_distribution<int> potionDist(0, 3);

        switch (potionDist(rng))
        {
        case 0:
            outItemId = ITEM_HP_POTION;
            outItemName = "HP Potion";
            break;

        case 1:
            outItemId = ITEM_MP_POTION;
            outItemName = "MP Potion";
            break;

        case 2:
            outItemId = ITEM_HIGH_HP_POTION;
            outItemName = "High HP Potion";
            break;

        default:
            outItemId = ITEM_HIGH_MP_POTION;
            outItemName = "High MP Potion";
            break;
        }

        outItemCategory = ITEM_CATEGORY_CONSUMABLE;
        outCount = 1;

        return true;
    }

    struct DropEquipmentCandidate
    {
        int itemId;
        const char* itemName;
    };

    static const DropEquipmentCandidate equipmentCandidates[] =
    {
        { ITEM_IRON_SWORD, "Iron Sword" },
        { ITEM_STEEL_SWORD, "Steel Sword" },
        { ITEM_FLAME_SWORD, "Flame Sword" },
        { ITEM_MAGE_STAFF, "Mage Staff" },
        { ITEM_ICE_WAND, "Ice Wand" },
        { ITEM_HUNTER_BOW, "Hunter Bow" },
        { ITEM_IRON_ARMOR, "Iron Armor" },
        { ITEM_STEEL_ARMOR, "Steel Armor" },
        { ITEM_MAGIC_ROBE, "Magic Robe" },
        { ITEM_LEATHER_ARMOR, "Leather Armor" },
        { ITEM_MAGIC_RING, "Magic Ring" },
        { ITEM_WARRIOR_RING, "Warrior Ring" },
        { ITEM_LUCKY_NECKLACE, "Lucky Necklace" },
        { ITEM_SPEED_CHARM, "Speed Charm" }
    };

    std::uniform_int_distribution<int> equipmentDist(
        0,
        static_cast<int>(sizeof(equipmentCandidates) / sizeof(equipmentCandidates[0])) - 1
    );

    const DropEquipmentCandidate& selectedEquipment =
        equipmentCandidates[equipmentDist(rng)];

    outItemId = selectedEquipment.itemId;
    outItemName = selectedEquipment.itemName;

    outItemCategory = ITEM_CATEGORY_EQUIPMENT;
    outCount = 1;

    return true;
}

void GameWorld::CreatePersonalDropsForMonster( const Monster& monster, const std::vector<std::shared_ptr<ClientSession>>& contributors )
{
    std::vector<std::pair<std::shared_ptr<ClientSession>, DropItem>> createdDrops;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (const std::shared_ptr<ClientSession>& contributor : contributors)
        {
            if (contributor == nullptr)
            {
                continue;
            }

            if (!contributor->IsConnected() ||
                !contributor->IsLoggedIn() ||
                !contributor->IsAlive())
            {
                continue;
            }

            int itemId = 0;
            int itemCategory = 0;
            int count = 0;
            std::string itemName;

            if (!RollMonsterDrop(
                contributor.get(),
                monster,
                itemId,
                itemCategory,
                count,
                itemName
            ))
            {
                continue;
            }

            DropItem drop{};
            drop.dropId = nextDropId_++;
            drop.ownerPlayerId = contributor->GetPlayerId();

            drop.itemId = itemId;
            drop.itemCategory = itemCategory;
            drop.count = count;
            drop.itemName = itemName;

            drop.x = monster.x;
            drop.y = monster.y;
            drop.expireTimeMs = GetNowMs() + DROP_EXPIRE_DELAY_MS;

            dropItems_[drop.dropId] = drop;

            createdDrops.push_back({ contributor, drop });
        }
    }

    for (const auto& [receiver, drop] : createdDrops)
    {
        if (receiver == nullptr)
        {
            continue;
        }

        SendAddDrop(receiver.get(), drop);

        SendBattleMessage(
            receiver.get(),
            "Item dropped: " + drop.itemName
        );
    }
}

void GameWorld::TryPickupDropsAt(ClientSession* player)
{
    if (player == nullptr)
    {
        return;
    }

    if (!player->IsConnected() ||
        !player->IsLoggedIn() ||
        !player->IsAlive())
    {
        return;
    }

    const int playerId = player->GetPlayerId();
    const int playerX = player->GetX();
    const int playerY = player->GetY();

    std::vector<int> removedDropIds;
    std::vector<InventoryItem> updatedItems;
    std::vector<std::string> pickedItemNames;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (auto iter = dropItems_.begin(); iter != dropItems_.end(); )
        {
            DropItem& drop = iter->second;

            if (drop.ownerPlayerId != playerId ||
                drop.x != playerX ||
                drop.y != playerY)
            {
                ++iter;
                continue;
            }

            InventoryItem& inventoryItem =
                inventoriesByPlayerId_[playerId][drop.itemId];

            inventoryItem.itemId = drop.itemId;
            inventoryItem.itemCategory = drop.itemCategory;
            inventoryItem.itemName = drop.itemName;
            inventoryItem.count += drop.count;

            removedDropIds.push_back(drop.dropId);
            updatedItems.push_back(inventoryItem);
            pickedItemNames.push_back(drop.itemName);

            iter = dropItems_.erase(iter);
        }
    }

    for (int dropId : removedDropIds)
    {
        SendRemoveDrop(player, dropId);
    }

    for (const InventoryItem& item : updatedItems)
    {
        SendInventoryItemUpdate(player, item);
    }

    for (const std::string& itemName : pickedItemNames)
    {
        SendBattleMessage(
            player,
            "Picked up: " + itemName
        );
    }
}

void GameWorld::UpdateDropExpirations(long long nowMs)
{
    struct ExpiredDropNotice
    {
        int ownerPlayerId = -1;
        int dropId = -1;
        std::string itemName;
    };

    std::vector<ExpiredDropNotice> expiredDrops;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        for (auto iter = dropItems_.begin(); iter != dropItems_.end(); )
        {
            const DropItem& drop = iter->second;

            if (drop.expireTimeMs <= 0 || nowMs < drop.expireTimeMs)
            {
                ++iter;
                continue;
            }

            expiredDrops.push_back(
                ExpiredDropNotice{
                    drop.ownerPlayerId,
                    drop.dropId,
                    drop.itemName
                }
            );

            iter = dropItems_.erase(iter);
        }
    }

    for (const ExpiredDropNotice& expiredDrop : expiredDrops)
    {
        std::shared_ptr<ClientSession> owner =
            FindPlayerById(expiredDrop.ownerPlayerId);

        if (owner == nullptr)
        {
            continue;
        }

        if (!owner->IsConnected() || !owner->IsLoggedIn())
        {
            continue;
        }

        SendRemoveDrop(owner.get(), expiredDrop.dropId);

        SendBattleMessage(
            owner.get(),
            "Drop expired: " + expiredDrop.itemName
        );
    }
}

void GameWorld::SendAddDrop(ClientSession* receiver, const DropItem& drop)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_AddDrop packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_AddDrop));
    packet.type = S2C_ADD_DROP;

    packet.dropId = drop.dropId;
    packet.ownerPlayerId = drop.ownerPlayerId;

    packet.itemId = drop.itemId;
    packet.itemCategory = drop.itemCategory;
    packet.count = drop.count;

    packet.x = drop.x;
    packet.y = drop.y;

    CopyStringToFixedBuffer(
        packet.itemName,
        sizeof(packet.itemName),
        drop.itemName
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendRemoveDrop(ClientSession* receiver, int dropId)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_RemoveDrop packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_RemoveDrop));
    packet.type = S2C_REMOVE_DROP;

    packet.dropId = dropId;

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendInventoryItemUpdate( ClientSession* receiver, const InventoryItem& item )
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_InventoryItemUpdate packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_InventoryItemUpdate));
    packet.type = S2C_INVENTORY_ITEM_UPDATE;

    packet.itemId = item.itemId;
    packet.itemCategory = item.itemCategory;
    packet.count = item.count;

    CopyStringToFixedBuffer(
        packet.itemName,
        sizeof(packet.itemName),
        item.itemName
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendInventorySnapshot(ClientSession* receiver)
{
    if (receiver == nullptr)
    {
        return;
    }

    std::vector<InventoryItem> items;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        auto inventoryIter = inventoriesByPlayerId_.find(receiver->GetPlayerId());

        if (inventoryIter != inventoriesByPlayerId_.end())
        {
            for (const auto& [itemId, item] : inventoryIter->second)
            {
                (void)itemId;

                if (item.count > 0)
                {
                    items.push_back(item);
                }
            }
        }
    }

    for (const InventoryItem& item : items)
    {
        SendInventoryItemUpdate(receiver, item);
    }
}

void GameWorld::SendEquipmentStatus(ClientSession* receiver)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_EquipmentStatus packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_EquipmentStatus));
    packet.type = S2C_EQUIPMENT_STATUS;

    packet.playerId = receiver->GetPlayerId();

    packet.weaponItemId = receiver->GetEquippedWeaponItemId();

    CopyStringToFixedBuffer(
        packet.weaponName,
        sizeof(packet.weaponName),
        receiver->GetEquippedWeaponName()
    );

    packet.armorItemId = receiver->GetEquippedArmorItemId();

    CopyStringToFixedBuffer(
        packet.armorName,
        sizeof(packet.armorName),
        receiver->GetEquippedArmorName()
    );

    packet.accessoryItemId = receiver->GetEquippedAccessoryItemId();

    CopyStringToFixedBuffer(
        packet.accessoryName,
        sizeof(packet.accessoryName),
        receiver->GetEquippedAccessoryName()
    );

    packet.strBonus = receiver->GetEquipmentStrBonus();
    packet.intBonus = receiver->GetEquipmentIntBonus();
    packet.dexBonus = receiver->GetEquipmentDexBonus();
    packet.lukBonus = receiver->GetEquipmentLukBonus();

    packet.physicalAttackBonus =
        receiver->GetEquipmentPhysicalAttackBonus();

    packet.physicalDefenseBonus =
        receiver->GetEquipmentPhysicalDefenseBonus();

    packet.magicAttackBonus =
        receiver->GetEquipmentMagicAttackBonus();

    packet.magicDefenseBonus =
        receiver->GetEquipmentMagicDefenseBonus();

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

void GameWorld::SendQuickSlotStatus(ClientSession* receiver)
{
    if (receiver == nullptr || sendFunction_ == nullptr)
    {
        return;
    }

    S2C_QuickSlotStatus packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_QuickSlotStatus));
    packet.type = S2C_QUICK_SLOT_STATUS;

    receiver->GetQuickSlotStatus(
        packet.slots,
        MAX_QUICK_SLOT_COUNT,
        packet.slotCount
    );

    sendFunction_(
        receiver,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool GameWorld::FindInventoryItem(
    int playerId,
    int itemId,
    InventoryItem& outItem
)
{
    outItem = InventoryItem{};

    auto inventoryIter = inventoriesByPlayerId_.find(playerId);

    if (inventoryIter == inventoriesByPlayerId_.end())
    {
        return false;
    }

    auto& inventory = inventoryIter->second;

    auto itemIter = inventory.find(itemId);

    if (itemIter == inventory.end())
    {
        return false;
    }

    const InventoryItem& item = itemIter->second;

    if (item.count <= 0)
    {
        return false;
    }

    outItem = item;
    return true;
}

bool GameWorld::ConsumeInventoryItem(
    int playerId,
    int itemId,
    InventoryItem& outUpdatedItem
)
{
    outUpdatedItem = InventoryItem{};

    auto inventoryIter = inventoriesByPlayerId_.find(playerId);

    if (inventoryIter == inventoriesByPlayerId_.end())
    {
        return false;
    }

    auto& inventory = inventoryIter->second;

    auto itemIter = inventory.find(itemId);

    if (itemIter == inventory.end())
    {
        return false;
    }

    InventoryItem& item = itemIter->second;

    if (item.count <= 0)
    {
        return false;
    }

    item.count -= 1;

    outUpdatedItem.itemId = item.itemId;
    outUpdatedItem.itemCategory = item.itemCategory;
    outUpdatedItem.itemName = item.itemName;
    outUpdatedItem.count = item.count;

    if (item.count <= 0)
    {
        outUpdatedItem.count = 0;
        inventory.erase(itemIter);
    }

    return true;
}

void GameWorld::AddInventoryItem(
    int playerId,
    int itemId,
    int itemCategory,
    int count,
    const std::string& itemName,
    InventoryItem& outUpdatedItem
)
{
    outUpdatedItem = InventoryItem{};

    if (itemId <= 0 || count <= 0)
    {
        return;
    }

    InventoryItem& item = inventoriesByPlayerId_[playerId][itemId];

    item.itemId = itemId;
    item.itemCategory = itemCategory;
    item.itemName = itemName;
    item.count += count;

    outUpdatedItem = item;
}




bool GameWorld::UseHealerNpc(ClientSession* player, int npcId)
{
    if (player == nullptr || !player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (!player->IsAlive())
    {
        SendBattleMessage(player, "You cannot use a healer while dead.");
        return false;
    }

    Npc npc{};

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        auto iter = npcsById_.find(npcId);

        if (iter == npcsById_.end())
        {
            SendBattleMessage(player, "There is no NPC here.");
            return false;
        }

        npc = iter->second;
    }

    if (npc.npcType != NPC_TYPE_HEALER)
    {
        SendBattleMessage(player, "This NPC is not a healer.");
        return false;
    }

    int dx = player->GetX() - npc.x;
    int dy = player->GetY() - npc.y;

    if (dx < 0)
    {
        dx = -dx;
    }

    if (dy < 0)
    {
        dy = -dy;
    }

    if (dx + dy > 3)
    {
        SendBattleMessage(player, "Move closer to the healer.");
        return false;
    }

    player->FullHeal();
    SendPlayerStatus(player, 0, 0);
    SendBattleMessage(player, npc.name + " restored your HP and MP.");
    SavePlayerDataToDatabase(player);

    return true;
}

bool GameWorld::SellItem(ClientSession* player, int itemId, int count)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (!player->IsAlive())
    {
        SendBattleMessage(player, "You cannot sell items while dead.");
        return false;
    }

    if (count <= 0)
    {
        count = 1;
    }

    const int sellPrice = GetSellPriceByItemId(itemId);

    if (sellPrice <= 0)
    {
        SendBattleMessage(player, "This item cannot be sold.");
        return false;
    }

    InventoryItem updatedItem{};
    std::string soldItemName;
    int sellCount = 0;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        auto inventoryIter = inventoriesByPlayerId_.find(player->GetPlayerId());

        if (inventoryIter == inventoriesByPlayerId_.end())
        {
            SendBattleMessage(player, "You do not have this item.");
            return false;
        }

        auto& inventory = inventoryIter->second;
        auto itemIter = inventory.find(itemId);

        if (itemIter == inventory.end() || itemIter->second.count <= 0)
        {
            SendBattleMessage(player, "You do not have this item.");
            return false;
        }

        InventoryItem& item = itemIter->second;

        if (item.itemCategory != ITEM_CATEGORY_ETC)
        {
            SendBattleMessage(player, "Only ETC items can be sold now.");
            return false;
        }

        sellCount = std::min(count, item.count);
        soldItemName = item.itemName;

        item.count -= sellCount;

        updatedItem.itemId = item.itemId;
        updatedItem.itemCategory = item.itemCategory;
        updatedItem.itemName = item.itemName;
        updatedItem.count = item.count;

        if (item.count <= 0)
        {
            updatedItem.count = 0;
            inventory.erase(itemIter);
        }
    }

    const int goldGained = sellPrice * sellCount;
    player->AddGold(goldGained);

    SendInventoryItemUpdate(player, updatedItem);
    SendPlayerStatus(player, 0, 0);

    SendBattleMessage(
        player,
        "Sold " +
        soldItemName +
        " x" +
        std::to_string(sellCount) +
        " for " +
        std::to_string(goldGained) +
        " Gold."
    );

    SavePlayerDataToDatabase(player);

    return true;
}

bool GameWorld::SetQuickSlot(
    ClientSession* player,
    int slotIndex,
    int slotType,
    int targetId,
    const std::string& name
)
{
    (void)name;

    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (slotIndex < 0 || slotIndex >= MAX_QUICK_SLOT_COUNT)
    {
        SendBattleMessage(player, "Invalid quick slot.");
        SendQuickSlotStatus(player);
        return false;
    }

    if (slotType != QUICK_SLOT_TYPE_ITEM)
    {
        SendBattleMessage(player, "Only consumable items can be registered now.");
        SendQuickSlotStatus(player);
        return false;
    }

    InventoryItem item{};

    bool hasItem = false;

    {
        std::lock_guard<std::mutex> lock(worldLock_);
        hasItem = FindInventoryItem(player->GetPlayerId(), targetId, item);
    }

    if (!hasItem)
    {
        SendBattleMessage(player, "You do not have this item.");
        SendQuickSlotStatus(player);
        return false;
    }

    if (item.itemCategory != ITEM_CATEGORY_CONSUMABLE)
    {
        SendBattleMessage(player, "Only consumable items can be registered now.");
        SendQuickSlotStatus(player);
        return false;
    }

    if (!player->SetQuickSlot(
        slotIndex,
        QUICK_SLOT_TYPE_ITEM,
        targetId,
        item.itemName
    ))
    {
        SendBattleMessage(player, "Failed to set quick slot.");
        SendQuickSlotStatus(player);
        return false;
    }

    SendQuickSlotStatus(player);
    SendBattleMessage(
        player,
        "Quick Slot " +
        std::to_string(slotIndex + 1) +
        " set to " +
        item.itemName +
        "."
    );

    SaveQuickSlotsToDatabase(player);

    return true;
}

bool GameWorld::ClearQuickSlot(ClientSession* player, int slotIndex)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (!player->ClearQuickSlot(slotIndex))
    {
        SendBattleMessage(player, "Invalid quick slot.");
        SendQuickSlotStatus(player);
        return false;
    }

    SendQuickSlotStatus(player);
    SendBattleMessage(
        player,
        "Quick Slot " +
        std::to_string(slotIndex + 1) +
        " cleared."
    );

    SaveQuickSlotsToDatabase(player);

    return true;
}

bool GameWorld::UnequipEquipment(ClientSession* player, int equipmentSlot)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (!player->IsAlive())
    {
        SendBattleMessage(player, "You cannot unequip equipment while dead.");
        return false;
    }

    int returnedItemId = 0;
    std::string returnedItemName;
    bool unequipped = false;

    switch (equipmentSlot)
    {
    case EQUIPMENT_SLOT_WEAPON:
        unequipped = player->UnequipWeapon(returnedItemId, returnedItemName);
        break;

    case EQUIPMENT_SLOT_ARMOR:
        unequipped = player->UnequipArmor(returnedItemId, returnedItemName);
        break;

    case EQUIPMENT_SLOT_ACCESSORY:
        unequipped = player->UnequipAccessory(returnedItemId, returnedItemName);
        break;

    default:
        SendBattleMessage(player, "This equipment slot cannot be unequipped.");
        return false;
    }

    if (!unequipped)
    {
        SendBattleMessage(
            player,
            "No " + GetEquipmentSlotName(equipmentSlot) + " is equipped."
        );
        return false;
    }

    InventoryItem updatedItem{};

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        AddInventoryItem(
            player->GetPlayerId(),
            returnedItemId,
            ITEM_CATEGORY_EQUIPMENT,
            1,
            returnedItemName,
            updatedItem
        );
    }

    SendInventoryItemUpdate(player, updatedItem);
    SendPlayerStatus(player, 0, 0);
    SendEquipmentStatus(player);

    SendBattleMessage(
        player,
        "Unequipped " + returnedItemName + "."
    );

    SavePlayerDataToDatabase(player);

    return true;
}

bool GameWorld::UseItem(ClientSession* player, int itemId)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (!player->IsAlive())
    {
        SendBattleMessage(player, "You cannot use item while dead.");
        return false;
    }

    if (IsEquipmentItemId(itemId))
    {
        const int equipmentSlot = GetEquipmentSlotByItemId(itemId);

        if (equipmentSlot == EQUIPMENT_SLOT_WEAPON &&
            player->GetEquippedWeaponItemId() > 0)
        {
            SendBattleMessage(player, "Unequip current weapon first.");
            return false;
        }

        if (equipmentSlot == EQUIPMENT_SLOT_ARMOR &&
            player->GetEquippedArmorItemId() > 0)
        {
            SendBattleMessage(player, "Unequip current armor first.");
            return false;
        }

        if (equipmentSlot == EQUIPMENT_SLOT_ACCESSORY &&
            player->GetEquippedAccessoryItemId() > 0)
        {
            SendBattleMessage(player, "Unequip current accessory first.");
            return false;
        }

        InventoryItem item{};
        InventoryItem updatedItem{};

        {
            std::lock_guard<std::mutex> lock(worldLock_);

            if (!FindInventoryItem(
                player->GetPlayerId(),
                itemId,
                item
            ))
            {
                SendBattleMessage(player, "You do not have this equipment.");
                return false;
            }

            if (item.itemCategory != ITEM_CATEGORY_EQUIPMENT)
            {
                SendBattleMessage(player, "This item is not equipment.");
                return false;
            }

            if (!ConsumeInventoryItem(
                player->GetPlayerId(),
                itemId,
                updatedItem
            ))
            {
                SendBattleMessage(player, "You do not have this equipment.");
                return false;
            }
        }

        bool equipped = false;

        switch (equipmentSlot)
        {
        case EQUIPMENT_SLOT_WEAPON:
            equipped = player->EquipWeapon(item.itemId, item.itemName);
            break;

        case EQUIPMENT_SLOT_ARMOR:
            equipped = player->EquipArmor(item.itemId, item.itemName);
            break;

        case EQUIPMENT_SLOT_ACCESSORY:
            equipped = player->EquipAccessory(item.itemId, item.itemName);
            break;

        default:
            equipped = false;
            break;
        }

        if (!equipped)
        {
            InventoryItem rollbackItem{};

            {
                std::lock_guard<std::mutex> lock(worldLock_);

                AddInventoryItem(
                    player->GetPlayerId(),
                    item.itemId,
                    item.itemCategory,
                    1,
                    item.itemName,
                    rollbackItem
                );
            }

            SendInventoryItemUpdate(player, rollbackItem);
            SendBattleMessage(player, "Cannot equip this item.");
            return false;
        }

        SendInventoryItemUpdate(player, updatedItem);
        SendPlayerStatus(player, 0, 0);
        SendEquipmentStatus(player);

        SendBattleMessage(
            player,
            "Equipped " + item.itemName + "."
        );

        SavePlayerDataToDatabase(player);

        return true;
    }

    if (itemId != ITEM_HP_POTION &&
        itemId != ITEM_MP_POTION &&
        itemId != ITEM_HIGH_HP_POTION &&
        itemId != ITEM_HIGH_MP_POTION &&
        itemId != ITEM_RETURN_SCROLL)
    {
        SendBattleMessage(player, "This item cannot be used.");
        return false;
    }

    if (itemId == ITEM_RETURN_SCROLL)
    {
        InventoryItem updatedItem{};

        int oldX = 0;
        int oldY = 0;

        {
            std::lock_guard<std::mutex> lock(worldLock_);

            if (!ConsumeInventoryItem(
                player->GetPlayerId(),
                itemId,
                updatedItem
            ))
            {
                SendBattleMessage(player, "You do not have Return Scroll.");
                return false;
            }

            oldX = player->GetX();
            oldY = player->GetY();

            player->SetPosition(PLAYER_START_X, PLAYER_START_Y);
            MovePlayerSectorIfNeeded(
                player,
                oldX,
                oldY,
                PLAYER_START_X,
                PLAYER_START_Y
            );
        }

        SendInventoryItemUpdate(player, updatedItem);
        SendPlayerStatus(player, 0, 0);
        BroadcastMove(player, 0);
        UpdateMonsterVisibilityFor(player);
        UpdatePlayerVisibilityFor(player);
        SendVisibleNpcsTo(player);
        SendBattleMessage(player, "Used Return Scroll. Returned to Start Village.");
        SavePlayerDataToDatabase(player);
        return true;
    }

    if (itemId == ITEM_HP_POTION || itemId == ITEM_HIGH_HP_POTION)
    {
        if (player->GetHp() >= player->GetMaxHp())
        {
            SendBattleMessage(player, "HP is already full.");
            return false;
        }

        InventoryItem updatedItem{};

        {
            std::lock_guard<std::mutex> lock(worldLock_);

            if (!ConsumeInventoryItem(
                player->GetPlayerId(),
                itemId,
                updatedItem
            ))
            {
                SendBattleMessage(player, "You do not have HP potion item.");
                return false;
            }
        }

        int healed = 0;
        int hp = 0;
        int maxHp = 0;

        const int healAmount =
            itemId == ITEM_HIGH_HP_POTION ? 150 : 50;

        const bool healedResult =
            player->HealHp(
                healAmount,
                healed,
                hp,
                maxHp
            );

        if (!healedResult)
        {
            SendBattleMessage(player, "HP Potion had no effect.");
            SendInventoryItemUpdate(player, updatedItem);
            return false;
        }

        SendInventoryItemUpdate(player, updatedItem);
        SendPlayerStatus(player, 0, 0);

        SendBattleMessage(
            player,
            "Used " + GetItemDisplayNameByItemId(itemId) + ". HP +" +
            std::to_string(healed) +
            ". HP: " +
            std::to_string(hp) +
            "/" +
            std::to_string(maxHp)
        );

        SavePlayerDataToDatabase(player);

        return true;
    }

    if (itemId == ITEM_MP_POTION || itemId == ITEM_HIGH_MP_POTION)
    {
        if (player->GetMp() >= player->GetMaxMp())
        {
            SendBattleMessage(player, "MP is already full.");
            return false;
        }

        InventoryItem updatedItem{};

        {
            std::lock_guard<std::mutex> lock(worldLock_);

            if (!ConsumeInventoryItem(
                player->GetPlayerId(),
                itemId,
                updatedItem
            ))
            {
                SendBattleMessage(player, "You do not have MP potion item.");
                return false;
            }
        }

        const int beforeMp = player->GetMp();

        int mp = 0;
        int maxMp = 0;

        const int recoverAmount =
            itemId == ITEM_HIGH_MP_POTION ? 150 : 50;

        const bool recoveredResult =
            player->RecoverMp(
                recoverAmount,
                mp,
                maxMp
            );

        const int recovered = mp - beforeMp;

        if (!recoveredResult || recovered <= 0)
        {
            SendBattleMessage(player, "MP Potion had no effect.");
            SendInventoryItemUpdate(player, updatedItem);
            return false;
        }

        SendInventoryItemUpdate(player, updatedItem);
        SendPlayerStatus(player, 0, 0);

        SendBattleMessage(
            player,
            "Used " + GetItemDisplayNameByItemId(itemId) + ". MP +" +
            std::to_string(recovered) +
            ". MP: " +
            std::to_string(mp) +
            "/" +
            std::to_string(maxMp)
        );

        SavePlayerDataToDatabase(player);

        return true;
    }

    return false;
}

bool GameWorld::UseSkill(ClientSession* player, int skillId)
{
    if (player == nullptr)
    {
        return false;
    }

    if (!player->IsConnected() || !player->IsLoggedIn())
    {
        return false;
    }

    if (!player->IsAlive())
    {
        SendBattleMessage(player, "You cannot use skill while dead.");
        return false;
    }

    const int skillLevel = player->GetSkillLevel(skillId);

    if (skillLevel <= 0)
    {
        switch (skillId)
        {
        case SKILL_VITALITY_TRAINING:
            SendBattleMessage(player, "You have not learned Rage.");
            break;

        case SKILL_POWER_STRIKE:
            SendBattleMessage(player, "You have not learned Power Strike.");
            break;

        case SKILL_HEAVY_SLASH:
            SendBattleMessage(player, "You have not learned Heavy Slash.");
            break;

        case SKILL_WHIRLWIND:
            SendBattleMessage(player, "You have not learned Whirlwind.");
            break;

        case SKILL_FIREBALL:
            SendBattleMessage(player, "You have not learned Fireball.");
            break;

        case SKILL_MANA_BURST:
            SendBattleMessage(player, "You have not learned Mana Burst.");
            break;

        case SKILL_ICE_BOLT:
            SendBattleMessage(player, "You have not learned Ice Bolt.");
            break;

        default:
            SendBattleMessage(player, "This skill cannot be used.");
            break;
        }

        return false;
    }

    bool used = false;

    switch (skillId)
    {
    case SKILL_VITALITY_TRAINING:
        used = UseRage(player, skillLevel);
        break;

    case SKILL_POWER_STRIKE:
        used = UsePowerStrike(player, skillLevel);
        break;

    case SKILL_HEAVY_SLASH:
        used = UseHeavySlash(player, skillLevel);
        break;

    case SKILL_WHIRLWIND:
        used = UseWhirlwind(player, skillLevel);
        break;

    case SKILL_FIREBALL:
        used = UseFireball(player, skillLevel);
        break;

    case SKILL_MANA_BURST:
        used = UseManaBurst(player, skillLevel);
        break;

    case SKILL_ICE_BOLT:
        used = UseIceBolt(player, skillLevel);
        break;

    default:
        SendBattleMessage(player, "This skill cannot be used.");
        return false;
    }

    if (used)
    {
        MarkPlayerDirty(player);
    }

    return used;
}

bool GameWorld::UseRage(ClientSession* player, int skillLevel)
{
    const long long nowMs = GetNowMs();

    int mp = 0;
    int maxMp = 0;
    int strBonus = 0;
    int physicalAttackBonus = 0;
    int durationSeconds = 0;
    std::string failReason;

    const bool activated = player->ActivateRage(
        nowMs,
        skillLevel,
        mp,
        maxMp,
        strBonus,
        physicalAttackBonus,
        durationSeconds,
        failReason
    );

    if (!activated)
    {
        if (failReason.empty())
        {
            failReason = "Failed to use Rage.";
        }

        SendBattleMessage(player, failReason);
        SendPlayerStatus(player, 0, 0);
        return false;
    }

    SendPlayerStatus(player, 0, 0);

    SendBattleMessage(
        player,
        "Rage activated. STR +" +
        std::to_string(strBonus) +
        ", P.ATK +" +
        std::to_string(physicalAttackBonus) +
        " for " +
        std::to_string(durationSeconds) +
        " sec. MP: " +
        std::to_string(mp) +
        "/" +
        std::to_string(maxMp)
    );

    return true;
}

bool GameWorld::UsePowerStrike(ClientSession* player, int skillLevel)
{
    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(player->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(player, "Invalid skill direction.");
        return false;
    }

    std::vector<std::pair<int, int>> targets;
    AppendDirectionalTargetPositions(
        targets,
        player->GetX(),
        player->GetY(),
        dx,
        dy,
        2,
        0
    );

    const int mpCost =
        POWER_STRIKE_BASE_MP_COST +
        skillLevel * POWER_STRIKE_MP_COST_PER_LEVEL;

    return ApplySkillDamageToTargets(
        player,
        "Power Strike",
        targets,
        1,
        mpCost,
        130,
        10,
        0,
        false
    );
}

bool GameWorld::UseHeavySlash(ClientSession* player, int skillLevel)
{
    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(player->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(player, "Invalid skill direction.");
        return false;
    }

    std::vector<std::pair<int, int>> targets;
    AppendDirectionalTargetPositions(
        targets,
        player->GetX(),
        player->GetY(),
        dx,
        dy,
        2,
        1
    );

    const int mpCost =
        HEAVY_SLASH_BASE_MP_COST +
        skillLevel * HEAVY_SLASH_MP_COST_PER_LEVEL;

    return ApplySkillDamageToTargets(
        player,
        "Heavy Slash",
        targets,
        1,
        mpCost,
        150,
        12,
        0,
        false
    );
}

bool GameWorld::UseWhirlwind(ClientSession* player, int skillLevel)
{
    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(player->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(player, "Invalid skill direction.");
        return false;
    }

    std::vector<std::pair<int, int>> targets;
    AppendForwardConeTargets(targets, player->GetX(), player->GetY(), dx, dy, 2, 1);

    const int mpCost =
        WHIRLWIND_BASE_MP_COST +
        skillLevel * WHIRLWIND_MP_COST_PER_LEVEL;

    return ApplySkillDamageToTargets(
        player,
        "Whirlwind",
        targets,
        8,
        mpCost,
        100,
        8,
        0,
        false
    );
}

bool GameWorld::UseFireball(ClientSession* player, int skillLevel)
{
    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(player->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(player, "Invalid skill direction.");
        return false;
    }

    std::vector<std::pair<int, int>> targets;

    AppendDirectionalTargetPositions(
        targets,
        player->GetX(),
        player->GetY(),
        dx,
        dy,
        5,
        0
    );

    const int mpCost =
        FIREBALL_BASE_MP_COST +
        skillLevel * FIREBALL_MP_COST_PER_LEVEL;

    return ApplySkillDamageToTargets(
        player,
        "Fireball",
        targets,
        1,
        mpCost,
        135,
        10,
        0,
        true
    );
}

bool GameWorld::UseManaBurst(ClientSession* player, int skillLevel)
{
    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(player->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(player, "Invalid skill direction.");
        return false;
    }

    std::vector<std::pair<int, int>> targets;
    AppendDirectionalTargetPositions(
        targets,
        player->GetX(),
        player->GetY(),
        dx,
        dy,
        4,
        2
    );

    const int mpCost =
        MANA_BURST_BASE_MP_COST +
        skillLevel * MANA_BURST_MP_COST_PER_LEVEL;

    return ApplySkillDamageToTargets(
        player,
        "Mana Burst",
        targets,
        3,
        mpCost,
        160,
        12,
        0,
        true
    );
}

bool GameWorld::UseIceBolt(ClientSession* player, int skillLevel)
{
    int dx = 0;
    int dy = 0;

    if (!DirectionToDelta(player->GetFacingDirection(), dx, dy))
    {
        SendBattleMessage(player, "Invalid skill direction.");
        return false;
    }

    std::vector<std::pair<int, int>> targets;

    AppendDirectionalTargetPositions(
        targets,
        player->GetX(),
        player->GetY(),
        dx,
        dy,
        5,
        0
    );

    const int mpCost =
        ICE_BOLT_BASE_MP_COST +
        skillLevel * ICE_BOLT_MP_COST_PER_LEVEL;

    return ApplySkillDamageToTargets(
        player,
        "Ice Bolt",
        targets,
        1,
        mpCost,
        120,
        8,
        0,
        true
    );
}

bool GameWorld::ApplySkillDamageToTargets(
    ClientSession* player,
    const std::string& skillName,
    const std::vector<std::pair<int, int>>& targetPositions,
    int maxHitCount,
    int mpCost,
    int skillPowerPercent,
    int skillPowerPercentPerLevel,
    int flatBonus,
    bool useIntStat
)
{
    if (player == nullptr)
    {
        return false;
    }

    if (targetPositions.empty() || maxHitCount <= 0)
    {
        SendBattleMessage(player, "There is no skill target.");
        return false;
    }

    int skillId = SKILL_NONE;

    if (skillName == "Power Strike")
    {
        skillId = SKILL_POWER_STRIKE;
    }
    else if (skillName == "Heavy Slash")
    {
        skillId = SKILL_HEAVY_SLASH;
    }
    else if (skillName == "Whirlwind")
    {
        skillId = SKILL_WHIRLWIND;
    }
    else if (skillName == "Fireball")
    {
        skillId = SKILL_FIREBALL;
    }
    else if (skillName == "Mana Burst")
    {
        skillId = SKILL_MANA_BURST;
    }
    else if (skillName == "Ice Bolt")
    {
        skillId = SKILL_ICE_BOLT;
    }

    if (skillId == SKILL_NONE)
    {
        SendBattleMessage(player, "Invalid skill.");
        return false;
    }

    const int skillLevel = player->GetSkillLevel(skillId);

    if (skillLevel <= 0)
    {
        SendBattleMessage(player, "You have not learned " + skillName + ".");
        return false;
    }

    bool hasTarget = false;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        std::unordered_set<int> checkedMonsterIds;
        int foundCount = 0;

        for (const std::pair<int, int>& targetPosition : targetPositions)
        {
            if (foundCount >= maxHitCount)
            {
                break;
            }

            const int targetX = targetPosition.first;
            const int targetY = targetPosition.second;

            std::shared_ptr<Monster> monsterPtr =
                FindAliveMonsterAt(targetX, targetY);

            if (monsterPtr == nullptr)
            {
                continue;
            }

            const int monsterId = monsterPtr->monsterId;

            if (checkedMonsterIds.find(monsterId) != checkedMonsterIds.end())
            {
                continue;
            }

            checkedMonsterIds.insert(monsterId);
            ++foundCount;
            hasTarget = true;
        }
    }

    if (!hasTarget)
    {
        SendBattleMessage(player, "There is no monster in skill range.");
        return false;
    }

    if (player->GetMp() < mpCost)
    {
        SendBattleMessage(player, "Not enough MP.");
        SendPlayerStatus(player, 0, 0);
        return false;
    }

    const long long nowMs = GetNowMs();

    if (!player->TryUseAttack(nowMs, PLAYER_ATTACK_INTERVAL_MS))
    {
        SendBattleMessage(player, "Skill cooldown.");
        return false;
    }

    int mp = 0;
    int maxMp = 0;

    if (!player->ConsumeMp(mpCost, mp, maxMp))
    {
        SendBattleMessage(player, "Not enough MP.");
        SendPlayerStatus(player, 0, 0);
        return false;
    }

    SendPlayerStatus(player, 0, 0);

    struct SkillHitResult
    {
        int monsterId = -1;
        int newHp = 0;
        int maxHp = 0;
        int damage = 0;
        bool monsterDead = false;
        bool critical = false;

        std::string monsterName;
        Monster monsterSnapshotForDrop{};

        std::vector<std::shared_ptr<ClientSession>> hpReceivers;
        std::vector<std::shared_ptr<ClientSession>> removeReceivers;
        std::vector<std::shared_ptr<ClientSession>> expReceivers;
    };

    std::vector<SkillHitResult> hitResults;

    const int playerId = player->GetPlayerId();

    const int attackPower =
        useIntStat ?
        player->GetMagicAttack() :
        player->GetPhysicalAttack();

    const int finalSkillPowerPercent =
        skillPowerPercent + skillLevel * skillPowerPercentPerLevel;

    {
        std::lock_guard<std::mutex> lock(worldLock_);

        std::unordered_set<int> alreadyHitMonsterIds;

        for (const std::pair<int, int>& targetPosition : targetPositions)
        {
            if (static_cast<int>(hitResults.size()) >= maxHitCount)
            {
                break;
            }

            const int targetX = targetPosition.first;
            const int targetY = targetPosition.second;

            std::shared_ptr<Monster> monsterPtr =
                FindAliveMonsterAt(targetX, targetY);

            if (monsterPtr == nullptr)
            {
                continue;
            }

            Monster& monster = *monsterPtr;

            if (alreadyHitMonsterIds.find(monster.monsterId) !=
                alreadyHitMonsterIds.end())
            {
                continue;
            }

            alreadyHitMonsterIds.insert(monster.monsterId);

            const int monsterDefense =
                useIntStat ?
                monster.level :
                monster.level * 2;

            const bool critical =
                RollPercent(player->GetCriticalRate());

            const int rawDamage =
                flatBonus +
                attackPower * finalSkillPowerPercent / 100;

            int damage = 0;

            if (critical)
            {
                damage = rawDamage * 120 / 100;
            }
            else
            {
                damage = rawDamage - monsterDefense;
            }

            if (damage < 1)
            {
                damage = 1;
            }

            monster.hp -= damage;

            if (monster.hp < 0)
            {
                monster.hp = 0;
            }

            monster.damageByPlayer[playerId] += damage;

            SkillHitResult result{};
            result.monsterId = monster.monsterId;
            result.newHp = monster.hp;
            result.maxHp = monster.maxHp;
            result.damage = damage;
            result.critical = critical;
            result.monsterName = monster.name;

            for (auto& [receiverPlayerId, receiver] : playersById_)
            {
                if (receiver == nullptr)
                {
                    continue;
                }

                if (!receiver->IsConnected() || !receiver->IsLoggedIn())
                {
                    continue;
                }

                if (IsInViewRange(receiver.get(), monster.x, monster.y))
                {
                    result.hpReceivers.push_back(receiver);
                }
            }

            if (monster.hp <= 0)
            {
                result.monsterDead = true;
                result.monsterSnapshotForDrop = monster;

                if (monster.damageByPlayer.empty())
                {
                    monster.damageByPlayer[playerId] = damage;
                }

                for (const auto& [contributorId, dealtDamage] :
                    monster.damageByPlayer)
                {
                    if (dealtDamage <= 0)
                    {
                        continue;
                    }

                    auto playerIter = playersById_.find(contributorId);

                    if (playerIter == playersById_.end() ||
                        playerIter->second == nullptr)
                    {
                        continue;
                    }

                    std::shared_ptr<ClientSession> contributor =
                        playerIter->second;

                    if (contributor->IsConnected() &&
                        contributor->IsLoggedIn())
                    {
                        result.expReceivers.push_back(contributor);
                    }
                }

                monster.alive = false;
                monster.targetPlayerId = -1;
                monster.respawnTimeMs = nowMs + MONSTER_RESPAWN_DELAY_MS;
                deadMonsterIds_.insert(monster.monsterId);
                monster.damageByPlayer.clear();

                RemoveMonsterFromSector(
                    monster.monsterId,
                    monster.x,
                    monster.y
                );

                for (auto& [visiblePlayerId, visibleState] : visibleStates_)
                {
                    if (visibleState == nullptr)
                    {
                        continue;
                    }

                    bool shouldRemove = false;

                    {
                        std::lock_guard<std::mutex> visibleLock(
                            visibleState->lock
                        );

                        const std::size_t erasedCount =
                            visibleState->visibleMonsterIds.erase(
                                monster.monsterId
                            );

                        shouldRemove = erasedCount > 0;
                    }

                    if (!shouldRemove)
                    {
                        continue;
                    }

                    auto receiverIter =
                        playersById_.find(visiblePlayerId);

                    if (receiverIter == playersById_.end() ||
                        receiverIter->second == nullptr)
                    {
                        continue;
                    }

                    std::shared_ptr<ClientSession> receiver =
                        receiverIter->second;

                    if (receiver->IsConnected() &&
                        receiver->IsLoggedIn())
                    {
                        result.removeReceivers.push_back(receiver);
                    }
                }
            }

            hitResults.push_back(result);
        }
    }

    if (hitResults.empty())
    {
        SendBattleMessage(player, skillName + " missed.");
        return false;
    }

    for (const SkillHitResult& result : hitResults)
    {
        for (const std::shared_ptr<ClientSession>& receiver :
            result.hpReceivers)
        {
            if (receiver != nullptr)
            {
                SendMonsterHp(
                    receiver.get(),
                    result.monsterId,
                    result.newHp,
                    result.maxHp
                );
            }
        }

        std::string message;

        if (result.critical)
        {
            message = "Critical! ";
        }

        message +=
            skillName +
            " hit " +
            result.monsterName +
            " for " +
            std::to_string(result.damage) +
            " damage.";

        SendBattleMessage(player, message);

        if (!result.monsterDead)
        {
            continue;
        }

        for (const std::shared_ptr<ClientSession>& receiver :
            result.removeReceivers)
        {
            if (receiver != nullptr)
            {
                SendRemoveMonster(receiver.get(), result.monsterId);
            }
        }

        const int expGain =
            CalculateMonsterExperience(result.monsterSnapshotForDrop);

        for (const std::shared_ptr<ClientSession>& receiver :
            result.expReceivers)
        {
            if (receiver == nullptr)
            {
                continue;
            }

            const int levelGained = receiver->AddExperience(expGain);

            SendPlayerStatus(receiver.get(), expGain, levelGained);

            SendBattleMessage(
                receiver.get(),
                result.monsterName +
                " died. EXP +" +
                std::to_string(expGain) +
                "."
            );

            if (levelGained > 0)
            {
                SendBattleMessage(
                    receiver.get(),
                    "Level up! Current level: " +
                    std::to_string(receiver->GetLevel()) +
                    "."
                );
            }

            MarkPlayerDirty(receiver.get());
        }

        CreatePersonalDropsForMonster(
            result.monsterSnapshotForDrop,
            result.expReceivers
        );
    }

    return true;
}