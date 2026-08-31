#include "IocpServer.h"

#include "ClientSession.h"
#include "IocpEvent.h"
#include "PacketDispatcher.h"
#include "GameWorld.h"

#include "ServerConfig.h"
#include "Logger.h"
#include "NetUtil.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <new>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

namespace
{
    constexpr int MAX_PENDING_SEND_PER_SESSION = 512;
}

IocpServer::IocpServer()
{
}

IocpServer::~IocpServer()
{
    Stop();
}

bool IocpServer::Start(unsigned short port)
{
    if (!InitializeWinsock())
    {
        Stop();
        return false;
    }

    if (!CreateListenSocket(port))
    {
        Stop();
        return false;
    }

    if (!CreateIocpHandle())
    {
        Stop();
        return false;
    }

    if (!AssociateSocket(listenSocket_, 0))
    {
        Stop();
        return false;
    }

    if (!LoadExtensionFunctions())
    {
        Stop();
        return false;
    }

    auto sendFunction =
        [this](ClientSession* session, const char* data, int size)
        {
            SendPacket(session, data, size);
        };

    gameWorld_ = std::make_unique<GameWorld>(sendFunction);

    dispatcher_ = std::make_unique<PacketDispatcher>(
        sendFunction,
        gameWorld_.get()
    );

    running_.store(true);

    const int workerCount = GetWorkerThreadCount();

    for (int i = 0; i < workerCount; ++i)
    {
        workerThreads_.emplace_back(&IocpServer::WorkerThreadLoop, this);
    }

    for (int i = 0; i < ServerConfig::DEFAULT_ACCEPT_POST_COUNT; ++i)
    {
        if (!PostAccept())
        {
            Logger::Error("PostAccept failed during startup.");
            Stop();
            return false;
        }
    }

    Logger::Info("Server started. port=" + std::to_string(port));
    Logger::Info("WorkerThreadCount=" + std::to_string(workerCount));

    return true;
}

void IocpServer::Stop()
{
    const bool wasRunning = running_.exchange(false);

    if (iocpHandle_ != nullptr && wasRunning)
    {
        for (std::size_t i = 0; i < workerThreads_.size(); ++i)
        {
            PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
        }
    }

    for (std::thread& worker : workerThreads_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workerThreads_.clear();

    {
        std::lock_guard<std::mutex> lock(sessionLock_);

        for (auto& [id, session] : sessions_)
        {
            if (session)
            {
                if (gameWorld_)
                {
                    gameWorld_->LogoutPlayer(session.get());
                }

                session->Disconnect();
            }
        }

        sessions_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(closingSessionLock_);
        closingSessions_.clear();
    }

    dispatcher_.reset();
    gameWorld_.reset();

    if (listenSocket_ != INVALID_SOCKET)
    {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }

    if (iocpHandle_ != nullptr)
    {
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
    }

    acceptEx_ = nullptr;
    getAcceptExSockaddrs_ = nullptr;

    if (winsockInitialized_)
    {
        WSACleanup();
        winsockInitialized_ = false;
    }

    if (wasRunning)
    {
        Logger::Info("Server stopped.");
    }
}

bool IocpServer::InitializeWinsock()
{
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData_);

    if (result != 0)
    {
        Logger::Error("WSAStartup failed. error=" + std::to_string(result));
        return false;
    }

    winsockInitialized_ = true;
    return true;
}

bool IocpServer::CreateListenSocket(unsigned short port)
{
    listenSocket_ = WSASocket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP,
        nullptr,
        0,
        WSA_FLAG_OVERLAPPED
    );

    if (listenSocket_ == INVALID_SOCKET)
    {
        Logger::Error("WSASocket failed. " + NetUtil::GetWsaErrorMessage());
        return false;
    }

    BOOL reuse = TRUE;

    setsockopt(
        listenSocket_,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        sizeof(reuse)
    );

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(
        listenSocket_,
        reinterpret_cast<sockaddr*>(&serverAddr),
        sizeof(serverAddr)) == SOCKET_ERROR)
    {
        Logger::Error("bind failed. " + NetUtil::GetWsaErrorMessage());
        return false;
    }

    if (listen(listenSocket_, ServerConfig::BACKLOG_SIZE) == SOCKET_ERROR)
    {
        Logger::Error("listen failed. " + NetUtil::GetWsaErrorMessage());
        return false;
    }

    return true;
}

