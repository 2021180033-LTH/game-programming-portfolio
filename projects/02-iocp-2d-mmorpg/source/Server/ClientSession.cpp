#include "ClientSession.h"

#include "NetUtil.h"

#include <algorithm>
#include <cstring>

namespace
{
    void CopyStringToFixedBuffer(char* dest, std::size_t destSize, const std::string& src)
    {
        if (dest == nullptr || destSize == 0)
        {
            return;
        }

        std::memset(dest, 0, destSize);

        const std::size_t copySize =
            src.size() < destSize - 1
            ? src.size()
            : destSize - 1;

        if (copySize > 0)
        {
            std::memcpy(dest, src.data(), copySize);
        }
    }

    int CalculateRequiredExpByLevel(int level)
    {
        if (level < 1)
        {
            level = 1;
        }

        int requiredExp = 100;

        for (int i = 1; i < level; ++i)
        {
            if (requiredExp > 500000000)
            {
                return 1000000000;
            }

            requiredExp *= 2;
        }

        return requiredExp;
    }

    std::string GetStatNameByType(int statType)
    {
        switch (statType)
        {
        case STAT_STR:
            return "STR";

        case STAT_INT:
            return "INT";

        case STAT_DEX:
            return "DEX";

        case STAT_LUK:
            return "LUK";

        default:
            return "UNKNOWN";
        }
    }

    const int* GetAllSkillIds()
    {
        static const int skillIds[MAX_SKILL_STATUS_COUNT] =
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

        return skillIds;
    }

    int GetSkillIndex(int skillId)
    {
        const int* skillIds = GetAllSkillIds();

        for (int i = 0; i < MAX_SKILL_STATUS_COUNT; ++i)
        {
            if (skillIds[i] == skillId)
            {
                return i;
            }
        }

        return -1;
    }

    std::string GetSkillNameById(int skillId)
    {
        switch (skillId)
        {
        case SKILL_POWER_STRIKE:
            return "Power Strike";

        case SKILL_HEAVY_SLASH:
            return "Heavy Slash";

        case SKILL_WHIRLWIND:
            return "Whirlwind";

        case SKILL_FIREBALL:
            return "Fireball";

        case SKILL_ICE_BOLT:
            return "Ice Bolt";

        case SKILL_MANA_BURST:
            return "Mana Burst";

        case SKILL_VITALITY_TRAINING:
            return "Rage";

        case SKILL_MANA_TRAINING:
            return "Mana Training";

        case SKILL_LUCKY_HUNTER:
            return "Lucky Hunter";

        default:
            return "Unknown Skill";
        }
    }

    int GetRequiredPreviousSkillId(int skillId)
    {
        switch (skillId)
        {
        case SKILL_HEAVY_SLASH:
            return SKILL_POWER_STRIKE;

        case SKILL_WHIRLWIND:
            return SKILL_HEAVY_SLASH;

        case SKILL_ICE_BOLT:
            return SKILL_FIREBALL;

        case SKILL_MANA_BURST:
            return SKILL_ICE_BOLT;

        case SKILL_MANA_TRAINING:
            return SKILL_VITALITY_TRAINING;

        case SKILL_LUCKY_HUNTER:
            return SKILL_MANA_TRAINING;

        default:
            return SKILL_NONE;
        }
    }
}

ClientSession::ClientSession()
    : recvEvent_(IoOperation::Recv, ServerConfig::RECV_BUFFER_SIZE)
{
    recvEvent_.SetOwner(this);
}

ClientSession::~ClientSession()
{
    Disconnect();
}

bool ClientSession::Initialize(int sessionId, SOCKET socket)
{
    if (socket == INVALID_SOCKET)
    {
        return false;
    }

    sessionId_ = sessionId;
    socket_ = socket;

    connected_.store(true);
    disconnecting_.store(false);
    loggedIn_.store(false);
    loginPending_.store(false);
    pendingSendCount_.store(0);

    username_.clear();

    {
        std::lock_guard<std::mutex> lock(packetBufferLock_);
        packetBuffer_.clear();
    }

    playerId_ = -1;
    x_ = 0;
    y_ = 0;

    facingDirection_.store(static_cast<int>(DOWN));
    lastAttackTimeMs_.store(0);

    {
        std::lock_guard<std::mutex> lock(progressLock_);

        level_ = 1;
        exp_ = 0;
        statPoint_ = 0;
        skillPoint_ = 0;

        str_ = 5;
        intStat_ = 5;
        dex_ = 5;
        luk_ = 5;

        for (int i = 0; i < MAX_SKILL_STATUS_COUNT; ++i)
        {
            skillLevels_[i] = 0;
        }

        maxHp_ = 100;
        hp_ = maxHp_;
        maxMp_ = 50;
        mp_ = maxMp_;
        nextHpRegenTimeMs_ = 0;
    }

    lastMoveTime_ =
        std::chrono::steady_clock::now() -
        std::chrono::milliseconds(ServerConfig::MOVE_INTERVAL_MS);

    recvEvent_.SetOwner(this);

    alive_.store(true);
    respawnTimeMs_.store(0);
    lastRespawnNoticeSecond_.store(-1);

    for (int i = 0; i < MAX_QUICK_SLOT_COUNT; ++i)
    {
        quickSlots_[i] = QuickSlotStatusEntry{};
        quickSlots_[i].slotIndex = i;
        quickSlots_[i].slotType = QUICK_SLOT_TYPE_EMPTY;
        quickSlots_[i].targetId = 0;
    }

    return true;
}

