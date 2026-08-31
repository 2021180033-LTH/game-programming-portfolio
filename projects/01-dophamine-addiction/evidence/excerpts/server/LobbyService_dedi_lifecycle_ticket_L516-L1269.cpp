/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Server/LobbyService.cpp
 * Original line range(s): 516-1269
 * Attribution evidence: current git blame at the fixed commit: 754/754 lines attributed to Lee Taehyun identities
 * Review purpose: Dedicated Server port isolation/reclaim, process and ghost-session sweep, ticket/control-token lifecycle
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 516-1269 ----
bool LobbyService::IsUdpPortAvailable(uint16_t port) const
{
    SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (probe == INVALID_SOCKET)
    {
        return false;
    }

    const BOOL exclusiveAddressUse = TRUE;
    setsockopt(
        probe,
        SOL_SOCKET,
        SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusiveAddressUse),
        sizeof(exclusiveAddressUse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    const bool available =
        bind(probe, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR;
    closesocket(probe);
    return available;
}

void LobbyService::QuarantinePort_Unsafe(uint16_t port, const char* reason)
{
    if (port == 0)
    {
        return;
    }

    if (std::find(m_quarantinedPorts.begin(), m_quarantinedPorts.end(), port) ==
        m_quarantinedPorts.end())
    {
        m_quarantinedPorts.push_back(port);
    }

    LOBBY_WARN("[DEDI][WARN] Port quarantined port=%u reason=%s quarantined=%zu\n",
        port,
        reason ? reason : "Unknown",
        m_quarantinedPorts.size());
}

void LobbyService::ReclaimQuarantinedPorts_Unsafe()
{
    for (auto it = m_quarantinedPorts.begin(); it != m_quarantinedPorts.end();)
    {
        const uint16_t port = *it;
        if (!IsUdpPortAvailable(port))
        {
            ++it;
            continue;
        }

        it = m_quarantinedPorts.erase(it);
        FreePort(port);
        LOBBY_INFO("[DEDI] Quarantined port reclaimed port=%u remaining=%zu\n",
            port,
            m_quarantinedPorts.size());
    }
}

void LobbyService::CleanupDedicatedServerForRoom_Unsafe(Room& room, const char* reason, bool terminateProcess)
{
    const uint16_t oldPort = room.dedicatedPort;
    const HANDLE oldProcessHandle = room.dedicatedProcessHandle;
    const DWORD oldProcessId = room.dedicatedProcessId;
    const uint32_t oldGeneration = room.dedicatedGeneration;

    room.dedicatedPort = 0;
    room.dedicatedProcessHandle = NULL;
    room.dedicatedProcessId = 0;
    room.dedicatedControlToken = 0;
    room.dedicatedReadyReceived = false;
    room.dedicatedLaunchTime = {};
    room.gameStartSent = false;
    room.handoverTickets.clear();
    room.ghostDisconnectTimes.clear();

    if (oldProcessHandle)
    {
        RetiringDedicatedProcess retiring{};
        retiring.roomId = room.id;
        retiring.port = oldPort;
        retiring.processHandle = oldProcessHandle;
        retiring.processId = oldProcessId;
        retiring.generation = oldGeneration;
        retiring.reason = reason ? reason : "Unknown";
        retiring.terminateAfter = std::chrono::steady_clock::now() +
            std::chrono::seconds(terminateProcess ? 0 : DEDI_GRACEFUL_SHUTDOWN_SECONDS);
        m_retiringDedicatedProcesses.push_back(std::move(retiring));

        LOBBY_INFO("[DEDI] Retire queued roomId=%u reason=%s port=%u pid=%lu generation=%u terminateNow=%d retiring=%zu\n",
            room.id,
            reason ? reason : "Unknown",
            oldPort,
            oldProcessId,
            oldGeneration,
            terminateProcess ? 1 : 0,
            m_retiringDedicatedProcesses.size());
        return;
    }

    if (oldPort != 0)
    {
        if (IsUdpPortAvailable(oldPort))
        {
            FreePort(oldPort);
        }
        else
        {
            QuarantinePort_Unsafe(oldPort, "MissingProcessHandlePortStillBound");
        }
    }
}

bool LobbyService::RecoverRoomToWaiting_Unsafe(
    Room& room,
    const char* reason,
    bool terminateProcess)
{
    const size_t oldMemberCount = room.members.size();
    const uint16_t oldPort = room.dedicatedPort;
    const DWORD oldProcessId = room.dedicatedProcessId;

    CleanupDedicatedServerForRoom_Unsafe(room, reason, terminateProcess);
    room.state = RoomState::WAITING;

    size_t removedOfflineMembers = 0;
    for (auto itMember = room.members.begin(); itMember != room.members.end();)
    {
        const uint32_t memberSid = *itMember;
        const auto itContext = m_ctxBySession.find(memberSid);
        if (itContext == m_ctxBySession.end() || itContext->second == nullptr)
        {
            itMember = room.members.erase(itMember);
            room.readyStatus.erase(memberSid);
            room.handoverTickets.erase(memberSid);
            room.ghostDisconnectTimes.erase(memberSid);
            m_roomBySession.erase(memberSid);
            m_accountIdBySid.erase(memberSid);
            m_nicknameBySid.erase(memberSid);
            if (room.hostId == memberSid)
            {
                room.hostId = 0;
            }
            ++removedOfflineMembers;
            continue;
        }

        ++itMember;
    }

    if (room.hostId != 0 &&
        std::find(room.members.begin(), room.members.end(), room.hostId) == room.members.end())
    {
        room.hostId = 0;
    }
    if (room.hostId == 0 && !room.members.empty())
    {
        room.hostId = room.members.front();
    }

    room.readyStatus.clear();
    for (uint32_t memberSid : room.members)
    {
        room.readyStatus[memberSid] = (memberSid == room.hostId);
    }

    LOBBY_INFO("[RECOVERY] ROOM_TO_WAITING rid=%u reason=%s oldPort=%u oldPid=%lu members=%zu->%zu removedOffline=%zu host=%u terminate=%d empty=%d\n",
        room.id,
        reason ? reason : "Unknown",
        oldPort,
        oldProcessId,
        oldMemberCount,
        room.members.size(),
        removedOfflineMembers,
        room.hostId,
        terminateProcess ? 1 : 0,
        room.members.empty() ? 1 : 0);

    return room.members.empty();
}

bool LobbyService::WasDediControlCompleted_Unsafe(
    PacketType notifyType,
    uint32_t roomId,
    uint16_t port,
    uint32_t generation,
    uint64_t controlToken) const
{
    return std::any_of(
        m_completedDediControls.begin(),
        m_completedDediControls.end(),
        [notifyType, roomId, port, generation, controlToken](const CompletedDediControl& completed)
        {
            return completed.notifyType == notifyType &&
                completed.roomId == roomId &&
                completed.port == port &&
                completed.generation == generation &&
                completed.controlToken == controlToken;
        });
}

void LobbyService::RememberDediControlCompleted_Unsafe(
    PacketType notifyType,
    uint32_t roomId,
    uint16_t port,
    uint32_t generation,
    uint64_t controlToken)
{
    m_completedDediControls.erase(
        std::remove_if(
            m_completedDediControls.begin(),
            m_completedDediControls.end(),
            [notifyType, roomId, port, generation, controlToken](const CompletedDediControl& completed)
            {
                return completed.notifyType == notifyType &&
                    completed.roomId == roomId &&
                    completed.port == port &&
                    completed.generation == generation &&
                    completed.controlToken == controlToken;
            }),
        m_completedDediControls.end());

    CompletedDediControl completed{};
    completed.notifyType = notifyType;
    completed.roomId = roomId;
    completed.port = port;
    completed.generation = generation;
    completed.controlToken = controlToken;
    completed.completedAt = std::chrono::steady_clock::now();
    m_completedDediControls.push_back(std::move(completed));
}

void LobbyService::SweepCompletedDediControls_Unsafe(
    const std::chrono::steady_clock::time_point& now)
{
    m_completedDediControls.erase(
        std::remove_if(
            m_completedDediControls.begin(),
            m_completedDediControls.end(),
            [&now](const CompletedDediControl& completed)
            {
                return std::chrono::duration_cast<std::chrono::seconds>(
                    now - completed.completedAt).count() >=
                    DEDI_COMPLETED_CONTROL_TTL_SECONDS;
            }),
        m_completedDediControls.end());
}

void LobbyService::SweepRetiringDedicatedServers_Unsafe(
    const std::chrono::steady_clock::time_point& now)
{
    for (auto it = m_retiringDedicatedProcesses.begin();
        it != m_retiringDedicatedProcesses.end();)
    {
        RetiringDedicatedProcess& retiring = *it;
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(retiring.processHandle, &exitCode))
        {
            LOBBY_ERR("[DEDI][ERR] Retire handle invalid roomId=%u port=%u pid=%lu generation=%u reason=%s gle=%lu\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                retiring.reason.c_str(),
                GetLastError());

            CloseHandle(retiring.processHandle);
            if (retiring.port != 0)
            {
                QuarantinePort_Unsafe(retiring.port, "RetireProcessHandleInvalid");
            }
            it = m_retiringDedicatedProcesses.erase(it);
            continue;
        }

        if (exitCode != STILL_ACTIVE)
        {
            CloseHandle(retiring.processHandle);

            const bool portAvailable = retiring.port == 0 || IsUdpPortAvailable(retiring.port);
            if (retiring.port != 0)
            {
                if (portAvailable)
                {
                    FreePort(retiring.port);
                }
                else
                {
                    QuarantinePort_Unsafe(retiring.port, "RetiredProcessExitedPortStillBound");
                }
            }

            LOBBY_INFO("[DEDI] Retire complete roomId=%u port=%u pid=%lu generation=%u exitCode=%lu portAvailable=%d reason=%s remaining=%zu\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                exitCode,
                portAvailable ? 1 : 0,
                retiring.reason.c_str(),
                m_retiringDedicatedProcesses.size() - 1);

            it = m_retiringDedicatedProcesses.erase(it);
            continue;
        }

        if (now < retiring.terminateAfter)
        {
            ++it;
            continue;
        }

        if (TerminateProcess(retiring.processHandle, 0))
        {
            retiring.terminationRequested = true;
            retiring.terminateAfter = now + std::chrono::seconds(DEDI_TERMINATE_RETRY_SECONDS);
            LOBBY_WARN("[DEDI][WARN] Retire terminate requested roomId=%u port=%u pid=%lu generation=%u reason=%s\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                retiring.reason.c_str());
        }
        else
        {
            const DWORD terminateError = GetLastError();
            retiring.terminateAfter = now + std::chrono::seconds(DEDI_TERMINATE_RETRY_SECONDS);
            LOBBY_ERR("[DEDI][ERR] Retire terminate failed roomId=%u port=%u pid=%lu generation=%u requestedBefore=%d reason=%s gle=%lu\n",
                retiring.roomId,
                retiring.port,
                retiring.processId,
                retiring.generation,
                retiring.terminationRequested ? 1 : 0,
                retiring.reason.c_str(),
                terminateError);
        }

        ++it;
    }
}

void LobbyService::SweepDedicatedServerProcesses()
{
    std::vector<uint32_t> changedRooms;
    bool shouldBroadcastRoomList = false;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);
    ReclaimQuarantinedPorts_Unsafe();
    SweepRetiringDedicatedServers_Unsafe(now);
    SweepCompletedDediControls_Unsafe(now);

    for (auto itRoom = m_rooms.begin(); itRoom != m_rooms.end();)
    {
        Room& room = itRoom->second;
        if (!room.dedicatedProcessHandle)
        {
            ++itRoom;
            continue;
        }

        const char* recoveryReason = nullptr;
        bool terminateProcess = false;
        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(room.dedicatedProcessHandle, &exitCode))
        {
            LOBBY_ERR("[DEDI][ERR] Process handle invalid before recovery. roomId=%u port=%u pid=%lu gle=%lu\n",
                room.id,
                room.dedicatedPort,
                room.dedicatedProcessId,
                GetLastError());
            recoveryReason = "ProcessHandleInvalid";
        }
        else if (exitCode == STILL_ACTIVE)
        {
            if (!room.dedicatedReadyReceived &&
                room.dedicatedLaunchTime.time_since_epoch().count() != 0)
            {
                const auto elapsedSeconds =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        now - room.dedicatedLaunchTime).count();
                if (elapsedSeconds >= DEDI_READY_TIMEOUT_SECONDS)
                {
                    LOBBY_ERR("[DEDI][ERR] Ready timeout roomId=%u port=%u pid=%lu elapsed=%lld timeout=%d\n",
                        room.id,
                        room.dedicatedPort,
                        room.dedicatedProcessId,
                        static_cast<long long>(elapsedSeconds),
                        DEDI_READY_TIMEOUT_SECONDS);
                    recoveryReason = "ReadyTimeout";
                    terminateProcess = true;
                }
            }

            if (!recoveryReason)
            {
                ++itRoom;
                continue;
            }
        }
        else
        {
            LOBBY_ERR("[DEDI][ERR] Process exited before control cleanup. roomId=%u port=%u pid=%lu exitCode=%lu\n",
                room.id,
                room.dedicatedPort,
                room.dedicatedProcessId,
                exitCode);
            recoveryReason = "ProcessExited";
        }

        const uint32_t roomId = room.id;
        const bool roomEmpty = RecoverRoomToWaiting_Unsafe(
            room,
            recoveryReason,
            terminateProcess);
        shouldBroadcastRoomList = true;

        if (roomEmpty)
        {
            m_roomOrder.erase(
                std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                m_roomOrder.end());
            itRoom = m_rooms.erase(itRoom);
            LOBBY_INFO("[RECOVERY] EMPTY_ROOM_DELETED rid=%u reason=%s\n",
                roomId,
                recoveryReason);
            continue;
        }

        changedRooms.push_back(roomId);
        ++itRoom;
    }

    ReleaseSRWLockExclusive(&m_lock);

    if (shouldBroadcastRoomList)
    {
        BroadcastRoomList();
    }
    for (uint32_t roomId : changedRooms)
    {
        BroadcastRoomMemberList(roomId);
    }
}

