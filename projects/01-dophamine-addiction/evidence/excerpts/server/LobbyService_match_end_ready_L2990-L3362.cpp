/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Server/LobbyService.cpp
 * Original line range(s): 2990-3362
 * Attribution evidence: current git blame at the fixed commit: 373/373 lines attributed to Lee Taehyun identities
 * Review purpose: match end/abort/readiness notifications and server control handlers
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 2990-3362 ----
void LobbyService::HandleDediMatchEndNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlHeaderSize = 18;
    static constexpr uint8_t MaxWinnerLength = 96;
    static constexpr uint8_t MaxSummaryLength = 180;
    if (!payload || payloadLen < ControlHeaderSize + 1)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t notifyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);
    uint16_t offset = ControlHeaderSize;

    const uint8_t winnerLen = p[offset++];
    if (winnerLen > MaxWinnerLength || offset + winnerLen > payloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad winner roomId=%u len=%u payloadLen=%u\n",
            roomId, winnerLen, payloadLen);
        m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
        return;
    }

    std::string winner(payload + offset, payload + offset + winnerLen);
    offset += winnerLen;

    std::string summary;
    if (offset < payloadLen)
    {
        const uint8_t summaryLen = p[offset++];
        if (summaryLen > MaxSummaryLength || offset + summaryLen != payloadLen)
        {
            LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY bad summary roomId=%u len=%u payloadLen=%u\n",
                roomId, summaryLen, payloadLen);
            m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
            return;
        }

        summary.assign(payload + offset, payload + offset + summaryLen);
        offset += summaryLen;
    }

    if (offset != payloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY trailing payload roomId=%u payloadLen=%u parsed=%u\n",
            roomId, payloadLen, offset);
        m_net.SendDediControlAck(c, PacketType::L2D_MATCH_END_ACK, roomId, notifyPort, generation, controlToken, false);
        return;
    }

    bool accepted = false;
    bool recovered = false;
    bool duplicate = false;
    bool roomDeleted = false;
    size_t memberCount = 0;

    AcquireSRWLockExclusive(&m_lock);

    if (WasDediControlCompleted_Unsafe(
        PacketType::D2L_MATCH_END_NOTIFY,
        roomId,
        notifyPort,
        generation,
        controlToken))
    {
        accepted = true;
        duplicate = true;
    }
    else
    {
        auto itRoom = m_rooms.find(roomId);
        if (itRoom != m_rooms.end())
        {
            Room& room = itRoom->second;
            if (room.state == RoomState::IN_GAME &&
                room.dedicatedPort == notifyPort &&
                room.dedicatedGeneration == generation &&
                room.dedicatedControlToken != 0 &&
                room.dedicatedControlToken == controlToken)
            {
                memberCount = room.members.size();
                RememberDediControlCompleted_Unsafe(
                    PacketType::D2L_MATCH_END_NOTIFY,
                    roomId,
                    notifyPort,
                    generation,
                    controlToken);

                roomDeleted = RecoverRoomToWaiting_Unsafe(room, "MatchEnd", false);
                if (roomDeleted)
                {
                    m_roomOrder.erase(
                        std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                        m_roomOrder.end());
                    m_rooms.erase(itRoom);
                }

                accepted = true;
                recovered = true;
            }
            else
            {
                LOBBY_WARN("[DEDI][WARN] MATCH_END_NOTIFY rejected roomId=%u port=%u generation=%u state=%s expectedPort=%u expectedGeneration=%u\n",
                    roomId,
                    notifyPort,
                    generation,
                    RoomStateToString(room.state),
                    room.dedicatedPort,
                    room.dedicatedGeneration);
            }
        }
    }

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(
        c,
        PacketType::L2D_MATCH_END_ACK,
        roomId,
        notifyPort,
        generation,
        controlToken,
        accepted);

    LOBBY_INFO("[DEDI] MATCH_END_NOTIFY roomId=%u winner=%s summary=%s port=%u members=%zu result=%s\n",
        roomId,
        winner.c_str(),
        summary.c_str(),
        notifyPort,
        memberCount,
        duplicate ? "DUPLICATE_ACK" : (recovered ? "RECOVERED" : "REJECTED"));

    if (recovered)
    {
        BroadcastRoomList();
        if (!roomDeleted)
        {
            BroadcastRoomMemberList(roomId);
        }
    }
}