void ClientSession::Disconnect()
{
    bool expected = true;

    if (connected_.compare_exchange_strong(expected, false))
    {
        NetUtil::CloseSocket(socket_);
    }

    loggedIn_.store(false);
    loginPending_.store(false);
}

bool ClientSession::TryMarkDisconnecting()
{
    bool expected = false;
    return disconnecting_.compare_exchange_strong(expected, true);
}

bool ClientSession::IsDisconnecting() const
{
    return disconnecting_.load();
}

bool ClientSession::TryBeginSend(int maxPendingSendCount)
{
    if (maxPendingSendCount <= 0)
    {
        return false;
    }

    int currentCount = pendingSendCount_.load();

    while (currentCount < maxPendingSendCount)
    {
        if (pendingSendCount_.compare_exchange_weak(
            currentCount,
            currentCount + 1
        ))
        {
            return true;
        }
    }

    return false;
}

void ClientSession::EndSend()
{
    const int previousCount = pendingSendCount_.fetch_sub(1);

    if (previousCount <= 0)
    {
        pendingSendCount_.store(0);
    }
}

int ClientSession::GetPendingSendCount() const
{
    return pendingSendCount_.load();
}

int ClientSession::GetSessionId() const
{
    return sessionId_;
}

SOCKET ClientSession::GetSocket() const
{
    return socket_;
}

bool ClientSession::IsConnected() const
{
    return connected_.load();
}

IocpEvent& ClientSession::GetRecvEvent()
{
    return recvEvent_;
}

void ClientSession::AppendReceivedData(const char* data, int byteCount)
{
    if (data == nullptr || byteCount <= 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(packetBufferLock_);

    packetBuffer_.insert(
        packetBuffer_.end(),
        data,
        data + byteCount
    );
}

std::vector<char>& ClientSession::GetPacketBuffer()
{
    return packetBuffer_;
}

void ClientSession::ClearPacketBuffer()
{
    std::lock_guard<std::mutex> lock(packetBufferLock_);
    packetBuffer_.clear();
}

void ClientSession::SetLoggedIn(bool loggedIn)
{
    loggedIn_.store(loggedIn);
}

bool ClientSession::IsLoggedIn() const
{
    return loggedIn_.load();
}

bool ClientSession::TryBeginLogin()
{
    bool expected = false;
    return loginPending_.compare_exchange_strong(expected, true);
}

void ClientSession::EndLogin()
{
    loginPending_.store(false);
}

bool ClientSession::IsLoginPending() const
{
    return loginPending_.load();
}

void ClientSession::SetUsername(const std::string& username)
{
    username_ = username;
}

const std::string& ClientSession::GetUsername() const
{
    return username_;
}

void ClientSession::SetPlayerId(int playerId)
{
    playerId_ = playerId;
}

int ClientSession::GetPlayerId() const
{
    return playerId_;
}

void ClientSession::SetPosition(int x, int y)
{
    x_ = x;
    y_ = y;
}

int ClientSession::GetX() const
{
    return x_;
}

int ClientSession::GetY() const
{
    return y_;
}

void ClientSession::LoadPersistentState(
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
)
{
    std::lock_guard<std::mutex> lock(progressLock_);

    level_ = std::max(1, level);
    exp_ = std::max(0, exp);

    maxHp_ = 100;
    maxMp_ = 50;

    hp_ = std::max(1, hp);
    mp_ = std::max(0, mp);

    x_ = x;
    y_ = y;

    gold_ = std::max(0, gold);
    statPoint_ = std::max(0, statPoint);
    skillPoint_ = std::max(0, skillPoint);

    str_ = std::max(1, str);
    intStat_ = std::max(1, intStat);
    dex_ = std::max(1, dex);
    luk_ = std::max(1, luk);

    for (int i = 0; i < MAX_SKILL_STATUS_COUNT; ++i)
    {
        skillLevels_[i] = 0;
    }

    equippedWeaponItemId_ = 0;
    equippedWeaponName_.clear();

    equippedArmorItemId_ = 0;
    equippedArmorName_.clear();

    equippedAccessoryItemId_ = 0;
    equippedAccessoryName_.clear();

    ResetEquipmentBonusNoLock();

    for (int i = 0; i < MAX_QUICK_SLOT_COUNT; ++i)
    {
        quickSlots_[i] = QuickSlotStatusEntry{};
        quickSlots_[i].slotIndex = i;
        quickSlots_[i].slotType = QUICK_SLOT_TYPE_EMPTY;
        quickSlots_[i].targetId = 0;
    }
}

void ClientSession::SetSkillLevelForLoad(int skillId, int skillLevel)
{
    std::lock_guard<std::mutex> lock(progressLock_);

    const int skillIndex = GetSkillIndex(skillId);

    if (skillIndex < 0)
    {
        return;
    }

    if (skillLevel < 0)
    {
        skillLevel = 0;
    }
    else if (skillLevel > SKILL_MAX_LEVEL)
    {
        skillLevel = SKILL_MAX_LEVEL;
    }

    skillLevels_[skillIndex] = skillLevel;
}

void ClientSession::ClampVitalsToCurrentMax()
{
    std::lock_guard<std::mutex> lock(progressLock_);

    const int currentMaxHp = CalculateMaxHpNoLock();
    const int currentMaxMp = CalculateMaxMpNoLock();

    if (hp_ > currentMaxHp)
    {
        hp_ = currentMaxHp;
    }

    if (hp_ <= 0)
    {
        hp_ = 1;
    }

    if (mp_ > currentMaxMp)
    {
        mp_ = currentMaxMp;
    }

    if (mp_ < 0)
    {
        mp_ = 0;
    }
}

bool ClientSession::TryConsumeMoveCooldown(int cooldownMs)
{
    const auto now = std::chrono::steady_clock::now();
    const auto interval = std::chrono::milliseconds(cooldownMs);

    if (now - lastMoveTime_ < interval)
    {
        return false;
    }

    lastMoveTime_ = now;
    return true;
}

void ClientSession::SetFacingDirection(DIRECTION direction)
{
    facingDirection_.store(static_cast<int>(direction));
}

DIRECTION ClientSession::GetFacingDirection() const
{
    return static_cast<DIRECTION>(facingDirection_.load());
}

bool ClientSession::TryUseAttack(long long nowMs, long long cooldownMs)
{
    long long lastAttackTime = lastAttackTimeMs_.load();

    while (true)
    {
        if (nowMs - lastAttackTime < cooldownMs)
        {
            return false;
        }

        if (lastAttackTimeMs_.compare_exchange_weak(lastAttackTime, nowMs))
        {
            return true;
        }
    }
}

int ClientSession::GetLevel() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return level_;
}

