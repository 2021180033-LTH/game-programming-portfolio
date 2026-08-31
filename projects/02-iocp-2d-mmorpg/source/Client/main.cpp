#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <SFML/Graphics.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <array>

#include "GameProtocol.h"

#pragma comment(lib, "ws2_32.lib")

namespace
{
    constexpr const char* DEFAULT_SERVER_IP = "127.0.0.1";
    constexpr unsigned short SERVER_PORT = PORT;

    constexpr int TILE_SIZE = 32;
    constexpr int VIEW_TILE_COUNT = 20;
    constexpr int MAP_PIXEL_SIZE = TILE_SIZE * VIEW_TILE_COUNT;
    constexpr int UI_HEIGHT = 118;

    constexpr int WINDOW_WIDTH = MAP_PIXEL_SIZE;
    constexpr int WINDOW_HEIGHT = MAP_PIXEL_SIZE + UI_HEIGHT;

    constexpr int QUICK_SLOT_COUNT = 5;

    constexpr int QUICK_SLOT_EMPTY = 0;
    constexpr int QUICK_SLOT_ITEM = 1;
    constexpr int QUICK_SLOT_SKILL = 2;

    constexpr float QUICK_SLOT_SIZE = 40.0f;
    constexpr float QUICK_SLOT_GAP = 10.0f;

    constexpr int INVENTORY_VISIBLE_ITEM_COUNT = 10;
    constexpr float INVENTORY_ITEM_ROW_HEIGHT = 30.0f;
    constexpr float INVENTORY_PANEL_X = 20.0f;
    constexpr float INVENTORY_PANEL_Y = 20.0f;
    constexpr float INVENTORY_PANEL_WIDTH = 380.0f;
    constexpr float INVENTORY_PANEL_HEIGHT = 430.0f;

    constexpr float SKILL_TREE_X = 35.0f;
    constexpr float SKILL_TREE_Y = 70.0f;
    constexpr float SKILL_TREE_WIDTH = 570.0f;
    constexpr float SKILL_TREE_HEIGHT = 470.0f;
    constexpr float SKILL_NODE_WIDTH = 150.0f;
    constexpr float SKILL_NODE_HEIGHT = 78.0f;
    constexpr float SKILL_NODE_GAP_X = 35.0f;
    constexpr float SKILL_NODE_GAP_Y = 58.0f;
    constexpr float SKILL_PLUS_SIZE = 22.0f;

    constexpr int CONTEXT_ACTION_NONE = 0;
    constexpr int CONTEXT_ACTION_USE_ITEM = 1;
    constexpr int CONTEXT_ACTION_SET_QUICK_SLOT = 2;
    constexpr int CONTEXT_ACTION_CLEAR_QUICK_SLOT = 3;
    constexpr int CONTEXT_ACTION_EQUIP_ITEM = 4;
    constexpr int CONTEXT_ACTION_UNEQUIP_EQUIPMENT = 5;

    constexpr int EFFECT_ATTACK = 1;
    constexpr int EFFECT_POWER_STRIKE = 2;
    constexpr int EFFECT_HEAVY_SLASH = 3;
    constexpr int EFFECT_WHIRLWIND = 4;
    constexpr int EFFECT_FIREBALL = 5;
    constexpr int EFFECT_MANA_BURST = 6;
    constexpr int EFFECT_ICE_BOLT = 7;
    constexpr int EFFECT_RAGE = 8;

    constexpr int OBJECT_TREE = 1;
    constexpr int OBJECT_ROCK = 2;
    constexpr int OBJECT_FENCE = 3;
    constexpr int OBJECT_HOUSE = 4;
    constexpr int OBJECT_TENT = 5;
    constexpr int OBJECT_RUIN_PILLAR = 6;
    constexpr int OBJECT_CASTLE_WALL = 7;
    constexpr int OBJECT_GRASS = 8;
    constexpr int OBJECT_CAMPFIRE = 9;
    constexpr int OBJECT_SIGN = 10;

    constexpr int REGION_START_VILLAGE = 1;
    constexpr int REGION_GREEN_FOREST = 2;
    constexpr int REGION_ORC_CAMP = 3;
    constexpr int REGION_ANCIENT_RUINS = 4;
    constexpr int REGION_CASTLE_GATE = 5;
    constexpr int REGION_OPEN_FIELD = 6;

    constexpr float CONTEXT_MENU_WIDTH = 180.0f;
    constexpr float CONTEXT_MENU_OPTION_HEIGHT = 26.0f;
}

struct PlayerView
{
    int playerId = -1;
    std::string username;
    int x = 0;
    int y = 0;
};

struct MonsterView
{
    int monsterId = -1;
    int monsterType = 0;
    std::string name;

    int level = 1;
    int hp = 1;
    int maxHp = 1;

    int x = 0;
    int y = 0;
};

struct NpcView
{
    int npcId = -1;
    int npcType = 0;
    std::string name;

    int x = 0;
    int y = 0;
};

struct DropView
{
    int dropId = -1;

    int itemId = 0;
    int itemCategory = 0;
    int count = 0;

    int x = 0;
    int y = 0;

    std::string itemName;
};

struct InventoryView
{
    int itemId = 0;
    int itemCategory = 0;
    int count = 0;

    std::string itemName;
};

struct QuickSlotView
{
    int slotType = QUICK_SLOT_EMPTY;
    int targetId = 0;               
    std::string name;
};

struct ContextMenuOption
{
    int action = CONTEXT_ACTION_NONE;
    int value = 0;
    std::string text;
};

struct SkillTreeNodeDefinition
{
    int skillId = SKILL_NONE;
    const char* title = "";
    const char* typeText = "";
    int column = 0;
    int row = 0;
};

struct VisualEffect
{
    int effectType = EFFECT_ATTACK;
    int x = 0;
    int y = 0;
    long long startTimeMs = 0;
    long long durationMs = 450;
};

struct FloatingText
{
    std::string text;
    int x = 0;
    int y = 0;
    sf::Color color = sf::Color::White;
    long long startTimeMs = 0;
    long long durationMs = 850;
};

struct ClientState
{
    std::mutex lock;

    int myPlayerId = -1;
    int myX = 100;
    int myY = 100;
    bool avatarReceived = false;

    int hp = 100;
    int maxHp = 100;
    int mp = 50;
    int maxMp = 50;

    int level = 1;
    int exp = 0;
    int requiredExp = 100;
    int statPoint = 0;
    int skillPoint = 0;
    int gold = 0;

    int str = 5;
    int intStat = 5;
    int dex = 5;
    int luk = 5;

    std::array<int, MAX_SKILL_STATUS_COUNT> skillLevels{};

    std::unordered_map<int, PlayerView> players;
    std::unordered_map<int, MonsterView> monsters;
    std::unordered_map<int, NpcView> npcs;

    std::unordered_map<int, DropView> drops;
    std::unordered_map<int, InventoryView> inventory;

    bool showInventory = false;
    int inventoryScrollOffset = 0;
    bool showStatWindow = false;
    bool showSkillWindow = false;
    bool showEquipmentWindow = false;
    bool showShopWindow = false;
    bool showWorldMap = false;

    int selectedInventoryItemId = 0;
    int selectedInventoryItemCategory = 0;
    std::string selectedInventoryItemName;

    std::array<QuickSlotView, QUICK_SLOT_COUNT> quickSlots;



    bool contextMenuOpen = false;
    float contextMenuX = 0.0f;
    float contextMenuY = 0.0f;

    int contextTargetItemId = 0;
    int contextTargetItemCategory = 0;
    std::string contextTargetItemName;

    int contextTargetQuickSlotIndex = -1;

    std::vector<ContextMenuOption> contextMenuOptions;

    std::vector<VisualEffect> effects;
    std::vector<FloatingText> floatingTexts;

    int pendingEffectType = 0;
    long long pendingEffectExpireTimeMs = 0;
    long long rageVisualEndTimeMs = 0;

    std::vector<std::string> messages;

    bool waitingRespawnNotice = false;

    bool chatInputMode = false;
    std::string chatInput;

    int equippedWeaponItemId = 0;
    std::string equippedWeaponName;

    int equippedArmorItemId = 0;
    std::string equippedArmorName;

    int equippedAccessoryItemId = 0;
    std::string equippedAccessoryName;

    int equipmentStrBonus = 0;
    int equipmentIntBonus = 0;
    int equipmentDexBonus = 0;
    int equipmentLukBonus = 0;

    int equipmentPhysicalAttackBonus = 0;
    int equipmentPhysicalDefenseBonus = 0;
    int equipmentMagicAttackBonus = 0;
    int equipmentMagicDefenseBonus = 0;
};

long long GetClientNowMs();

void AddVisualEffectAt(ClientState& state, int effectType, int worldX, int worldY);

void AddFloatingText(
    ClientState& state,
    int worldX,
    int worldY,
    const std::string& text,
    const sf::Color& color
);

void SetPendingEffect(ClientState& state, int effectType);

std::mutex g_coutLock;

std::mutex g_loginLock;
std::condition_variable g_loginCv;
bool g_loginResultReceived = false;
bool g_loginAccepted = false;
std::string g_loginMessage;

std::atomic_bool g_running = true;

void Log(const std::string& text)
{
    std::lock_guard<std::mutex> lock(g_coutLock);
    std::cout << text << std::endl;
}

std::string EncodeUtf8(sf::Uint32 unicode)
{
    std::string result;

    if (unicode <= 0x7F)
    {
        result.push_back(static_cast<char>(unicode));
    }
    else if (unicode <= 0x7FF)
    {
        result.push_back(static_cast<char>(0xC0 | ((unicode >> 6) & 0x1F)));
        result.push_back(static_cast<char>(0x80 | (unicode & 0x3F)));
    }
    else if (unicode <= 0xFFFF)
    {
        result.push_back(static_cast<char>(0xE0 | ((unicode >> 12) & 0x0F)));
        result.push_back(static_cast<char>(0x80 | ((unicode >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (unicode & 0x3F)));
    }
    else if (unicode <= 0x10FFFF)
    {
        result.push_back(static_cast<char>(0xF0 | ((unicode >> 18) & 0x07)));
        result.push_back(static_cast<char>(0x80 | ((unicode >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((unicode >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (unicode & 0x3F)));
    }

    return result;
}

void PopBackUtf8(std::string& text)
{
    if (text.empty())
    {
        return;
    }

    text.pop_back();

    while (!text.empty() &&
        (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80)
    {
        text.pop_back();
    }
}

void AddClientMessage(ClientState& state, const std::string& message)
{
    std::lock_guard<std::mutex> lock(state.lock);

    state.messages.push_back(message);

    constexpr int MAX_MESSAGE_COUNT = 8;

    while (static_cast<int>(state.messages.size()) > MAX_MESSAGE_COUNT)
    {
        state.messages.erase(state.messages.begin());
    }
}

std::string MakeSafeString(const char* text, int maxLen)
{
    if (text == nullptr || maxLen <= 0)
    {
        return {};
    }

    int length = 0;

    while (length < maxLen && text[length] != '\0')
    {
        ++length;
    }

    return std::string(text, text + length);
}

std::string MonsterTypeToString(int monsterType)
{
    switch (monsterType)
    {
    case MONSTER_SLIME:
        return "Slime";

    case MONSTER_GOBLIN:
        return "Goblin";

    case MONSTER_WOLF:
        return "Wolf";

    case MONSTER_ORC:
        return "Orc";

    case MONSTER_SKELETON:
        return "Skeleton";

    case MONSTER_DARK_MAGE:
        return "DarkMage";

    default:
        return "Unknown";
    }
}

bool SendAll(SOCKET socketHandle, const char* data, int size)
{
    int sentBytes = 0;

    while (sentBytes < size)
    {
        const int result = send(
            socketHandle,
            data + sentBytes,
            size - sentBytes,
            0
        );

        if (result == SOCKET_ERROR || result == 0)
        {
            return false;
        }

        sentBytes += result;
    }

    return true;
}

bool SendLoginPacket(SOCKET socketHandle, const std::string& username)
{
    C2S_Login packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_Login));
    packet.type = C2S_LOGIN;

    strncpy_s(
        packet.username,
        sizeof(packet.username),
        username.c_str(),
        _TRUNCATE
    );

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendMovePacket(SOCKET socketHandle, DIRECTION direction, int moveTime)
{
    C2S_Move packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_Move));
    packet.type = C2S_MOVE;
    packet.dir = direction;
    packet.move_time = moveTime;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendAttackPacket(SOCKET socketHandle)
{
    C2S_Attack packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_Attack));
    packet.type = C2S_ATTACK;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendUseItem(SOCKET socketHandle, int itemId)
{
    if (itemId <= 0)
    {
        return false;
    }

    C2S_UseItem packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_UseItem));
    packet.type = C2S_USE_ITEM;
    packet.itemId = itemId;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendUnequipEquipment(SOCKET socketHandle, int equipmentSlot)
{
    if (equipmentSlot <= 0)
    {
        return false;
    }

    C2S_UnequipEquipment packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_UnequipEquipment));
    packet.type = C2S_UNEQUIP_EQUIPMENT;
    packet.equipmentSlot = equipmentSlot;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}


bool SendSellItem(SOCKET socketHandle, int itemId, int count)
{
    if (itemId <= 0 || count <= 0)
    {
        return false;
    }

    C2S_SellItem packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_SellItem));
    packet.type = C2S_SELL_ITEM;
    packet.itemId = itemId;
    packet.count = count;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendSetQuickSlot(
    SOCKET socketHandle,
    int slotIndex,
    int slotType,
    int targetId,
    const std::string& name
)
{
    if (slotIndex < 0 || slotIndex >= QUICK_SLOT_COUNT)
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

    C2S_SetQuickSlot packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_SetQuickSlot));
    packet.type = C2S_SET_QUICK_SLOT;
    packet.slotIndex = slotIndex;
    packet.slotType = slotType;
    packet.targetId = targetId;

    std::memset(packet.name, 0, sizeof(packet.name));

    const std::size_t copySize =
        name.size() < sizeof(packet.name) - 1
        ? name.size()
        : sizeof(packet.name) - 1;

    if (copySize > 0)
    {
        std::memcpy(packet.name, name.data(), copySize);
    }

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendClearQuickSlot(SOCKET socketHandle, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= QUICK_SLOT_COUNT)
    {
        return false;
    }

    C2S_ClearQuickSlot packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_ClearQuickSlot));
    packet.type = C2S_CLEAR_QUICK_SLOT;
    packet.slotIndex = slotIndex;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}


bool SendNpcHealPacket(SOCKET socketHandle, int npcId)
{
    if (npcId <= 0)
    {
        return false;
    }

    C2S_NpcHeal packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_NpcHeal));
    packet.type = C2S_NPC_HEAL;
    packet.npcId = npcId;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendUseSkillPacket(SOCKET socketHandle, int skillId)
{
    if (skillId <= 0)
    {
        return false;
    }

    C2S_UseSkill packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_UseSkill));
    packet.type = C2S_USE_SKILL;
    packet.skillId = skillId;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendAddStatPacket(SOCKET socketHandle, int statType)
{
    C2S_AddStat packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_AddStat));
    packet.type = C2S_ADD_STAT;
    packet.statType = statType;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendAddSkillPointPacket(SOCKET socketHandle, int skillId)
{
    C2S_AddSkillPoint packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_AddSkillPoint));
    packet.type = C2S_ADD_SKILL_POINT;
    packet.skillId = skillId;

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

