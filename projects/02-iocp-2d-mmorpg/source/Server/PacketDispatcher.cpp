#include "PacketDispatcher.h"


namespace
{
    std::string ExtractFixedString(const char* text, int maxLen)
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
}

PacketDispatcher::PacketDispatcher(SendFunction sendFunction, GameWorld* gameWorld)
    : sendFunction_(std::move(sendFunction)),
    gameWorld_(gameWorld)
{
}

void PacketDispatcher::Dispatch(ClientSession* session, const char* packet, int packetSize)
{
    if (session == nullptr || packet == nullptr)
    {
        return;
    }

    if (packetSize < static_cast<int>(1 + sizeof(PACKET_TYPE)))
    {
        Logger::Warn("Invalid packet header size.");
        return;
    }

    const unsigned char declaredSize = static_cast<unsigned char>(packet[0]);

    if (declaredSize != packetSize)
    {
        Logger::Warn(
            "Packet size mismatch. declared=" +
            std::to_string(declaredSize) +
            ", actual=" +
            std::to_string(packetSize)
        );
        return;
    }

    PACKET_TYPE packetType{};
    std::memcpy(&packetType, packet + 1, sizeof(PACKET_TYPE));

    switch (packetType)
    {
    case C2S_LOGIN:
        HandleLogin(session, packet, packetSize);
        break;

    case C2S_MOVE:
        HandleMove(session, packet, packetSize);
        break;

    case C2S_ATTACK:
        HandleAttack(session, packet, packetSize);
        break;

    case C2S_CHAT:
        HandleChat(session, packet, packetSize);
        break;

    case C2S_USE_ITEM:
    {
        if (packetSize != sizeof(C2S_UseItem))
        {
            Logger::Info("Invalid C2S_USE_ITEM size.");
            return;
        }

        C2S_UseItem request{};
        std::memcpy(&request, packet, sizeof(request));

        if (gameWorld_ != nullptr)
        {
            gameWorld_->UseItem(session, request.itemId);
        }

        break;
    }

    case C2S_UNEQUIP_EQUIPMENT:
    {
        if (packetSize != sizeof(C2S_UnequipEquipment))
        {
            Logger::Info("Invalid C2S_UNEQUIP_EQUIPMENT size.");
            return;
        }

        C2S_UnequipEquipment request{};
        std::memcpy(&request, packet, sizeof(request));

        if (gameWorld_ != nullptr)
        {
            gameWorld_->UnequipEquipment(session, request.equipmentSlot);
        }

        break;
    }

    case C2S_SELL_ITEM:
    {
        if (packetSize != sizeof(C2S_SellItem))
        {
            Logger::Info("Invalid C2S_SELL_ITEM size.");
            return;
        }

        C2S_SellItem request{};
        std::memcpy(&request, packet, sizeof(request));

        if (gameWorld_ != nullptr)
        {
            gameWorld_->SellItem(session, request.itemId, request.count);
        }

        break;
    }

    case C2S_SET_QUICK_SLOT:
    {
        if (packetSize != sizeof(C2S_SetQuickSlot))
        {
            Logger::Info("Invalid C2S_SET_QUICK_SLOT size.");
            return;
        }

        C2S_SetQuickSlot request{};
        std::memcpy(&request, packet, sizeof(request));

        if (gameWorld_ != nullptr)
        {
            gameWorld_->SetQuickSlot(
                session,
                request.slotIndex,
                request.slotType,
                request.targetId,
                ExtractFixedString(request.name, sizeof(request.name))
            );
        }

        break;
    }

    case C2S_CLEAR_QUICK_SLOT:
    {
        if (packetSize != sizeof(C2S_ClearQuickSlot))
        {
            Logger::Info("Invalid C2S_CLEAR_QUICK_SLOT size.");
            return;
        }

        C2S_ClearQuickSlot request{};
        std::memcpy(&request, packet, sizeof(request));

        if (gameWorld_ != nullptr)
        {
            gameWorld_->ClearQuickSlot(session, request.slotIndex);
        }

        break;
    }


    case C2S_NPC_HEAL:
    {
        if (packetSize != sizeof(C2S_NpcHeal))
        {
            Logger::Info("Invalid C2S_NPC_HEAL size.");
            return;
        }

        C2S_NpcHeal request{};
        std::memcpy(&request, packet, sizeof(request));

        if (gameWorld_ != nullptr)
        {
            gameWorld_->UseHealerNpc(session, request.npcId);
        }

        break;
    }

    case C2S_USE_SKILL:
        HandleUseSkill(session, packet, packetSize);
        break;

    case C2S_ADD_STAT:
        HandleAddStat(session, packet, packetSize);
        break;

    case C2S_ADD_SKILL_POINT:
        HandleAddSkillPoint(session, packet, packetSize);
        break;

    default:
        HandleUnknown(session, packet, packetSize);
        break;
    }
}

void PacketDispatcher::HandleLogin(ClientSession* session, const char* packet, int packetSize)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (packetSize != sizeof(C2S_Login))
    {
        Logger::Warn("Invalid C2S_Login size.");
        SendLoginResult(session, false, "Invalid login packet.");
        return;
    }

    C2S_Login loginPacket{};
    std::memcpy(&loginPacket, packet, sizeof(C2S_Login));

    std::string username = ExtractUsername(loginPacket.username);

    std::string failMessage;

    if (!gameWorld_->BeginLoginPlayer(session, username, failMessage))
    {
        SendLoginResult(session, false, failMessage);
        return;
    }

}

