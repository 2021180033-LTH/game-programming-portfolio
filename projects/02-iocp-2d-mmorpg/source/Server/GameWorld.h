#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <concurrent_unordered_map.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <random>

#include "ClientSession.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include "ServerConfig.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <limits>
#include <queue>

#include "GameProtocol.h"

class ClientSession;

class GameWorld
{
public:
    using SendFunction = std::function<void(ClientSession*, const char*, int)>;

    explicit GameWorld(SendFunction sendFunction);
    ~GameWorld();

    bool BeginLoginPlayer(ClientSession* session, const std::string& username, std::string& failMessage);
    bool LoginPlayer(ClientSession* session, const std::string& username, std::string& failMessage);
    void LogoutPlayer(ClientSession* session);

    bool MovePlayer(ClientSession* session, DIRECTION direction);
    void HandlePlayerAttack(ClientSession* attacker);

    void SendExistingPlayersTo(ClientSession* newPlayer);
    void NotifyPlayerEnter(ClientSession* newPlayer);
    void BroadcastMove(ClientSession* movedPlayer, int moveTime);

    void SendVisibleMonstersTo(ClientSession* receiver);
    void SendVisibleNpcsTo(ClientSession* receiver);
    void UpdateMonsterVisibilityFor(ClientSession* receiver);
    void UpdatePlayerVisibilityFor(ClientSession* movedPlayer);

    bool UseItem(ClientSession* player, int itemId);
    bool UnequipEquipment(ClientSession* player, int equipmentSlot);
    bool SellItem(ClientSession* player, int itemId, int count);
    bool SetQuickSlot(
        ClientSession* player,
        int slotIndex,
        int slotType,
        int targetId,
        const std::string& name
    );
    bool ClearQuickSlot(ClientSession* player, int slotIndex);
    bool UseSkill(ClientSession* player, int skillId);
    bool UseHealerNpc(ClientSession* player, int npcId);

    bool AddPlayerStat(ClientSession* player, int statType);
    bool AddPlayerSkillPoint(ClientSession* player, int skillId);

    void SendPlayerStatus(
        ClientSession* receiver,
        int expGained,
        int levelGained
    );

    void SendSkillStatus(ClientSession* receiver);
    void SendQuickSlotStatus(ClientSession* receiver);
    void SendInventorySnapshot(ClientSession* receiver);
    void GiveDefaultTestItems(ClientSession* receiver);

    bool Chat(ClientSession* sender, const std::string& message);

    bool UseRage(ClientSession* player, int skillLevel);
    bool UsePowerStrike(ClientSession* player, int skillLevel);
    bool UseHeavySlash(ClientSession* player, int skillLevel);
    bool UseWhirlwind(ClientSession* player, int skillLevel);
    bool UseFireball(ClientSession* player, int skillLevel);
    bool UseManaBurst(ClientSession* player, int skillLevel);
    bool UseIceBolt(ClientSession* player, int skillLevel);
    bool ApplySkillDamageToTargets(
        ClientSession* player,
        const std::string& skillName,
        const std::vector<std::pair<int, int>>& targetPositions,
        int maxHitCount,
        int mpCost,
        int skillPowerPercent,
        int skillPowerPercentPerLevel,
        int flatBonus,
        bool useIntStat
    );

    void SendEquipmentStatus(ClientSession* receiver);

private:
    enum class MonsterMoveType
    {
        Fixed = 0,
        Roaming = 1
    };

    enum class MonsterAiType
    {
        Peace = 0,
        Agro = 1
    };

    struct Monster
    {
        int monsterId = -1;
        int monsterType = 0;

        std::string name;

        int level = 1;
        int hp = 1;
        int maxHp = 1;

        int x = 0;
        int y = 0;

        int spawnX = 0;
        int spawnY = 0;

        bool alive = true;

        MonsterMoveType moveType = MonsterMoveType::Fixed;
        MonsterAiType aiType = MonsterAiType::Peace;

        int targetPlayerId = -1;

        long long nextMoveTimeMs = 0;
        long long respawnTimeMs = 0;

        int attackPower = 1;
        long long nextAttackTimeMs = 0;

        std::unordered_map<int, int> damageByPlayer;
    };

    struct Npc
    {
        int npcId = -1;
        int npcType = 0;

