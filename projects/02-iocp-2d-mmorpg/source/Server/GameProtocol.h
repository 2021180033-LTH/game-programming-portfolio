#pragma once

#include "protocol_2026.h"


constexpr int MONSTER_SLIME = 1;
constexpr int MONSTER_GOBLIN = 2;
constexpr int MONSTER_WOLF = 3;
constexpr int MONSTER_ORC = 4;
constexpr int MONSTER_SKELETON = 5;
constexpr int MONSTER_DARK_MAGE = 6;



constexpr int ITEM_CATEGORY_EQUIPMENT = 1;
constexpr int ITEM_CATEGORY_CONSUMABLE = 2;
constexpr int ITEM_CATEGORY_ETC = 3;
constexpr int EQUIPMENT_NAME_LEN = 32;
constexpr int QUICK_SLOT_NAME_LEN = 32;
constexpr int MAX_QUICK_SLOT_COUNT = 5;

constexpr int QUICK_SLOT_TYPE_EMPTY = 0;
constexpr int QUICK_SLOT_TYPE_ITEM = 1;
constexpr int QUICK_SLOT_TYPE_SKILL = 2;

constexpr int NPC_TYPE_MERCHANT = 1;
constexpr int NPC_TYPE_BLACKSMITH = 2;
constexpr int NPC_TYPE_HEALER = 3;
constexpr int NPC_TYPE_SKILL_TRAINER = 4;
constexpr int NPC_TYPE_GUARD = 5;
constexpr int NPC_TYPE_GUIDE = 6;
constexpr int EQUIPMENT_SLOT_WEAPON = 1;
constexpr int EQUIPMENT_SLOT_ARMOR = 2;
constexpr int EQUIPMENT_SLOT_ACCESSORY = 3;

constexpr int ITEM_MONSTER_PART_BASE = 1000;
constexpr int ITEM_MONSTER_PART = ITEM_MONSTER_PART_BASE; // Legacy base value. Do not use as a real inventory item.

constexpr int ITEM_SLIME_PART = ITEM_MONSTER_PART_BASE + MONSTER_SLIME;
constexpr int ITEM_GOBLIN_PART = ITEM_MONSTER_PART_BASE + MONSTER_GOBLIN;
constexpr int ITEM_WOLF_PART = ITEM_MONSTER_PART_BASE + MONSTER_WOLF;
constexpr int ITEM_ORC_PART = ITEM_MONSTER_PART_BASE + MONSTER_ORC;
constexpr int ITEM_SKELETON_PART = ITEM_MONSTER_PART_BASE + MONSTER_SKELETON;
constexpr int ITEM_DARK_MAGE_PART = ITEM_MONSTER_PART_BASE + MONSTER_DARK_MAGE;

constexpr int ITEM_HP_POTION = 2001;
constexpr int ITEM_MP_POTION = 2002;
constexpr int ITEM_HIGH_HP_POTION = 2003;
constexpr int ITEM_HIGH_MP_POTION = 2004;
constexpr int ITEM_RETURN_SCROLL = 2005;

constexpr int ITEM_IRON_SWORD = 3001;
constexpr int ITEM_IRON_ARMOR = 3002;
constexpr int ITEM_MAGIC_RING = 3003;
constexpr int ITEM_STEEL_SWORD = 3004;
constexpr int ITEM_FLAME_SWORD = 3005;
constexpr int ITEM_MAGE_STAFF = 3006;
constexpr int ITEM_ICE_WAND = 3007;
constexpr int ITEM_HUNTER_BOW = 3008;
constexpr int ITEM_STEEL_ARMOR = 3009;
constexpr int ITEM_MAGIC_ROBE = 3010;
constexpr int ITEM_LEATHER_ARMOR = 3011;
constexpr int ITEM_WARRIOR_RING = 3012;
constexpr int ITEM_LUCKY_NECKLACE = 3013;
constexpr int ITEM_SPEED_CHARM = 3014;


constexpr int STAT_STR = 1;
constexpr int STAT_INT = 2;
constexpr int STAT_DEX = 3;
constexpr int STAT_LUK = 4;


constexpr int SKILL_NONE = 0;

constexpr int SKILL_POWER_STRIKE = 1001;
constexpr int SKILL_HEAVY_SLASH = 1002;
constexpr int SKILL_WHIRLWIND = 1003;

