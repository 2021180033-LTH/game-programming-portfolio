/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Server/ServerMain.cpp
 * Original line range(s): 294-484
 * Attribution evidence: current git blame at the fixed commit: 191/191 lines attributed to Lee Taehyun identities
 * Review purpose: local/remote admission, inbound type/size/rate/auth-count limits and listen-address construction
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 294-484 ----
bool IsLocalConnection(SOCKET clientSock, const sockaddr_in& clientAddr) {
    const std::uint32_t peerAddress = ntohl(clientAddr.sin_addr.s_addr);
    if ((peerAddress & 0xFF000000u) == 0x7F000000u) {
        return true;
    }

    sockaddr_in localAddr{};
    int localAddrLength = sizeof(localAddr);
    if (getsockname(
            clientSock,
            reinterpret_cast<sockaddr*>(&localAddr),
            &localAddrLength) == SOCKET_ERROR) {
        return false;
    }

    return localAddr.sin_family == AF_INET &&
        localAddr.sin_addr.s_addr == clientAddr.sin_addr.s_addr;
}

bool TryAcquireAdmissionSlot(SOCKET clientSock, const sockaddr_in& clientAddr) {
    const bool isLocal = IsLocalConnection(clientSock, clientAddr);
    const std::size_t connectionLimit = isLocal
        ? g_maxConnections
        : g_maxConnections - g_localConnectionReserve;

    std::size_t active = g_activeClientCount.load(std::memory_order_acquire);
    while (active < connectionLimit) {
        if (g_activeClientCount.compare_exchange_weak(
                active,
                active + 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return true;
        }
    }

    const std::uint64_t rejectedTotal =
        g_rejectedConnectionCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (rejectedTotal == 1 || rejectedTotal % 64 == 0) {
        IOCP_INFO(
            "[ADMISSION] Connection rejected source=%s active=%zu limit=%zu rejectedTotal=%llu\n",
            isLocal ? "local" : "remote",
            active,
            connectionLimit,
            static_cast<unsigned long long>(rejectedTotal));
    }
    return false;
}

void ConfigureConnectionAdmission() {
    g_maxConnections = GetEnvSizeOrDefault(
        "MANAGER_IOCP_MAX_CONNECTIONS",
        DEFAULT_MAX_CONNECTIONS,
        1,
        MAX_CONFIGURED_CONNECTIONS);
    g_localConnectionReserve = GetEnvSizeOrDefault(
        "MANAGER_IOCP_LOCAL_RESERVE",
        DEFAULT_LOCAL_CONNECTION_RESERVE,
        0,
        MAX_CONFIGURED_CONNECTIONS);

    if (g_localConnectionReserve >= g_maxConnections) {
        const std::size_t adjustedReserve =
            g_maxConnections > 1 ? g_maxConnections - 1 : 0;
        IOCP_INFO(
            "[Server][WARN] MANAGER_IOCP_LOCAL_RESERVE=%zu must be below max=%zu. adjusted=%zu\n",
            g_localConnectionReserve,
            g_maxConnections,
            adjustedReserve);
        g_localConnectionReserve = adjustedReserve;
    }

    IOCP_INFO(
        "[ADMISSION] Limits max=%zu localReserve=%zu remoteLimit=%zu\n",
        g_maxConnections,
        g_localConnectionReserve,
        g_maxConnections - g_localConnectionReserve);
}

void ConfigureInboundLimits() {
    g_inboundPacketsPerSecond = GetEnvSizeOrDefault(
        "MANAGER_IOCP_PACKETS_PER_SECOND",
        DEFAULT_INBOUND_PACKETS_PER_SECOND,
        8,
        MAX_CONFIGURED_PACKET_RATE);
    g_authPacketsPerConnection = GetEnvSizeOrDefault(
        "MANAGER_IOCP_AUTH_PACKETS_PER_CONNECTION",
        DEFAULT_AUTH_PACKETS_PER_CONNECTION,
        1,
        MAX_CONFIGURED_PACKET_RATE);

    IOCP_INFO(
        "[TRANSPORT] Limits packetsPerSecond=%zu authPacketsPerConnection=%zu\n",
        g_inboundPacketsPerSecond,
        g_authPacketsPerConnection);
}

bool IsSupportedInboundPacketType(uint16_t type) {
    switch (static_cast<PacketType>(type)) {
    case PacketType::C2S_PING:
    case PacketType::C2S_LOGIN_REQ:
    case PacketType::C2S_REGISTER_REQ:
    case PacketType::C2S_ROOM_LIST_REQ:
    case PacketType::C2S_ROOM_CREATE_REQ:
    case PacketType::C2S_ROOM_JOIN_REQ:
    case PacketType::C2S_ROOM_LEAVE_REQ:
    case PacketType::C2S_ROOM_READY_REQ:
    case PacketType::C2S_ROOM_START_REQ:
    case PacketType::D2L_MATCH_END_NOTIFY:
    case PacketType::D2L_SERVER_READY_NOTIFY:
    case PacketType::D2L_MATCH_ABORT_NOTIFY:
        return true;
    default:
        return false;
    }
}

bool IsFixedInboundPayloadSizeValid(uint16_t type, uint16_t payloadLen) {
    switch (static_cast<PacketType>(type)) {
    case PacketType::C2S_PING:
    case PacketType::C2S_ROOM_LIST_REQ:
    case PacketType::C2S_ROOM_LEAVE_REQ:
    case PacketType::C2S_ROOM_START_REQ:
        return payloadLen == 0;
    case PacketType::C2S_ROOM_JOIN_REQ:
        return payloadLen == sizeof(uint32_t);
    case PacketType::C2S_ROOM_READY_REQ:
        return payloadLen == sizeof(uint8_t);
    case PacketType::D2L_SERVER_READY_NOTIFY:
        return payloadLen == 18;
    default:
        return true;
    }
}

bool ConsumeInboundPacketBudget(ClientContext* client, uint16_t type) {
    const ULONGLONG nowMs = GetTickCount64();
    if (nowMs < client->inboundPacketWindowStartMs ||
        nowMs - client->inboundPacketWindowStartMs >= 1000) {
        client->inboundPacketWindowStartMs = nowMs;
        client->inboundPacketCount = 0;
    }

    ++client->inboundPacketCount;
    if (client->inboundPacketCount > g_inboundPacketsPerSecond) {
        IOCP_ERR(
            "[TRANSPORT][LIMIT] Packet rate exceeded from=%d.%d.%d.%d count=%zu limit=%zu\n",
            client->addr.sin_addr.S_un.S_un_b.s_b1,
            client->addr.sin_addr.S_un.S_un_b.s_b2,
            client->addr.sin_addr.S_un.S_un_b.s_b3,
            client->addr.sin_addr.S_un.S_un_b.s_b4,
            client->inboundPacketCount,
            g_inboundPacketsPerSecond);
        return false;
    }

    const PacketType packetType = static_cast<PacketType>(type);
    if (packetType == PacketType::C2S_LOGIN_REQ ||
        packetType == PacketType::C2S_REGISTER_REQ) {
        ++client->authPacketCount;
        if (client->authPacketCount > g_authPacketsPerConnection) {
            IOCP_ERR(
                "[TRANSPORT][LIMIT] Auth request limit exceeded from=%d.%d.%d.%d count=%zu limit=%zu\n",
                client->addr.sin_addr.S_un.S_un_b.s_b1,
                client->addr.sin_addr.S_un.S_un_b.s_b2,
                client->addr.sin_addr.S_un.S_un_b.s_b3,
                client->addr.sin_addr.S_un.S_un_b.s_b4,
                client->authPacketCount,
                g_authPacketsPerConnection);
            return false;
        }
    }

    return true;
}

bool BuildListenAddress(const std::string& ip, uint16_t port, sockaddr_in& outAddr) {
    std::memset(&outAddr, 0, sizeof(outAddr));
    outAddr.sin_family = AF_INET;
    outAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &outAddr.sin_addr) != 1) {
        printf("[Server][ERR] inet_pton failed. ip=%s WSA=%d\n",
            ip.c_str(),
            WSAGetLastError());
        return false;
    }

    return true;
}