void PacketDispatcher::HandleMove(ClientSession* session, const char* packet, int packetSize)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (!session->IsLoggedIn())
    {
        Logger::Warn(
            "Move packet before login. session=" +
            std::to_string(session->GetSessionId())
        );
        return;
    }

    if (packetSize != sizeof(C2S_Move))
    {
        Logger::Warn("Invalid C2S_Move size.");
        return;
    }

    C2S_Move movePacket{};
    std::memcpy(&movePacket, packet, sizeof(C2S_Move));

    if (!gameWorld_->MovePlayer(session, movePacket.dir))
    {
        return;
    }

    gameWorld_->UpdatePlayerVisibilityFor(session);
    gameWorld_->UpdateMonsterVisibilityFor(session);
    gameWorld_->BroadcastMove(session, movePacket.move_time);
}

void PacketDispatcher::HandleChat(
    ClientSession* session,
    const char* packet,
    int packetSize
)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (!session->IsLoggedIn())
    {
        Logger::Warn(
            "Chat packet before login. session=" +
            std::to_string(session->GetSessionId())
        );
        return;
    }

    if (packetSize != sizeof(C2S_Chat))
    {
        Logger::Warn("Invalid C2S_Chat size.");
        return;
    }

    C2S_Chat chatPacket{};
    std::memcpy(&chatPacket, packet, sizeof(C2S_Chat));

    const std::string message = ExtractFixedString(
        chatPacket.message,
        static_cast<int>(sizeof(chatPacket.message))
    );

    gameWorld_->Chat(session, message);
}

void PacketDispatcher::HandleUseSkill(
    ClientSession* session,
    const char* packet,
    int packetSize
)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (!session->IsLoggedIn())
    {
        Logger::Warn(
            "Use skill packet before login. session=" +
            std::to_string(session->GetSessionId())
        );
        return;
    }

    if (packetSize != sizeof(C2S_UseSkill))
    {
        Logger::Warn("Invalid C2S_UseSkill size.");
        return;
    }

    C2S_UseSkill request{};
    std::memcpy(&request, packet, sizeof(request));

    gameWorld_->UseSkill(session, request.skillId);
}

void PacketDispatcher::HandleAddStat(
    ClientSession* session,
    const char* packet,
    int packetSize
)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (!session->IsLoggedIn())
    {
        Logger::Warn(
            "Add stat packet before login. session=" +
            std::to_string(session->GetSessionId())
        );
        return;
    }

    if (packetSize != sizeof(C2S_AddStat))
    {
        Logger::Warn("Invalid C2S_AddStat size.");
        return;
    }

    C2S_AddStat request{};
    std::memcpy(&request, packet, sizeof(request));

    gameWorld_->AddPlayerStat(session, request.statType);
}

void PacketDispatcher::HandleAddSkillPoint(
    ClientSession* session,
    const char* packet,
    int packetSize
)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (!session->IsLoggedIn())
    {
        Logger::Warn(
            "Add skill point packet before login. session=" +
            std::to_string(session->GetSessionId())
        );
        return;
    }

    if (packetSize != sizeof(C2S_AddSkillPoint))
    {
        Logger::Warn("Invalid C2S_AddSkillPoint size.");
        return;
    }

    C2S_AddSkillPoint request{};
    std::memcpy(&request, packet, sizeof(request));

    gameWorld_->AddPlayerSkillPoint(session, request.skillId);
}

void PacketDispatcher::HandleUnknown(ClientSession* session, const char* packet, int packetSize)
{
    if (session == nullptr)
    {
        return;
    }

    PACKET_TYPE packetType{};

    if (packet != nullptr && packetSize >= static_cast<int>(1 + sizeof(PACKET_TYPE)))
    {
        std::memcpy(&packetType, packet + 1, sizeof(PACKET_TYPE));
    }

    Logger::Warn(
        "Unknown packet. session=" +
        std::to_string(session->GetSessionId()) +
        ", type=" +
        std::to_string(static_cast<int>(packetType)) +
        ", size=" +
        std::to_string(packetSize)
    );
}

void PacketDispatcher::SendLoginResult(ClientSession* session, bool success, const std::string& message)
{
    if (session == nullptr)
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

void PacketDispatcher::SendAvatarInfo(ClientSession* session)
{
    if (session == nullptr)
    {
        return;
    }

    S2C_AvatarInfo packet{};
    packet.size = static_cast<unsigned char>(sizeof(S2C_AvatarInfo));
    packet.type = S2C_AVATAR_INFO;
    packet.playerId = session->GetPlayerId();
    packet.x = static_cast<short>(session->GetX());
    packet.y = static_cast<short>(session->GetY());

    sendFunction_(
        session,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(packet.size)
    );
}

std::string PacketDispatcher::ExtractUsername(const char* username)
{
    if (username == nullptr)
    {
        return {};
    }

    int length = 0;

    while (length < MAX_NAME_LEN && username[length] != '\0')
    {
        ++length;
    }

    return std::string(username, username + length);
}

void PacketDispatcher::HandleAttack(ClientSession* session, const char* packet, int packetSize)
{
    if (session == nullptr || gameWorld_ == nullptr)
    {
        return;
    }

    if (!session->IsLoggedIn())
    {
        Logger::Warn(
            "Attack packet before login. session=" +
            std::to_string(session->GetSessionId())
        );
        return;
    }

    if (packetSize != sizeof(C2S_Attack))
    {
        Logger::Warn("Invalid C2S_Attack size.");
        return;
    }

    gameWorld_->HandlePlayerAttack(session);
}