bool SendChatPacket(SOCKET socketHandle, const std::string& message)
{
    if (message.empty())
    {
        return false;
    }

    C2S_Chat packet{};
    packet.size = static_cast<unsigned char>(sizeof(C2S_Chat));
    packet.type = C2S_CHAT;

    strncpy_s(
        packet.message,
        sizeof(packet.message),
        message.c_str(),
        _TRUNCATE
    );

    return SendAll(
        socketHandle,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

std::string GetSkillNameById(int skillId);
int GetSkillIdByIndex(int index);
int GetSkillIndexById(int skillId);

void HandlePacket(ClientState& state, const char* packet, int packetSize)
{
    if (packet == nullptr || packetSize < static_cast<int>(1 + sizeof(PACKET_TYPE)))
    {
        return;
    }

    PACKET_TYPE type{};
    std::memcpy(&type, packet + 1, sizeof(PACKET_TYPE));

    switch (type)
    {
    case S2C_LOGIN_RESULT:
    {
        if (packetSize != sizeof(S2C_LoginResult))
        {
            Log("[RECV] Invalid S2C_LOGIN_RESULT size.");
            return;
        }

        S2C_LoginResult result{};
        std::memcpy(&result, packet, sizeof(result));

        const std::string message = MakeSafeString(
            result.message,
            static_cast<int>(sizeof(result.message))
        );

        {
            std::lock_guard<std::mutex> lock(g_loginLock);
            g_loginResultReceived = true;
            g_loginAccepted = result.success;
            g_loginMessage = message;
        }

        g_loginCv.notify_all();

        Log(
            "[LOGIN_RESULT] success=" +
            std::string(result.success ? "true" : "false") +
            ", message=" +
            message
        );

        break;
    }

    case S2C_AVATAR_INFO:
    {
        if (packetSize != sizeof(S2C_AvatarInfo))
        {
            Log("[RECV] Invalid S2C_AVATAR_INFO size.");
            return;
        }

        S2C_AvatarInfo avatar{};
        std::memcpy(&avatar, packet, sizeof(avatar));

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.myPlayerId = avatar.playerId;
            state.myX = avatar.x;
            state.myY = avatar.y;
            state.avatarReceived = true;
        }

        g_loginCv.notify_all();

        Log(
            "[AVATAR_INFO] playerId=" +
            std::to_string(avatar.playerId) +
            ", x=" +
            std::to_string(avatar.x) +
            ", y=" +
            std::to_string(avatar.y)
        );

        break;
    }

    case S2C_ADD_PLAYER:
    {
        if (packetSize != sizeof(S2C_AddPlayer))
        {
            Log("[RECV] Invalid S2C_ADD_PLAYER size.");
            return;
        }

        S2C_AddPlayer add{};
        std::memcpy(&add, packet, sizeof(add));

        PlayerView view{};
        view.playerId = add.playerId;
        view.username = MakeSafeString(add.username, static_cast<int>(sizeof(add.username)));
        view.x = add.x;
        view.y = add.y;

        {
            std::lock_guard<std::mutex> lock(state.lock);

            if (add.playerId != state.myPlayerId)
            {
                state.players[add.playerId] = view;
            }
        }

        Log(
            "[ADD_PLAYER] playerId=" +
            std::to_string(add.playerId) +
            ", username=" +
            view.username +
            ", x=" +
            std::to_string(add.x) +
            ", y=" +
            std::to_string(add.y)
        );

        break;
    }

    case S2C_REMOVE_PLAYER:
    {
        if (packetSize != sizeof(S2C_RemovePlayer))
        {
            Log("[RECV] Invalid S2C_REMOVE_PLAYER size.");
            return;
        }

        S2C_RemovePlayer remove{};
        std::memcpy(&remove, packet, sizeof(remove));

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.players.erase(remove.playerId);
        }

        Log(
            "[REMOVE_PLAYER] playerId=" +
            std::to_string(remove.playerId)
        );

        break;
    }

    case S2C_MOVE_PLAYER:
    {
        if (packetSize != sizeof(S2C_MovePlayer))
        {
            Log("[RECV] Invalid S2C_MOVE_PLAYER size.");
            return;
        }

        S2C_MovePlayer move{};
        std::memcpy(&move, packet, sizeof(move));

        {
            std::lock_guard<std::mutex> lock(state.lock);

            if (move.playerId == state.myPlayerId)
            {
                state.myX = move.x;
                state.myY = move.y;
            }
            else
            {
                auto iter = state.players.find(move.playerId);

                if (iter != state.players.end())
                {
                    iter->second.x = move.x;
                    iter->second.y = move.y;
                }
            }
        }

        Log(
            "[MOVE_PLAYER] playerId=" +
            std::to_string(move.playerId) +
            ", x=" +
            std::to_string(move.x) +
            ", y=" +
            std::to_string(move.y)
        );

        break;
    }

    case S2C_ADD_MONSTER:
    {
        if (packetSize != sizeof(S2C_AddMonster))
        {
            Log("[RECV] Invalid S2C_ADD_MONSTER size.");
            return;
        }

        S2C_AddMonster add{};
        std::memcpy(&add, packet, sizeof(add));

        MonsterView view{};
        view.monsterId = add.monsterId;
        view.monsterType = add.monsterType;
        view.name = MakeSafeString(add.name, static_cast<int>(sizeof(add.name)));
        view.level = add.level;
        view.hp = add.hp;
        view.maxHp = add.maxHp;
        view.x = add.x;
        view.y = add.y;

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.monsters[add.monsterId] = view;
        }

        Log(
            "[ADD_MONSTER] monsterId=" +
            std::to_string(add.monsterId) +
            ", type=" +
            std::to_string(add.monsterType) +
            "(" +
            MonsterTypeToString(add.monsterType) +
            "), name=" +
            view.name +
            ", x=" +
            std::to_string(add.x) +
            ", y=" +
            std::to_string(add.y)
        );

        break;
    }

    case S2C_REMOVE_MONSTER:
    {
        if (packetSize != sizeof(S2C_RemoveMonster))
        {
            Log("[RECV] Invalid S2C_REMOVE_MONSTER size.");
            return;
        }

        S2C_RemoveMonster remove{};
        std::memcpy(&remove, packet, sizeof(remove));

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.monsters.erase(remove.monsterId);
        }

        Log(
            "[REMOVE_MONSTER] monsterId=" +
            std::to_string(remove.monsterId)
        );

        break;
    }

    case S2C_MOVE_MONSTER:
    {
        if (packetSize != sizeof(S2C_MoveMonster))
        {
            Log("[RECV] Invalid S2C_MOVE_MONSTER size.");
            return;
        }

        S2C_MoveMonster move{};
        std::memcpy(&move, packet, sizeof(move));

        {
            std::lock_guard<std::mutex> lock(state.lock);

            auto iter = state.monsters.find(move.monsterId);

            if (iter != state.monsters.end())
            {
                iter->second.x = move.x;
                iter->second.y = move.y;
            }
        }

        /*Log(
            "[MOVE_MONSTER] monsterId=" +
            std::to_string(move.monsterId) +
            ", x=" +
            std::to_string(move.x) +
            ", y=" +
            std::to_string(move.y)
        );*/

        break;
    }

    case S2C_MONSTER_HP:
    {
        if (packetSize != sizeof(S2C_MonsterHp))
        {
            Log("[RECV] Invalid S2C_MONSTER_HP size.");
            return;
        }

        S2C_MonsterHp hpPacket{};
        std::memcpy(&hpPacket, packet, sizeof(hpPacket));

        bool showMonsterDamage = false;
        int monsterDamage = 0;
        int monsterDamageX = 0;
        int monsterDamageY = 0;
        int confirmedEffectType = 0;

        {
            std::lock_guard<std::mutex> lock(state.lock);

            auto iter = state.monsters.find(hpPacket.monsterId);

            if (iter != state.monsters.end())
            {
                monsterDamage = iter->second.hp - hpPacket.hp;
                monsterDamageX = iter->second.x;
                monsterDamageY = iter->second.y;

                if (monsterDamage > 0)
                {
                    showMonsterDamage = true;

                    if (state.pendingEffectType != 0 &&
                        GetClientNowMs() <= state.pendingEffectExpireTimeMs)
                    {
                        confirmedEffectType = state.pendingEffectType;
                    }
                }

                iter->second.hp = hpPacket.hp;
                iter->second.maxHp = hpPacket.maxHp;
            }
        }

        if (showMonsterDamage)
        {
            AddFloatingText(
                state,
                monsterDamageX,
                monsterDamageY,
                "-" + std::to_string(monsterDamage),
                sf::Color(255, 80, 80)
            );

            if (confirmedEffectType != 0)
            {
                AddVisualEffectAt(state, confirmedEffectType, monsterDamageX, monsterDamageY);
            }
        }

        Log(
            "[MONSTER_HP] monsterId=" +
            std::to_string(hpPacket.monsterId) +
            ", hp=" +
            std::to_string(hpPacket.hp) +
            "/" +
            std::to_string(hpPacket.maxHp)
        );

        break;
    }

    case S2C_BATTLE_MESSAGE:
    {
        if (packetSize != sizeof(S2C_BattleMessage))
        {
            Log("[RECV] Invalid S2C_BATTLE_MESSAGE size.");
            return;
        }

        S2C_BattleMessage messagePacket{};
        std::memcpy(&messagePacket, packet, sizeof(messagePacket));

        const std::string message = MakeSafeString(
            messagePacket.message,
            static_cast<int>(sizeof(messagePacket.message))
        );

        Log("[BATTLE] " + message);

        AddClientMessage(state, "[BATTLE] " + message);

        break;
    }

    case S2C_PLAYER_STATUS:
    {
        if (packetSize != sizeof(S2C_PlayerStatus))
        {
            Log("[RECV] Invalid S2C_PLAYER_STATUS size.");
            return;
        }

        S2C_PlayerStatus status{};
        std::memcpy(&status, packet, sizeof(status));

        bool shouldShowRespawnMessage = false;
        bool showPlayerDamage = false;
        bool showExpGain = false;
        int playerDamage = 0;
        int floatingX = 0;
        int floatingY = 0;

        {
            std::lock_guard<std::mutex> lock(state.lock);

            if (state.avatarReceived && status.hp < state.hp)
            {
                playerDamage = state.hp - status.hp;
                floatingX = state.myX;
                floatingY = state.myY;

                if (playerDamage > 0)
                {
                    showPlayerDamage = true;
                }
            }

            if (status.expGained > 0)
            {
                floatingX = state.myX;
                floatingY = state.myY;
                showExpGain = true;
            }

            if (status.hp <= 0)
            {
                state.waitingRespawnNotice = true;
            }
            else if (state.waitingRespawnNotice && state.hp <= 0)
            {
                shouldShowRespawnMessage = true;
                state.waitingRespawnNotice = false;
            }

            state.hp = status.hp;
            state.maxHp = status.maxHp;
            state.mp = status.mp;
            state.maxMp = status.maxMp;

            state.level = status.level;
            state.exp = status.exp;
            state.requiredExp = status.requiredExp;
            state.statPoint = status.statPoint;
            state.skillPoint = status.skillPoint;
            state.gold = status.gold;

            state.str = status.str;
            state.intStat = status.intStat;
            state.dex = status.dex;
            state.luk = status.luk;
        }

        if (showPlayerDamage)
        {
            AddFloatingText(
                state,
                floatingX,
                floatingY,
                "-" + std::to_string(playerDamage),
                sf::Color(255, 80, 80)
            );
        }

        if (showExpGain)
        {
            AddFloatingText(
                state,
                floatingX,
                floatingY,
                "+" + std::to_string(status.expGained) + " EXP",
                sf::Color(240, 210, 80)
            );
        }

        if (shouldShowRespawnMessage)
        {
            AddClientMessage(
                state,
                "[BATTLE] You respawned at the start village."
            );

            Log("[BATTLE] You respawned at the start village.");
        }

        Log(
            "[STATUS] hp=" +
            std::to_string(status.hp) +
            "/" +
            std::to_string(status.maxHp) +
            ", mp=" +
            std::to_string(status.mp) +
            "/" +
            std::to_string(status.maxMp) +
            ", level=" +
            std::to_string(status.level) +
            ", exp=" +
            std::to_string(status.exp) +
            "/" +
            std::to_string(status.requiredExp) +
            ", statPoint=" +
            std::to_string(status.statPoint) +
            ", skillPoint=" +
            std::to_string(status.skillPoint) +
            ", gold=" +
            std::to_string(status.gold) +
            ", STR=" +
            std::to_string(status.str) +
            ", INT=" +
            std::to_string(status.intStat) +
            ", DEX=" +
            std::to_string(status.dex) +
            ", LUK=" +
            std::to_string(status.luk) +
            ", expGained=" +
            std::to_string(status.expGained) +
            ", levelGained=" +
            std::to_string(status.levelGained)
        );

        break;
    }

    case S2C_SKILL_STATUS:
    {
        if (packetSize != sizeof(S2C_SkillStatus))
        {
            Log("[RECV] Invalid S2C_SKILL_STATUS size.");
            return;
        }

        S2C_SkillStatus skillStatus{};
        std::memcpy(&skillStatus, packet, sizeof(skillStatus));

        {
            std::lock_guard<std::mutex> lock(state.lock);

            state.skillPoint = skillStatus.skillPoint;

            for (int i = 0; i < MAX_SKILL_STATUS_COUNT; ++i)
            {
                state.skillLevels[i] = 0;
            }

            const int count =
                skillStatus.skillCount < MAX_SKILL_STATUS_COUNT
                ? skillStatus.skillCount
                : MAX_SKILL_STATUS_COUNT;

            for (int i = 0; i < count; ++i)
            {
                const int skillIndex =
                    GetSkillIndexById(skillStatus.skills[i].skillId);

                if (skillIndex < 0 || skillIndex >= MAX_SKILL_STATUS_COUNT)
                {
                    continue;
                }

                state.skillLevels[skillIndex] =
                    skillStatus.skills[i].level;
            }
        }

        Log(
            "[SKILL_STATUS] skillPoint=" +
            std::to_string(skillStatus.skillPoint)
        );

        break;
    }

    case S2C_ADD_NPC:
    {
        if (packetSize != sizeof(S2C_AddNpc))
        {
            Log("[RECV] Invalid S2C_ADD_NPC size.");
            return;
        }

        S2C_AddNpc add{};
        std::memcpy(&add, packet, sizeof(add));

        NpcView npc{};
        npc.npcId = add.npcId;
        npc.npcType = add.npcType;
        npc.name = MakeSafeString(add.name, static_cast<int>(sizeof(add.name)));
        npc.x = add.x;
        npc.y = add.y;

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.npcs[npc.npcId] = npc;
        }

        Log(
            "[ADD_NPC] npcId=" +
            std::to_string(npc.npcId) +
            ", name=" +
            npc.name +
            ", x=" +
            std::to_string(npc.x) +
            ", y=" +
            std::to_string(npc.y)
        );

        break;
    }

    case S2C_ADD_DROP:
    {
        if (packetSize != sizeof(S2C_AddDrop))
        {
            Log("[RECV] Invalid S2C_ADD_DROP size.");
            return;
        }

        S2C_AddDrop dropPacket{};
        std::memcpy(&dropPacket, packet, sizeof(dropPacket));

        DropView drop{};
        drop.dropId = dropPacket.dropId;
        drop.itemId = dropPacket.itemId;
        drop.itemCategory = dropPacket.itemCategory;
        drop.count = dropPacket.count;
        drop.x = dropPacket.x;
        drop.y = dropPacket.y;
        drop.itemName = MakeSafeString(
            dropPacket.itemName,
            static_cast<int>(sizeof(dropPacket.itemName))
        );

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.drops[drop.dropId] = drop;
        }

        Log(
            "[ADD_DROP] dropId=" +
            std::to_string(drop.dropId) +
            ", item=" +
            drop.itemName +
            ", count=" +
            std::to_string(drop.count) +
            ", x=" +
            std::to_string(drop.x) +
            ", y=" +
            std::to_string(drop.y)
        );

        break;
    }

    case S2C_REMOVE_DROP:
    {
        if (packetSize != sizeof(S2C_RemoveDrop))
        {
            Log("[RECV] Invalid S2C_REMOVE_DROP size.");
            return;
        }

        S2C_RemoveDrop removePacket{};
        std::memcpy(&removePacket, packet, sizeof(removePacket));

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.drops.erase(removePacket.dropId);
        }

        Log(
            "[REMOVE_DROP] dropId=" +
            std::to_string(removePacket.dropId)
        );

        break;
    }

    case S2C_INVENTORY_ITEM_UPDATE:
    {
        if (packetSize != sizeof(S2C_InventoryItemUpdate))
        {
            Log("[RECV] Invalid S2C_INVENTORY_ITEM_UPDATE size.");
            return;
        }

        S2C_InventoryItemUpdate updatePacket{};
        std::memcpy(&updatePacket, packet, sizeof(updatePacket));

        InventoryView item{};
        item.itemId = updatePacket.itemId;
        item.itemCategory = updatePacket.itemCategory;
        item.count = updatePacket.count;
        item.itemName = MakeSafeString(
            updatePacket.itemName,
            static_cast<int>(sizeof(updatePacket.itemName))
        );

        {
            std::lock_guard<std::mutex> lock(state.lock);

            if (item.count <= 0)
            {
                state.inventory.erase(item.itemId);

                if (state.selectedInventoryItemId == item.itemId)
                {
                    state.selectedInventoryItemId = 0;
                    state.selectedInventoryItemCategory = 0;
                    state.selectedInventoryItemName.clear();
                }
            }
            else
            {
                state.inventory[item.itemId] = item;
            }
        }

        Log(
            "[INVENTORY] item=" +
            item.itemName +
            ", count=" +
            std::to_string(item.count)
        );

        break;
    }

    case S2C_CHAT:
    {
        if (packetSize != sizeof(S2C_Chat))
        {
            Log("[RECV] Invalid S2C_CHAT size.");
            return;
        }

        S2C_Chat chat{};
        std::memcpy(&chat, packet, sizeof(chat));

        const std::string username = MakeSafeString(
            chat.username,
            static_cast<int>(sizeof(chat.username))
        );

        const std::string message = MakeSafeString(
            chat.message,
            static_cast<int>(sizeof(chat.message))
        );

        const std::string finalMessage =
            "[CHAT] " + username + ": " + message;

        Log(finalMessage);
        AddClientMessage(state, finalMessage);

        break;
    }


    case S2C_EQUIPMENT_STATUS:
    {
        if (packetSize != sizeof(S2C_EquipmentStatus))
        {
            Log("[RECV] Invalid S2C_EQUIPMENT_STATUS size.");
            return;
        }

        S2C_EquipmentStatus equipmentStatus{};
        std::memcpy(&equipmentStatus, packet, sizeof(equipmentStatus));

        const std::string weaponName = MakeSafeString(
            equipmentStatus.weaponName,
            static_cast<int>(sizeof(equipmentStatus.weaponName))
        );

        const std::string armorName = MakeSafeString(
            equipmentStatus.armorName,
            static_cast<int>(sizeof(equipmentStatus.armorName))
        );

        const std::string accessoryName = MakeSafeString(
            equipmentStatus.accessoryName,
            static_cast<int>(sizeof(equipmentStatus.accessoryName))
        );

        {
            std::lock_guard<std::mutex> lock(state.lock);

            state.equippedWeaponItemId = equipmentStatus.weaponItemId;
            state.equippedWeaponName = weaponName;

            state.equippedArmorItemId = equipmentStatus.armorItemId;
            state.equippedArmorName = armorName;

            state.equippedAccessoryItemId = equipmentStatus.accessoryItemId;
            state.equippedAccessoryName = accessoryName;

            state.equipmentStrBonus = equipmentStatus.strBonus;
            state.equipmentIntBonus = equipmentStatus.intBonus;
            state.equipmentDexBonus = equipmentStatus.dexBonus;
            state.equipmentLukBonus = equipmentStatus.lukBonus;

            state.equipmentPhysicalAttackBonus =
                equipmentStatus.physicalAttackBonus;

            state.equipmentPhysicalDefenseBonus =
                equipmentStatus.physicalDefenseBonus;

            state.equipmentMagicAttackBonus =
                equipmentStatus.magicAttackBonus;

            state.equipmentMagicDefenseBonus =
                equipmentStatus.magicDefenseBonus;
        }

        Log(
            "[EQUIPMENT] Weapon=" +
            weaponName +
            ", Armor=" +
            armorName +
            ", Accessory=" +
            accessoryName
        );

        break;
    }

    case S2C_QUICK_SLOT_STATUS:
    {
        if (packetSize != sizeof(S2C_QuickSlotStatus))
        {
            Log("[RECV] Invalid S2C_QUICK_SLOT_STATUS size.");
            return;
        }

        S2C_QuickSlotStatus quickSlotStatus{};
        std::memcpy(&quickSlotStatus, packet, sizeof(quickSlotStatus));

        {
            std::lock_guard<std::mutex> lock(state.lock);

            for (int i = 0; i < QUICK_SLOT_COUNT; ++i)
            {
                state.quickSlots[i] = QuickSlotView{};
            }

            const int slotCount =
                quickSlotStatus.slotCount < MAX_QUICK_SLOT_COUNT
                ? quickSlotStatus.slotCount
                : MAX_QUICK_SLOT_COUNT;

            for (int i = 0; i < slotCount; ++i)
            {
                const QuickSlotStatusEntry& slot = quickSlotStatus.slots[i];

                if (slot.slotIndex < 0 || slot.slotIndex >= QUICK_SLOT_COUNT)
                {
                    continue;
                }

                state.quickSlots[slot.slotIndex].slotType = slot.slotType;
                state.quickSlots[slot.slotIndex].targetId = slot.targetId;
                state.quickSlots[slot.slotIndex].name = MakeSafeString(
                    slot.name,
                    static_cast<int>(sizeof(slot.name))
                );
            }
        }

        Log("[QUICK_SLOT] Status updated.");

        break;
    }

    default:
        Log(
            "[RECV] Unknown packet. type=" +
            std::to_string(static_cast<int>(type)) +
            ", size=" +
            std::to_string(packetSize)
        );
        break;
    }
}

void ReceiveThreadMain(SOCKET socketHandle, ClientState& state)
{
    std::vector<char> packetBuffer;
    packetBuffer.reserve(8192);

    char recvBuffer[4096];

    while (g_running)
    {
        const int received = recv(
            socketHandle,
            recvBuffer,
            static_cast<int>(sizeof(recvBuffer)),
            0
        );

        if (received == 0)
        {
            Log("[CLIENT] Server disconnected.");
            g_running = false;
            g_loginCv.notify_all();
            break;
        }

        if (received == SOCKET_ERROR)
        {
            Log("[CLIENT] recv failed.");
            g_running = false;
            g_loginCv.notify_all();
            break;
        }

        packetBuffer.insert(
            packetBuffer.end(),
            recvBuffer,
            recvBuffer + received
        );

        while (!packetBuffer.empty())
        {
            const unsigned char packetSize =
                static_cast<unsigned char>(packetBuffer[0]);

            if (packetSize == 0)
            {
                packetBuffer.erase(packetBuffer.begin());
                continue;
            }

            if (packetBuffer.size() < packetSize)
            {
                break;
            }

            HandlePacket(state, packetBuffer.data(), packetSize);

            packetBuffer.erase(
                packetBuffer.begin(),
                packetBuffer.begin() + packetSize
            );
        }
    }
}

sf::Color GetMonsterColor(int monsterType)
{
    switch (monsterType)
    {
    case MONSTER_SLIME:
        return sf::Color(80, 220, 120);

    case MONSTER_GOBLIN:
        return sf::Color(180, 120, 60);

    case MONSTER_WOLF:
        return sf::Color(160, 160, 160);

    case MONSTER_ORC:
        return sf::Color(80, 150, 80);

    case MONSTER_SKELETON:
        return sf::Color(230, 230, 210);

    case MONSTER_DARK_MAGE:
        return sf::Color(150, 80, 220);

    default:
        return sf::Color::Red;
    }
}

sf::Color GetDropColor(int itemCategory)
{
    if (itemCategory == ITEM_CATEGORY_EQUIPMENT)
    {
        return sf::Color(240, 210, 60);
    }

    if (itemCategory == ITEM_CATEGORY_CONSUMABLE)
    {
        return sf::Color(220, 80, 80);
    }


    return sf::Color(170, 170, 170);
}

long long GetClientNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

int GetRegionType(int worldX, int worldY)
{
    if (worldX < 250 && worldY < 250)
    {
        return REGION_START_VILLAGE;
    }

    if (worldX >= 1600 && worldY >= 1450)
    {
        return REGION_CASTLE_GATE;
    }

    if (worldX >= 1300 && worldY >= 1000)
    {
        return REGION_ANCIENT_RUINS;
    }

    if (worldX < 700 && worldY < 700)
    {
        return REGION_GREEN_FOREST;
    }

    if (worldX >= 700 && worldX < 1300 && worldY < 900)
    {
        return REGION_ORC_CAMP;
    }

    return REGION_OPEN_FIELD;
}


int HashTile(int x, int y)
{
    int value = x * 73856093 ^ y * 19349663;

    if (value < 0)
    {
        value = -value;
    }

    return value;
}

bool IsRectTile(int x, int y, int left, int top, int right, int bottom)
{
    return x >= left && x <= right && y >= top && y <= bottom;
}

bool IsClientBlockedTile(int x, int y)
{
    if (x < 0 || y < 0 || x >= WORLD_WIDTH || y >= WORLD_HEIGHT)
    {
        return true;
    }

    if (IsRectTile(x, y, 105, 98, 105, 102))
    {
        return true;
    }

    if (IsRectTile(x, y, 400, 100, 430, 650))
    {
        return true;
    }

    if (IsRectTile(x, y, 700, 300, 1200, 330))
    {
        return true;
    }

    if (IsRectTile(x, y, 1000, 700, 1030, 1300))
    {
        return true;
    }

    if (IsRectTile(x, y, 1600, 1500, 1900, 1530) &&
        !IsRectTile(x, y, 1730, 1500, 1770, 1530))
    {
        return true;
    }

    if (IsRectTile(x, y, 1600, 1530, 1630, 1900))
    {
        return true;
    }

    if (IsRectTile(x, y, 1870, 1530, 1900, 1900))
    {
        return true;
    }

    return false;
}

int GetObstacleObjectType(int worldX, int worldY)
{
    const int regionType = GetRegionType(worldX, worldY);
    const int hash = HashTile(worldX, worldY);

    switch (regionType)
    {
    case REGION_START_VILLAGE:
        return (hash % 4 == 0) ? OBJECT_HOUSE : OBJECT_FENCE;

    case REGION_GREEN_FOREST:
        return (hash % 5 == 0) ? OBJECT_ROCK : OBJECT_TREE;

    case REGION_ORC_CAMP:
        return (hash % 4 == 0) ? OBJECT_TENT : OBJECT_FENCE;

    case REGION_ANCIENT_RUINS:
        return (hash % 3 == 0) ? OBJECT_ROCK : OBJECT_RUIN_PILLAR;

    case REGION_CASTLE_GATE:
        return OBJECT_CASTLE_WALL;

    default:
        return (hash % 3 == 0) ? OBJECT_ROCK : OBJECT_TREE;
    }
}