void LobbyService::SweepGhostSessions()
{
    std::vector<uint32_t> changedRooms;
    bool shouldBroadcastRoomList = false;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);

    for (auto itRoom = m_rooms.begin(); itRoom != m_rooms.end();)
    {
        Room& room = itRoom->second;
        if (room.state != RoomState::IN_GAME || room.ghostDisconnectTimes.empty())
        {
            ++itRoom;
            continue;
        }

        std::vector<uint32_t> expiredSids;
        for (const auto& ghost : room.ghostDisconnectTimes)
        {
            const uint32_t sid = ghost.first;
            if (m_ctxBySession.find(sid) != m_ctxBySession.end())
            {
                continue;
            }

            const auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                now - ghost.second).count();
            if (elapsedSeconds >= IOCP_GHOST_RECONNECT_GRACE_SECONDS)
            {
                expiredSids.push_back(sid);
            }
        }

        if (expiredSids.empty())
        {
            ++itRoom;
            continue;
        }

        const uint32_t roomId = room.id;
        for (uint32_t sid : expiredSids)
        {
            room.ghostDisconnectTimes.erase(sid);
            room.members.erase(
                std::remove(room.members.begin(), room.members.end(), sid),
                room.members.end());
            room.readyStatus.erase(sid);
            room.handoverTickets.erase(sid);
            m_roomBySession.erase(sid);
            m_accountIdBySid.erase(sid);
            m_nicknameBySid.erase(sid);

            if (room.hostId == sid)
            {
                room.hostId = 0;
            }

            LOBBY_WARN("[LOBBY][WARN] GHOST_EXPIRED rid=%u sid=%u grace=%d\n",
                roomId,
                sid,
                IOCP_GHOST_RECONNECT_GRACE_SECONDS);
        }

        if (room.hostId == 0 && !room.members.empty())
        {
            room.hostId = room.members.front();
            room.readyStatus[room.hostId] = true;
            LOBBY_INFO("[ROOM] HOST_CHANGE rid=%u reason=GHOST_EXPIRED newHost=%u\n",
                roomId,
                room.hostId);
        }

        if (room.members.empty())
        {
            const uint16_t oldPort = room.dedicatedPort;
            const DWORD oldProcessId = room.dedicatedProcessId;

            if (room.dedicatedPort != 0 || room.dedicatedProcessHandle)
            {
                CleanupDedicatedServerForRoom_Unsafe(room, "GhostGraceAllExpired", true);
            }

            m_roomOrder.erase(
                std::remove(m_roomOrder.begin(), m_roomOrder.end(), roomId),
                m_roomOrder.end());

            itRoom = m_rooms.erase(itRoom);
            shouldBroadcastRoomList = true;

            LOBBY_INFO("[ROOM] EMPTY_DELETE rid=%u reason=GHOST_GRACE_EXPIRED freePort=%u pid=%lu\n",
                roomId,
                oldPort,
                oldProcessId);
            continue;
        }

        changedRooms.push_back(roomId);
        shouldBroadcastRoomList = true;
        ++itRoom;
    }

    ReleaseSRWLockExclusive(&m_lock);

    if (shouldBroadcastRoomList)
    {
        BroadcastRoomList();
    }

    for (uint32_t roomId : changedRooms)
    {
        BroadcastRoomMemberList(roomId);
    }
}