int ClientSession::GetExp() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return exp_;
}

int ClientSession::GetRequiredExp() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateRequiredExpByLevel(level_);
}

int ClientSession::GetStatPoint() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return statPoint_;
}

int ClientSession::GetSkillPoint() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return skillPoint_;
}

int ClientSession::GetGold() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return gold_;
}

void ClientSession::AddGold(int amount)
{
    if (amount <= 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    if (gold_ > 1000000000 - amount)
    {
        gold_ = 1000000000;
        return;
    }

    gold_ += amount;
}

int ClientSession::GetStr() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return str_;
}

int ClientSession::GetIntStat() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return intStat_;
}

int ClientSession::GetDex() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return dex_;
}

int ClientSession::GetLuk() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return luk_;
}

int ClientSession::GetSkillLevelNoLock(int skillId) const
{
    const int skillIndex = GetSkillIndex(skillId);

    if (skillIndex < 0)
    {
        return 0;
    }

    return skillLevels_[skillIndex];
}

int ClientSession::CalculateMaxHpNoLock() const
{
    int strBonus = GetTotalStrNoLock() - 5;

    if (strBonus < 0)
    {
        strBonus = 0;
    }

    return maxHp_ + strBonus * 5;
}

int ClientSession::CalculateMaxMpNoLock() const
{
    const int manaTrainingLevel =
        GetSkillLevelNoLock(SKILL_MANA_TRAINING);

    int intBonus = GetTotalIntNoLock() - 5;

    if (intBonus < 0)
    {
        intBonus = 0;
    }

    return maxMp_ + intBonus * 5 + manaTrainingLevel * 20;
}

int ClientSession::CalculatePhysicalAttackNoLock() const
{
    return
        10 +
        GetTotalStrNoLock() * 3 +
        GetTotalDexNoLock() / 2 +
        equipmentPhysicalAttack_ +
        ragePhysicalAttackBonus_;
}

int ClientSession::CalculatePhysicalDefenseNoLock() const
{
    return
        GetTotalStrNoLock() * 2 +
        equipmentPhysicalDefense_;
}

int ClientSession::CalculateMagicAttackNoLock() const
{
    return
        10 +
        GetTotalIntNoLock() * 3 +
        equipmentMagicAttack_;
}

int ClientSession::CalculateMagicDefenseNoLock() const
{
    return
        GetTotalIntNoLock() * 2 +
        equipmentMagicDefense_;
}

int ClientSession::CalculateAccuracyRateNoLock() const
{
    int accuracy = 80 + GetTotalDexNoLock();

    if (accuracy > 98)
    {
        accuracy = 98;
    }

    return accuracy;
}

int ClientSession::CalculateEvasionRateNoLock() const
{
    int evasion = GetTotalDexNoLock() / 2;

    if (evasion > 35)
    {
        evasion = 35;
    }

    return evasion;
}