bool ShouldDrawDecoration(int worldX, int worldY)
{
    if (IsClientBlockedTile(worldX, worldY))
    {
        return false;
    }

    const int regionType = GetRegionType(worldX, worldY);
    const int hash = HashTile(worldX, worldY);

    switch (regionType)
    {
    case REGION_START_VILLAGE:
        return hash % 97 == 0;

    case REGION_GREEN_FOREST:
        return hash % 17 == 0;

    case REGION_ORC_CAMP:
        return hash % 61 == 0;

    case REGION_ANCIENT_RUINS:
        return hash % 53 == 0;

    case REGION_CASTLE_GATE:
        return hash % 79 == 0;

    default:
        return hash % 41 == 0;
    }
}

int GetDecorationObjectType(int worldX, int worldY)
{
    const int regionType = GetRegionType(worldX, worldY);
    const int hash = HashTile(worldX, worldY);

    switch (regionType)
    {
    case REGION_START_VILLAGE:
        return (hash % 2 == 0) ? OBJECT_SIGN : OBJECT_GRASS;

    case REGION_GREEN_FOREST:
        return (hash % 3 == 0) ? OBJECT_ROCK : OBJECT_GRASS;

    case REGION_ORC_CAMP:
        return (hash % 2 == 0) ? OBJECT_CAMPFIRE : OBJECT_TENT;

    case REGION_ANCIENT_RUINS:
        return (hash % 2 == 0) ? OBJECT_RUIN_PILLAR : OBJECT_ROCK;

    case REGION_CASTLE_GATE:
        return (hash % 2 == 0) ? OBJECT_SIGN : OBJECT_FENCE;

    default:
        return (hash % 2 == 0) ? OBJECT_GRASS : OBJECT_ROCK;
    }
}

std::string GetRegionName(int worldX, int worldY)
{
    switch (GetRegionType(worldX, worldY))
    {
    case REGION_START_VILLAGE:
        return "Start Village";

    case REGION_GREEN_FOREST:
        return "Green Forest";

    case REGION_ORC_CAMP:
        return "Orc Camp";

    case REGION_ANCIENT_RUINS:
        return "Ancient Ruins";

    case REGION_CASTLE_GATE:
        return "Castle Gate";

    default:
        return "Open Field";
    }
}

sf::Color GetRegionTileColor(int worldX, int worldY)
{
    const std::string region = GetRegionName(worldX, worldY);

    if (region == "Start Village")
    {
        return sf::Color(65, 60, 50);
    }

    if (region == "Green Forest")
    {
        return sf::Color(40, 80, 45);
    }

    if (region == "Orc Camp")
    {
        return sf::Color(80, 65, 45);
    }

    if (region == "Ancient Ruins")
    {
        return sf::Color(55, 50, 70);
    }

    if (region == "Castle Gate")
    {
        return sf::Color(70, 70, 75);
    }

    return sf::Color(45, 65, 50);
}

sf::Color GetNpcColor(int npcType)
{
    switch (npcType)
    {
    case NPC_TYPE_MERCHANT:
        return sf::Color(240, 190, 70);

    case NPC_TYPE_BLACKSMITH:
        return sf::Color(150, 150, 160);

    case NPC_TYPE_HEALER:
        return sf::Color(180, 240, 220);

    case NPC_TYPE_SKILL_TRAINER:
        return sf::Color(180, 120, 240);

    case NPC_TYPE_GUARD:
        return sf::Color(120, 150, 210);

    case NPC_TYPE_GUIDE:
        return sf::Color(100, 220, 170);

    default:
        return sf::Color(240, 200, 80);
    }
}

std::string GetNpcRoleText(int npcType)
{
    switch (npcType)
    {
    case NPC_TYPE_MERCHANT:
        return "Shop";

    case NPC_TYPE_BLACKSMITH:
        return "Equip";

    case NPC_TYPE_HEALER:
        return "Heal";

    case NPC_TYPE_SKILL_TRAINER:
        return "Skill";

    case NPC_TYPE_GUARD:
        return "Guard";

    case NPC_TYPE_GUIDE:
        return "Guide";

    default:
        return "NPC";
    }
}

std::string GetNpcInteractionText(const NpcView& npc)
{
    switch (npc.npcType)
    {
    case NPC_TYPE_MERCHANT:
        return "F: Shop - " + npc.name;

    case NPC_TYPE_BLACKSMITH:
        return "F: Equipment - " + npc.name;

    case NPC_TYPE_HEALER:
        return "F: Heal - " + npc.name;

    case NPC_TYPE_SKILL_TRAINER:
        return "F: Skill - " + npc.name;

    case NPC_TYPE_GUARD:
        return "F: Talk - " + npc.name;

    case NPC_TYPE_GUIDE:
        return "F: Map Guide - " + npc.name;

    default:
        return "F: Talk - " + npc.name;
    }
}

struct TextureAssets
{
    bool loaded = false;
    sf::Texture player;
    sf::Texture otherPlayer;
    std::unordered_map<int, sf::Texture> monsterTextures;
    std::unordered_map<int, sf::Texture> npcTextures;
    std::unordered_map<int, sf::Texture> itemTextures;
    std::unordered_map<int, sf::Texture> regionTextures;
    std::unordered_map<int, sf::Texture> effectTextures;
    std::unordered_map<int, sf::Texture> objectTextures;
};

bool LoadTextureFromCandidates(sf::Texture& texture, const std::vector<std::string>& paths)
{
    for (const std::string& path : paths)
    {
        if (texture.loadFromFile(path))
        {
            texture.setSmooth(false);
            return true;
        }
    }

    return false;
}

bool LoadAssetTexture(sf::Texture& texture, const std::string& relativePath)
{
    std::vector<std::string> paths;
    paths.push_back("Assets/" + relativePath);
    paths.push_back("Client/Assets/" + relativePath);
    paths.push_back("../Client/Assets/" + relativePath);

    return LoadTextureFromCandidates(texture, paths);
}

TextureAssets& GetTextureAssets()
{
    static TextureAssets assets;

    if (assets.loaded)
    {
        return assets;
    }

    assets.loaded = true;

    LoadAssetTexture(assets.player, "player.png");
    LoadAssetTexture(assets.otherPlayer, "other_player.png");

    LoadAssetTexture(assets.monsterTextures[MONSTER_SLIME], "monsters/slime.png");
    LoadAssetTexture(assets.monsterTextures[MONSTER_GOBLIN], "monsters/goblin.png");
    LoadAssetTexture(assets.monsterTextures[MONSTER_WOLF], "monsters/wolf.png");
    LoadAssetTexture(assets.monsterTextures[MONSTER_ORC], "monsters/orc.png");
    LoadAssetTexture(assets.monsterTextures[MONSTER_SKELETON], "monsters/skeleton.png");
    LoadAssetTexture(assets.monsterTextures[MONSTER_DARK_MAGE], "monsters/darkmage.png");

    LoadAssetTexture(assets.npcTextures[NPC_TYPE_MERCHANT], "npcs/merchant.png");
    LoadAssetTexture(assets.npcTextures[NPC_TYPE_BLACKSMITH], "npcs/blacksmith.png");
    LoadAssetTexture(assets.npcTextures[NPC_TYPE_HEALER], "npcs/healer.png");
    LoadAssetTexture(assets.npcTextures[NPC_TYPE_SKILL_TRAINER], "npcs/trainer.png");
    LoadAssetTexture(assets.npcTextures[NPC_TYPE_GUARD], "npcs/guard.png");
    LoadAssetTexture(assets.npcTextures[NPC_TYPE_GUIDE], "npcs/guide.png");

    LoadAssetTexture(assets.itemTextures[ITEM_HP_POTION], "items/hp_potion.png");
    LoadAssetTexture(assets.itemTextures[ITEM_MP_POTION], "items/mp_potion.png");
    LoadAssetTexture(assets.itemTextures[ITEM_HIGH_HP_POTION], "items/high_hp_potion.png");
    LoadAssetTexture(assets.itemTextures[ITEM_HIGH_MP_POTION], "items/high_mp_potion.png");
    LoadAssetTexture(assets.itemTextures[ITEM_RETURN_SCROLL], "items/return_scroll.png");
    LoadAssetTexture(assets.itemTextures[ITEM_IRON_SWORD], "items/iron_sword.png");
    LoadAssetTexture(assets.itemTextures[ITEM_STEEL_SWORD], "items/steel_sword.png");
    LoadAssetTexture(assets.itemTextures[ITEM_FLAME_SWORD], "items/flame_sword.png");
    LoadAssetTexture(assets.itemTextures[ITEM_MAGE_STAFF], "items/mage_staff.png");
    LoadAssetTexture(assets.itemTextures[ITEM_ICE_WAND], "items/ice_wand.png");
    LoadAssetTexture(assets.itemTextures[ITEM_HUNTER_BOW], "items/hunter_bow.png");
    LoadAssetTexture(assets.itemTextures[ITEM_IRON_ARMOR], "items/iron_armor.png");
    LoadAssetTexture(assets.itemTextures[ITEM_STEEL_ARMOR], "items/steel_armor.png");
    LoadAssetTexture(assets.itemTextures[ITEM_MAGIC_ROBE], "items/magic_robe.png");
    LoadAssetTexture(assets.itemTextures[ITEM_LEATHER_ARMOR], "items/leather_armor.png");
    LoadAssetTexture(assets.itemTextures[ITEM_MAGIC_RING], "items/magic_ring.png");
    LoadAssetTexture(assets.itemTextures[ITEM_WARRIOR_RING], "items/warrior_ring.png");
    LoadAssetTexture(assets.itemTextures[ITEM_LUCKY_NECKLACE], "items/lucky_necklace.png");
    LoadAssetTexture(assets.itemTextures[ITEM_SPEED_CHARM], "items/speed_charm.png");

    LoadAssetTexture(assets.regionTextures[REGION_START_VILLAGE], "tiles/village.png");
    LoadAssetTexture(assets.regionTextures[REGION_GREEN_FOREST], "tiles/forest.png");
    LoadAssetTexture(assets.regionTextures[REGION_ORC_CAMP], "tiles/camp.png");
    LoadAssetTexture(assets.regionTextures[REGION_ANCIENT_RUINS], "tiles/ruins.png");
    LoadAssetTexture(assets.regionTextures[REGION_CASTLE_GATE], "tiles/castle.png");
    LoadAssetTexture(assets.regionTextures[REGION_OPEN_FIELD], "tiles/field.png");

    LoadAssetTexture(assets.effectTextures[EFFECT_ATTACK], "effects/normal_attack.png");
    LoadAssetTexture(assets.effectTextures[EFFECT_POWER_STRIKE], "effects/power_strike.png");
    LoadAssetTexture(assets.effectTextures[EFFECT_HEAVY_SLASH], "effects/heavy_slash.png");
    LoadAssetTexture(assets.effectTextures[EFFECT_WHIRLWIND], "effects/whirlwind.png");
    LoadAssetTexture(assets.effectTextures[EFFECT_FIREBALL], "effects/fireball.png");
    LoadAssetTexture(assets.effectTextures[EFFECT_MANA_BURST], "effects/mana_burst.png");
    LoadAssetTexture(assets.effectTextures[EFFECT_ICE_BOLT], "effects/ice_bolt.png");

    LoadAssetTexture(assets.objectTextures[OBJECT_TREE], "objects/tree.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_ROCK], "objects/rock.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_FENCE], "objects/fence.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_HOUSE], "objects/house.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_TENT], "objects/tent.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_RUIN_PILLAR], "objects/ruin_pillar.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_CASTLE_WALL], "objects/castle_wall.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_GRASS], "objects/grass.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_CAMPFIRE], "objects/campfire.png");
    LoadAssetTexture(assets.objectTextures[OBJECT_SIGN], "objects/sign.png");

    return assets;
}

const sf::Texture* FindTexture(std::unordered_map<int, sf::Texture>& textures, int key)
{
    auto iter = textures.find(key);

    if (iter == textures.end())
    {
        return nullptr;
    }

    const sf::Vector2u size = iter->second.getSize();

    if (size.x == 0 || size.y == 0)
    {
        return nullptr;
    }

    return &iter->second;
}

void DrawTextureSprite(sf::RenderWindow& window, const sf::Texture& texture, const sf::Vector2f& position, float size)
{
    sf::Sprite sprite;
    sprite.setTexture(texture);

    const sf::Vector2u textureSize = texture.getSize();

    if (textureSize.x > 0 && textureSize.y > 0)
    {
        sprite.setScale(size / textureSize.x, size / textureSize.y);
    }

    sprite.setPosition(position.x, position.y);
    window.draw(sprite);
}


void AddVisualEffectAt(ClientState& state, int effectType, int worldX, int worldY)
{
    std::lock_guard<std::mutex> lock(state.lock);

    VisualEffect effect{};
    effect.effectType = effectType;
    effect.x = worldX;
    effect.y = worldY;
    effect.startTimeMs = GetClientNowMs();

    switch (effectType)
    {
    case EFFECT_ATTACK:
        effect.durationMs = 300;
        break;

    case EFFECT_POWER_STRIKE:
        effect.durationMs = 420;
        break;

    case EFFECT_HEAVY_SLASH:
        effect.durationMs = 620;
        break;

    case EFFECT_WHIRLWIND:
        effect.durationMs = 760;
        break;

    case EFFECT_FIREBALL:
    case EFFECT_ICE_BOLT:
        effect.durationMs = 650;
        break;

    case EFFECT_MANA_BURST:
        effect.durationMs = 780;
        break;

    default:
        effect.durationMs = 500;
        break;
    }

    state.effects.push_back(effect);
}

bool FindNearestMonsterPosition(ClientState& state, int maxDistance, int& outX, int& outY)
{
    std::lock_guard<std::mutex> lock(state.lock);

    bool found = false;
    int bestDistance = 999999;

    for (const auto& [monsterId, monster] : state.monsters)
    {
        int dx = state.myX - monster.x;
        int dy = state.myY - monster.y;

        if (dx < 0)
        {
            dx = -dx;
        }

        if (dy < 0)
        {
            dy = -dy;
        }

        const int distance = dx + dy;

        if (distance <= maxDistance && distance < bestDistance)
        {
            found = true;
            bestDistance = distance;
            outX = monster.x;
            outY = monster.y;
        }
    }

    return found;
}

void GetMyPosition(ClientState& state, int& outX, int& outY)
{
    std::lock_guard<std::mutex> lock(state.lock);
    outX = state.myX;
    outY = state.myY;
}

void SetPendingEffect(ClientState& state, int effectType)
{
    std::lock_guard<std::mutex> lock(state.lock);
    state.pendingEffectType = effectType;
    state.pendingEffectExpireTimeMs = GetClientNowMs() + 900;
}

void AddFloatingText(
    ClientState& state,
    int worldX,
    int worldY,
    const std::string& text,
    const sf::Color& color
)
{
    std::lock_guard<std::mutex> lock(state.lock);

    FloatingText floatingText{};
    floatingText.text = text;
    floatingText.x = worldX;
    floatingText.y = worldY;
    floatingText.color = color;
    floatingText.startTimeMs = GetClientNowMs();
    floatingText.durationMs = 850;

    state.floatingTexts.push_back(floatingText);
}

void AddVisualEffect(ClientState& state, int effectType)
{
    int worldX = 0;
    int worldY = 0;
    GetMyPosition(state, worldX, worldY);
    AddVisualEffectAt(state, effectType, worldX, worldY);
}

void StartRageVisual(ClientState& state)
{
    std::lock_guard<std::mutex> lock(state.lock);
    state.rageVisualEndTimeMs = GetClientNowMs() + 10000;
}

int GetEffectTypeForSkill(int skillId)
{
    switch (skillId)
    {
    case SKILL_POWER_STRIKE:
        return EFFECT_POWER_STRIKE;

    case SKILL_HEAVY_SLASH:
        return EFFECT_HEAVY_SLASH;

    case SKILL_WHIRLWIND:
        return EFFECT_WHIRLWIND;

    case SKILL_FIREBALL:
        return EFFECT_FIREBALL;

    case SKILL_MANA_BURST:
        return EFFECT_MANA_BURST;

    case SKILL_ICE_BOLT:
        return EFFECT_ICE_BOLT;

    default:
        return EFFECT_ATTACK;
    }
}

int GetEffectTargetRangeForSkill(int skillId)
{
    switch (skillId)
    {
    case SKILL_POWER_STRIKE:
    case SKILL_HEAVY_SLASH:
        return 3;

    case SKILL_FIREBALL:
    case SKILL_ICE_BOLT:
        return 7;

    case SKILL_MANA_BURST:
        return 4;

    case SKILL_WHIRLWIND:
        return 0;

    default:
        return 2;
    }
}

bool SkillEffectUsesSelfPosition(int skillId)
{
    return skillId == SKILL_WHIRLWIND;
}

sf::Color GetEffectColor(int effectType)
{
    switch (effectType)
    {
    case EFFECT_FIREBALL:
        return sf::Color(255, 100, 30, 190);

    case EFFECT_ICE_BOLT:
        return sf::Color(120, 210, 255, 190);

    case EFFECT_MANA_BURST:
        return sf::Color(220, 120, 255, 210);

    case EFFECT_RAGE:
        return sf::Color(255, 70, 20, 210);

    case EFFECT_POWER_STRIKE:
        return sf::Color(255, 245, 120, 190);

    case EFFECT_HEAVY_SLASH:
        return sf::Color(255, 190, 80, 210);

    case EFFECT_WHIRLWIND:
        return sf::Color(150, 255, 180, 190);

    default:
        return sf::Color(255, 255, 255, 170);
    }
}

float GetEffectBaseSize(int effectType)
{
    switch (effectType)
    {
    case EFFECT_ATTACK:
        return TILE_SIZE * 0.95f;

    case EFFECT_POWER_STRIKE:
        return TILE_SIZE * 1.35f;

    case EFFECT_HEAVY_SLASH:
        return TILE_SIZE * 1.85f;

    case EFFECT_WHIRLWIND:
        return TILE_SIZE * 2.15f;

    case EFFECT_FIREBALL:
    case EFFECT_ICE_BOLT:
        return TILE_SIZE * 1.45f;

    case EFFECT_MANA_BURST:
        return TILE_SIZE * 2.35f;

    case EFFECT_RAGE:
        return TILE_SIZE * 1.85f;

    default:
        return TILE_SIZE * 1.0f;
    }
}

float GetEffectRotation(int effectType, float progress)
{
    switch (effectType)
    {
    case EFFECT_POWER_STRIKE:
        return -35.0f;

    case EFFECT_HEAVY_SLASH:
        return 35.0f;

    case EFFECT_WHIRLWIND:
        return progress * 900.0f;

    case EFFECT_FIREBALL:
        return progress * 180.0f;

    case EFFECT_ICE_BOLT:
        return -20.0f;

    case EFFECT_MANA_BURST:
        return progress * 240.0f;

    default:
        return progress * 35.0f;
    }
}

void DrawSpriteEffect(
    sf::RenderWindow& window,
    const sf::Texture& texture,
    const sf::Vector2f& center,
    int effectType,
    float progress
)
{
    const sf::Vector2u textureSize = texture.getSize();

    if (textureSize.x == 0 || textureSize.y == 0)
    {
        return;
    }

    sf::Sprite sprite(texture);
    sprite.setOrigin(
        static_cast<float>(textureSize.x) * 0.5f,
        static_cast<float>(textureSize.y) * 0.5f
    );

    const float pulse = 0.75f + progress * 0.55f;
    const float size = GetEffectBaseSize(effectType) * pulse;
    const float largestSide = static_cast<float>(std::max(textureSize.x, textureSize.y));
    const float scale = size / std::max(1.0f, largestSide);

    sprite.setScale(scale, scale);
    sprite.setRotation(GetEffectRotation(effectType, progress));
    sprite.setPosition(center);

    const sf::Uint8 alpha = static_cast<sf::Uint8>(255.0f * (1.0f - progress * 0.35f));
    sprite.setColor(sf::Color(255, 255, 255, alpha));

    window.draw(sprite);
}

void DrawFallbackEffect(
    sf::RenderWindow& window,
    const sf::Vector2f& center,
    int effectType,
    float progress
)
{
    const sf::Color color = GetEffectColor(effectType);
    const float baseSize = GetEffectBaseSize(effectType);

    if (effectType == EFFECT_POWER_STRIKE ||
        effectType == EFFECT_HEAVY_SLASH ||
        effectType == EFFECT_ATTACK)
    {
        sf::RectangleShape slash;
        slash.setSize(sf::Vector2f(baseSize * (0.9f + progress * 0.35f), 7.0f));
        slash.setOrigin(slash.getSize().x * 0.5f, slash.getSize().y * 0.5f);
        slash.setPosition(center);
        slash.setRotation(GetEffectRotation(effectType, progress));
        slash.setFillColor(color);
        window.draw(slash);
        return;
    }

    if (effectType == EFFECT_WHIRLWIND)
    {
        for (int i = 0; i < 3; ++i)
        {
            sf::CircleShape arc;
            const float radius = baseSize * (0.25f + progress * 0.18f + i * 0.08f);
            arc.setRadius(radius);
            arc.setOrigin(radius, radius);
            arc.setPosition(center);
            arc.setFillColor(sf::Color(0, 0, 0, 0));
            arc.setOutlineThickness(4.0f);
            arc.setOutlineColor(color);
            arc.setRotation(progress * 720.0f + i * 120.0f);
            window.draw(arc);
        }
        return;
    }

    sf::CircleShape wave;
    const float radius = baseSize * (0.22f + progress * 0.42f);
    wave.setRadius(radius);
    wave.setOrigin(radius, radius);
    wave.setPosition(center);
    wave.setFillColor(sf::Color(0, 0, 0, 0));
    wave.setOutlineThickness(4.0f);
    wave.setOutlineColor(color);
    window.draw(wave);
}