        std::string name;

        int x = 0;
        int y = 0;
    };

    struct DropItem
    {
        int dropId = -1;
        int ownerPlayerId = -1;

        int itemId = 0;
        int itemCategory = 0;
        int count = 1;

        std::string itemName;

        int x = 0;
        int y = 0;

        long long expireTimeMs = 0;
    };

    struct InventoryItem
    {
        int itemId = 0;
        int itemCategory = 0;
        int count = 0;

        std::string itemName;
    };

    struct VisibleState
    {
        std::mutex lock;

        std::unordered_set<int> visibleMonsterIds;
        std::unordered_set<int> visiblePlayerIds;
    };

    struct Sector
    {
        std::mutex lock;
        std::unordered_set<int> playerIds;
        std::unordered_set<int> monsterIds;
    };

    struct MovedMonsterInfo
    {
        Monster monster;
        int oldX = 0;
        int oldY = 0;
    };

    struct PlayerSaveSnapshot
    {
        DbPlayerData player;
        DbEquipmentData equipment;
        std::vector<DbSkillData> skills;
        std::vector<DbInventoryItemData> inventoryItems;
        std::vector<DbQuickSlotData> quickSlots;
    };

    struct PlayerLoadSnapshot
    {
        DbPlayerData player;
        DbEquipmentData equipment;
        std::vector<DbSkillData> skills;
        std::vector<DbInventoryItemData> inventoryItems;
        std::vector<DbQuickSlotData> quickSlots;
        bool created = false;
    };

    enum class DbJobType
    {
        Login,
        Save
    };

    enum class DbSaveMode
    {
        Full,
        PlayerOnly
    };

    struct DbJob
    {
        DbJobType type = DbJobType::Save;
        std::uint64_t sequence = 0;
        std::string username;
        std::weak_ptr<ClientSession> session;
        int sessionId = -1;
        DbSaveMode saveMode = DbSaveMode::Full;
        PlayerSaveSnapshot saveSnapshot;
    };

    struct DbLoginCompletion
    {
        std::uint64_t sequence = 0;
        std::weak_ptr<ClientSession> session;
        int sessionId = -1;
        std::string username;
        bool success = false;
        bool loadedFromDatabase = false;
        std::string failMessage;
        PlayerLoadSnapshot loadSnapshot;
    };


private:
    std::vector<std::shared_ptr<ClientSession>> GetLoggedInPlayersSnapshot();

    void SendAddPlayer(ClientSession* receiver, ClientSession* target);
    void SendRemovePlayer(ClientSession* receiver, int removedPlayerId);
    void SendMovePlayer(ClientSession* receiver, ClientSession* movedPlayer, int moveTime);

private:
    void InitializeMap();
    void LoadItemSellPrices();
    int GetSellPriceByItemId(int itemId) const;

    bool IsInsideMap(int x, int y) const;
    bool IsBlocked(int x, int y) const;
    bool CanMoveTo(int x, int y) const;

    void SetBlocked(int x, int y, bool blocked);
    void FillRect(int left, int top, int right, int bottom, bool blocked);

    int ToMapIndex(int x, int y) const;

private:
    void InitializeMonsters();
    void InitializeNpcs();
    bool LoadMonstersFromLua(long long nowMs);
    bool LoadNpcsFromLua();

    bool IsInViewRange(ClientSession* viewer, int targetX, int targetY) const;

    std::shared_ptr<VisibleState> GetOrCreateVisibleState(int playerId);
    void RemoveVisibleState(int playerId);

    void BuildVisibleMonsterSnapshot(
        ClientSession* receiver,
        std::vector<Monster>& outMonsters,
        std::unordered_set<int>& outMonsterIds
    );
    void BuildVisiblePlayerSnapshot(
        ClientSession* receiver,
        std::vector<std::shared_ptr<ClientSession>>& outPlayers,
        std::unordered_set<int>& outPlayerIds
    );


    void SendAddMonster(ClientSession* receiver, const Monster& monster);
    void SendRemoveMonster(ClientSession* receiver, int monsterId);
    void SendMoveMonster(ClientSession* receiver, const Monster& monster);
    void SendAddNpc(ClientSession* receiver, const Npc& npc);

    int GetSectorCountX() const;
    int GetSectorCountY() const;