bool IocpServer::CreateIocpHandle()
{
    iocpHandle_ = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,
        nullptr,
        0,
        0
    );

    if (iocpHandle_ == nullptr)
    {
        Logger::Error(
            "CreateIoCompletionPort failed. " +
            NetUtil::GetWindowsErrorMessage(GetLastError())
        );

        return false;
    }

    return true;
}

bool IocpServer::LoadExtensionFunctions()
{
    DWORD bytes = 0;

    GUID acceptExGuid = WSAID_ACCEPTEX;

    int result = WSAIoctl(
        listenSocket_,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &acceptExGuid,
        sizeof(acceptExGuid),
        &acceptEx_,
        sizeof(acceptEx_),
        &bytes,
        nullptr,
        nullptr
    );

    if (result == SOCKET_ERROR)
    {
        Logger::Error("Load AcceptEx failed. " + NetUtil::GetWsaErrorMessage());
        return false;
    }

    GUID getAcceptExSockaddrsGuid = WSAID_GETACCEPTEXSOCKADDRS;

    result = WSAIoctl(
        listenSocket_,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &getAcceptExSockaddrsGuid,
        sizeof(getAcceptExSockaddrsGuid),
        &getAcceptExSockaddrs_,
        sizeof(getAcceptExSockaddrs_),
        &bytes,
        nullptr,
        nullptr
    );

    if (result == SOCKET_ERROR)
    {
        Logger::Error("Load GetAcceptExSockaddrs failed. " + NetUtil::GetWsaErrorMessage());
        return false;
    }

    return true;
}

bool IocpServer::AssociateSocket(SOCKET socket, ULONG_PTR completionKey)
{
    HANDLE result = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(socket),
        iocpHandle_,
        completionKey,
        0
    );

    if (result == nullptr)
    {
        Logger::Error(
            "AssociateSocket failed. " +
            NetUtil::GetWindowsErrorMessage(GetLastError())
        );

        return false;
    }

    return true;
}

bool IocpServer::PostAccept()
{
    SOCKET acceptSocket = WSASocket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP,
        nullptr,
        0,
        WSA_FLAG_OVERLAPPED
    );

    if (acceptSocket == INVALID_SOCKET)
    {
        Logger::Error("Create accept socket failed. " + NetUtil::GetWsaErrorMessage());
        return false;
    }

    auto* acceptEvent = new IocpEvent(
        IoOperation::Accept,
        ServerConfig::ACCEPT_ADDRESS_BUFFER_SIZE
    );

    acceptEvent->SetAcceptSocket(acceptSocket);

    DWORD receivedBytes = 0;
    const DWORD addressLength = sizeof(sockaddr_in) + 16;

    BOOL result = acceptEx_(
        listenSocket_,
        acceptSocket,
        acceptEvent->GetBuffer(),
        0,
        addressLength,
        addressLength,
        &receivedBytes,
        acceptEvent->GetOverlapped()
    );

    if (result == FALSE)
    {
        const int error = WSAGetLastError();

        if (error != WSA_IO_PENDING)
        {
            Logger::Error("AcceptEx failed. " + NetUtil::GetWsaErrorMessage(error));

            closesocket(acceptSocket);
            delete acceptEvent;

            return false;
        }
    }

    return true;
}

bool IocpServer::PostRecv(ClientSession* session)
{
    if (session == nullptr || !session->IsConnected() || session->IsDisconnecting())
    {
        return false;
    }

    IocpEvent& recvEvent = session->GetRecvEvent();

    recvEvent.Reset();
    recvEvent.SetOwner(session);

    DWORD flags = 0;
    DWORD receivedBytes = 0;

    int result = WSARecv(
        session->GetSocket(),
        recvEvent.GetWsaBuffer(),
        1,
        &receivedBytes,
        &flags,
        recvEvent.GetOverlapped(),
        nullptr
    );

    if (result == SOCKET_ERROR)
    {
        const int error = WSAGetLastError();

        if (error != WSA_IO_PENDING)
        {
            Logger::Error(
                "WSARecv failed. session=" +
                std::to_string(session->GetSessionId()) +
                ", error=" +
                NetUtil::GetWsaErrorMessage(error)
            );

            DisconnectSession(session);
            RemoveSession(session);
            return false;
        }
    }

    return true;
}