void DrawRageAura(
    sf::RenderWindow& window,
    const sf::Vector2f& screenPos,
    long long nowMs
)
{
    const float pulse =
        0.5f +
        0.5f * std::sin(static_cast<float>(nowMs % 600) / 600.0f * 6.2831853f);

    const sf::Vector2f center(
        screenPos.x + TILE_SIZE * 0.5f,
        screenPos.y + TILE_SIZE * 0.58f
    );

    sf::CircleShape aura;
    const float radius = TILE_SIZE * (0.55f + pulse * 0.12f);
    aura.setRadius(radius);
    aura.setOrigin(radius, radius);
    aura.setPosition(center);
    aura.setFillColor(sf::Color(255, 55, 20, 45));
    aura.setOutlineThickness(3.0f);
    aura.setOutlineColor(sf::Color(255, 105, 30, 170));
    window.draw(aura);

    for (int i = 0; i < 5; ++i)
    {
        const float offsetX = static_cast<float>((i - 2) * 7);
        const float height = TILE_SIZE * (0.55f + 0.08f * static_cast<float>((nowMs / 100 + i) % 3));

        sf::ConvexShape flame;
        flame.setPointCount(3);
        flame.setPoint(0, sf::Vector2f(center.x + offsetX, center.y - height));
        flame.setPoint(1, sf::Vector2f(center.x + offsetX - 6.0f, center.y - 4.0f));
        flame.setPoint(2, sf::Vector2f(center.x + offsetX + 6.0f, center.y - 4.0f));
        flame.setFillColor(sf::Color(255, 95, 25, 105));
        window.draw(flame);
    }
}

void DrawVisualEffect(
    sf::RenderWindow& window,
    TextureAssets& assets,
    const VisualEffect& effect,
    const sf::Vector2f& screenPos,
    float progress
)
{
    const sf::Vector2f center(
        screenPos.x + TILE_SIZE * 0.5f,
        screenPos.y + TILE_SIZE * 0.5f
    );

    const sf::Texture* effectTexture = FindTexture(assets.effectTextures, effect.effectType);

    if (effectTexture != nullptr)
    {
        DrawSpriteEffect(window, *effectTexture, center, effect.effectType, progress);
    }
    else
    {
        DrawFallbackEffect(window, center, effect.effectType, progress);
    }

    if (effect.effectType == EFFECT_FIREBALL ||
        effect.effectType == EFFECT_ICE_BOLT ||
        effect.effectType == EFFECT_MANA_BURST)
    {
        sf::CircleShape ring;
        const float ringRadius = TILE_SIZE * (0.25f + progress * 0.95f);
        ring.setRadius(ringRadius);
        ring.setOrigin(ringRadius, ringRadius);
        ring.setPosition(center);
        ring.setFillColor(sf::Color(0, 0, 0, 0));
        ring.setOutlineThickness(2.0f);
        ring.setOutlineColor(GetEffectColor(effect.effectType));
        window.draw(ring);
    }
}

std::string GetItemCategoryText(int itemCategory)
{
    if (itemCategory == ITEM_CATEGORY_EQUIPMENT)
    {
        return "EQUIP";
    }

    if (itemCategory == ITEM_CATEGORY_CONSUMABLE)
    {
        return "USE";
    }

    return "ETC";
}