void LobbyService::HandleDediMatchAbortNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlHeaderSize = 18;
    static constexpr uint8_t MaxReasonLength = 180;
    if (!payload || payloadLen < ControlHeaderSize + 1)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_ABORT_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t notifyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);
    const uint8_t reasonLen = p[ControlHeaderSize];
    const uint16_t expectedPayloadLen = static_cast<uint16_t>(
        ControlHeaderSize + 1 + reasonLen);

    if (reasonLen > MaxReasonLength || payloadLen != expectedPayloadLen)
    {
        LOBBY_WARN("[DEDI][WARN] MATCH_ABORT_NOTIFY bad reason roomId=%u len=%u payloadLen=%u expected=%u\n",
            roomId,
            reasonLen,
            payloadLen,
            expectedPayloadLen);
        m_net.SendDediControlAck(c, PacketType::L2D_MATCH_ABORT_ACK, roomId, notifyPort, generation, controlToken, false);
        return;
    }

    std::string reason(
        payload + ControlHeaderSize + 1,
        payload + ControlHeaderSize + 1 + reasonLen);
    std::replace(reason.begin(), reason.end(), '\r', ' ');
    std::replace(reason.begin(), reason.end(), '\n', ' ');
    if (reason.empty())
    {
        reason = "Unspecified";
    }

    bool accepted = false;
    bool recovered = false;
    bool duplicate = false;
    bool roomDeleted = false;
    size_t memberCount = 0;

    AcquireSRWLockExclusive(&m_lock);

    if (WasDediControlCompleted_Unsafe(
        PacketType::D2L_MATCH_ABORT_NOTIFY,
        roomId,
        notifyPort,
        generation,
        controlToken))
    {
        accepted = true;
        duplicate = true;
    }
    else
    {
        auto itRoom = m_rooms.find(roomId);
        if (itRoom != m_rooms.end())
        {
            Room& room = itRoom->second;
            if (room.state == RoomState::IN_GAME &&
                room.dedicatedPort == notifyPort &&
                room.dedicatedGeneration == generation &&
                room.dedicatedControlToken != 0 &&
                room.dedicatedControlToken == controlToken)
            {
                memberCount = room.members.size();
                RememberDediControlCompleted_Unsafe(
                    PacketType::D2L_MATCH_ABORT_NOTIFY,
                    roomId,
                    notifyPort,
                    generation,
                    controlToken);

                roomDeleted = RecoverRoomToWaiting_Unsafe(
                    room,
                    reason.c_str(),
                    false);
                if (roomDeleted)
                {
                    m_roomOrder.erase(
                        std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                        m_roomOrder.end());
                    m_rooms.erase(itRoom);
                }

                accepted = true;
                recovered = true;
            }
            else
            {
                LOBBY_WARN("[DEDI][WARN] MATCH_ABORT_NOTIFY rejected roomId=%u port=%u generation=%u state=%s expectedPort=%u expectedGeneration=%u reason=%s\n",
                    roomId,
                    notifyPort,
                    generation,
                    RoomStateToString(room.state),
                    room.dedicatedPort,
                    room.dedicatedGeneration,
                    reason.c_str());
            }
        }
    }

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(
        c,
        PacketType::L2D_MATCH_ABORT_ACK,
        roomId,
        notifyPort,
        generation,
        controlToken,
        accepted);

    LOBBY_WARN("[RECOVERY] MATCH_ABORT_NOTIFY roomId=%u port=%u generation=%u members=%zu reason=%s result=%s roomDeleted=%d\n",
        roomId,
        notifyPort,
        generation,
        memberCount,
        reason.c_str(),
        duplicate ? "DUPLICATE_ACK" : (recovered ? "RECOVERED" : "REJECTED"),
        roomDeleted ? 1 : 0);

    if (recovered)
    {
        BroadcastRoomList();
        if (!roomDeleted)
        {
            BroadcastRoomMemberList(roomId);
        }
    }
}

void LobbyService::HandleDediServerReadyNotify(ClientContext* c, const char* payload, uint16_t payloadLen)
{
    static constexpr uint16_t ControlPayloadSize = 18;
    if (!payload || payloadLen != ControlPayloadSize)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY bad payload len=%u\n", payloadLen);
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(payload);
    const uint32_t roomId = ReadControlU32BE(p);
    const uint16_t readyPort = ReadControlU16BE(p + 4);
    const uint32_t generation = ReadControlU32BE(p + 6);
    const uint64_t controlToken = ReadControlU64BE(p + 10);
    uint16_t gameStartPort = 0;
    std::string gameStartHost;
    std::vector<GameStartTarget> gameStartTargets;
    bool gameStartPrepared = false;

    AcquireSRWLockExclusive(&m_lock);

    auto itRoom = m_rooms.find(roomId);
    if (itRoom == m_rooms.end())
    {
        ReleaseSRWLockExclusive(&m_lock);

        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY roomId=%u port=%u result=ROOM_NOT_FOUND\n",
            roomId,
            readyPort);
        m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, false);
        return;
    }

    Room& room = itRoom->second;
    const uint16_t roomPort = room.dedicatedPort;
    if (room.state != RoomState::IN_GAME ||
        readyPort == 0 ||
        roomPort != readyPort ||
        room.dedicatedGeneration != generation ||
        room.dedicatedControlToken == 0 ||
        room.dedicatedControlToken != controlToken)
    {
        LOBBY_WARN("[DEDI][WARN] SERVER_READY_NOTIFY rejected roomId=%u readyPort=%u roomPort=%u generation=%u expectedGeneration=%u state=%s\n",
            roomId,
            readyPort,
            roomPort,
            generation,
            room.dedicatedGeneration,
            RoomStateToString(room.state));
        ReleaseSRWLockExclusive(&m_lock);
        m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, false);
        return;
    }

    room.dedicatedReadyReceived = true;
    LOBBY_INFO("[DEDI] SERVER_READY_NOTIFY roomId=%u port=%u generation=%u result=OK\n",
        roomId,
        roomPort,
        generation);

    gameStartPrepared = PrepareGameStartForRoom_Unsafe(
        roomId,
        "DediReady",
        gameStartPort,
        gameStartHost,
        gameStartTargets);

    ReleaseSRWLockExclusive(&m_lock);

    m_net.SendDediControlAck(c, PacketType::L2D_SERVER_READY_ACK, roomId, readyPort, generation, controlToken, true);

    if (gameStartPrepared)
    {
        for (const GameStartTarget& target : gameStartTargets)
        {
            LOBBY_INFO("[ROOM] GAME_START rid=%u sid=%u ip=%s port=%u reason=DediReady ticketIssued=1\n",
                roomId,
                target.sessionId,
                gameStartHost.c_str(),
                gameStartPort);

            m_net.SendGameStart(
                target.context,
                gameStartHost.c_str(),
                gameStartPort,
                target.ticket);
            ReleaseIO(target.context);
        }
    }
}