void IocpServer::SendPacket(ClientSession* session, const char* data, int size)
{
    if (session == nullptr || data == nullptr || size <= 0)
    {
        return;
    }

    if (!session->IsConnected() || session->IsDisconnecting())
    {
        return;
    }

    if (!session->TryBeginSend(MAX_PENDING_SEND_PER_SESSION))
    {
        const int pendingCount = session->GetPendingSendCount();

        if (DisconnectSession(session))
        {
            Logger::Warn(
                "Send backlog overflow. disconnect session=" +
                std::to_string(session->GetSessionId()) +
                ", pending=" +
                std::to_string(pendingCount)
            );
        }

        return;
    }

    if (session->IsDisconnecting())
    {
        session->EndSend();
        return;
    }

    auto* sendEvent = new (std::nothrow) IocpEvent(
        IoOperation::Send,
        static_cast<std::size_t>(size)
    );

    if (sendEvent == nullptr)
    {
        session->EndSend();

        Logger::Error(
            "Failed to allocate send event. disconnect session=" +
            std::to_string(session->GetSessionId())
        );

        DisconnectSession(session);
        return;
    }

    sendEvent->SetOwner(session);

    if (!sendEvent->CopyFrom(data, size))
    {
        session->EndSend();
        delete sendEvent;
        return;
    }

    DWORD sentBytes = 0;

    int result = WSASend(
        session->GetSocket(),
        sendEvent->GetWsaBuffer(),
        1,
        &sentBytes,
        0,
        sendEvent->GetOverlapped(),
        nullptr
    );

    if (result == SOCKET_ERROR)
    {
        const int error = WSAGetLastError();

        if (error != WSA_IO_PENDING)
        {
            Logger::Error(
                "WSASend failed. session=" +
                std::to_string(session->GetSessionId()) +
                ", error=" +
                NetUtil::GetWsaErrorMessage(error)
            );

            session->EndSend();
            delete sendEvent;
            DisconnectSession(session);
        }
    }
}

void IocpServer::WorkerThreadLoop()
{
    while (running_.load())
    {
        DWORD transferredBytes = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* overlapped = nullptr;

        BOOL result = GetQueuedCompletionStatus(
            iocpHandle_,
            &transferredBytes,
            &completionKey,
            &overlapped,
            INFINITE
        );

        if (!running_.load())
        {
            break;
        }

        if (overlapped == nullptr)
        {
            continue;
        }

        IocpEvent* event = IocpEvent::FromOverlapped(overlapped);

        if (result == FALSE)
        {
            const DWORD error = GetLastError();
            const IoOperation operation = event->GetOperation();

            if (operation == IoOperation::Accept)
            {
                SOCKET acceptSocket = event->GetAcceptSocket();

                if (acceptSocket != INVALID_SOCKET)
                {
                    closesocket(acceptSocket);
                }

                delete event;

                if (running_.load())
                {
                    PostAccept();
                }

                continue;
            }

            if (operation == IoOperation::Send)
            {
                ClientSession* owner = event->GetOwner();

                if (owner != nullptr)
                {
                    owner->EndSend();
                }

                delete event;
                CleanupClosingSessions();
                continue;
            }

            ClientSession* owner = event->GetOwner();

            if (owner != nullptr)
            {
                if (!owner->IsDisconnecting())
                {
                    Logger::Warn(
                        "GQCS failed. session=" +
                        std::to_string(owner->GetSessionId()) +
                        ", error=" +
                        NetUtil::GetWindowsErrorMessage(error)
                    );
                }

                DisconnectSession(owner);
                RemoveSession(owner);
            }

            continue;
        }

        switch (event->GetOperation())
        {
        case IoOperation::Accept:
            HandleAccept(event, transferredBytes);
            break;

        case IoOperation::Recv:
            HandleRecv(event, transferredBytes);
            break;

        case IoOperation::Send:
            HandleSend(event, transferredBytes);
            break;

        default:
            Logger::Warn("Unknown IO operation completed.");
            break;
        }
    }
}

