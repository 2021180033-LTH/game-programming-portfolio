#pragma once

#include <functional>
#include <cstdint>
#include <string>
#include <cstring>
#include <utility>

#include "GameProtocol.h"
#include "ClientSession.h"
#include "GameWorld.h"
#include "Logger.h"

class ClientSession;
class GameWorld;

class PacketDispatcher
{
public:
    using SendFunction = std::function<void(ClientSession*, const char*, int)>;

    PacketDispatcher(SendFunction sendFunction, GameWorld* gameWorld);

    void Dispatch(ClientSession* session, const char* packet, int packetSize);

private:
    void HandleLogin(ClientSession* session, const char* packet, int packetSize);
    void HandleMove(ClientSession* session, const char* packet, int packetSize);
    void HandleUnknown(ClientSession* session, const char* packet, int packetSize);

    void SendLoginResult(ClientSession* session, bool success, const std::string& message);
    void SendAvatarInfo(ClientSession* session);

    static std::string ExtractUsername(const char* username);

    void HandleAttack(ClientSession* session, const char* packet, int packetSize);

    void HandleChat(ClientSession* session, const char* packet, int packetSize);

    void HandleUseSkill(ClientSession* session, const char* packet, int packetSize);

    void HandleAddStat(ClientSession* session, const char* packet, int packetSize);
    void HandleAddSkillPoint(ClientSession* session, const char* packet, int packetSize);

private:
    SendFunction sendFunction_;
    GameWorld* gameWorld_ = nullptr;
};