int ClientSession::CalculateCriticalRateNoLock() const
{
    const int luckyHunterLevel =
        GetSkillLevelNoLock(SKILL_LUCKY_HUNTER);

    int criticalRate =
        5 +
        GetTotalLukNoLock() / 2 +
        luckyHunterLevel * 3;

    if (criticalRate > 50)
    {
        criticalRate = 50;
    }

    return criticalRate;
}

int ClientSession::CalculateDropBonusBasisPointNoLock() const
{
    const int luckyHunterLevel =
        GetSkillLevelNoLock(SKILL_LUCKY_HUNTER);

    int bonus =
        GetTotalLukNoLock() * 50 +
        luckyHunterLevel * 300;

    if (bonus > 3000)
    {
        bonus = 3000;
    }

    return bonus;
}

int ClientSession::GetPhysicalAttack() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculatePhysicalAttackNoLock();
}

int ClientSession::GetPhysicalDefense() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculatePhysicalDefenseNoLock();
}

int ClientSession::GetMagicAttack() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateMagicAttackNoLock();
}

int ClientSession::GetMagicDefense() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateMagicDefenseNoLock();
}

int ClientSession::GetAccuracyRate() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateAccuracyRateNoLock();
}

int ClientSession::GetEvasionRate() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateEvasionRateNoLock();
}

int ClientSession::GetCriticalRate() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateCriticalRateNoLock();
}

int ClientSession::GetDropBonusBasisPoint() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateDropBonusBasisPointNoLock();
}

bool ClientSession::TryAddStat(
    int statType,
    int& outRemainingStatPoint,
    int& outNewStatValue,
    std::string& outStatName
)
{
    std::lock_guard<std::mutex> lock(progressLock_);

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    outRemainingStatPoint = statPoint_;
    outNewStatValue = 0;
    outStatName = GetStatNameByType(statType);

    if (statPoint_ <= 0)
    {
        return false;
    }

    switch (statType)
    {
    case STAT_STR:
        ++str_;
        outNewStatValue = str_;
        break;

    case STAT_INT:
        ++intStat_;
        outNewStatValue = intStat_;
        break;

    case STAT_DEX:
        ++dex_;
        outNewStatValue = dex_;
        break;

    case STAT_LUK:
        ++luk_;
        outNewStatValue = luk_;
        break;

    default:
        outStatName = "UNKNOWN";
        return false;
    }

    --statPoint_;
    outRemainingStatPoint = statPoint_;

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

bool ClientSession::TryAddSkillPoint(
    int skillId,
    int& outRemainingSkillPoint,
    int& outNewSkillLevel,
    std::string& outSkillName,
    std::string& outFailReason
)
{
    std::lock_guard<std::mutex> lock(progressLock_);

    outRemainingSkillPoint = skillPoint_;
    outNewSkillLevel = 0;
    outSkillName = GetSkillNameById(skillId);
    outFailReason.clear();

    const int skillIndex = GetSkillIndex(skillId);

    if (skillIndex < 0)
    {
        outFailReason = "Invalid skill.";
        return false;
    }

    if (skillPoint_ <= 0)
    {
        outFailReason = "Not enough skill point.";
        return false;
    }

    if (skillLevels_[skillIndex] >= SKILL_MAX_LEVEL)
    {
        outFailReason = "Skill already reached max level.";
        outNewSkillLevel = skillLevels_[skillIndex];
        return false;
    }

    const int requiredPreviousSkillId =
        GetRequiredPreviousSkillId(skillId);

    if (requiredPreviousSkillId != SKILL_NONE)
    {
        const int previousSkillIndex =
            GetSkillIndex(requiredPreviousSkillId);

        if (previousSkillIndex < 0 ||
            skillLevels_[previousSkillIndex] < 5)
        {
            outFailReason =
                "Previous skill must be level 5 or higher.";
            return false;
        }
    }

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    ++skillLevels_[skillIndex];
    --skillPoint_;

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    outRemainingSkillPoint = skillPoint_;
    outNewSkillLevel = skillLevels_[skillIndex];

    return true;
}

bool ClientSession::ActivateRage(
    long long nowMs,
    int skillLevel,
    int& outMp,
    int& outMaxMp,
    int& outStrBonus,
    int& outPhysicalAttackBonus,
    int& outDurationSeconds,
    std::string& outFailReason
)
{
    outMp = 0;
    outMaxMp = 0;
    outStrBonus = 0;
    outPhysicalAttackBonus = 0;
    outDurationSeconds = 0;
    outFailReason.clear();

    if (skillLevel <= 0)
    {
        outFailReason = "You have not learned Rage.";
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    if (rageEndTimeMs_ > nowMs)
    {
        outFailReason = "Rage is already active.";
        outMp = mp_;
        outMaxMp = CalculateMaxMpNoLock();
        return false;
    }

    const int mpCost = 20 + skillLevel * 3;
    const int maxMp = CalculateMaxMpNoLock();

    if (mp_ < mpCost)
    {
        outFailReason = "Not enough MP for Rage.";
        outMp = mp_;
        outMaxMp = maxMp;
        return false;
    }

    const int oldMaxHp = CalculateMaxHpNoLock();
    const bool wasHpFull = hp_ >= oldMaxHp;

    mp_ -= mpCost;

    rageStrBonus_ = 5 + (skillLevel - 1) * 2;
    ragePhysicalAttackBonus_ = 10 + (skillLevel - 1) * 4;
    rageEndTimeMs_ = nowMs + 10000;

    const int newMaxHp = CalculateMaxHpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    outMp = mp_;
    outMaxMp = maxMp;
    outStrBonus = rageStrBonus_;
    outPhysicalAttackBonus = ragePhysicalAttackBonus_;
    outDurationSeconds = 10;

    return true;
}

bool ClientSession::UpdateTimedBuffs(long long nowMs, bool& outRageExpired)
{
    outRageExpired = false;

    std::lock_guard<std::mutex> lock(progressLock_);

    if (rageEndTimeMs_ > 0 && nowMs >= rageEndTimeMs_)
    {
        rageEndTimeMs_ = 0;
        rageStrBonus_ = 0;
        ragePhysicalAttackBonus_ = 0;

        const int maxHp = CalculateMaxHpNoLock();

        if (hp_ > maxHp)
        {
            hp_ = maxHp;
        }

        outRageExpired = true;
        return true;
    }

    return false;
}

bool ClientSession::IsRageActive(long long nowMs) const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return rageEndTimeMs_ > nowMs;
}

int ClientSession::GetSkillLevel(int skillId) const
{
    std::lock_guard<std::mutex> lock(progressLock_);

    const int skillIndex = GetSkillIndex(skillId);

    if (skillIndex < 0)
    {
        return 0;
    }

    return skillLevels_[skillIndex];
}

void ClientSession::GetSkillStatus(
    SkillStatusEntry* outSkills,
    int maxSkillCount,
    int& outSkillCount
) const
{
    outSkillCount = 0;

    if (outSkills == nullptr || maxSkillCount <= 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int* skillIds = GetAllSkillIds();

    const int count =
        maxSkillCount < MAX_SKILL_STATUS_COUNT
        ? maxSkillCount
        : MAX_SKILL_STATUS_COUNT;

    for (int i = 0; i < count; ++i)
    {
        outSkills[i].skillId = skillIds[i];
        outSkills[i].level = skillLevels_[i];
    }

    outSkillCount = count;
}

int ClientSession::AddExperience(int expGained)
{
    if (expGained <= 0)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    exp_ += expGained;

    int levelGained = 0;

    while (true)
    {
        const int requiredExp = CalculateRequiredExpByLevel(level_);

        if (exp_ < requiredExp)
        {
            break;
        }

        exp_ -= requiredExp;
        ++level_;
        ++levelGained;

        statPoint_ += 3;
        skillPoint_ += 1;

        maxHp_ += 10;
        maxMp_ += 5;

        hp_ = CalculateMaxHpNoLock();
        mp_ = CalculateMaxMpNoLock();
    }

    return levelGained;
}

int ClientSession::GetHp() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return hp_;
}

int ClientSession::GetMaxHp() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateMaxHpNoLock();
}