constexpr int SKILL_FIREBALL = 2001;
constexpr int SKILL_ICE_BOLT = 2002;
constexpr int SKILL_MANA_BURST = 2003;

constexpr int SKILL_RAGE = 3001;
constexpr int SKILL_VITALITY_TRAINING = SKILL_RAGE; 
constexpr int SKILL_MANA_TRAINING = 3002;
constexpr int SKILL_LUCKY_HUNTER = 3003;

constexpr int SKILL_MAX_LEVEL = 5;
constexpr int MAX_SKILL_STATUS_COUNT = 9;

constexpr PACKET_TYPE S2C_ADD_MONSTER =
static_cast<PACKET_TYPE>(100);

constexpr PACKET_TYPE S2C_REMOVE_MONSTER =
static_cast<PACKET_TYPE>(101);

constexpr PACKET_TYPE S2C_MOVE_MONSTER =
static_cast<PACKET_TYPE>(102);

constexpr PACKET_TYPE S2C_ADD_NPC =
static_cast<PACKET_TYPE>(150);


constexpr PACKET_TYPE C2S_ATTACK =
static_cast<PACKET_TYPE>(300);

constexpr PACKET_TYPE S2C_MONSTER_HP =
static_cast<PACKET_TYPE>(301);

constexpr PACKET_TYPE S2C_BATTLE_MESSAGE =
static_cast<PACKET_TYPE>(302);

constexpr PACKET_TYPE S2C_PLAYER_STATUS =
static_cast<PACKET_TYPE>(303);

constexpr PACKET_TYPE S2C_ADD_DROP =
static_cast<PACKET_TYPE>(400);

constexpr PACKET_TYPE S2C_REMOVE_DROP =
static_cast<PACKET_TYPE>(401);

constexpr PACKET_TYPE S2C_INVENTORY_ITEM_UPDATE =
static_cast<PACKET_TYPE>(402);

constexpr PACKET_TYPE C2S_USE_ITEM =
static_cast<PACKET_TYPE>(403);

constexpr PACKET_TYPE C2S_ADD_STAT =
static_cast<PACKET_TYPE>(501);

constexpr PACKET_TYPE C2S_ADD_SKILL_POINT =
static_cast<PACKET_TYPE>(502);

constexpr PACKET_TYPE S2C_SKILL_STATUS =
static_cast<PACKET_TYPE>(503);

constexpr PACKET_TYPE C2S_USE_SKILL =
static_cast<PACKET_TYPE>(504);

constexpr PACKET_TYPE S2C_EQUIPMENT_STATUS =
static_cast<PACKET_TYPE>(505);

constexpr PACKET_TYPE C2S_UNEQUIP_EQUIPMENT =
static_cast<PACKET_TYPE>(506);

constexpr PACKET_TYPE C2S_SELL_ITEM =
static_cast<PACKET_TYPE>(507);

constexpr PACKET_TYPE C2S_SET_QUICK_SLOT =
static_cast<PACKET_TYPE>(508);

constexpr PACKET_TYPE C2S_CLEAR_QUICK_SLOT =
static_cast<PACKET_TYPE>(509);

constexpr PACKET_TYPE S2C_QUICK_SLOT_STATUS =
static_cast<PACKET_TYPE>(510);

constexpr PACKET_TYPE C2S_NPC_HEAL =
static_cast<PACKET_TYPE>(511);


constexpr int CHAT_MESSAGE_LEN = 128;

constexpr PACKET_TYPE C2S_CHAT =
static_cast<PACKET_TYPE>(410);

constexpr PACKET_TYPE S2C_CHAT =
static_cast<PACKET_TYPE>(411);

#pragma pack(push, 1)


struct S2C_AddMonster
{
    unsigned char size;
    PACKET_TYPE type;

    int monsterId;
    int monsterType;

    char name[MAX_NAME_LEN];

    int level;
    int hp;
    int maxHp;

    short x;
    short y;
};

struct S2C_RemoveMonster
{
    unsigned char size;
    PACKET_TYPE type;

    int monsterId;
};

struct S2C_MoveMonster
{
    unsigned char size;
    PACKET_TYPE type;

    int monsterId;
    short x;
    short y;
};