void IocpServer::HandleAccept(IocpEvent* event, DWORD transferredBytes)
{
    if (event == nullptr)
    {
        return;
    }

    SOCKET clientSocket = event->GetAcceptSocket();

    if (clientSocket == INVALID_SOCKET)
    {
        delete event;

        if (running_.load())
        {
            PostAccept();
        }

        return;
    }

    setsockopt(
        clientSocket,
        SOL_SOCKET,
        SO_UPDATE_ACCEPT_CONTEXT,
        reinterpret_cast<const char*>(&listenSocket_),
        sizeof(listenSocket_)
    );

    const int sessionId = nextSessionId_.fetch_add(1);

    auto session = std::make_shared<ClientSession>();

    if (!session->Initialize(sessionId, clientSocket))
    {
        closesocket(clientSocket);
        delete event;

        if (running_.load())
        {
            PostAccept();
        }

        return;
    }

    if (!AssociateSocket(clientSocket, reinterpret_cast<ULONG_PTR>(session.get())))
    {
        session->Disconnect();
        delete event;

        if (running_.load())
        {
            PostAccept();
        }

        return;
    }

    {
        std::lock_guard<std::mutex> lock(sessionLock_);
        sessions_.emplace(sessionId, session);
    }

    Logger::Info("Client connected. session=" + std::to_string(sessionId));

    PostRecv(session.get());

    delete event;

    if (running_.load())
    {
        PostAccept();
    }
}

void IocpServer::HandleRecv(IocpEvent* event, DWORD transferredBytes)
{
    if (event == nullptr)
    {
        return;
    }

    ClientSession* session = event->GetOwner();

    if (session == nullptr)
    {
        return;
    }

    if (transferredBytes == 0)
    {
        Logger::Info(
            "Client disconnected. session=" +
            std::to_string(session->GetSessionId())
        );

        DisconnectSession(session);
        RemoveSession(session);
        return;
    }

    session->AppendReceivedData(
        event->GetBuffer(),
        static_cast<int>(transferredBytes)
    );

    std::vector<char>& packetBuffer = session->GetPacketBuffer();

    while (true)
    {
        if (packetBuffer.size() < 1)
        {
            break;
        }

        const unsigned char packetSize =
            static_cast<unsigned char>(packetBuffer[0]);

        if (packetSize == 0)
        {
            Logger::Warn(
                "Invalid packet size 0. session=" +
                std::to_string(session->GetSessionId())
            );

            DisconnectSession(session);
            RemoveSession(session);
            return;
        }

        if (packetBuffer.size() < packetSize)
        {
            break;
        }

        if (dispatcher_)
        {
            dispatcher_->Dispatch(
                session,
                packetBuffer.data(),
                static_cast<int>(packetSize)
            );
        }

        packetBuffer.erase(
            packetBuffer.begin(),
            packetBuffer.begin() + packetSize
        );
    }

    PostRecv(session);
}

void IocpServer::HandleSend(IocpEvent* event, DWORD transferredBytes)
{
    if (event == nullptr)
    {
        return;
    }

    ClientSession* owner = event->GetOwner();

    if (owner != nullptr)
    {
        owner->EndSend();
    }

    delete event;

    CleanupClosingSessions();
}

bool IocpServer::DisconnectSession(ClientSession* session)
{
    if (session == nullptr)
    {
        return false;
    }

    if (!session->TryMarkDisconnecting())
    {
        return false;
    }

    if (gameWorld_)
    {
        gameWorld_->LogoutPlayer(session);
    }

    session->Disconnect();
    return true;
}

void IocpServer::RemoveSession(ClientSession* session)
{
    if (session == nullptr)
    {
        return;
    }

    const int sessionId = session->GetSessionId();
    std::shared_ptr<ClientSession> removedSession;

    {
        std::lock_guard<std::mutex> lock(sessionLock_);

        auto iter = sessions_.find(sessionId);

        if (iter == sessions_.end())
        {
            return;
        }

        if (iter->second.get() != session)
        {
            return;
        }

        removedSession = iter->second;
        sessions_.erase(iter);
    }

    if (removedSession && removedSession->GetPendingSendCount() > 0)
    {
        std::lock_guard<std::mutex> lock(closingSessionLock_);
        closingSessions_.push_back(std::move(removedSession));
    }
}

void IocpServer::CleanupClosingSessions()
{
    std::lock_guard<std::mutex> lock(closingSessionLock_);

    closingSessions_.erase(
        std::remove_if(
            closingSessions_.begin(),
            closingSessions_.end(),
            [](const std::shared_ptr<ClientSession>& session)
            {
                return session == nullptr || session->GetPendingSendCount() <= 0;
            }
        ),
        closingSessions_.end()
    );
}

int IocpServer::GetWorkerThreadCount() const
{
    if (ServerConfig::WORKER_THREAD_COUNT > 0)
    {
        return ServerConfig::WORKER_THREAD_COUNT;
    }

    unsigned int hardwareCount = std::thread::hardware_concurrency();

    if (hardwareCount == 0)
    {
        hardwareCount = 4;
    }

    return static_cast<int>(hardwareCount);
}