int ClientSession::GetMp() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return mp_;
}

int ClientSession::GetMaxMp() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return CalculateMaxMpNoLock();
}

bool ClientSession::ApplyDamage(int damage, int& outHp, int& outMaxHp)
{
    if (damage < 1)
    {
        damage = 1;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int maxHp = CalculateMaxHpNoLock();

    if (hp_ > maxHp)
    {
        hp_ = maxHp;
    }

    hp_ -= damage;

    if (hp_ < 0)
    {
        hp_ = 0;
    }

    outHp = hp_;
    outMaxHp = maxHp;

    return hp_ <= 0;
}

void ClientSession::FullHeal()
{
    std::lock_guard<std::mutex> lock(progressLock_);

    hp_ = CalculateMaxHpNoLock();
    mp_ = CalculateMaxMpNoLock();
}

bool ClientSession::ConsumeMp(int amount, int& outMp, int& outMaxMp)
{
    std::lock_guard<std::mutex> lock(progressLock_);

    const int maxMp = CalculateMaxMpNoLock();

    if (mp_ > maxMp)
    {
        mp_ = maxMp;
    }

    outMp = mp_;
    outMaxMp = maxMp;

    if (amount <= 0)
    {
        return true;
    }

    if (mp_ < amount)
    {
        return false;
    }

    mp_ -= amount;

    outMp = mp_;
    outMaxMp = maxMp;

    return true;
}

bool ClientSession::RecoverMp(int amount, int& outMp, int& outMaxMp)
{
    std::lock_guard<std::mutex> lock(progressLock_);

    const int maxMp = CalculateMaxMpNoLock();

    if (amount <= 0)
    {
        if (mp_ > maxMp)
        {
            mp_ = maxMp;
        }

        outMp = mp_;
        outMaxMp = maxMp;
        return false;
    }

    mp_ += amount;

    if (mp_ > maxMp)
    {
        mp_ = maxMp;
    }

    outMp = mp_;
    outMaxMp = maxMp;

    return true;
}

void ClientSession::FullRecoverMp()
{
    std::lock_guard<std::mutex> lock(progressLock_);

    mp_ = CalculateMaxMpNoLock();
}

bool ClientSession::TryRegenerateHp(
    long long nowMs,
    long long intervalMs,
    int percent,
    int& outHealed,
    int& outHp,
    int& outMaxHp
)
{
    outHealed = 0;
    outHp = 0;
    outMaxHp = 0;

    if (intervalMs <= 0)
    {
        intervalMs = 5000;
    }

    if (percent <= 0)
    {
        percent = 10;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int maxHp = CalculateMaxHpNoLock();

    if (hp_ > maxHp)
    {
        hp_ = maxHp;
    }

    outHp = hp_;
    outMaxHp = maxHp;

    if (nextHpRegenTimeMs_ == 0)
    {
        nextHpRegenTimeMs_ = nowMs + intervalMs;
        return false;
    }

    if (nowMs < nextHpRegenTimeMs_)
    {
        return false;
    }

    nextHpRegenTimeMs_ = nowMs + intervalMs;

    if (hp_ <= 0)
    {
        return false;
    }

    if (hp_ >= maxHp)
    {
        return false;
    }

    int healAmount = maxHp * percent / 100;

    if (healAmount < 1)
    {
        healAmount = 1;
    }

    const int beforeHp = hp_;

    hp_ += healAmount;

    if (hp_ > maxHp)
    {
        hp_ = maxHp;
    }

    outHealed = hp_ - beforeHp;
    outHp = hp_;
    outMaxHp = maxHp;

    return outHealed > 0;
}

void ClientSession::ReduceExpByHalf()
{
    std::lock_guard<std::mutex> lock(progressLock_);
    exp_ /= 2;
}

void ClientSession::ResetHpRegenTimer(long long nextTimeMs)
{
    std::lock_guard<std::mutex> lock(progressLock_);
    nextHpRegenTimeMs_ = nextTimeMs;
}

bool ClientSession::IsAlive() const
{
    return alive_.load();
}

bool ClientSession::IsWaitingRespawn() const
{
    return !alive_.load() && respawnTimeMs_.load() > 0;
}

void ClientSession::MarkDead(long long respawnTimeMs)
{
    alive_.store(false);
    respawnTimeMs_.store(respawnTimeMs);
    lastRespawnNoticeSecond_.store(-1);

    std::lock_guard<std::mutex> lock(progressLock_);
    hp_ = 0;
}

bool ClientSession::IsRespawnReady(long long nowMs) const
{
    if (alive_.load())
    {
        return false;
    }

    const long long respawnTimeMs = respawnTimeMs_.load();

    if (respawnTimeMs <= 0)
    {
        return false;
    }

    return nowMs >= respawnTimeMs;
}

int ClientSession::GetRespawnRemainingSeconds(long long nowMs) const
{
    if (alive_.load())
    {
        return 0;
    }

    const long long respawnTimeMs = respawnTimeMs_.load();

    if (respawnTimeMs <= nowMs)
    {
        return 0;
    }

    const long long remainingMs = respawnTimeMs - nowMs;

    return static_cast<int>((remainingMs + 999) / 1000);
}

bool ClientSession::TryConsumeRespawnCountdownNotice(
    long long nowMs,
    int& outRemainingSeconds
)
{
    outRemainingSeconds = GetRespawnRemainingSeconds(nowMs);

    if (outRemainingSeconds <= 0)
    {
        return false;
    }

    const int lastNoticeSecond = lastRespawnNoticeSecond_.load();

    if (lastNoticeSecond == outRemainingSeconds)
    {
        return false;
    }

    lastRespawnNoticeSecond_.store(outRemainingSeconds);

    return true;
}

void ClientSession::ReviveFull()
{
    std::lock_guard<std::mutex> lock(progressLock_);

    hp_ = CalculateMaxHpNoLock();
    mp_ = CalculateMaxMpNoLock();

    alive_.store(true);
    respawnTimeMs_.store(0);
    lastRespawnNoticeSecond_.store(-1);
}

bool ClientSession::HealHp(
    int healAmount,
    int& outHealed,
    int& outHp,
    int& outMaxHp
)
{
    outHealed = 0;
    outHp = 0;
    outMaxHp = 0;

    if (healAmount <= 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int maxHp = CalculateMaxHpNoLock();

    if (hp_ > maxHp)
    {
        hp_ = maxHp;
    }

    if (hp_ >= maxHp)
    {
        outHp = hp_;
        outMaxHp = maxHp;
        return false;
    }

    const int beforeHp = hp_;

    hp_ += healAmount;

    if (hp_ > maxHp)
    {
        hp_ = maxHp;
    }

    outHealed = hp_ - beforeHp;
    outHp = hp_;
    outMaxHp = maxHp;

    return outHealed > 0;
}

int ClientSession::GetTotalStrNoLock() const
{
    return str_ + equipmentStr_ + rageStrBonus_;
}

int ClientSession::GetTotalIntNoLock() const
{
    return intStat_ + equipmentInt_;
}

int ClientSession::GetTotalDexNoLock() const
{
    return dex_ + equipmentDex_;
}

int ClientSession::GetTotalLukNoLock() const
{
    return luk_ + equipmentLuk_;
}

void ClientSession::ResetEquipmentBonusNoLock()
{
    equipmentStr_ = 0;
    equipmentInt_ = 0;
    equipmentDex_ = 0;
    equipmentLuk_ = 0;

    equipmentPhysicalAttack_ = 0;
    equipmentPhysicalDefense_ = 0;
    equipmentMagicAttack_ = 0;
    equipmentMagicDefense_ = 0;
}

void ClientSession::AddEquipmentBonusNoLock(int itemId)
{
    switch (itemId)
    {
    case ITEM_IRON_SWORD:
        equipmentStr_ += 3;
        equipmentPhysicalAttack_ += 10;
        break;

    case ITEM_STEEL_SWORD:
        equipmentStr_ += 5;
        equipmentPhysicalAttack_ += 18;
        break;

    case ITEM_FLAME_SWORD:
        equipmentStr_ += 7;
        equipmentInt_ += 2;
        equipmentPhysicalAttack_ += 25;
        equipmentMagicAttack_ += 8;
        break;

    case ITEM_MAGE_STAFF:
        equipmentInt_ += 6;
        equipmentMagicAttack_ += 22;
        break;

    case ITEM_ICE_WAND:
        equipmentInt_ += 5;
        equipmentMagicAttack_ += 18;
        equipmentMagicDefense_ += 5;
        break;

    case ITEM_HUNTER_BOW:
        equipmentDex_ += 6;
        equipmentPhysicalAttack_ += 16;
        break;

    case ITEM_IRON_ARMOR:
        equipmentStr_ += 2;
        equipmentPhysicalDefense_ += 10;
        break;

    case ITEM_STEEL_ARMOR:
        equipmentStr_ += 4;
        equipmentPhysicalDefense_ += 18;
        break;

    case ITEM_MAGIC_ROBE:
        equipmentInt_ += 4;
        equipmentMagicDefense_ += 16;
        break;

    case ITEM_LEATHER_ARMOR:
        equipmentDex_ += 3;
        equipmentPhysicalDefense_ += 8;
        break;

    case ITEM_MAGIC_RING:
        equipmentInt_ += 3;
        equipmentMagicAttack_ += 8;
        break;

    case ITEM_WARRIOR_RING:
        equipmentStr_ += 4;
        equipmentPhysicalAttack_ += 8;
        break;

    case ITEM_LUCKY_NECKLACE:
        equipmentLuk_ += 8;
        break;

    case ITEM_SPEED_CHARM:
        equipmentDex_ += 5;
        equipmentLuk_ += 2;
        break;

    default:
        break;
    }
}

void ClientSession::RecalculateEquipmentBonusNoLock()
{
    ResetEquipmentBonusNoLock();
    AddEquipmentBonusNoLock(equippedWeaponItemId_);
    AddEquipmentBonusNoLock(equippedArmorItemId_);
    AddEquipmentBonusNoLock(equippedAccessoryItemId_);
}

bool ClientSession::EquipWeapon(int itemId, const std::string& itemName)
{
    if (itemId != ITEM_IRON_SWORD &&
        itemId != ITEM_STEEL_SWORD &&
        itemId != ITEM_FLAME_SWORD &&
        itemId != ITEM_MAGE_STAFF &&
        itemId != ITEM_ICE_WAND &&
        itemId != ITEM_HUNTER_BOW)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    equippedWeaponItemId_ = itemId;
    equippedWeaponName_ = itemName;

    RecalculateEquipmentBonusNoLock();

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

bool ClientSession::EquipArmor(int itemId, const std::string& itemName)
{
    if (itemId != ITEM_IRON_ARMOR &&
        itemId != ITEM_STEEL_ARMOR &&
        itemId != ITEM_MAGIC_ROBE &&
        itemId != ITEM_LEATHER_ARMOR)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    equippedArmorItemId_ = itemId;
    equippedArmorName_ = itemName;

    RecalculateEquipmentBonusNoLock();

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

bool ClientSession::EquipAccessory(int itemId, const std::string& itemName)
{
    if (itemId != ITEM_MAGIC_RING &&
        itemId != ITEM_WARRIOR_RING &&
        itemId != ITEM_LUCKY_NECKLACE &&
        itemId != ITEM_SPEED_CHARM)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    equippedAccessoryItemId_ = itemId;
    equippedAccessoryName_ = itemName;

    RecalculateEquipmentBonusNoLock();

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

bool ClientSession::UnequipWeapon(int& outItemId, std::string& outItemName)
{
    outItemId = 0;
    outItemName.clear();

    std::lock_guard<std::mutex> lock(progressLock_);

    if (equippedWeaponItemId_ <= 0)
    {
        return false;
    }

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    outItemId = equippedWeaponItemId_;
    outItemName = equippedWeaponName_;

    equippedWeaponItemId_ = 0;
    equippedWeaponName_.clear();

    RecalculateEquipmentBonusNoLock();

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

bool ClientSession::UnequipArmor(int& outItemId, std::string& outItemName)
{
    outItemId = 0;
    outItemName.clear();

    std::lock_guard<std::mutex> lock(progressLock_);

    if (equippedArmorItemId_ <= 0)
    {
        return false;
    }

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    outItemId = equippedArmorItemId_;
    outItemName = equippedArmorName_;

    equippedArmorItemId_ = 0;
    equippedArmorName_.clear();

    RecalculateEquipmentBonusNoLock();

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

bool ClientSession::UnequipAccessory(int& outItemId, std::string& outItemName)
{
    outItemId = 0;
    outItemName.clear();

    std::lock_guard<std::mutex> lock(progressLock_);

    if (equippedAccessoryItemId_ <= 0)
    {
        return false;
    }

    const int oldMaxHp = CalculateMaxHpNoLock();
    const int oldMaxMp = CalculateMaxMpNoLock();

    const bool wasHpFull = hp_ >= oldMaxHp;
    const bool wasMpFull = mp_ >= oldMaxMp;

    outItemId = equippedAccessoryItemId_;
    outItemName = equippedAccessoryName_;

    equippedAccessoryItemId_ = 0;
    equippedAccessoryName_.clear();

    RecalculateEquipmentBonusNoLock();

    const int newMaxHp = CalculateMaxHpNoLock();
    const int newMaxMp = CalculateMaxMpNoLock();

    if (wasHpFull)
    {
        hp_ = newMaxHp;
    }
    else if (hp_ > newMaxHp)
    {
        hp_ = newMaxHp;
    }

    if (wasMpFull)
    {
        mp_ = newMaxMp;
    }
    else if (mp_ > newMaxMp)
    {
        mp_ = newMaxMp;
    }

    return true;
}

int ClientSession::GetEquippedWeaponItemId() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equippedWeaponItemId_;
}

std::string ClientSession::GetEquippedWeaponName() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equippedWeaponName_;
}

int ClientSession::GetEquippedArmorItemId() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equippedArmorItemId_;
}

std::string ClientSession::GetEquippedArmorName() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equippedArmorName_;
}

int ClientSession::GetEquippedAccessoryItemId() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equippedAccessoryItemId_;
}

std::string ClientSession::GetEquippedAccessoryName() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equippedAccessoryName_;
}

int ClientSession::GetEquipmentStrBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentStr_;
}

int ClientSession::GetEquipmentIntBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentInt_;
}