std::string GetQuickSlotDisplayName(const std::string& itemName)
{
    if (itemName == "HP Potion")
    {
        return "HP";
    }

    if (itemName == "MP Potion")
    {
        return "MP";
    }

    if (itemName == "High HP Potion")
    {
        return "HiHP";
    }

    if (itemName == "High MP Potion")
    {
        return "HiMP";
    }

    if (itemName == "Return Scroll")
    {
        return "Town";
    }

    if (itemName.size() <= 6)
    {
        return itemName;
    }

    return itemName.substr(0, 6);
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

int GetSkillIdByIndex(int index)
{
    switch (index)
    {
    case 0:
        return SKILL_POWER_STRIKE;

    case 1:
        return SKILL_HEAVY_SLASH;

    case 2:
        return SKILL_WHIRLWIND;

    case 3:
        return SKILL_FIREBALL;

    case 4:
        return SKILL_MANA_BURST;

    case 5:
        return SKILL_ICE_BOLT;

    case 6:
        return SKILL_VITALITY_TRAINING;

    case 7:
        return SKILL_MANA_TRAINING;

    case 8:
        return SKILL_LUCKY_HUNTER;

    default:
        return SKILL_NONE;
    }
}

int GetSkillIndexById(int skillId)
{
    switch (skillId)
    {
    case SKILL_POWER_STRIKE:
        return 0;

    case SKILL_HEAVY_SLASH:
        return 1;

    case SKILL_WHIRLWIND:
        return 2;

    case SKILL_FIREBALL:
        return 3;

    case SKILL_MANA_BURST:
        return 4;

    case SKILL_ICE_BOLT:
        return 5;

    case SKILL_VITALITY_TRAINING:
        return 6;

    case SKILL_MANA_TRAINING:
        return 7;

    case SKILL_LUCKY_HUNTER:
        return 8;

    default:
        return -1;
    }
}

const SkillTreeNodeDefinition* GetSkillTreeNodes()
{
    static const SkillTreeNodeDefinition nodes[] =
    {
        { SKILL_POWER_STRIKE, "Power", "STR", 0, 0 },
        { SKILL_HEAVY_SLASH, "Heavy", "STR", 0, 1 },
        { SKILL_WHIRLWIND, "Whirlwind", "STR", 0, 2 },

        { SKILL_FIREBALL, "Fireball", "INT", 1, 0 },
        { SKILL_ICE_BOLT, "Ice Bolt", "INT", 1, 1 },
        { SKILL_MANA_BURST, "Mana Burst", "INT", 1, 2 },

        { SKILL_VITALITY_TRAINING, "Rage", "BUFF", 2, 0 },
        { SKILL_MANA_TRAINING, "Mana Train", "PASSIVE", 2, 1 },
        { SKILL_LUCKY_HUNTER, "Lucky", "PASSIVE", 2, 2 }
    };

    return nodes;
}

int GetSkillTreeNodeCount()
{
    return 9;
}

int GetRequiredPreviousSkillIdForUi(int skillId)
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

int GetSkillLevelFromArray(
    const std::array<int, MAX_SKILL_STATUS_COUNT>& skillLevels,
    int skillId
)
{
    const int skillIndex = GetSkillIndexById(skillId);

    if (skillIndex < 0 || skillIndex >= MAX_SKILL_STATUS_COUNT)
    {
        return 0;
    }

    return skillLevels[skillIndex];
}

bool IsSkillUnlockedForUi(
    const std::array<int, MAX_SKILL_STATUS_COUNT>& skillLevels,
    int skillId
)
{
    const int requiredSkillId = GetRequiredPreviousSkillIdForUi(skillId);

    if (requiredSkillId == SKILL_NONE)
    {
        return true;
    }

    return GetSkillLevelFromArray(skillLevels, requiredSkillId) >= SKILL_MAX_LEVEL;
}

std::string GetSkillNodeStatusText(
    const std::array<int, MAX_SKILL_STATUS_COUNT>& skillLevels,
    int skillId,
    int skillPoint
)
{
    const int currentLevel = GetSkillLevelFromArray(skillLevels, skillId);

    if (!IsSkillUnlockedForUi(skillLevels, skillId))
    {
        const int requiredSkillId = GetRequiredPreviousSkillIdForUi(skillId);
        return "Need " + GetSkillNameById(requiredSkillId) + " Lv.5";
    }

    if (currentLevel >= SKILL_MAX_LEVEL)
    {
        if (skillId == SKILL_VITALITY_TRAINING)
        {
            return "MASTER / U: Cast";
        }

        return "MASTER";
    }

    if (skillId == SKILL_VITALITY_TRAINING && currentLevel > 0)
    {
        return "U: Cast / + LvUp";
    }

    if (skillPoint <= 0)
    {
        return "Need SP";
    }

    if (currentLevel > 0)
    {
        return "Click + to LvUp";
    }

    return "Click + to learn";
}

sf::FloatRect GetSkillWindowRect()
{
    return sf::FloatRect(
        SKILL_TREE_X,
        SKILL_TREE_Y,
        SKILL_TREE_WIDTH,
        SKILL_TREE_HEIGHT
    );
}

sf::FloatRect GetSkillNodeRect(int column, int row)
{
    const float startX = SKILL_TREE_X + 25.0f;
    const float startY = SKILL_TREE_Y + 88.0f;

    return sf::FloatRect(
        startX + static_cast<float>(column) * (SKILL_NODE_WIDTH + SKILL_NODE_GAP_X),
        startY + static_cast<float>(row) * (SKILL_NODE_HEIGHT + SKILL_NODE_GAP_Y),
        SKILL_NODE_WIDTH,
        SKILL_NODE_HEIGHT
    );
}

sf::FloatRect GetSkillPlusButtonRect(int column, int row)
{
    const sf::FloatRect nodeRect = GetSkillNodeRect(column, row);

    return sf::FloatRect(
        nodeRect.left + nodeRect.width - SKILL_PLUS_SIZE - 7.0f,
        nodeRect.top + nodeRect.height - SKILL_PLUS_SIZE - 7.0f,
        SKILL_PLUS_SIZE,
        SKILL_PLUS_SIZE
    );
}

int GetSkillIdFromPlusButtonPosition(float mouseX, float mouseY)
{
    const SkillTreeNodeDefinition* nodes = GetSkillTreeNodes();

    for (int i = 0; i < GetSkillTreeNodeCount(); ++i)
    {
        const SkillTreeNodeDefinition& node = nodes[i];
        const sf::FloatRect plusRect = GetSkillPlusButtonRect(node.column, node.row);

        if (plusRect.contains(mouseX, mouseY))
        {
            return node.skillId;
        }
    }

    return SKILL_NONE;
}

std::vector<InventoryView> BuildInventoryList(
    const std::unordered_map<int, InventoryView>& inventory
)
{
    std::vector<InventoryView> items;
    items.reserve(inventory.size());

    for (const auto& [itemId, item] : inventory)
    {
        items.push_back(item);
    }

    std::sort(
        items.begin(),
        items.end(),
        [](const InventoryView& lhs, const InventoryView& rhs)
        {
            if (lhs.itemCategory != rhs.itemCategory)
            {
                return lhs.itemCategory < rhs.itemCategory;
            }

            return lhs.itemId < rhs.itemId;
        }
    );

    return items;
}

int ClampInventoryScrollOffset(int scrollOffset, int itemCount)
{
    const int maxOffset = std::max(0, itemCount - INVENTORY_VISIBLE_ITEM_COUNT);

    if (scrollOffset < 0)
    {
        return 0;
    }

    if (scrollOffset > maxOffset)
    {
        return maxOffset;
    }

    return scrollOffset;
}

sf::FloatRect GetInventoryPanelRect()
{
    return sf::FloatRect(
        INVENTORY_PANEL_X,
        INVENTORY_PANEL_Y,
        INVENTORY_PANEL_WIDTH,
        INVENTORY_PANEL_HEIGHT
    );
}

int GetInventoryItemIndexFromVisibleIndex(int visibleIndex, int scrollOffset)
{
    return scrollOffset + visibleIndex;
}

int GetShopSellPrice(int itemId)
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

std::vector<InventoryView> BuildShopSellList(
    const std::unordered_map<int, InventoryView>& inventory
)
{
    std::vector<InventoryView> result;

    const std::vector<InventoryView> items = BuildInventoryList(inventory);

    for (const InventoryView& item : items)
    {
        if (item.itemCategory == ITEM_CATEGORY_ETC &&
            item.count > 0 &&
            GetShopSellPrice(item.itemId) > 0)
        {
            result.push_back(item);
        }
    }

    return result;
}

sf::FloatRect GetInventoryItemRect(int visibleIndex)
{
    return sf::FloatRect(
        35.0f,
        70.0f + static_cast<float>(visibleIndex) * INVENTORY_ITEM_ROW_HEIGHT,
        330.0f,
        26.0f
    );
}


sf::FloatRect GetShopItemRect(int index)
{
    return sf::FloatRect(
        185.0f,
        155.0f + static_cast<float>(index) * 28.0f,
        260.0f,
        24.0f
    );
}

sf::FloatRect GetInventoryUseButtonRect()
{
    return sf::FloatRect(
        245.0f,
        390.0f,
        110.0f,
        32.0f
    );
}

sf::FloatRect GetEquipmentWeaponSlotRect()
{
    return sf::FloatRect(
        335.0f,
        70.0f,
        260.0f,
        30.0f
    );
}

sf::FloatRect GetEquipmentArmorSlotRect()
{
    return sf::FloatRect(
        335.0f,
        105.0f,
        260.0f,
        30.0f
    );
}

sf::FloatRect GetEquipmentAccessorySlotRect()
{
    return sf::FloatRect(
        335.0f,
        140.0f,
        260.0f,
        30.0f
    );
}

sf::FloatRect GetQuickSlotRect(int slotIndex)
{
    const float totalWidth =
        QUICK_SLOT_COUNT * QUICK_SLOT_SIZE +
        (QUICK_SLOT_COUNT - 1) * QUICK_SLOT_GAP;

    const float startX =
        static_cast<float>(WINDOW_WIDTH) * 0.5f -
        totalWidth * 0.5f;

    const float y =
        static_cast<float>(MAP_PIXEL_SIZE) + 54.0f;

    return sf::FloatRect(
        startX + slotIndex * (QUICK_SLOT_SIZE + QUICK_SLOT_GAP),
        y,
        QUICK_SLOT_SIZE,
        QUICK_SLOT_SIZE
    );
}

sf::FloatRect GetContextMenuOptionRect(
    float menuX,
    float menuY,
    int optionIndex
)
{
    return sf::FloatRect(
        menuX,
        menuY + static_cast<float>(optionIndex) * CONTEXT_MENU_OPTION_HEIGHT,
        CONTEXT_MENU_WIDTH,
        CONTEXT_MENU_OPTION_HEIGHT
    );
}

void CloseContextMenu(ClientState& state)
{
    state.contextMenuOpen = false;
    state.contextTargetItemId = 0;
    state.contextTargetItemCategory = 0;
    state.contextTargetItemName.clear();
    state.contextTargetQuickSlotIndex = -1;
    state.contextMenuOptions.clear();
}

void OpenInventoryItemContextMenu(
    ClientState& state,
    float mouseX,
    float mouseY,
    const InventoryView& item
)
{
    CloseContextMenu(state);

    state.contextMenuOpen = true;
    state.contextTargetItemId = item.itemId;
    state.contextTargetItemCategory = item.itemCategory;
    state.contextTargetItemName = item.itemName;
    state.contextTargetQuickSlotIndex = -1;

    if (item.itemCategory == ITEM_CATEGORY_CONSUMABLE)
    {
        state.contextMenuOptions.push_back(
            { CONTEXT_ACTION_USE_ITEM, 0, "Use" }
        );

        for (int i = 0; i < QUICK_SLOT_COUNT; ++i)
        {
            state.contextMenuOptions.push_back(
                {
                    CONTEXT_ACTION_SET_QUICK_SLOT,
                    i,
                    "Set Slot " + std::to_string(i + 1)
                }
            );
        }
    }
    else if (item.itemCategory == ITEM_CATEGORY_EQUIPMENT)
    {
        state.contextMenuOptions.push_back(
            { CONTEXT_ACTION_EQUIP_ITEM, 0, "Equip" }
        );
    }
    else
    {
        state.contextMenuOptions.push_back(
            { CONTEXT_ACTION_NONE, 0, "No action" }
        );
    }

    const float menuHeight =
        static_cast<float>(state.contextMenuOptions.size()) *
        CONTEXT_MENU_OPTION_HEIGHT;

    state.contextMenuX = std::min(
        mouseX,
        static_cast<float>(WINDOW_WIDTH) - CONTEXT_MENU_WIDTH - 5.0f
    );

    state.contextMenuY = std::min(
        mouseY,
        static_cast<float>(MAP_PIXEL_SIZE + UI_HEIGHT) - menuHeight - 5.0f
    );
}


void OpenEquipmentContextMenu(
    ClientState& state,
    float mouseX,
    float mouseY,
    int equipmentSlot
)
{
    CloseContextMenu(state);

    int equippedItemId = 0;
    std::string equippedItemName;

    if (equipmentSlot == EQUIPMENT_SLOT_WEAPON)
    {
        equippedItemId = state.equippedWeaponItemId;
        equippedItemName = state.equippedWeaponName;
    }
    else if (equipmentSlot == EQUIPMENT_SLOT_ARMOR)
    {
        equippedItemId = state.equippedArmorItemId;
        equippedItemName = state.equippedArmorName;
    }
    else if (equipmentSlot == EQUIPMENT_SLOT_ACCESSORY)
    {
        equippedItemId = state.equippedAccessoryItemId;
        equippedItemName = state.equippedAccessoryName;
    }

    if (equippedItemId <= 0)
    {
        return;
    }

    state.contextMenuOpen = true;
    state.contextTargetItemId = equippedItemId;
    state.contextTargetItemCategory = ITEM_CATEGORY_EQUIPMENT;
    state.contextTargetItemName = equippedItemName;
    state.contextTargetQuickSlotIndex = -1;

    state.contextMenuOptions.push_back(
        { CONTEXT_ACTION_UNEQUIP_EQUIPMENT, equipmentSlot, "Unequip" }
    );

    const float menuHeight =
        static_cast<float>(state.contextMenuOptions.size()) *
        CONTEXT_MENU_OPTION_HEIGHT;

    state.contextMenuX = std::min(
        mouseX,
        static_cast<float>(WINDOW_WIDTH) - CONTEXT_MENU_WIDTH - 5.0f
    );

    state.contextMenuY = std::min(
        mouseY,
        static_cast<float>(MAP_PIXEL_SIZE + UI_HEIGHT) - menuHeight - 5.0f
    );
}

void OpenQuickSlotContextMenu(
    ClientState& state,
    float mouseX,
    float mouseY,
    int slotIndex
)
{
    CloseContextMenu(state);

    if (slotIndex < 0 || slotIndex >= QUICK_SLOT_COUNT)
    {
        return;
    }

    const QuickSlotView& slot = state.quickSlots[slotIndex];

    if (slot.slotType == QUICK_SLOT_EMPTY)
    {
        return;
    }

    state.contextMenuOpen = true;
    state.contextTargetQuickSlotIndex = slotIndex;

    if (slot.slotType == QUICK_SLOT_ITEM)
    {
        state.contextTargetItemId = slot.targetId;
        state.contextTargetItemCategory = ITEM_CATEGORY_CONSUMABLE;
        state.contextTargetItemName = slot.name;

        state.contextMenuOptions.push_back(
            { CONTEXT_ACTION_USE_ITEM, 0, "Use" }
        );
    }

    state.contextMenuOptions.push_back(
        { CONTEXT_ACTION_CLEAR_QUICK_SLOT, slotIndex, "Clear Slot" }
    );

    const float menuHeight =
        static_cast<float>(state.contextMenuOptions.size()) *
        CONTEXT_MENU_OPTION_HEIGHT;

    state.contextMenuX = std::min(
        mouseX,
        static_cast<float>(WINDOW_WIDTH) - CONTEXT_MENU_WIDTH - 5.0f
    );

    state.contextMenuY = std::min(
        mouseY,
        static_cast<float>(MAP_PIXEL_SIZE + UI_HEIGHT) - menuHeight - 5.0f
    );
}


bool WorldToScreen(int worldX, int worldY, int cameraLeft, int cameraTop, sf::Vector2f& outPosition)
{
    const int tileX = worldX - cameraLeft;
    const int tileY = worldY - cameraTop;

    if (tileX < 0 || tileX >= VIEW_TILE_COUNT)
    {
        return false;
    }

    if (tileY < 0 || tileY >= VIEW_TILE_COUNT)
    {
        return false;
    }

    outPosition.x = static_cast<float>(tileX * TILE_SIZE);
    outPosition.y = static_cast<float>(tileY * TILE_SIZE);

    return true;
}

void DrawWorldMapOverlay(
    sf::RenderWindow& window,
    sf::Font* font,
    int myX,
    int myY,
    const std::unordered_map<int, NpcView>& npcs
)
{
    if (font == nullptr)
    {
        return;
    }

    sf::RectangleShape dim;
    dim.setSize(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    dim.setPosition(0.0f, 0.0f);
    dim.setFillColor(sf::Color(0, 0, 0, 170));
    window.draw(dim);

    const float mapLeft = 55.0f;
    const float mapTop = 35.0f;
    const float mapSize = 530.0f;
    const float scale = mapSize / static_cast<float>(WORLD_WIDTH);

    sf::RectangleShape mapBackground;
    mapBackground.setSize(sf::Vector2f(mapSize, mapSize));
    mapBackground.setPosition(mapLeft, mapTop);
    mapBackground.setFillColor(sf::Color(20, 22, 28, 245));
    mapBackground.setOutlineThickness(2.0f);
    mapBackground.setOutlineColor(sf::Color::White);
    window.draw(mapBackground);

    auto drawRegion = [&](int left, int top, int right, int bottom, int regionType, const std::string& label)
    {
        sf::RectangleShape regionRect;
        regionRect.setSize(
            sf::Vector2f(
                static_cast<float>(right - left) * scale,
                static_cast<float>(bottom - top) * scale
            )
        );
        regionRect.setPosition(
            mapLeft + static_cast<float>(left) * scale,
            mapTop + static_cast<float>(top) * scale
        );
        sf::Color color = GetRegionTileColor((left + right) / 2, (top + bottom) / 2);
        color.a = 180;
        regionRect.setFillColor(color);
        regionRect.setOutlineThickness(1.0f);
        regionRect.setOutlineColor(sf::Color(20, 20, 25, 220));
        window.draw(regionRect);

        sf::Text labelText;
        labelText.setFont(*font);
        labelText.setCharacterSize(12);
        labelText.setFillColor(sf::Color::White);
        labelText.setOutlineThickness(1.0f);
        labelText.setOutlineColor(sf::Color::Black);
        labelText.setString(label);
        labelText.setPosition(
            mapLeft + static_cast<float>(left) * scale + 5.0f,
            mapTop + static_cast<float>(top) * scale + 5.0f
        );
        window.draw(labelText);
    };

    drawRegion(0, 0, 2000, 2000, REGION_OPEN_FIELD, "Open Field");
    drawRegion(0, 0, 700, 700, REGION_GREEN_FOREST, "Green Forest");
    drawRegion(0, 0, 250, 250, REGION_START_VILLAGE, "Start Village");
    drawRegion(700, 0, 1300, 900, REGION_ORC_CAMP, "Orc Camp");
    drawRegion(1300, 1000, 2000, 2000, REGION_ANCIENT_RUINS, "Ancient Ruins");
    drawRegion(1600, 1450, 2000, 2000, REGION_CASTLE_GATE, "Castle Gate");

    for (const auto& [npcId, npc] : npcs)
    {
        sf::CircleShape npcDot;
        npcDot.setRadius(3.0f);
        npcDot.setFillColor(GetNpcColor(npc.npcType));
        npcDot.setOutlineThickness(1.0f);
        npcDot.setOutlineColor(sf::Color::Black);
        npcDot.setPosition(
            mapLeft + static_cast<float>(npc.x) * scale - 3.0f,
            mapTop + static_cast<float>(npc.y) * scale - 3.0f
        );
        window.draw(npcDot);
    }

    sf::CircleShape playerDot;
    playerDot.setRadius(5.0f);
    playerDot.setFillColor(sf::Color::Yellow);
    playerDot.setOutlineThickness(2.0f);
    playerDot.setOutlineColor(sf::Color::Black);
    playerDot.setPosition(
        mapLeft + static_cast<float>(myX) * scale - 5.0f,
        mapTop + static_cast<float>(myY) * scale - 5.0f
    );
    window.draw(playerDot);

    sf::Text title;
    title.setFont(*font);
    title.setCharacterSize(17);
    title.setFillColor(sf::Color::White);
    title.setOutlineThickness(1.0f);
    title.setOutlineColor(sf::Color::Black);
    title.setPosition(70.0f, 8.0f);
    title.setString(
        "World Map   Current: " + GetRegionName(myX, myY) +
        "   Pos: (" + std::to_string(myX) + ", " + std::to_string(myY) + ")   M: Close"
    );
    window.draw(title);

    sf::Text legend;
    legend.setFont(*font);
    legend.setCharacterSize(12);
    legend.setFillColor(sf::Color(230, 230, 230));
    legend.setPosition(70.0f, 575.0f);
    legend.setString(
        "NPC: colored dots   Player: yellow dot\n"
        "Start Village: Merchant/Blacksmith/Healer/Trainer   Green Forest: Slime/Goblin/Wolf\n"
        "Orc Camp: Orc/Goblin   Ancient Ruins: Skeleton/DarkMage   Castle Gate: Trader/Gatekeeper"
    );
    window.draw(legend);
}

void DrawStatusBar(
    sf::RenderWindow& window,
    sf::Font* font,
    float x,
    float y,
    float width,
    float height,
    int current,
    int maximum,
    const std::string& label,
    const sf::Color& fillColor
)
{
    const int safeMax = std::max(1, maximum);
    const float ratio = std::clamp(
        static_cast<float>(current) / static_cast<float>(safeMax),
        0.0f,
        1.0f
    );

    sf::RectangleShape background;
    background.setSize(sf::Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(sf::Color(35, 35, 42));
    background.setOutlineThickness(1.0f);
    background.setOutlineColor(sf::Color(120, 120, 130));
    window.draw(background);

    sf::RectangleShape fill;
    fill.setSize(sf::Vector2f((width - 2.0f) * ratio, height - 2.0f));
    fill.setPosition(x + 1.0f, y + 1.0f);
    fill.setFillColor(fillColor);
    window.draw(fill);

    if (font != nullptr)
    {
        sf::Text text;
        text.setFont(*font);
        text.setCharacterSize(12);
        text.setFillColor(sf::Color::White);
        text.setOutlineThickness(1.0f);
        text.setOutlineColor(sf::Color::Black);
        text.setString(
            label + " " + std::to_string(current) + "/" + std::to_string(maximum)
        );
        text.setPosition(x + 6.0f, y + 1.0f);
        window.draw(text);
    }
}

int GetInventoryItemCount(const std::unordered_map<int, InventoryView>& inventory, int itemId)
{
    const auto iter = inventory.find(itemId);

    if (iter == inventory.end())
    {
        return 0;
    }

    return iter->second.count;
}

void DrawMiniHpBar(
    sf::RenderWindow& window,
    float x,
    float y,
    float width,
    float height,
    int hp,
    int maxHp
)
{
    float ratio = 0.0f;

    if (maxHp > 0)
    {
        ratio = static_cast<float>(hp) / static_cast<float>(maxHp);
    }

    if (ratio < 0.0f)
    {
        ratio = 0.0f;
    }

    if (ratio > 1.0f)
    {
        ratio = 1.0f;
    }

    sf::RectangleShape back;
    back.setSize(sf::Vector2f(width, height));
    back.setPosition(x, y);
    back.setFillColor(sf::Color(35, 20, 20, 220));
    back.setOutlineThickness(1.0f);
    back.setOutlineColor(sf::Color::Black);
    window.draw(back);

    sf::RectangleShape fill;
    fill.setSize(sf::Vector2f(width * ratio, height));
    fill.setPosition(x, y);
    fill.setFillColor(sf::Color(210, 50, 60, 230));
    window.draw(fill);
}

void DrawGame(sf::RenderWindow& window, ClientState& state, sf::Font* font)
{
    int myPlayerId = -1;
    int myX = 0;
    int myY = 0;

    int level = 1;
    int exp = 0;
    int requiredExp = 100;
    int statPoint = 0;
    int skillPoint = 0;
    int gold = 0;

    int str = 5;
    int intStat = 5;
    int dex = 5;
    int luk = 5;

    std::array<int, MAX_SKILL_STATUS_COUNT> skillLevels{};

    int hp = 100;
    int maxHp = 100;
    int mp = 50;
    int maxMp = 50;

    std::unordered_map<int, DropView> drops;
    std::unordered_map<int, InventoryView> inventory;
    bool showInventory = false;
    int inventoryScrollOffset = 0;
    bool showStatWindow = false;
    bool showSkillWindow = false;
    bool showEquipmentWindow = false;
    bool showShopWindow = false;
    bool showWorldMap = false;

    int equippedWeaponItemId = 0;
    std::string equippedWeaponName;

    int equippedArmorItemId = 0;
    std::string equippedArmorName;

    int equippedAccessoryItemId = 0;
    std::string equippedAccessoryName;

    int equipmentStrBonus = 0;
    int equipmentIntBonus = 0;
    int equipmentDexBonus = 0;
    int equipmentLukBonus = 0;

    int equipmentPhysicalAttackBonus = 0;
    int equipmentPhysicalDefenseBonus = 0;
    int equipmentMagicAttackBonus = 0;
    int equipmentMagicDefenseBonus = 0;

    std::array<QuickSlotView, QUICK_SLOT_COUNT> quickSlots;

    std::unordered_map<int, PlayerView> players;
    std::unordered_map<int, MonsterView> monsters;
    std::unordered_map<int, NpcView> npcs;

    bool contextMenuOpen = false;
    float contextMenuX = 0.0f;
    float contextMenuY = 0.0f;
    std::vector<ContextMenuOption> contextMenuOptions;

    std::vector<std::string> messages;
    std::vector<VisualEffect> effects;
    std::vector<FloatingText> floatingTexts;
    long long rageVisualEndTimeMs = 0;
    bool chatInputMode = false;
    std::string chatInput;

    {
        std::lock_guard<std::mutex> lock(state.lock);

        myPlayerId = state.myPlayerId;
        myX = state.myX;
        myY = state.myY;

        level = state.level;
        exp = state.exp;
        requiredExp = state.requiredExp;
        statPoint = state.statPoint;
        skillPoint = state.skillPoint;
        gold = state.gold;

        str = state.str;
        intStat = state.intStat;
        dex = state.dex;
        luk = state.luk;

        skillLevels = state.skillLevels;

        players = state.players;
        monsters = state.monsters;
        npcs = state.npcs;

        hp = state.hp;
        maxHp = state.maxHp;

        mp = state.mp;
        maxMp = state.maxMp;

        drops = state.drops;
        inventory = state.inventory;
        showInventory = state.showInventory;
        inventoryScrollOffset = state.inventoryScrollOffset;
        showStatWindow = state.showStatWindow;
        showSkillWindow = state.showSkillWindow;
        showEquipmentWindow = state.showEquipmentWindow;
        showShopWindow = state.showShopWindow;
        showWorldMap = state.showWorldMap;

        equippedWeaponItemId = state.equippedWeaponItemId;
        equippedWeaponName = state.equippedWeaponName;

        equippedArmorItemId = state.equippedArmorItemId;
        equippedArmorName = state.equippedArmorName;

        equippedAccessoryItemId = state.equippedAccessoryItemId;
        equippedAccessoryName = state.equippedAccessoryName;

        equipmentStrBonus = state.equipmentStrBonus;
        equipmentIntBonus = state.equipmentIntBonus;
        equipmentDexBonus = state.equipmentDexBonus;
        equipmentLukBonus = state.equipmentLukBonus;

        equipmentPhysicalAttackBonus = state.equipmentPhysicalAttackBonus;
        equipmentPhysicalDefenseBonus = state.equipmentPhysicalDefenseBonus;
        equipmentMagicAttackBonus = state.equipmentMagicAttackBonus;
        equipmentMagicDefenseBonus = state.equipmentMagicDefenseBonus;

        quickSlots = state.quickSlots;

        contextMenuOpen = state.contextMenuOpen;
        contextMenuX = state.contextMenuX;
        contextMenuY = state.contextMenuY;
        contextMenuOptions = state.contextMenuOptions;

        const long long nowMs = GetClientNowMs();
        state.effects.erase(
            std::remove_if(
                state.effects.begin(),
                state.effects.end(),
                [nowMs](const VisualEffect& effect)
                {
                    return nowMs - effect.startTimeMs > effect.durationMs;
                }
            ),
            state.effects.end()
        );
        effects = state.effects;

        state.floatingTexts.erase(
            std::remove_if(
                state.floatingTexts.begin(),
                state.floatingTexts.end(),
                [nowMs](const FloatingText& floatingText)
                {
                    return nowMs - floatingText.startTimeMs > floatingText.durationMs;
                }
            ),
            state.floatingTexts.end()
        );
        floatingTexts = state.floatingTexts;

        rageVisualEndTimeMs = state.rageVisualEndTimeMs;
        messages = state.messages;
        chatInputMode = state.chatInputMode;
        chatInput = state.chatInput;
    }

    TextureAssets& assets = GetTextureAssets();

    const int cameraLeft = myX - VIEW_TILE_COUNT / 2;
    const int cameraTop = myY - VIEW_TILE_COUNT / 2;

    window.clear(sf::Color(25, 25, 30));

    sf::RectangleShape tileShape;
    tileShape.setSize(sf::Vector2f(TILE_SIZE - 1.0f, TILE_SIZE - 1.0f));
    tileShape.setFillColor(sf::Color(45, 45, 50));

    for (int y = 0; y < VIEW_TILE_COUNT; ++y)
    {
        for (int x = 0; x < VIEW_TILE_COUNT; ++x)
        {
            const int worldTileX = cameraLeft + x;
            const int worldTileY = cameraTop + y;

            const int regionType = GetRegionType(worldTileX, worldTileY);
            const sf::Texture* regionTexture = FindTexture(assets.regionTextures, regionType);

            const sf::Vector2f tilePos(
                static_cast<float>(x * TILE_SIZE),
                static_cast<float>(y * TILE_SIZE)
            );

            if (regionTexture != nullptr)
            {
                DrawTextureSprite(
                    window,
                    *regionTexture,
                    tilePos,
                    static_cast<float>(TILE_SIZE)
                );
            }
            else
            {
                tileShape.setFillColor(GetRegionTileColor(worldTileX, worldTileY));
                tileShape.setPosition(tilePos);

                window.draw(tileShape);
            }

            int objectType = 0;
            float objectSize = static_cast<float>(TILE_SIZE);
            sf::Vector2f objectPos = tilePos;

            if (IsClientBlockedTile(worldTileX, worldTileY))
            {
                objectType = GetObstacleObjectType(worldTileX, worldTileY);
                objectSize = static_cast<float>(TILE_SIZE);
            }
            else if (ShouldDrawDecoration(worldTileX, worldTileY))
            {
                objectType = GetDecorationObjectType(worldTileX, worldTileY);
                objectSize = static_cast<float>(TILE_SIZE) * 0.78f;
                objectPos.x += static_cast<float>(TILE_SIZE) * 0.11f;
                objectPos.y += static_cast<float>(TILE_SIZE) * 0.11f;
            }

            if (objectType != 0)
            {
                const sf::Texture* objectTexture = FindTexture(assets.objectTextures, objectType);

                if (objectTexture != nullptr)
                {
                    DrawTextureSprite(window, *objectTexture, objectPos, objectSize);
                }
                else
                {
                    sf::RectangleShape objectShape;
                    objectShape.setSize(sf::Vector2f(objectSize - 2.0f, objectSize - 2.0f));
                    objectShape.setPosition(objectPos.x + 1.0f, objectPos.y + 1.0f);
                    objectShape.setFillColor(sf::Color(55, 45, 35, 210));
                    objectShape.setOutlineThickness(1.0f);
                    objectShape.setOutlineColor(sf::Color::Black);
                    window.draw(objectShape);
                }
            }
        }
    }

    for (const auto& [monsterId, monster] : monsters)
    {
        sf::Vector2f screenPos;

        if (!WorldToScreen(monster.x, monster.y, cameraLeft, cameraTop, screenPos))
        {
            continue;
        }

        const sf::Texture* monsterTexture = FindTexture(assets.monsterTextures, monster.monsterType);

        if (monsterTexture != nullptr)
        {
            DrawTextureSprite(
                window,
                *monsterTexture,
                sf::Vector2f(screenPos.x + TILE_SIZE * 0.05f, screenPos.y + TILE_SIZE * 0.02f),
                TILE_SIZE * 0.9f
            );
        }
        else
        {
            sf::CircleShape monsterShape;
            monsterShape.setRadius(TILE_SIZE * 0.35f);
            monsterShape.setFillColor(GetMonsterColor(monster.monsterType));
            monsterShape.setOutlineThickness(2.0f);
            monsterShape.setOutlineColor(sf::Color::Black);
            monsterShape.setPosition(
                screenPos.x + TILE_SIZE * 0.15f,
                screenPos.y + TILE_SIZE * 0.15f
            );

            window.draw(monsterShape);
        }

        if (font != nullptr)
        {
            sf::Text nameText;
            nameText.setFont(*font);
            nameText.setCharacterSize(10);
            nameText.setFillColor(sf::Color::White);
            nameText.setOutlineThickness(1.0f);
            nameText.setOutlineColor(sf::Color::Black);
            nameText.setString(monster.name + " Lv." + std::to_string(monster.level));
            nameText.setPosition(screenPos.x - 4.0f, screenPos.y - 18.0f);
            window.draw(nameText);

            DrawMiniHpBar(
                window,
                screenPos.x + 1.0f,
                screenPos.y - 5.0f,
                static_cast<float>(TILE_SIZE - 2),
                5.0f,
                monster.hp,
                monster.maxHp
            );
        }
    }

    for (const auto& [dropId, drop] : drops)
    {
        sf::Vector2f screenPos;

        if (!WorldToScreen(drop.x, drop.y, cameraLeft, cameraTop, screenPos))
        {
            continue;
        }

        sf::RectangleShape dropShape;
        dropShape.setSize(sf::Vector2f(TILE_SIZE * 0.35f, TILE_SIZE * 0.35f));
        dropShape.setFillColor(GetDropColor(drop.itemCategory));
        dropShape.setOutlineThickness(1.0f);
        dropShape.setOutlineColor(sf::Color::Black);
        dropShape.setPosition(
            screenPos.x + TILE_SIZE * 0.325f,
            screenPos.y + TILE_SIZE * 0.325f
        );

        window.draw(dropShape);
    }

    for (const auto& [npcId, npc] : npcs)
    {
        sf::Vector2f screenPos;

        if (!WorldToScreen(npc.x, npc.y, cameraLeft, cameraTop, screenPos))
        {
            continue;
        }

        const sf::Texture* npcTexture = FindTexture(assets.npcTextures, npc.npcType);

        if (npcTexture != nullptr)
        {
            DrawTextureSprite(
                window,
                *npcTexture,
                sf::Vector2f(screenPos.x + TILE_SIZE * 0.05f, screenPos.y + TILE_SIZE * 0.02f),
                TILE_SIZE * 0.9f
            );
        }
        else
        {
            sf::CircleShape npcShape;
            npcShape.setRadius(TILE_SIZE * 0.35f);
            npcShape.setFillColor(GetNpcColor(npc.npcType));
            npcShape.setOutlineThickness(2.0f);
            npcShape.setOutlineColor(sf::Color::Black);
            npcShape.setPosition(
                screenPos.x + TILE_SIZE * 0.15f,
                screenPos.y + TILE_SIZE * 0.15f
            );

            window.draw(npcShape);
        }

        if (font != nullptr)
        {
            sf::Text npcText;
            npcText.setFont(*font);
            npcText.setCharacterSize(11);
            npcText.setFillColor(sf::Color::White);
            npcText.setOutlineThickness(1.0f);
            npcText.setOutlineColor(sf::Color::Black);
            npcText.setString(npc.name);
            npcText.setPosition(screenPos.x, screenPos.y - 18.0f);
            window.draw(npcText);

            sf::Text roleText;
            roleText.setFont(*font);
            roleText.setCharacterSize(10);
            roleText.setFillColor(GetNpcColor(npc.npcType));
            roleText.setOutlineThickness(1.0f);
            roleText.setOutlineColor(sf::Color::Black);
            roleText.setString("[" + GetNpcRoleText(npc.npcType) + "]");
            roleText.setPosition(screenPos.x + 2.0f, screenPos.y - 6.0f);
            window.draw(roleText);
        }
    }

    for (const auto& [playerId, player] : players)
    {
        sf::Vector2f screenPos;

        if (!WorldToScreen(player.x, player.y, cameraLeft, cameraTop, screenPos))
        {
            continue;
        }

        if (assets.otherPlayer.getSize().x > 0)
        {
            DrawTextureSprite(
                window,
                assets.otherPlayer,
                sf::Vector2f(screenPos.x + TILE_SIZE * 0.05f, screenPos.y + TILE_SIZE * 0.02f),
                TILE_SIZE * 0.9f
            );
        }
        else
        {
            sf::RectangleShape playerShape;
            playerShape.setSize(sf::Vector2f(TILE_SIZE * 0.75f, TILE_SIZE * 0.75f));
            playerShape.setFillColor(sf::Color(70, 130, 230));
            playerShape.setOutlineThickness(2.0f);
            playerShape.setOutlineColor(sf::Color::Black);
            playerShape.setPosition(
                screenPos.x + TILE_SIZE * 0.125f,
                screenPos.y + TILE_SIZE * 0.125f
            );

            window.draw(playerShape);
        }

        if (font != nullptr)
        {
            std::string displayName = player.username;

            if (displayName.empty())
            {
                displayName = "Player" + std::to_string(player.playerId);
            }

            sf::Text playerNameText;
            playerNameText.setFont(*font);
            playerNameText.setCharacterSize(11);
            playerNameText.setFillColor(sf::Color::White);
            playerNameText.setOutlineThickness(1.0f);
            playerNameText.setOutlineColor(sf::Color::Black);
            playerNameText.setString(displayName);

            const sf::FloatRect nameBounds = playerNameText.getLocalBounds();
            playerNameText.setPosition(
                screenPos.x + TILE_SIZE * 0.5f - nameBounds.width * 0.5f,
                screenPos.y - 18.0f
            );

            window.draw(playerNameText);
        }
    }

    {
        sf::Vector2f screenPos;

        if (WorldToScreen(myX, myY, cameraLeft, cameraTop, screenPos))
        {
            if (assets.player.getSize().x > 0)
            {
                DrawTextureSprite(
                    window,
                    assets.player,
                    sf::Vector2f(screenPos.x + TILE_SIZE * 0.02f, screenPos.y),
                    TILE_SIZE * 0.95f
                );
            }
            else
            {
                sf::RectangleShape myShape;
                myShape.setSize(sf::Vector2f(TILE_SIZE * 0.8f, TILE_SIZE * 0.8f));
                myShape.setFillColor(sf::Color(80, 230, 120));
                myShape.setOutlineThickness(3.0f);
                myShape.setOutlineColor(sf::Color::White);
                myShape.setPosition(
                    screenPos.x + TILE_SIZE * 0.1f,
                    screenPos.y + TILE_SIZE * 0.1f
                );

                window.draw(myShape);
            }
        }
    }

    if (rageVisualEndTimeMs > GetClientNowMs())
    {
        sf::Vector2f myScreenPos;

        if (WorldToScreen(myX, myY, cameraLeft, cameraTop, myScreenPos))
        {
            DrawRageAura(window, myScreenPos, GetClientNowMs());
        }
    }

    for (const VisualEffect& effect : effects)
    {
        sf::Vector2f screenPos;

        if (!WorldToScreen(effect.x, effect.y, cameraLeft, cameraTop, screenPos))
        {
            continue;
        }

        const long long nowMs = GetClientNowMs();
        const float progress = std::min(
            1.0f,
            static_cast<float>(nowMs - effect.startTimeMs) /
            static_cast<float>(std::max(1LL, effect.durationMs))
        );

        DrawVisualEffect(window, assets, effect, screenPos, progress);
    }

    if (font != nullptr)
    {
        for (const FloatingText& floatingText : floatingTexts)
        {
            sf::Vector2f screenPos;

            if (!WorldToScreen(floatingText.x, floatingText.y, cameraLeft, cameraTop, screenPos))
            {
                continue;
            }

            const long long nowMs = GetClientNowMs();
            const float progress = std::min(
                1.0f,
                static_cast<float>(nowMs - floatingText.startTimeMs) /
                static_cast<float>(std::max(1LL, floatingText.durationMs))
            );

            sf::Color color = floatingText.color;
            color.a = static_cast<sf::Uint8>(255.0f * (1.0f - progress));

            sf::Text text;
            text.setFont(*font);
            text.setCharacterSize(16);
            text.setFillColor(color);
            text.setOutlineThickness(1.5f);
            text.setOutlineColor(sf::Color(0, 0, 0, color.a));
            text.setString(floatingText.text);
            text.setPosition(
                screenPos.x + 6.0f,
                screenPos.y - 10.0f - progress * 28.0f
            );
            window.draw(text);
        }
    }

    if (font != nullptr)
    {
        bool foundNearbyNpc = false;
        NpcView nearestNpc{};
        int bestDistance = 999999;

        for (const auto& [npcId, npc] : npcs)
        {
            int dx = myX - npc.x;
            int dy = myY - npc.y;

            if (dx < 0)
            {
                dx = -dx;
            }

            if (dy < 0)
            {
                dy = -dy;
            }

            const int distance = dx + dy;

            if (distance <= 3 && distance < bestDistance)
            {
                foundNearbyNpc = true;
                bestDistance = distance;
                nearestNpc = npc;
            }
        }

        if (foundNearbyNpc)
        {
            sf::RectangleShape promptBackground;
            promptBackground.setSize(sf::Vector2f(360.0f, 28.0f));
            promptBackground.setPosition(
                static_cast<float>(WINDOW_WIDTH) * 0.5f - 180.0f,
                static_cast<float>(MAP_PIXEL_SIZE) - 36.0f
            );
            promptBackground.setFillColor(sf::Color(10, 10, 15, 210));
            promptBackground.setOutlineThickness(1.0f);
            promptBackground.setOutlineColor(GetNpcColor(nearestNpc.npcType));
            window.draw(promptBackground);

            sf::Text promptText;
            promptText.setFont(*font);
            promptText.setCharacterSize(14);
            promptText.setFillColor(sf::Color::White);
            promptText.setOutlineThickness(1.0f);
            promptText.setOutlineColor(sf::Color::Black);
            promptText.setString(GetNpcInteractionText(nearestNpc));
            promptText.setPosition(
                static_cast<float>(WINDOW_WIDTH) * 0.5f - 170.0f,
                static_cast<float>(MAP_PIXEL_SIZE) - 31.0f
            );
            window.draw(promptText);
        }
    }

    sf::RectangleShape uiBackground;
    uiBackground.setSize(sf::Vector2f(WINDOW_WIDTH, UI_HEIGHT));
    uiBackground.setPosition(0.0f, static_cast<float>(MAP_PIXEL_SIZE));
    uiBackground.setFillColor(sf::Color(15, 15, 18));
    window.draw(uiBackground);

    if (font != nullptr)
    {
        DrawStatusBar(
            window,
            font,
            10.0f,
            static_cast<float>(MAP_PIXEL_SIZE + 7),
            170.0f,
            18.0f,
            hp,
            maxHp,
            "HP",
            sf::Color(210, 55, 65)
        );

        DrawStatusBar(
            window,
            font,
            190.0f,
            static_cast<float>(MAP_PIXEL_SIZE + 7),
            160.0f,
            18.0f,
            mp,
            maxMp,
            "MP",
            sf::Color(60, 110, 230)
        );

        DrawStatusBar(
            window,
            font,
            360.0f,
            static_cast<float>(MAP_PIXEL_SIZE + 7),
            170.0f,
            18.0f,
            exp,
            requiredExp,
            "EXP",
            sf::Color(220, 180, 70)
        );

        sf::Text infoText;
        infoText.setFont(*font);
        infoText.setCharacterSize(13);
        infoText.setFillColor(sf::Color::White);
        infoText.setPosition(10.0f, static_cast<float>(MAP_PIXEL_SIZE + 30));
        infoText.setString(
            "Lv." + std::to_string(level) +
            "   Region: " + GetRegionName(myX, myY) +
            "   Pos: (" + std::to_string(myX) + ", " + std::to_string(myY) + ")" +
            "   Gold: " + std::to_string(gold) +
            "   SP: " + std::to_string(statPoint) +
            "   Skill: " + std::to_string(skillPoint) +
            "   Players: " + std::to_string(players.size()) +
            "   Monsters: " + std::to_string(monsters.size())
        );
        window.draw(infoText);

        sf::Text helpText;
        helpText.setFont(*font);
        helpText.setCharacterSize(12);
        helpText.setFillColor(sf::Color(210, 210, 210));
        helpText.setPosition(10.0f, static_cast<float>(MAP_PIXEL_SIZE + 100));
        helpText.setString(
            "Move: Arrow   A: Attack   U: Rage   F: NPC   M: Map   QWE/RTY: Skills   I/S/K/O: UI   1~5: QuickSlot"
        );
        window.draw(helpText);
    }

    if (font != nullptr)
    {
        for (int i = 0; i < QUICK_SLOT_COUNT; ++i)
        {
            const sf::FloatRect rect = GetQuickSlotRect(i);

            sf::RectangleShape slotShape;
            slotShape.setSize(sf::Vector2f(rect.width, rect.height));
            slotShape.setPosition(rect.left, rect.top);
            slotShape.setFillColor(sf::Color(28, 28, 34, 235));
            slotShape.setOutlineThickness(2.0f);
            slotShape.setOutlineColor(sf::Color::White);

            window.draw(slotShape);

            sf::Text numberText;
            numberText.setFont(*font);
            numberText.setCharacterSize(11);
            numberText.setFillColor(sf::Color::White);
            numberText.setString(std::to_string(i + 1));
            numberText.setPosition(rect.left + 3.0f, rect.top + 1.0f);

            window.draw(numberText);

            if (quickSlots[i].slotType != QUICK_SLOT_EMPTY)
            {
                const sf::Texture* itemTexture = nullptr;

                if (quickSlots[i].slotType == QUICK_SLOT_ITEM)
                {
                    itemTexture = FindTexture(assets.itemTextures, quickSlots[i].targetId);
                }

                if (itemTexture != nullptr)
                {
                    DrawTextureSprite(
                        window,
                        *itemTexture,
                        sf::Vector2f(rect.left + 8.0f, rect.top + 8.0f),
                        24.0f
                    );
                }

                sf::Text slotText;
                slotText.setFont(*font);
                slotText.setCharacterSize(11);
                slotText.setFillColor(sf::Color::Yellow);
                slotText.setString(GetQuickSlotDisplayName(quickSlots[i].name));
                slotText.setPosition(rect.left + 5.0f, rect.top + 29.0f);

                window.draw(slotText);

                if (quickSlots[i].slotType == QUICK_SLOT_ITEM && quickSlots[i].targetId > 0)
                {
                    const int itemCount = GetInventoryItemCount(inventory, quickSlots[i].targetId);

                    sf::Text countText;
                    countText.setFont(*font);
                    countText.setCharacterSize(11);
                    countText.setFillColor(sf::Color::White);
                    countText.setOutlineThickness(1.0f);
                    countText.setOutlineColor(sf::Color::Black);
                    countText.setString("x" + std::to_string(itemCount));
                    countText.setPosition(rect.left + 22.0f, rect.top + 3.0f);
                    window.draw(countText);
                }
            }
        }
    }

    if (font != nullptr && showEquipmentWindow)
    {
        sf::RectangleShape equipmentBackground;
        equipmentBackground.setSize(sf::Vector2f(310.0f, 430.0f));
        equipmentBackground.setPosition(320.0f, 20.0f);
        equipmentBackground.setFillColor(sf::Color(20, 20, 25, 230));
        equipmentBackground.setOutlineThickness(2.0f);
        equipmentBackground.setOutlineColor(sf::Color::White);
        window.draw(equipmentBackground);

        sf::Text titleText;
        titleText.setFont(*font);
        titleText.setCharacterSize(18);
        titleText.setFillColor(sf::Color::White);
        titleText.setString("[Equipment]");
        titleText.setPosition(335.0f, 35.0f);
        window.draw(titleText);

        auto drawEquipmentSlot = [font, &window](
            const sf::FloatRect& slotRect,
            const std::string& label,
            int itemId,
            const std::string& itemName
        )
        {
            sf::RectangleShape slotShape;
            slotShape.setPosition(slotRect.left, slotRect.top);
            slotShape.setSize(sf::Vector2f(slotRect.width, slotRect.height));
            slotShape.setFillColor(sf::Color(40, 40, 48, 230));
            slotShape.setOutlineThickness(1.0f);
            slotShape.setOutlineColor(sf::Color(100, 100, 120));
            window.draw(slotShape);

            std::string text = label + ": None";

            if (itemId > 0)
            {
                text =
                    label + ": " +
                    itemName +
                    " [" +
                    std::to_string(itemId) +
                    "]";
            }

            sf::Text textView;
            textView.setFont(*font);
            textView.setCharacterSize(13);
            textView.setFillColor(sf::Color::White);
            textView.setString(text);
            textView.setPosition(slotRect.left + 7.0f, slotRect.top + 6.0f);
            window.draw(textView);
        };

        drawEquipmentSlot(
            GetEquipmentWeaponSlotRect(),
            "Weapon",
            equippedWeaponItemId,
            equippedWeaponName
        );

        drawEquipmentSlot(
            GetEquipmentArmorSlotRect(),
            "Armor",
            equippedArmorItemId,
            equippedArmorName
        );

        drawEquipmentSlot(
            GetEquipmentAccessorySlotRect(),
            "Accessory",
            equippedAccessoryItemId,
            equippedAccessoryName
        );

        sf::Text bonusText;
        bonusText.setFont(*font);
        bonusText.setCharacterSize(13);
        bonusText.setFillColor(sf::Color::White);
        bonusText.setPosition(335.0f, 185.0f);

        bonusText.setString(
            std::string("[Bonus]\n") +
            "STR +" + std::to_string(equipmentStrBonus) + "\n" +
            "INT +" + std::to_string(equipmentIntBonus) + "\n" +
            "DEX +" + std::to_string(equipmentDexBonus) + "\n" +
            "LUK +" + std::to_string(equipmentLukBonus) + "\n\n" +
            "P.ATK +" + std::to_string(equipmentPhysicalAttackBonus) + "\n" +
            "P.DEF +" + std::to_string(equipmentPhysicalDefenseBonus) + "\n" +
            "M.ATK +" + std::to_string(equipmentMagicAttackBonus) + "\n" +
            "M.DEF +" + std::to_string(equipmentMagicDefenseBonus) + "\n\n" +
            "Left click slot: Unequip\n" +
            "O: Close"
        );

        window.draw(bonusText);
    }

    if (font != nullptr && showInventory)
    {
        sf::RectangleShape inventoryBackground;
        inventoryBackground.setSize(sf::Vector2f(INVENTORY_PANEL_WIDTH, INVENTORY_PANEL_HEIGHT));
        inventoryBackground.setPosition(INVENTORY_PANEL_X, INVENTORY_PANEL_Y);
        inventoryBackground.setFillColor(sf::Color(20, 20, 25, 230));
        inventoryBackground.setOutlineThickness(2.0f);
        inventoryBackground.setOutlineColor(sf::Color::White);

        window.draw(inventoryBackground);

        sf::Text titleText;
        titleText.setFont(*font);
        titleText.setCharacterSize(18);
        titleText.setFillColor(sf::Color::White);
        titleText.setString("[Inventory]");
        titleText.setPosition(35.0f, 35.0f);
        window.draw(titleText);

        sf::Text goldText;
        goldText.setFont(*font);
        goldText.setCharacterSize(15);
        goldText.setFillColor(sf::Color(255, 220, 80));
        goldText.setString("Gold: " + std::to_string(gold));
        goldText.setPosition(235.0f, 38.0f);
        window.draw(goldText);

        const std::vector<InventoryView> inventoryItems =
            BuildInventoryList(inventory);

        if (inventoryItems.empty())
        {
            sf::Text emptyText;
            emptyText.setFont(*font);
            emptyText.setCharacterSize(15);
            emptyText.setFillColor(sf::Color::White);
            emptyText.setString("Empty");
            emptyText.setPosition(35.0f, 75.0f);
            window.draw(emptyText);
        }
        else
        {
            const int itemCount = static_cast<int>(inventoryItems.size());
            const int scrollOffset = ClampInventoryScrollOffset(inventoryScrollOffset, itemCount);
            const int visibleEnd = std::min(
                itemCount,
                scrollOffset + INVENTORY_VISIBLE_ITEM_COUNT
            );

            sf::Text pageText;
            pageText.setFont(*font);
            pageText.setCharacterSize(11);
            pageText.setFillColor(sf::Color(180, 220, 255));
            pageText.setString(
                "Items " +
                std::to_string(scrollOffset + 1) +
                "-" +
                std::to_string(visibleEnd) +
                " / " +
                std::to_string(itemCount)
            );
            pageText.setPosition(35.0f, 54.0f);
            window.draw(pageText);

            for (int visibleIndex = 0;
                visibleIndex < INVENTORY_VISIBLE_ITEM_COUNT &&
                scrollOffset + visibleIndex < itemCount;
                ++visibleIndex)
            {
                const int itemIndex = GetInventoryItemIndexFromVisibleIndex(
                    visibleIndex,
                    scrollOffset
                );
                const InventoryView& item = inventoryItems[itemIndex];
                const sf::FloatRect rect = GetInventoryItemRect(visibleIndex);

                sf::RectangleShape itemRect;
                itemRect.setSize(sf::Vector2f(rect.width, rect.height));
                itemRect.setPosition(rect.left, rect.top);

                itemRect.setFillColor(sf::Color(40, 40, 48, 230));
                itemRect.setOutlineColor(sf::Color(90, 90, 100));

                itemRect.setOutlineThickness(1.0f);
                window.draw(itemRect);

                sf::Text itemText;
                itemText.setFont(*font);
                itemText.setCharacterSize(14);
                itemText.setFillColor(sf::Color::White);

                itemText.setString(
                    "[" + GetItemCategoryText(item.itemCategory) + "] " +
                    item.itemName +
                    " x" +
                    std::to_string(item.count)
                );

                itemText.setPosition(rect.left + 6.0f, rect.top + 4.0f);
                window.draw(itemText);
            }

            if (itemCount > INVENTORY_VISIBLE_ITEM_COUNT)
            {
                const float trackX = 370.0f;
                const float trackY = 70.0f;
                const float trackHeight = INVENTORY_VISIBLE_ITEM_COUNT * INVENTORY_ITEM_ROW_HEIGHT - 4.0f;
                const int maxScrollOffset = std::max(0, itemCount - INVENTORY_VISIBLE_ITEM_COUNT);

                sf::RectangleShape scrollTrack;
                scrollTrack.setPosition(trackX, trackY);
                scrollTrack.setSize(sf::Vector2f(8.0f, trackHeight));
                scrollTrack.setFillColor(sf::Color(45, 45, 55, 230));
                window.draw(scrollTrack);

                const float knobHeight = std::max(
                    28.0f,
                    trackHeight * static_cast<float>(INVENTORY_VISIBLE_ITEM_COUNT) /
                        static_cast<float>(itemCount)
                );

                float knobY = trackY;

                if (maxScrollOffset > 0)
                {
                    knobY +=
                        (trackHeight - knobHeight) *
                        static_cast<float>(scrollOffset) /
                        static_cast<float>(maxScrollOffset);
                }

                sf::RectangleShape scrollKnob;
                scrollKnob.setPosition(trackX, knobY);
                scrollKnob.setSize(sf::Vector2f(8.0f, knobHeight));
                scrollKnob.setFillColor(sf::Color(120, 170, 230, 230));
                window.draw(scrollKnob);
            }
        }



        sf::Text helpText;
        helpText.setFont(*font);
        helpText.setCharacterSize(12);
        helpText.setFillColor(sf::Color(210, 210, 210));
        helpText.setString(
            "Wheel / PgUp / PgDn: Scroll\n"
            "Left click item: Open menu\n"
            "Potion/Scroll: Use / Set Slot 1~5\n"
            "Equipment: Equip    I: Close"
        );
        helpText.setPosition(35.0f, 382.0f);
        window.draw(helpText);

    }

    if (font != nullptr && showShopWindow)
    {
        sf::RectangleShape shopBackground;
        shopBackground.setSize(sf::Vector2f(300.0f, 300.0f));
        shopBackground.setPosition(165.0f, 80.0f);
        shopBackground.setFillColor(sf::Color(20, 20, 25, 235));
        shopBackground.setOutlineThickness(2.0f);
        shopBackground.setOutlineColor(sf::Color::White);
        window.draw(shopBackground);

        sf::Text titleText;
        titleText.setFont(*font);
        titleText.setCharacterSize(16);
        titleText.setFillColor(sf::Color::White);
        titleText.setPosition(185.0f, 100.0f);
        titleText.setString(
            "[Merchant]\n"
            "Gold: " + std::to_string(gold) + "\n"
            "Click ETC item to sell 1"
        );
        window.draw(titleText);

        const std::vector<InventoryView> sellItems =
            BuildShopSellList(inventory);

        if (sellItems.empty())
        {
            sf::Text emptyText;
            emptyText.setFont(*font);
            emptyText.setCharacterSize(14);
            emptyText.setFillColor(sf::Color(210, 210, 210));
            emptyText.setPosition(185.0f, 155.0f);
            emptyText.setString("No ETC items to sell.");
            window.draw(emptyText);
        }
        else
        {
            const int maxVisibleSellItems = 7;

            for (int i = 0;
                i < static_cast<int>(sellItems.size()) &&
                i < maxVisibleSellItems;
                ++i)
            {
                const InventoryView& item = sellItems[i];
                const sf::FloatRect rowRect = GetShopItemRect(i);

                sf::RectangleShape rowBackground;
                rowBackground.setSize(sf::Vector2f(rowRect.width, rowRect.height));
                rowBackground.setPosition(rowRect.left, rowRect.top);
                rowBackground.setFillColor(sf::Color(40, 40, 48, 235));
                rowBackground.setOutlineThickness(1.0f);
                rowBackground.setOutlineColor(sf::Color(90, 90, 100));
                window.draw(rowBackground);

                sf::Text rowText;
                rowText.setFont(*font);
                rowText.setCharacterSize(13);
                rowText.setFillColor(sf::Color::White);
                rowText.setPosition(rowRect.left + 6.0f, rowRect.top + 4.0f);
                rowText.setString(
                    item.itemName +
                    " x" +
                    std::to_string(item.count) +
                    "   Sell +" +
                    std::to_string(GetShopSellPrice(item.itemId)) +
                    "G"
                );
                window.draw(rowText);
            }
        }

        sf::Text helpText;
        helpText.setFont(*font);
        helpText.setCharacterSize(12);
        helpText.setFillColor(sf::Color(210, 210, 210));
        helpText.setPosition(185.0f, 335.0f);
        helpText.setString("F: Close");
        window.draw(helpText);
    }

    if (font != nullptr && showStatWindow)
    {
        sf::RectangleShape statBackground;
        statBackground.setSize(sf::Vector2f(240.0f, 240.0f));
        statBackground.setPosition(390.0f, 20.0f);
        statBackground.setFillColor(sf::Color(20, 20, 25, 230));
        statBackground.setOutlineThickness(2.0f);
        statBackground.setOutlineColor(sf::Color::White);
        window.draw(statBackground);

        const int totalStr = str + equipmentStrBonus;
        const int totalInt = intStat + equipmentIntBonus;
        const int totalDex = dex + equipmentDexBonus;
        const int totalLuk = luk + equipmentLukBonus;

        sf::Text statText;
        statText.setFont(*font);
        statText.setCharacterSize(13);
        statText.setFillColor(sf::Color::White);
        statText.setPosition(405.0f, 35.0f);

        statText.setString(
            std::string("[Stat]\n\n") +
            "Stat Point: " + std::to_string(statPoint) + "\n\n" +
            "Z  STR: " + std::to_string(totalStr) +
            " (Base " + std::to_string(str) +
            " + Equip " + std::to_string(equipmentStrBonus) + ")\n" +
            "X  INT: " + std::to_string(totalInt) +
            " (Base " + std::to_string(intStat) +
            " + Equip " + std::to_string(equipmentIntBonus) + ")\n" +
            "C  DEX: " + std::to_string(totalDex) +
            " (Base " + std::to_string(dex) +
            " + Equip " + std::to_string(equipmentDexBonus) + ")\n" +
            "V  LUK: " + std::to_string(totalLuk) +
            " (Base " + std::to_string(luk) +
            " + Equip " + std::to_string(equipmentLukBonus) + ")\n\n" +
            "S: Close"
        );

        window.draw(statText);
    }

    if (font != nullptr && showSkillWindow)
    {
        const sf::FloatRect windowRect = GetSkillWindowRect();

        sf::RectangleShape skillBackground;
        skillBackground.setSize(sf::Vector2f(windowRect.width, windowRect.height));
        skillBackground.setPosition(windowRect.left, windowRect.top);
        skillBackground.setFillColor(sf::Color(18, 18, 24, 238));
        skillBackground.setOutlineThickness(2.0f);
        skillBackground.setOutlineColor(sf::Color(210, 210, 230));
        window.draw(skillBackground);

        sf::Text titleText;
        titleText.setFont(*font);
        titleText.setCharacterSize(18);
        titleText.setFillColor(sf::Color::White);
        titleText.setPosition(windowRect.left + 18.0f, windowRect.top + 14.0f);
        titleText.setString(
            "[Skill Tree]   Skill Point: " +
            std::to_string(skillPoint) +
            "   (+): Invest point   U: Rage   K: Close"
        );
        window.draw(titleText);

        const char* columnTitles[] =
        {
            "Warrior / Physical",
            "Mage / Magic",
            "Passive / Growth"
        };

        for (int column = 0; column < 3; ++column)
        {
            sf::Text columnText;
            columnText.setFont(*font);
            columnText.setCharacterSize(13);
            columnText.setFillColor(sf::Color(230, 220, 140));

            const sf::FloatRect topNodeRect = GetSkillNodeRect(column, 0);
            columnText.setPosition(topNodeRect.left + 4.0f, windowRect.top + 54.0f);
            columnText.setString(columnTitles[column]);
            window.draw(columnText);
        }

        for (int column = 0; column < 3; ++column)
        {
            for (int row = 0; row < 2; ++row)
            {
                const sf::FloatRect fromRect = GetSkillNodeRect(column, row);
                const sf::FloatRect toRect = GetSkillNodeRect(column, row + 1);

                sf::RectangleShape connector;
                connector.setSize(sf::Vector2f(4.0f, SKILL_NODE_GAP_Y - 8.0f));
                connector.setPosition(
                    fromRect.left + fromRect.width * 0.5f - 2.0f,
                    fromRect.top + fromRect.height + 4.0f
                );
                connector.setFillColor(sf::Color(100, 100, 120, 220));
                window.draw(connector);
            }
        }

        const SkillTreeNodeDefinition* nodes = GetSkillTreeNodes();

        for (int i = 0; i < GetSkillTreeNodeCount(); ++i)
        {
            const SkillTreeNodeDefinition& node = nodes[i];
            const int currentLevel = GetSkillLevelFromArray(skillLevels, node.skillId);
            const bool unlocked = IsSkillUnlockedForUi(skillLevels, node.skillId);
            const bool mastered = currentLevel >= SKILL_MAX_LEVEL;
            const bool canInvest = unlocked && !mastered && skillPoint > 0;

            const sf::FloatRect nodeRect = GetSkillNodeRect(node.column, node.row);

            sf::Color fillColor(50, 50, 60, 235);
            sf::Color outlineColor(120, 120, 135, 255);

            if (!unlocked)
            {
                fillColor = sf::Color(35, 35, 42, 225);
                outlineColor = sf::Color(75, 75, 85, 255);
            }
            else if (mastered)
            {
                fillColor = sf::Color(70, 58, 25, 240);
                outlineColor = sf::Color(255, 205, 70, 255);
            }
            else if (currentLevel > 0)
            {
                fillColor = sf::Color(32, 55, 82, 235);
                outlineColor = sf::Color(95, 165, 240, 255);
            }
            else if (canInvest)
            {
                fillColor = sf::Color(45, 60, 45, 235);
                outlineColor = sf::Color(130, 220, 130, 255);
            }

            sf::RectangleShape nodeShape;
            nodeShape.setPosition(nodeRect.left, nodeRect.top);
            nodeShape.setSize(sf::Vector2f(nodeRect.width, nodeRect.height));
            nodeShape.setFillColor(fillColor);
            nodeShape.setOutlineThickness(2.0f);
            nodeShape.setOutlineColor(outlineColor);
            window.draw(nodeShape);

            sf::Text nameText;
            nameText.setFont(*font);
            nameText.setCharacterSize(13);
            nameText.setFillColor(unlocked ? sf::Color::White : sf::Color(145, 145, 150));
            nameText.setPosition(nodeRect.left + 8.0f, nodeRect.top + 7.0f);
            nameText.setString(node.title);
            window.draw(nameText);

            sf::Text typeText;
            typeText.setFont(*font);
            typeText.setCharacterSize(10);
            typeText.setFillColor(sf::Color(180, 180, 190));
            typeText.setPosition(nodeRect.left + 8.0f, nodeRect.top + 25.0f);
            typeText.setString(node.typeText);
            window.draw(typeText);

            sf::Text levelText;
            levelText.setFont(*font);
            levelText.setCharacterSize(12);
            levelText.setFillColor(sf::Color(210, 230, 255));
            levelText.setPosition(nodeRect.left + 8.0f, nodeRect.top + 43.0f);
            levelText.setString(
                "Lv." +
                std::to_string(currentLevel) +
                " / " +
                std::to_string(SKILL_MAX_LEVEL)
            );
            window.draw(levelText);

            sf::Text statusText;
            statusText.setFont(*font);
            statusText.setCharacterSize(9);
            statusText.setFillColor(unlocked ? sf::Color(210, 210, 210) : sf::Color(190, 120, 120));
            statusText.setPosition(nodeRect.left + 8.0f, nodeRect.top + 61.0f);
            statusText.setString(GetSkillNodeStatusText(skillLevels, node.skillId, skillPoint));
            window.draw(statusText);

            const sf::FloatRect plusRect = GetSkillPlusButtonRect(node.column, node.row);

            sf::RectangleShape plusButton;
            plusButton.setPosition(plusRect.left, plusRect.top);
            plusButton.setSize(sf::Vector2f(plusRect.width, plusRect.height));
            plusButton.setFillColor(canInvest ? sf::Color(70, 130, 70, 245) : sf::Color(60, 60, 65, 220));
            plusButton.setOutlineThickness(1.0f);
            plusButton.setOutlineColor(canInvest ? sf::Color(170, 255, 170) : sf::Color(105, 105, 110));
            window.draw(plusButton);

            sf::Text plusText;
            plusText.setFont(*font);
            plusText.setCharacterSize(18);
            plusText.setFillColor(canInvest ? sf::Color::White : sf::Color(140, 140, 145));
            plusText.setString("+");
            plusText.setPosition(plusRect.left + 5.0f, plusRect.top - 3.0f);
            window.draw(plusText);
        }
    }

    if (font != nullptr)
    {
        sf::RectangleShape messageBackground;
        messageBackground.setSize(sf::Vector2f(610.0f, 140.0f));
        messageBackground.setPosition(15.0f, static_cast<float>(MAP_PIXEL_SIZE - 150));
        messageBackground.setFillColor(sf::Color(10, 10, 15, 180));
        messageBackground.setOutlineThickness(1.0f);
        messageBackground.setOutlineColor(sf::Color(120, 120, 120));

        window.draw(messageBackground);

        float textY = static_cast<float>(MAP_PIXEL_SIZE - 143);

        for (const std::string& message : messages)
        {
            sf::Text messageText;
            messageText.setFont(*font);
            messageText.setCharacterSize(13);
            messageText.setFillColor(sf::Color::White);
            messageText.setString(
                sf::String::fromUtf8(message.begin(), message.end())
            );
            messageText.setPosition(25.0f, textY);

            window.draw(messageText);

            textY += 16.0f;
        }

        sf::RectangleShape inputBackground;
        inputBackground.setSize(sf::Vector2f(610.0f, 24.0f));
        inputBackground.setPosition(15.0f, static_cast<float>(MAP_PIXEL_SIZE - 28));

        if (chatInputMode)
        {
            inputBackground.setFillColor(sf::Color(35, 35, 50, 230));
            inputBackground.setOutlineColor(sf::Color::Yellow);
        }
        else
        {
            inputBackground.setFillColor(sf::Color(20, 20, 25, 200));
            inputBackground.setOutlineColor(sf::Color(80, 80, 80));
        }

        inputBackground.setOutlineThickness(1.0f);
        window.draw(inputBackground);

        sf::Text inputText;
        inputText.setFont(*font);
        inputText.setCharacterSize(14);
        inputText.setFillColor(sf::Color::White);

        if (chatInputMode)
        {
            std::string inputLine = "> " + chatInput + "_";
            inputText.setString(
                sf::String::fromUtf8(inputLine.begin(), inputLine.end())
            );
        }
        else
        {
            inputText.setString("Press Enter to chat");
        }

        inputText.setPosition(25.0f, static_cast<float>(MAP_PIXEL_SIZE - 25));
        window.draw(inputText);
    }

    if (showWorldMap)
    {
        DrawWorldMapOverlay(window, font, myX, myY, npcs);
    }

    if (font != nullptr && contextMenuOpen)
    {
        const float menuHeight =
            static_cast<float>(contextMenuOptions.size()) *
            CONTEXT_MENU_OPTION_HEIGHT;

        sf::RectangleShape menuBackground;
        menuBackground.setSize(sf::Vector2f(CONTEXT_MENU_WIDTH, menuHeight));
        menuBackground.setPosition(contextMenuX, contextMenuY);
        menuBackground.setFillColor(sf::Color(25, 25, 30, 240));
        menuBackground.setOutlineThickness(1.0f);
        menuBackground.setOutlineColor(sf::Color::White);

        window.draw(menuBackground);

        for (int i = 0; i < static_cast<int>(contextMenuOptions.size()); ++i)
        {
            const sf::FloatRect optionRect =
                GetContextMenuOptionRect(contextMenuX, contextMenuY, i);

            sf::RectangleShape optionBackground;
            optionBackground.setSize(
                sf::Vector2f(optionRect.width, optionRect.height)
            );
            optionBackground.setPosition(optionRect.left, optionRect.top);

            if (contextMenuOptions[i].action == CONTEXT_ACTION_NONE)
            {
                optionBackground.setFillColor(sf::Color(45, 45, 48, 240));
            }
            else
            {
                optionBackground.setFillColor(sf::Color(35, 35, 40, 240));
            }

            window.draw(optionBackground);

            sf::Text optionText;
            optionText.setFont(*font);
            optionText.setCharacterSize(13);

            if (contextMenuOptions[i].action == CONTEXT_ACTION_NONE)
            {
                optionText.setFillColor(sf::Color(150, 150, 150));
            }
            else
            {
                optionText.setFillColor(sf::Color::White);
            }

            optionText.setString(contextMenuOptions[i].text);
            optionText.setPosition(optionRect.left + 8.0f, optionRect.top + 5.0f);

            window.draw(optionText);
        }
    }

    window.display();
}

int main()
{
    WSADATA wsaData{};

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "WSAStartup failed.\n";
        return 1;
    }

    std::string serverIp;
    std::cout << "Input Server IP (default " << DEFAULT_SERVER_IP << "): ";
    std::getline(std::cin, serverIp);

    if (serverIp.empty())
    {
        serverIp = DEFAULT_SERVER_IP;
    }

    SOCKET socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (socketHandle == INVALID_SOCKET)
    {
        std::cout << "socket failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) != 1)
    {
        std::cout << "Invalid server IP: " << serverIp << "\n";
        closesocket(socketHandle);
        WSACleanup();
        return 1;
    }

    if (connect(socketHandle, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        std::cout << "connect failed. server=" << serverIp << ":" << SERVER_PORT << "\n";
        closesocket(socketHandle);
        WSACleanup();
        return 1;
    }

    std::cout << "[CLIENT] Connected to server. " << serverIp << ":" << SERVER_PORT << "\n";

    ClientState state;

    std::thread recvThread(ReceiveThreadMain, socketHandle, std::ref(state));

    while (g_running)
    {
        {
            std::lock_guard<std::mutex> lock(g_loginLock);
            g_loginResultReceived = false;
            g_loginAccepted = false;
            g_loginMessage.clear();
        }

        {
            std::lock_guard<std::mutex> lock(state.lock);
            state.avatarReceived = false;
        }

        std::string username;
        std::cout << "Input ID: ";
        std::cin >> username;

        if (!SendLoginPacket(socketHandle, username))
        {
            std::cout << "Send login failed.\n";
            g_running = false;
            break;
        }

        bool accepted = false;

        {
            std::unique_lock<std::mutex> lock(g_loginLock);

            g_loginCv.wait(
                lock,
                []()
                {
                    return g_loginResultReceived || !g_running;
                }
            );

            accepted = g_loginAccepted;
        }

        if (!g_running)
        {
            break;
        }

        if (!accepted)
        {
            std::cout << "[LOGIN] Login failed. Try again.\n";
            continue;
        }

        {
            std::unique_lock<std::mutex> lock(g_loginLock);

            g_loginCv.wait(
                lock,
                [&state]()
                {
                    std::lock_guard<std::mutex> stateLock(state.lock);
                    return state.avatarReceived || !g_running;
                }
            );
        }

        std::cout << "[LOGIN] Login success. Opening SFML window.\n";
        break;
    }

    if (g_running)
    {
        sf::RenderWindow window(
            sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
            "MMORPG SFML Client"
        );

        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        sf::Font font;
        sf::Font* fontPtr = nullptr;

        if (font.loadFromFile("C:\\Windows\\Fonts\\malgun.ttf") ||
            font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf"))
        {
            fontPtr = &font;
        }

        const auto appStartTime = std::chrono::steady_clock::now();

        while (window.isOpen() && g_running)
        {
            sf::Event event{};

            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                {
                    g_running = false;
                    window.close();
                }

                if (event.type == sf::Event::KeyPressed)
                {
                    bool chatMode = false;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        chatMode = state.chatInputMode;
                    }

                    if (event.key.code == sf::Keyboard::Enter)
                    {
                        std::string messageToSend;

                        {
                            std::lock_guard<std::mutex> lock(state.lock);

                            if (!state.chatInputMode)
                            {
                                state.chatInputMode = true;
                                CloseContextMenu(state);
                                continue;
                            }

                            messageToSend = state.chatInput;
                            state.chatInput.clear();
                            state.chatInputMode = false;
                        }

                        if (!messageToSend.empty())
                        {
                            if (!SendChatPacket(socketHandle, messageToSend))
                            {
                                g_running = false;
                                window.close();
                            }
                        }

                        continue;
                    }

                    if (chatMode)
                    {
                        if (event.key.code == sf::Keyboard::BackSpace)
                        {
                            std::lock_guard<std::mutex> lock(state.lock);

                            if (!state.chatInput.empty())
                            {
                                PopBackUtf8(state.chatInput);
                            }
                        }
                        else if (event.key.code == sf::Keyboard::Escape)
                        {
                            std::lock_guard<std::mutex> lock(state.lock);
                            state.chatInput.clear();
                            state.chatInputMode = false;
                        }

                        continue;
                    }

                    if (event.key.code == sf::Keyboard::Escape)
                    {
                        g_running = false;
                        window.close();
                    }

                    DIRECTION direction{};
                    bool shouldSendMove = false;

                    if (event.key.code == sf::Keyboard::Up)
                    {
                        direction = UP;
                        shouldSendMove = true;
                    }
                    else if (event.key.code == sf::Keyboard::Down)
                    {
                        direction = DOWN;
                        shouldSendMove = true;
                    }
                    else if (event.key.code == sf::Keyboard::Left)
                    {
                        direction = LEFT;
                        shouldSendMove = true;
                    }
                    else if (event.key.code == sf::Keyboard::Right)
                    {
                        direction = RIGHT;
                        shouldSendMove = true;
                    }
                    else if (event.key.code == sf::Keyboard::A)
                    {
                        if (!SendAttackPacket(socketHandle))
                        {
                            g_running = false;
                            window.close();
                        }
                        else
                        {
                            SetPendingEffect(state, EFFECT_ATTACK);
                        }
                    }

                    else if (event.key.code == sf::Keyboard::U)
                    {
                        int rageLevel = 0;
                        int mpSnapshot = 0;

                        {
                            std::lock_guard<std::mutex> lock(state.lock);
                            rageLevel = GetSkillLevelFromArray(
                                state.skillLevels,
                                SKILL_VITALITY_TRAINING
                            );
                            mpSnapshot = state.mp;
                        }

                        if (!SendUseSkillPacket(socketHandle, SKILL_VITALITY_TRAINING))
                        {
                            g_running = false;
                            window.close();
                        }
                        else if (rageLevel > 0 && mpSnapshot >= 20)
                        {
                            StartRageVisual(state);
                        }
                    }

                    else if (event.key.code == sf::Keyboard::Q ||
                        event.key.code == sf::Keyboard::W ||
                        event.key.code == sf::Keyboard::E ||
                        event.key.code == sf::Keyboard::R ||
                        event.key.code == sf::Keyboard::T ||
                        event.key.code == sf::Keyboard::Y)
                    {
                        int skillId = SKILL_NONE;

                        if (event.key.code == sf::Keyboard::Q)
                        {
                            skillId = SKILL_POWER_STRIKE;
                        }
                        else if (event.key.code == sf::Keyboard::W)
                        {
                            skillId = SKILL_HEAVY_SLASH;
                        }
                        else if (event.key.code == sf::Keyboard::E)
                        {
                            skillId = SKILL_WHIRLWIND;
                        }
                        else if (event.key.code == sf::Keyboard::R)
                        {
                            skillId = SKILL_FIREBALL;
                        }
                        else if (event.key.code == sf::Keyboard::T)
                        {
                            skillId = SKILL_MANA_BURST;
                        }
                        else if (event.key.code == sf::Keyboard::Y)
                        {
                            skillId = SKILL_ICE_BOLT;
                        }

                        if (skillId != SKILL_NONE)
                        {
                            if (!SendUseSkillPacket(socketHandle, skillId))
                            {
                                g_running = false;
                                window.close();
                            }
                            else
                            {
                                SetPendingEffect(state, GetEffectTypeForSkill(skillId));
                            }
                        }
                    }

                    else if (event.key.code == sf::Keyboard::F)
                    {
                        bool foundNpc = false;
                        bool toggleShop = false;
                        bool requestHeal = false;
                        bool openSkillWindow = false;
                        bool openEquipmentWindow = false;
                        bool openWorldMap = false;
                        int targetNpcId = -1;
                        std::string npcMessage;

                        {
                            std::lock_guard<std::mutex> lock(state.lock);

                            int bestDistance = 999999;
                            NpcView nearestNpc{};

                            for (const auto& [npcId, npc] : state.npcs)
                            {
                                int dx = state.myX - npc.x;
                                int dy = state.myY - npc.y;

                                if (dx < 0)
                                {
                                    dx = -dx;
                                }

                                if (dy < 0)
                                {
                                    dy = -dy;
                                }

                                const int distance = dx + dy;

                                if (distance <= 3 && distance < bestDistance)
                                {
                                    foundNpc = true;
                                    bestDistance = distance;
                                    nearestNpc = npc;
                                }
                            }

                            if (foundNpc)
                            {
                                targetNpcId = nearestNpc.npcId;

                                switch (nearestNpc.npcType)
                                {
                                case NPC_TYPE_MERCHANT:
                                    toggleShop = true;
                                    npcMessage = "[NPC] " + nearestNpc.name + ": I can buy your monster parts.";
                                    break;

                                case NPC_TYPE_HEALER:
                                    requestHeal = true;
                                    npcMessage = "[NPC] " + nearestNpc.name + ": Restoring your HP and MP.";
                                    break;

                                case NPC_TYPE_BLACKSMITH:
                                    openEquipmentWindow = true;
                                    npcMessage = "[NPC] " + nearestNpc.name + ": Check your weapon, armor, and accessory here.";
                                    break;

                                case NPC_TYPE_SKILL_TRAINER:
                                    openSkillWindow = true;
                                    npcMessage = "[NPC] " + nearestNpc.name + ": Opened the skill window. Spend skill points here.";
                                    break;

                                case NPC_TYPE_GUIDE:
                                    openWorldMap = true;
                                    npcMessage = "[NPC] " + nearestNpc.name + ": M opens the world map. Village, Forest, Field, Camp, Ruins, and Castle Gate are marked.";
                                    break;

                                case NPC_TYPE_GUARD:
                                    npcMessage = "[NPC] " + nearestNpc.name + ": " + GetRegionName(state.myX, state.myY) + " is secured. Be careful outside safe areas.";
                                    break;

                                default:
                                    npcMessage = "[NPC] " + nearestNpc.name + ": Welcome to " + GetRegionName(state.myX, state.myY) + ".";
                                    break;
                                }

                                if (toggleShop)
                                {
                                    state.showShopWindow = !state.showShopWindow;
                                    CloseContextMenu(state);
                                }

                                if (openEquipmentWindow)
                                {
                                    state.showEquipmentWindow = true;
                                    CloseContextMenu(state);
                                }

                                if (openSkillWindow)
                                {
                                    state.showSkillWindow = true;
                                    CloseContextMenu(state);
                                }

                                if (openWorldMap)
                                {
                                    state.showWorldMap = true;
                                    CloseContextMenu(state);
                                }
                            }
                        }

                        if (!foundNpc)
                        {
                            AddClientMessage(state, "[SYS] No NPC nearby.");
                        }
                        else
                        {
                            if (!npcMessage.empty())
                            {
                                AddClientMessage(state, npcMessage);
                            }

                            if (requestHeal)
                            {
                                if (!SendNpcHealPacket(socketHandle, targetNpcId))
                                {
                                    g_running = false;
                                    window.close();
                                }
                            }
                        }
                    }

                    else if (event.key.code == sf::Keyboard::M)
                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        state.showWorldMap = !state.showWorldMap;
                        CloseContextMenu(state);
                    }

                    else if (event.key.code == sf::Keyboard::I)
                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        state.showInventory = !state.showInventory;

                        if (state.showInventory)
                        {
                            state.inventoryScrollOffset = ClampInventoryScrollOffset(
                                state.inventoryScrollOffset,
                                static_cast<int>(state.inventory.size())
                            );
                        }

                        CloseContextMenu(state);
                    }

                    else if (event.key.code == sf::Keyboard::PageUp ||
                        event.key.code == sf::Keyboard::PageDown)
                    {
                        std::lock_guard<std::mutex> lock(state.lock);

                        if (state.showInventory)
                        {
                            const int itemCount = static_cast<int>(state.inventory.size());
                            const int delta =
                                event.key.code == sf::Keyboard::PageUp ?
                                -INVENTORY_VISIBLE_ITEM_COUNT :
                                INVENTORY_VISIBLE_ITEM_COUNT;

                            state.inventoryScrollOffset = ClampInventoryScrollOffset(
                                state.inventoryScrollOffset + delta,
                                itemCount
                            );

                            CloseContextMenu(state);
                        }
                    }

                    else if (event.key.code == sf::Keyboard::S)
                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        state.showStatWindow = !state.showStatWindow;
                        CloseContextMenu(state);
                    }

                    else if (event.key.code == sf::Keyboard::K)
                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        state.showSkillWindow = !state.showSkillWindow;
                        CloseContextMenu(state);
                    }

                    else if (event.key.code == sf::Keyboard::O)
                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        state.showEquipmentWindow = !state.showEquipmentWindow;
                        CloseContextMenu(state);
                    }

                    else if (event.key.code == sf::Keyboard::Z ||
                        event.key.code == sf::Keyboard::X ||
                        event.key.code == sf::Keyboard::C ||
                        event.key.code == sf::Keyboard::V)
                    {
                        bool statWindowOpen = false;

                        {
                            std::lock_guard<std::mutex> lock(state.lock);
                            statWindowOpen = state.showStatWindow;
                        }

                        if (statWindowOpen)
                        {
                            int statType = 0;

                            if (event.key.code == sf::Keyboard::Z)
                            {
                                statType = STAT_STR;
                            }
                            else if (event.key.code == sf::Keyboard::X)
                            {
                                statType = STAT_INT;
                            }
                            else if (event.key.code == sf::Keyboard::C)
                            {
                                statType = STAT_DEX;
                            }
                            else if (event.key.code == sf::Keyboard::V)
                            {
                                statType = STAT_LUK;
                            }

                            if (statType > 0)
                            {
                                if (!SendAddStatPacket(socketHandle, statType))
                                {
                                    g_running = false;
                                    window.close();
                                }
                            }
                        }
                    }

                    else if (event.key.code == sf::Keyboard::Num1 ||
                        event.key.code == sf::Keyboard::Num2 ||
                        event.key.code == sf::Keyboard::Num3 ||
                        event.key.code == sf::Keyboard::Num4 ||
                        event.key.code == sf::Keyboard::Num5 ||
                        event.key.code == sf::Keyboard::Num6 ||
                        event.key.code == sf::Keyboard::Num7 ||
                        event.key.code == sf::Keyboard::Num8 ||
                        event.key.code == sf::Keyboard::Num9 ||
                        event.key.code == sf::Keyboard::Numpad1 ||
                        event.key.code == sf::Keyboard::Numpad2 ||
                        event.key.code == sf::Keyboard::Numpad3 ||
                        event.key.code == sf::Keyboard::Numpad4 ||
                        event.key.code == sf::Keyboard::Numpad5 ||
                        event.key.code == sf::Keyboard::Numpad6 ||
                        event.key.code == sf::Keyboard::Numpad7 ||
                        event.key.code == sf::Keyboard::Numpad8 ||
                        event.key.code == sf::Keyboard::Numpad9)
                    {
                        bool skillWindowOpen = false;

                        {
                            std::lock_guard<std::mutex> lock(state.lock);
                            skillWindowOpen = state.showSkillWindow;
                        }

                        if (skillWindowOpen)
                        {
                            int skillIndex = -1;

                            if (event.key.code == sf::Keyboard::Num1 ||
                                event.key.code == sf::Keyboard::Numpad1)
                            {
                                skillIndex = 0;
                            }
                            else if (event.key.code == sf::Keyboard::Num2 ||
                                event.key.code == sf::Keyboard::Numpad2)
                            {
                                skillIndex = 1;
                            }
                            else if (event.key.code == sf::Keyboard::Num3 ||
                                event.key.code == sf::Keyboard::Numpad3)
                            {
                                skillIndex = 2;
                            }
                            else if (event.key.code == sf::Keyboard::Num4 ||
                                event.key.code == sf::Keyboard::Numpad4)
                            {
                                skillIndex = 3;
                            }
                            else if (event.key.code == sf::Keyboard::Num5 ||
                                event.key.code == sf::Keyboard::Numpad5)
                            {
                                skillIndex = 4;
                            }
                            else if (event.key.code == sf::Keyboard::Num6 ||
                                event.key.code == sf::Keyboard::Numpad6)
                            {
                                skillIndex = 5;
                            }
                            else if (event.key.code == sf::Keyboard::Num7 ||
                                event.key.code == sf::Keyboard::Numpad7)
                            {
                                skillIndex = 6;
                            }
                            else if (event.key.code == sf::Keyboard::Num8 ||
                                event.key.code == sf::Keyboard::Numpad8)
                            {
                                skillIndex = 7;
                            }
                            else if (event.key.code == sf::Keyboard::Num9 ||
                                event.key.code == sf::Keyboard::Numpad9)
                            {
                                skillIndex = 8;
                            }

                            const int skillId = GetSkillIdByIndex(skillIndex);

                            if (skillId != SKILL_NONE)
                            {
                                if (!SendAddSkillPointPacket(socketHandle, skillId))
                                {
                                    g_running = false;
                                    window.close();
                                }
                            }

                            continue;
                        }

                        int slotIndex = -1;

                        if (event.key.code == sf::Keyboard::Num1 ||
                            event.key.code == sf::Keyboard::Numpad1)
                        {
                            slotIndex = 0;
                        }
                        else if (event.key.code == sf::Keyboard::Num2 ||
                            event.key.code == sf::Keyboard::Numpad2)
                        {
                            slotIndex = 1;
                        }
                        else if (event.key.code == sf::Keyboard::Num3 ||
                            event.key.code == sf::Keyboard::Numpad3)
                        {
                            slotIndex = 2;
                        }
                        else if (event.key.code == sf::Keyboard::Num4 ||
                            event.key.code == sf::Keyboard::Numpad4)
                        {
                            slotIndex = 3;
                        }
                        else if (event.key.code == sf::Keyboard::Num5 ||
                            event.key.code == sf::Keyboard::Numpad5)
                        {
                            slotIndex = 4;
                        }

                        QuickSlotView slot{};

                        {
                            std::lock_guard<std::mutex> lock(state.lock);

                            if (slotIndex >= 0 && slotIndex < QUICK_SLOT_COUNT)
                            {
                                slot = state.quickSlots[slotIndex];
                            }
                        }

                        if (slot.slotType == QUICK_SLOT_ITEM && slot.targetId > 0)
                        {
                            if (!SendUseItem(socketHandle, slot.targetId))
                            {
                                g_running = false;
                                window.close();
                            }
                        }
                    }

                    if (shouldSendMove)
                    {
                        const auto now = std::chrono::steady_clock::now();

                        const int moveTime = static_cast<int>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - appStartTime
                            ).count()
                            );

                        if (!SendMovePacket(socketHandle, direction, moveTime))
                        {
                            g_running = false;
                            window.close();
                        }
                    }
                }

                if (event.type == sf::Event::MouseWheelScrolled)
                {
                    const float mouseX = static_cast<float>(event.mouseWheelScroll.x);
                    const float mouseY = static_cast<float>(event.mouseWheelScroll.y);

                    std::lock_guard<std::mutex> lock(state.lock);

                    if (state.showInventory &&
                        GetInventoryPanelRect().contains(mouseX, mouseY))
                    {
                        const int itemCount = static_cast<int>(state.inventory.size());
                        const int delta = event.mouseWheelScroll.delta > 0.0f ? -1 : 1;

                        state.inventoryScrollOffset = ClampInventoryScrollOffset(
                            state.inventoryScrollOffset + delta,
                            itemCount
                        );

                        CloseContextMenu(state);
                        continue;
                    }
                }

                if (event.type == sf::Event::MouseButtonPressed &&
                    event.mouseButton.button == sf::Mouse::Left)
                {
                    const float mouseX = static_cast<float>(event.mouseButton.x);
                    const float mouseY = static_cast<float>(event.mouseButton.y);

                    bool menuWasOpen = false;
                    float menuX = 0.0f;
                    float menuY = 0.0f;
                    int targetItemId = 0;
                    int targetItemCategory = 0;
                    std::string targetItemName;
                    std::vector<ContextMenuOption> menuOptions;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);

                        menuWasOpen = state.contextMenuOpen;
                        menuX = state.contextMenuX;
                        menuY = state.contextMenuY;
                        targetItemId = state.contextTargetItemId;
                        targetItemCategory = state.contextTargetItemCategory;
                        targetItemName = state.contextTargetItemName;
                        menuOptions = state.contextMenuOptions;
                    }

                    if (menuWasOpen)
                    {
                        bool clickedMenuOption = false;

                        for (int i = 0; i < static_cast<int>(menuOptions.size()); ++i)
                        {
                            const sf::FloatRect optionRect =
                                GetContextMenuOptionRect(menuX, menuY, i);

                            if (!optionRect.contains(mouseX, mouseY))
                            {
                                continue;
                            }

                            clickedMenuOption = true;

                            const ContextMenuOption& option = menuOptions[i];

                            if (option.action == CONTEXT_ACTION_USE_ITEM)
                            {
                                if (targetItemId > 0)
                                {
                                    if (!SendUseItem(socketHandle, targetItemId))
                                    {
                                        g_running = false;
                                        window.close();
                                    }
                                }
                            }
                            else if (option.action == CONTEXT_ACTION_EQUIP_ITEM)
                            {
                                if (targetItemId > 0)
                                {
                                    if (!SendUseItem(socketHandle, targetItemId))
                                    {
                                        g_running = false;
                                        window.close();
                                    }
                                }
                            }
                            else if (option.action == CONTEXT_ACTION_UNEQUIP_EQUIPMENT)
                            {
                                if (option.value > 0)
                                {
                                    if (!SendUnequipEquipment(socketHandle, option.value))
                                    {
                                        g_running = false;
                                        window.close();
                                    }
                                }
                            }
                            else if (option.action == CONTEXT_ACTION_SET_QUICK_SLOT)
                            {
                                if (targetItemId > 0 &&
                                    targetItemCategory == ITEM_CATEGORY_CONSUMABLE &&
                                    option.value >= 0 &&
                                    option.value < QUICK_SLOT_COUNT)
                                {
                                    if (!SendSetQuickSlot(
                                        socketHandle,
                                        option.value,
                                        QUICK_SLOT_TYPE_ITEM,
                                        targetItemId,
                                        targetItemName
                                    ))
                                    {
                                        g_running = false;
                                        window.close();
                                    }
                                    else
                                    {
                                        std::lock_guard<std::mutex> lock(state.lock);

                                        state.quickSlots[option.value].slotType = QUICK_SLOT_ITEM;
                                        state.quickSlots[option.value].targetId = targetItemId;
                                        state.quickSlots[option.value].name = targetItemName;
                                    }
                                }
                            }
                            else if (option.action == CONTEXT_ACTION_CLEAR_QUICK_SLOT)
                            {
                                if (option.value >= 0 && option.value < QUICK_SLOT_COUNT)
                                {
                                    if (!SendClearQuickSlot(socketHandle, option.value))
                                    {
                                        g_running = false;
                                        window.close();
                                    }
                                    else
                                    {
                                        std::lock_guard<std::mutex> lock(state.lock);

                                        state.quickSlots[option.value] = QuickSlotView{};
                                    }
                                }
                            }

                            {
                                std::lock_guard<std::mutex> lock(state.lock);
                                CloseContextMenu(state);
                            }

                            break;
                        }

                        if (!clickedMenuOption)
                        {
                            std::lock_guard<std::mutex> lock(state.lock);
                            CloseContextMenu(state);
                        }

                        continue;
                    }


                    bool skillTreeOpenForClick = false;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        skillTreeOpenForClick = state.showSkillWindow;
                    }

                    if (skillTreeOpenForClick)
                    {
                        const int clickedSkillId = GetSkillIdFromPlusButtonPosition(mouseX, mouseY);

                        if (clickedSkillId != SKILL_NONE)
                        {
                            if (!SendAddSkillPointPacket(socketHandle, clickedSkillId))
                            {
                                g_running = false;
                                window.close();
                            }

                            continue;
                        }
                    }

                    bool shopOpen = false;
                    std::unordered_map<int, InventoryView> shopInventorySnapshot;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);

                        shopOpen = state.showShopWindow;
                        shopInventorySnapshot = state.inventory;
                    }

                    if (shopOpen)
                    {
                        bool handledShopClick = false;

                        const std::vector<InventoryView> sellItems =
                            BuildShopSellList(shopInventorySnapshot);

                        const int maxVisibleSellItems = 7;

                        for (int i = 0;
                            i < static_cast<int>(sellItems.size()) &&
                            i < maxVisibleSellItems;
                            ++i)
                        {
                            const sf::FloatRect itemRect = GetShopItemRect(i);

                            if (!itemRect.contains(mouseX, mouseY))
                            {
                                continue;
                            }

                            if (!SendSellItem(socketHandle, sellItems[i].itemId, 1))
                            {
                                g_running = false;
                                window.close();
                            }

                            handledShopClick = true;
                            break;
                        }

                        if (handledShopClick)
                        {
                            continue;
                        }
                    }

                    bool inventoryOpen = false;
                    int inventoryScrollOffsetSnapshot = 0;
                    std::unordered_map<int, InventoryView> inventorySnapshot;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);

                        inventoryOpen = state.showInventory;
                        inventoryScrollOffsetSnapshot = state.inventoryScrollOffset;
                        inventorySnapshot = state.inventory;
                    }

                    if (inventoryOpen)
                    {
                        bool handledInventoryClick = false;

                        const std::vector<InventoryView> inventoryItems =
                            BuildInventoryList(inventorySnapshot);

                        const int itemCount = static_cast<int>(inventoryItems.size());
                        const int scrollOffset = ClampInventoryScrollOffset(
                            inventoryScrollOffsetSnapshot,
                            itemCount
                        );

                        for (int visibleIndex = 0;
                            visibleIndex < INVENTORY_VISIBLE_ITEM_COUNT &&
                            scrollOffset + visibleIndex < itemCount;
                            ++visibleIndex)
                        {
                            const sf::FloatRect itemRect = GetInventoryItemRect(visibleIndex);

                            if (!itemRect.contains(mouseX, mouseY))
                            {
                                continue;
                            }

                            const int itemIndex = GetInventoryItemIndexFromVisibleIndex(
                                visibleIndex,
                                scrollOffset
                            );
                            const InventoryView& item = inventoryItems[itemIndex];

                            {
                                std::lock_guard<std::mutex> lock(state.lock);

                                state.selectedInventoryItemId = 0;
                                state.selectedInventoryItemCategory = 0;
                                state.selectedInventoryItemName.clear();

                                OpenInventoryItemContextMenu(state, mouseX, mouseY, item);
                            }

                            handledInventoryClick = true;
                            break;
                        }

                        if (handledInventoryClick)
                        {
                            continue;
                        }
                    }

                    bool equipmentOpen = false;
                    int equippedWeaponSnapshot = 0;
                    int equippedArmorSnapshot = 0;
                    int equippedAccessorySnapshot = 0;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);

                        equipmentOpen = state.showEquipmentWindow;
                        equippedWeaponSnapshot = state.equippedWeaponItemId;
                        equippedArmorSnapshot = state.equippedArmorItemId;
                        equippedAccessorySnapshot = state.equippedAccessoryItemId;
                    }

                    if (equipmentOpen)
                    {
                        int clickedEquipmentSlot = 0;

                        if (equippedWeaponSnapshot > 0 &&
                            GetEquipmentWeaponSlotRect().contains(mouseX, mouseY))
                        {
                            clickedEquipmentSlot = EQUIPMENT_SLOT_WEAPON;
                        }
                        else if (equippedArmorSnapshot > 0 &&
                            GetEquipmentArmorSlotRect().contains(mouseX, mouseY))
                        {
                            clickedEquipmentSlot = EQUIPMENT_SLOT_ARMOR;
                        }
                        else if (equippedAccessorySnapshot > 0 &&
                            GetEquipmentAccessorySlotRect().contains(mouseX, mouseY))
                        {
                            clickedEquipmentSlot = EQUIPMENT_SLOT_ACCESSORY;
                        }

                        if (clickedEquipmentSlot > 0)
                        {
                            std::lock_guard<std::mutex> lock(state.lock);
                            OpenEquipmentContextMenu(
                                state,
                                mouseX,
                                mouseY,
                                clickedEquipmentSlot
                            );
                            continue;
                        }
                    }

                    for (int i = 0; i < QUICK_SLOT_COUNT; ++i)
                    {
                        const sf::FloatRect quickSlotRect = GetQuickSlotRect(i);

                        if (!quickSlotRect.contains(mouseX, mouseY))
                        {
                            continue;
                        }

                        {
                            std::lock_guard<std::mutex> lock(state.lock);

                            if (state.quickSlots[i].slotType != QUICK_SLOT_EMPTY)
                            {
                                OpenQuickSlotContextMenu(state, mouseX, mouseY, i);
                            }
                        }

                        break;
                    }
                }

                if (event.type == sf::Event::TextEntered)
                {
                    bool chatMode = false;

                    {
                        std::lock_guard<std::mutex> lock(state.lock);
                        chatMode = state.chatInputMode;
                    }

                    if (chatMode)
                    {
                        const sf::Uint32 unicode = event.text.unicode;

                        if (unicode >= 32 && unicode != 127)
                        {
                            const std::string utf8Text = EncodeUtf8(unicode);

                            std::lock_guard<std::mutex> lock(state.lock);

                            if (state.chatInput.size() + utf8Text.size() < CHAT_MESSAGE_LEN)
                            {
                                state.chatInput += utf8Text;
                            }
                        }
                    }
                }

            }

            DrawGame(window, state, fontPtr);
        }
    }

    g_running = false;

    shutdown(socketHandle, SD_BOTH);
    closesocket(socketHandle);

    if (recvThread.joinable())
    {
        recvThread.join();
    }

    WSACleanup();

    return 0;
}