    int GetSectorXFromPosition(int x) const;
    int GetSectorYFromPosition(int y) const;
    int GetSectorIndexFromPosition(int x, int y) const;
    int GetSectorIndexFromCoord(int sectorX, int sectorY) const;

    std::shared_ptr<Sector> GetOrCreateSector(int sectorIndex);
    std::shared_ptr<Sector> FindSector(int sectorIndex);

    void AddPlayerToSector(ClientSession* session);
    void RemovePlayerFromSector(int playerId, int x, int y);
    void MovePlayerSectorIfNeeded(ClientSession* session, int oldX, int oldY, int newX, int newY);

    void AddMonsterToWorld(const Monster& monster);
    void AddMonsterToSector(int monsterId, int x, int y);

    void CollectCandidateMonsterIdsInViewRange(
        int centerX,
        int centerY,
        std::unordered_set<int>& outMonsterIds
    );

    void CollectCandidatePlayerIdsInViewRange(
        int centerX,
        int centerY,
        std::unordered_set<int>& outPlayerIds
    );

    void StartWorldLoop();
    void StopWorldLoop();
    void WorldUpdateLoop();

    void UpdateRoamingMonsters(long long nowMs);
    bool TryMoveMonster(Monster& monster, int dx, int dy, MovedMonsterInfo& outMovedInfo);

    bool FindPathAStar(
        int startX,
        int startY,
        int goalX,
        int goalY,
        int ignoreMonsterId,
        int ignorePlayerId,
        std::vector<std::pair<int, int>>& outPath
    );

    bool IsActorOccupied(
        int x,
        int y,
        int ignoreMonsterId,
        int ignorePlayerId
    );

    bool CanActorMoveTo(
        int x,
        int y,
        int ignoreMonsterId,
        int ignorePlayerId
    );

    bool FindSpawnPositionNear(
        int centerX,
        int centerY,
        int& outX,
        int& outY
    );

    void UpdateMonsterAi(long long nowMs);
    void CollectActiveMonsterIdsNearPlayers(std::unordered_set<int>& outMonsterIds);

    bool UpdateAgroTarget(Monster& monster);
    std::shared_ptr<ClientSession> FindNearestPlayerInAggroRange(const Monster& monster);
    std::shared_ptr<ClientSession> FindPlayerById(int playerId);

    bool IsPlayerInAggroRange(const Monster& monster, ClientSession* player) const;
    bool TryMoveMonsterTowardPlayer(Monster& monster, ClientSession* targetPlayer, MovedMonsterInfo& outMovedInfo);

    void RemoveMonsterFromSector(int monsterId, int x, int y);
    void MoveMonsterSectorIfNeeded(int monsterId, int oldX, int oldY, int newX, int newY);

    void UpdateMonsterVisibilityAfterMove(const Monster& monster, int oldX, int oldY);

    std::shared_ptr<Monster> FindAliveMonsterAt(int x, int y);
    std::shared_ptr<Monster> FindNearestAliveMonsterInRange(int centerX, int centerY, int range);

    int CalculatePlayerAttackDamage(ClientSession* attacker, const Monster& monster) const;

    void SendMonsterHp(
        ClientSession* receiver,
        int monsterId,
        int hp,
        int maxHp
    );

    void SendBattleMessage(
        ClientSession* receiver,
        const std::string& message
    );

    void UpdateMonsterRespawns(long long nowMs);
    void NotifyMonsterSpawnToVisiblePlayers(const Monster& monster);

    int CalculateMonsterExperience(const Monster& monster) const;

    int CalculateMonsterAttackDamage(
        const Monster& monster,
        ClientSession* targetPlayer
    ) const;

    void UpdatePlayerHpRegeneration(long long nowMs);
    void UpdatePlayerBuffs(long long nowMs);
    void HandlePlayerDeathAndRespawn(ClientSession* player);

    void UpdatePlayerRespawns(long long nowMs);

    bool RollMonsterDrop(
        ClientSession* player,
        const Monster& monster,
        int& outItemId,
        int& outItemCategory,
        int& outCount,
        std::string& outItemName
    );

    void CreatePersonalDropsForMonster(
        const Monster& monster,
        const std::vector<std::shared_ptr<ClientSession>>& contributors
    );