void LobbyService::SweepUnauthenticatedClients()
{
    struct TimedOutClient
    {
        ClientContext* context = nullptr;
        uint32_t sessionId = 0;
        long long idleSeconds = 0;
    };

    std::vector<TimedOutClient> timedOutClients;
    const auto now = std::chrono::steady_clock::now();

    AcquireSRWLockExclusive(&m_lock);

    for (auto& activityEntry : m_clientActivityByCtx)
    {
        ClientContext* context = activityEntry.first;
        auto itSession = m_sessionByCtx.find(context);
        if (!context || itSession == m_sessionByCtx.end())
        {
            continue;
        }

        const uint32_t sessionId = itSession->second;
        if (m_accountIdBySid.find(sessionId) != m_accountIdBySid.end())
        {
            continue;
        }

        const long long idleSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            now - activityEntry.second.lastPacketAt).count();
        if (idleSeconds < m_unauthenticatedIdleTimeoutSeconds)
        {
            continue;
        }

        AddIO(context);
        timedOutClients.push_back({ context, sessionId, idleSeconds });
        activityEntry.second.lastPacketAt = now;
    }

    ReleaseSRWLockExclusive(&m_lock);

    for (const TimedOutClient& timedOut : timedOutClients)
    {
        LOBBY_WARN("[ADMISSION][WARN] Closing unauthenticated idle client sid=%u idle=%lld timeout=%d\n",
            timedOut.sessionId,
            timedOut.idleSeconds,
            m_unauthenticatedIdleTimeoutSeconds);
        MarkClientClosing(timedOut.context, "UnauthenticatedIdleTimeout");
        ReleaseIO(timedOut.context);
    }
}