int ClientSession::GetEquipmentDexBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentDex_;
}

int ClientSession::GetEquipmentLukBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentLuk_;
}

int ClientSession::GetEquipmentPhysicalAttackBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentPhysicalAttack_;
}

int ClientSession::GetEquipmentPhysicalDefenseBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentPhysicalDefense_;
}

int ClientSession::GetEquipmentMagicAttackBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentMagicAttack_;
}

int ClientSession::GetEquipmentMagicDefenseBonus() const
{
    std::lock_guard<std::mutex> lock(progressLock_);
    return equipmentMagicDefense_;
}
bool ClientSession::SetQuickSlot(
    int slotIndex,
    int slotType,
    int targetId,
    const std::string& name
)
{
    if (slotIndex < 0 || slotIndex >= MAX_QUICK_SLOT_COUNT)
    {
        return false;
    }

    if (slotType != QUICK_SLOT_TYPE_ITEM &&
        slotType != QUICK_SLOT_TYPE_SKILL)
    {
        return false;
    }

    if (targetId <= 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    quickSlots_[slotIndex] = QuickSlotStatusEntry{};
    quickSlots_[slotIndex].slotIndex = slotIndex;
    quickSlots_[slotIndex].slotType = slotType;
    quickSlots_[slotIndex].targetId = targetId;

    CopyStringToFixedBuffer(
        quickSlots_[slotIndex].name,
        sizeof(quickSlots_[slotIndex].name),
        name
    );

    return true;
}

bool ClientSession::ClearQuickSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= MAX_QUICK_SLOT_COUNT)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    quickSlots_[slotIndex] = QuickSlotStatusEntry{};
    quickSlots_[slotIndex].slotIndex = slotIndex;
    quickSlots_[slotIndex].slotType = QUICK_SLOT_TYPE_EMPTY;
    quickSlots_[slotIndex].targetId = 0;

    return true;
}

void ClientSession::GetQuickSlotStatus(
    QuickSlotStatusEntry* outSlots,
    int maxSlotCount,
    int& outSlotCount
) const
{
    outSlotCount = 0;

    if (outSlots == nullptr || maxSlotCount <= 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(progressLock_);

    const int copyCount =
        maxSlotCount < MAX_QUICK_SLOT_COUNT
        ? maxSlotCount
        : MAX_QUICK_SLOT_COUNT;

    for (int i = 0; i < copyCount; ++i)
    {
        outSlots[i] = quickSlots_[i];
        outSlots[i].slotIndex = i;
        ++outSlotCount;
    }
}
