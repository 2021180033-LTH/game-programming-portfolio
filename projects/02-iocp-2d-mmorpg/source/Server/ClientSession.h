#pragma once

#include <WinSock2.h>

#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>
#include <string>
#include <chrono>
#include <memory>

#include "GameProtocol.h"
#include "ServerConfig.h"
#include "IocpEvent.h"

class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
    ClientSession();
    ~ClientSession();

    ClientSession(const ClientSession&) = delete;
    ClientSession& operator=(const ClientSession&) = delete;

    bool Initialize(int sessionId, SOCKET socket);
    void Disconnect();
    bool TryMarkDisconnecting();
    bool IsDisconnecting() const;

    int GetSessionId() const;
    SOCKET GetSocket() const;

    bool IsConnected() const;

    bool TryBeginSend(int maxPendingSendCount);
    void EndSend();
    int GetPendingSendCount() const;

    IocpEvent& GetRecvEvent();

    void AppendReceivedData(const char* data, int byteCount);
    std::vector<char>& GetPacketBuffer();

    void ClearPacketBuffer();

    void SetLoggedIn(bool loggedIn);
    bool IsLoggedIn() const;

    bool TryBeginLogin();
    void EndLogin();
    bool IsLoginPending() const;

    void SetUsername(const std::string& username);
    const std::string& GetUsername() const;

    void SetPlayerId(int playerId);
    int GetPlayerId() const;

    void SetPosition(int x, int y);
    int GetX() const;
    int GetY() const;

    void LoadPersistentState(
        int level,
        int exp,
        int hp,
        int mp,
        int x,
        int y,
        int gold,
        int statPoint,
        int skillPoint,
        int str,
        int intStat,
        int dex,
        int luk
    );

    void SetSkillLevelForLoad(int skillId, int skillLevel);
    void ClampVitalsToCurrentMax();

    bool TryConsumeMoveCooldown(int cooldownMs);

    void SetFacingDirection(DIRECTION direction);
    DIRECTION GetFacingDirection() const;

    bool TryUseAttack(long long nowMs, long long cooldownMs);

    int GetLevel() const;
    int GetExp() const;
    int GetRequiredExp() const;
    int GetStatPoint() const;
    int GetSkillPoint() const;
    int GetGold() const;
    void AddGold(int amount);

    int GetStr() const;
    int GetIntStat() const;
    int GetDex() const;
    int GetLuk() const;

    bool TryAddStat(
        int statType,
        int& outRemainingStatPoint,
        int& outNewStatValue,
        std::string& outStatName
    );

    bool TryAddSkillPoint(
        int skillId,
        int& outRemainingSkillPoint,
        int& outNewSkillLevel,
        std::string& outSkillName,
        std::string& outFailReason
    );

    int GetSkillLevel(int skillId) const;

    bool ActivateRage(
        long long nowMs,
        int skillLevel,
        int& outMp,
        int& outMaxMp,
        int& outStrBonus,
        int& outPhysicalAttackBonus,
        int& outDurationSeconds,
        std::string& outFailReason
    );

    bool UpdateTimedBuffs(long long nowMs, bool& outRageExpired);
    bool IsRageActive(long long nowMs) const;

    void GetSkillStatus(
        SkillStatusEntry* outSkills,
        int maxSkillCount,
        int& outSkillCount
    ) const;

    int AddExperience(int expGained);

    int GetHp() const;
    int GetMaxHp() const;

    int GetMp() const;
    int GetMaxMp() const;

    int GetPhysicalAttack() const;
    int GetPhysicalDefense() const;

    int GetMagicAttack() const;
    int GetMagicDefense() const;

    int GetAccuracyRate() const;
    int GetEvasionRate() const;

    int GetCriticalRate() const;
    int GetDropBonusBasisPoint() const;

    bool ApplyDamage(int damage, int& outHp, int& outMaxHp);
    void FullHeal();

    bool ConsumeMp(int amount, int& outMp, int& outMaxMp);
    bool RecoverMp(int amount, int& outMp, int& outMaxMp);
    void FullRecoverMp();

    bool TryRegenerateHp(
        long long nowMs,
        long long intervalMs,
        int percent,
        int& outHealed,
        int& outHp,
        int& outMaxHp
    );

    void ReduceExpByHalf();
    void ResetHpRegenTimer(long long nextTimeMs);

    bool IsAlive() const;
    bool IsWaitingRespawn() const;

    void MarkDead(long long respawnTimeMs);
    bool IsRespawnReady(long long nowMs) const;
    int GetRespawnRemainingSeconds(long long nowMs) const;
    bool TryConsumeRespawnCountdownNotice(long long nowMs, int& outRemainingSeconds);
    void ReviveFull();

    bool HealHp(
        int healAmount,
        int& outHealed,
        int& outHp,
        int& outMaxHp
    );

    bool EquipWeapon(int itemId, const std::string& itemName);
    bool EquipArmor(int itemId, const std::string& itemName);
    bool EquipAccessory(int itemId, const std::string& itemName);

    bool UnequipWeapon(int& outItemId, std::string& outItemName);
    bool UnequipArmor(int& outItemId, std::string& outItemName);
    bool UnequipAccessory(int& outItemId, std::string& outItemName);

    int GetEquippedWeaponItemId() const;
    std::string GetEquippedWeaponName() const;

    int GetEquippedArmorItemId() const;
    std::string GetEquippedArmorName() const;

    int GetEquippedAccessoryItemId() const;
    std::string GetEquippedAccessoryName() const;

    int GetEquipmentStrBonus() const;
    int GetEquipmentIntBonus() const;
    int GetEquipmentDexBonus() const;
    int GetEquipmentLukBonus() const;

    int GetEquipmentPhysicalAttackBonus() const;
    int GetEquipmentPhysicalDefenseBonus() const;
    int GetEquipmentMagicAttackBonus() const;
    int GetEquipmentMagicDefenseBonus() const;

    bool SetQuickSlot(
        int slotIndex,
        int slotType,
        int targetId,
        const std::string& name
    );
    bool ClearQuickSlot(int slotIndex);
    void GetQuickSlotStatus(
        QuickSlotStatusEntry* outSlots,
        int maxSlotCount,
        int& outSlotCount
    ) const;

