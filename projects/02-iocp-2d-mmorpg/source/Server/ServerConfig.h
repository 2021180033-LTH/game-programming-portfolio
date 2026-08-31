#pragma once

#include <cstdint>
#include <WinSock2.h>

#include "protocol_2026.h"

namespace ServerConfig
{
    constexpr uint16_t SERVER_PORT = PORT;

    constexpr int MAX_CLIENT_COUNT = MAX_PLAYERS;

    constexpr int RECV_BUFFER_SIZE = 4096;
    constexpr int SEND_BUFFER_SIZE = 4096;

    constexpr int ACCEPT_ADDRESS_BUFFER_SIZE = (sizeof(sockaddr_in) + 16) * 2;

    constexpr int DEFAULT_ACCEPT_POST_COUNT = 64;

    constexpr int WORKER_THREAD_COUNT = 0;

    constexpr int BACKLOG_SIZE = SOMAXCONN;

    constexpr int MOVE_INTERVAL_MS = 500;
}