    void TryPickupDropsAt(ClientSession* player);
    void UpdateDropExpirations(long long nowMs);

    void SendAddDrop(ClientSession* receiver, const DropItem& drop);
    void SendRemoveDrop(ClientSession* receiver, int dropId);
    void SendInventoryItemUpdate(ClientSession* receiver, const InventoryItem& item);

    void SavePlayerDataToDatabase(ClientSession* player);
    void SavePlayerCoreToDatabase(ClientSession* player);
    void SaveQuickSlotsToDatabase(ClientSession* player);
    void MarkPlayerDirty(ClientSession* player);
    void FlushDirtySaves(long long nowMs);

    bool BuildPlayerSaveSnapshot(ClientSession* player, PlayerSaveSnapshot& outSnapshot);
    bool EnqueueDbJob(DbJob&& job);
    bool EnqueueSaveSnapshot(PlayerSaveSnapshot&& snapshot, DbSaveMode saveMode);
    void StartDbWorker();
    void StopDbWorker();
    void DbWorkerLoop();
    void ExecuteLoginDbJob(const DbJob& job);
    void ExecuteSaveDbJob(const DbJob& job);
    void PushLoginCompletion(DbLoginCompletion&& completion);
    void ProcessDbCompletions();
    void CompleteLoginFromDatabase(DbLoginCompletion&& completion);
    void ApplyLoadedSnapshotToSession(
        ClientSession* session,
        const PlayerLoadSnapshot& snapshot,
        int finalX,
        int finalY
    );
    void SendLoginResultPacket(ClientSession* session, bool success, const std::string& message);
    void SendInitialLoginPackets(ClientSession* session);
    void AdjustStressLoginPosition(const std::string& username, int& x, int& y) const;
    void RemovePendingLoginName(const std::string& username, ClientSession* session);

    bool ConsumeInventoryItem(
        int playerId,
        int itemId,
        InventoryItem& outUpdatedItem
    );

    void AddInventoryItem(
        int playerId,
        int itemId,
        int itemCategory,
        int count,
        const std::string& itemName,
        InventoryItem& outUpdatedItem
    );

    void SendChatMessage(
        ClientSession* receiver,
        int senderPlayerId,
        const std::string& senderName,
        const std::string& message
    );

    bool FindInventoryItem(
        int playerId,
        int itemId,
        InventoryItem& outItem
    );

private:
    SendFunction sendFunction_;

    DatabaseManager databaseManager_;
    bool databaseReady_ = false;

    std::atomic_bool dbWorkerRunning_ = false;
    std::atomic_bool dbWorkerStopRequested_ = false;
    std::thread dbWorkerThread_;

    std::mutex dbJobLock_;
    std::condition_variable dbJobCv_;
    std::queue<DbJob> dbJobs_;
    std::uint64_t nextDbJobSequence_ = 1;

    std::mutex dbCompletionLock_;
    std::queue<DbLoginCompletion> dbLoginCompletions_;

    std::unordered_map<std::string, std::weak_ptr<ClientSession>> pendingLoginsByName_;

    std::mutex worldLock_;

    std::unordered_map<int, std::shared_ptr<ClientSession>> playersById_;
    std::unordered_map<std::string, std::weak_ptr<ClientSession>> playersByName_;

    int mapWidth_ = 0;
    int mapHeight_ = 0;

    std::vector<std::uint8_t> blockedTiles_;

    concurrency::concurrent_unordered_map<int, std::shared_ptr<Monster>> monstersById_;
    std::unordered_set<int> deadMonsterIds_;
    std::unordered_map<int, Npc> npcsById_;
    std::unordered_map<int, int> itemSellPrices_;

    concurrency::concurrent_unordered_map<int, std::shared_ptr<Sector>> sectors_;

    concurrency::concurrent_unordered_map<int, std::shared_ptr<VisibleState>> visibleStates_;

    std::atomic_bool worldLoopRunning_ = false;
    std::thread worldLoopThread_;

    int nextDropId_ = 1;

    std::unordered_map<int, DropItem> dropItems_;
    std::unordered_map<int, std::unordered_map<int, InventoryItem>> inventoriesByPlayerId_;

    std::mutex dirtySaveLock_;
    std::unordered_map<int, long long> dirtySaveDueMsByPlayerId_;
};