void LobbyService::TickMaintenance()
{
    SweepDedicatedServerProcesses();
    SweepGhostSessions();
    SweepUnauthenticatedClients();
}

void LobbyService::ShutdownDedicatedServers()
{
    std::vector<HANDLE> processHandles;
    HANDLE jobHandle = NULL;

    AcquireSRWLockExclusive(&m_lock);
    if (m_dedicatedServerShutdown)
    {
        ReleaseSRWLockExclusive(&m_lock);
        return;
    }

    m_dedicatedServerShutdown = true;

    for (auto& entry : m_rooms)
    {
        Room& room = entry.second;
        if (room.dedicatedProcessHandle)
        {
            processHandles.push_back(room.dedicatedProcessHandle);
            room.dedicatedProcessHandle = NULL;
        }
        room.dedicatedProcessId = 0;
        room.dedicatedPort = 0;
        room.dedicatedControlToken = 0;
        room.dedicatedReadyReceived = false;
        room.dedicatedLaunchTime = {};
        room.gameStartSent = false;
        room.handoverTickets.clear();
        room.ghostDisconnectTimes.clear();
    }

    for (RetiringDedicatedProcess& retiring : m_retiringDedicatedProcesses)
    {
        if (retiring.processHandle)
        {
            processHandles.push_back(retiring.processHandle);
            retiring.processHandle = NULL;
        }
    }
    m_retiringDedicatedProcesses.clear();

    jobHandle = m_dedicatedServerJob;
    m_dedicatedServerJob = NULL;
    ReleaseSRWLockExclusive(&m_lock);

    if (jobHandle)
    {
        if (!TerminateJobObject(jobHandle, 0))
        {
            LOBBY_ERR("[DEDI][ERR] TerminateJobObject during shutdown failed gle=%lu\n", GetLastError());
        }
    }

    size_t exitedCount = 0;
    for (HANDLE processHandle : processHandles)
    {
        if (!processHandle)
        {
            continue;
        }

        if (!jobHandle)
        {
            TerminateProcess(processHandle, 0);
        }

        const DWORD waitResult = WaitForSingleObject(processHandle, DEDI_TERMINATE_WAIT_MS);
        if (waitResult == WAIT_OBJECT_0)
        {
            ++exitedCount;
        }
        else
        {
            LOBBY_WARN("[DEDI][WARN] Shutdown wait incomplete wait=%lu\n", waitResult);
        }
        CloseHandle(processHandle);
    }

    if (jobHandle)
    {
        CloseHandle(jobHandle);
    }

    LOBBY_INFO("[DEDI] Shutdown complete tracked=%zu exited=%zu job=%d\n",
        processHandles.size(),
        exitedCount,
        jobHandle ? 1 : 0);
}

uint32_t LobbyService::GenerateHandoverTicket_Unsafe(const Room& room) const
{
    static thread_local std::random_device randomDevice;
    static thread_local std::mt19937 rng(randomDevice());
    std::uniform_int_distribution<uint32_t> dist(100000000u, 0xFFFFFFFEu);

    uint32_t ticket = 0;
    do
    {
        ticket = dist(rng);
    }
    while (ticket == 0 ||
        std::find_if(
            room.handoverTickets.begin(),
            room.handoverTickets.end(),
            [ticket](const std::pair<const uint32_t, uint32_t>& entry)
            {
                return entry.second == ticket;
            }) != room.handoverTickets.end());

    return ticket;
}

uint64_t LobbyService::GenerateDedicatedControlToken_Unsafe() const
{
    static thread_local std::random_device randomDevice;
    static thread_local std::mt19937_64 rng(
        (static_cast<uint64_t>(randomDevice()) << 32) ^ randomDevice());

    uint64_t token = 0;
    while (token == 0)
    {
        token = rng();
    }
    return token;
}