private:
    int sessionId_ = -1;
    SOCKET socket_ = INVALID_SOCKET;

    std::atomic_bool connected_ = false;
    std::atomic_bool disconnecting_ = false;
    std::atomic_bool loggedIn_ = false;
    std::atomic_bool loginPending_ = false;
    std::atomic_int pendingSendCount_ = 0;

    std::string username_;

    IocpEvent recvEvent_;

    std::mutex packetBufferLock_;
    std::vector<char> packetBuffer_;

    int playerId_ = -1;
    int x_ = 0;
    int y_ = 0;

    std::chrono::steady_clock::time_point lastMoveTime_{};

    std::atomic_int facingDirection_ = static_cast<int>(DOWN);
    std::atomic_llong lastAttackTimeMs_ = 0;

    mutable std::mutex progressLock_;

    int level_ = 1;
    int exp_ = 0;
    int statPoint_ = 0;
    int skillPoint_ = 0;
    int gold_ = 0;

    int str_ = 5;
    int intStat_ = 5;
    int dex_ = 5;
    int luk_ = 5;

    int skillLevels_[MAX_SKILL_STATUS_COUNT]{};

    int hp_ = 100;
    int maxHp_ = 100; 

    int mp_ = 50;
    int maxMp_ = 50; 

    long long nextHpRegenTimeMs_ = 0;

    std::atomic_bool alive_ = true;
    std::atomic_llong respawnTimeMs_ = 0;
    std::atomic_int lastRespawnNoticeSecond_ = -1;

    int equippedWeaponItemId_ = 0;
    std::string equippedWeaponName_;

    int equippedArmorItemId_ = 0;
    std::string equippedArmorName_;

    int equippedAccessoryItemId_ = 0;
    std::string equippedAccessoryName_;

    int equipmentStr_ = 0;
    int equipmentInt_ = 0;
    int equipmentDex_ = 0;
    int equipmentLuk_ = 0;

    int equipmentPhysicalAttack_ = 0;
    int equipmentPhysicalDefense_ = 0;
    int equipmentMagicAttack_ = 0;
    int equipmentMagicDefense_ = 0;

    int rageStrBonus_ = 0;
    int ragePhysicalAttackBonus_ = 0;
    long long rageEndTimeMs_ = 0;

    QuickSlotStatusEntry quickSlots_[MAX_QUICK_SLOT_COUNT]{};

    int GetSkillLevelNoLock(int skillId) const;

    int CalculateMaxHpNoLock() const;
    int CalculateMaxMpNoLock() const;

    int CalculatePhysicalAttackNoLock() const;
    int CalculatePhysicalDefenseNoLock() const;

    int CalculateMagicAttackNoLock() const;
    int CalculateMagicDefenseNoLock() const;

    int CalculateAccuracyRateNoLock() const;
    int CalculateEvasionRateNoLock() const;

    int CalculateCriticalRateNoLock() const;
    int CalculateDropBonusBasisPointNoLock() const;

    int GetTotalStrNoLock() const;
    int GetTotalIntNoLock() const;
    int GetTotalDexNoLock() const;
    int GetTotalLukNoLock() const;

    void ResetEquipmentBonusNoLock();
    void AddEquipmentBonusNoLock(int itemId);
    void RecalculateEquipmentBonusNoLock();
};