struct S2C_AddNpc
{
    unsigned char size;
    PACKET_TYPE type;

    int npcId;
    int npcType;

    char name[MAX_NAME_LEN];

    short x;
    short y;
};


struct C2S_Attack
{
    unsigned char size;
    PACKET_TYPE type;
};

struct S2C_MonsterHp
{
    unsigned char size;
    PACKET_TYPE type;

    int monsterId;
    int hp;
    int maxHp;
};

struct S2C_BattleMessage
{
    unsigned char size;
    PACKET_TYPE type;

    char message[100];
};

struct S2C_PlayerStatus
{
    unsigned char size;
    PACKET_TYPE type;

    int playerId;

    int hp;
    int maxHp;

    int mp;
    int maxMp;

    int level;
    int exp;
    int requiredExp;

    int statPoint;
    int skillPoint;
    int gold;

    int str;
    int intStat;
    int dex;
    int luk;

    int expGained;
    int levelGained;
};

struct S2C_AddDrop
{
    unsigned char size;
    PACKET_TYPE type;

    int dropId;
    int ownerPlayerId;

    int itemId;
    int itemCategory;
    int count;

    int x;
    int y;

    char itemName[32];
};

struct S2C_RemoveDrop
{
    unsigned char size;
    PACKET_TYPE type;

    int dropId;
};

struct S2C_InventoryItemUpdate
{
    unsigned char size;
    PACKET_TYPE type;

    int itemId;
    int itemCategory;
    int count;

    char itemName[32];
};

struct C2S_UseItem
{
    unsigned char size;
    PACKET_TYPE type;

    int itemId;
};

struct C2S_UnequipEquipment
{
    unsigned char size;
    PACKET_TYPE type;

    int equipmentSlot;
};

struct C2S_SellItem
{
    unsigned char size;
    PACKET_TYPE type;

    int itemId;
    int count;
};

struct QuickSlotStatusEntry
{
    int slotIndex;
    int slotType;
    int targetId;
    char name[QUICK_SLOT_NAME_LEN];
};

struct C2S_SetQuickSlot
{
    unsigned char size;
    PACKET_TYPE type;

    int slotIndex;
    int slotType;
    int targetId;
    char name[QUICK_SLOT_NAME_LEN];
};

struct C2S_ClearQuickSlot
{
    unsigned char size;
    PACKET_TYPE type;

    int slotIndex;
};

struct C2S_NpcHeal
{
    unsigned char size;
    PACKET_TYPE type;

    int npcId;
};

struct S2C_QuickSlotStatus
{
    unsigned char size;
    PACKET_TYPE type;

    int slotCount;
    QuickSlotStatusEntry slots[MAX_QUICK_SLOT_COUNT];
};

struct C2S_AddStat
{
    unsigned char size;
    PACKET_TYPE type;

    int statType;
};

struct C2S_AddSkillPoint
{
    unsigned char size;
    PACKET_TYPE type;

    int skillId;
};

struct C2S_UseSkill
{
    unsigned char size;
    PACKET_TYPE type;

    int skillId;
};

struct SkillStatusEntry
{
    int skillId;
    int level;
};

struct S2C_SkillStatus
{
    unsigned char size;
    PACKET_TYPE type;

    int skillPoint;
    int skillCount;

    SkillStatusEntry skills[MAX_SKILL_STATUS_COUNT];
};

struct S2C_EquipmentStatus
{
    unsigned char size;
    PACKET_TYPE type;

    int playerId;

    int weaponItemId;
    char weaponName[EQUIPMENT_NAME_LEN];

    int armorItemId;
    char armorName[EQUIPMENT_NAME_LEN];

    int accessoryItemId;
    char accessoryName[EQUIPMENT_NAME_LEN];

    int strBonus;
    int intBonus;
    int dexBonus;
    int lukBonus;

    int physicalAttackBonus;
    int physicalDefenseBonus;
    int magicAttackBonus;
    int magicDefenseBonus;
};

struct C2S_Chat
{
    unsigned char size;
    PACKET_TYPE type;

    char message[CHAT_MESSAGE_LEN];
};

struct S2C_Chat
{
    unsigned char size;
    PACKET_TYPE type;

    int playerId;
    char username[MAX_NAME_LEN];
    char message[CHAT_MESSAGE_LEN];
};

#pragma pack(pop)