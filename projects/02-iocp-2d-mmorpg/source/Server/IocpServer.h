#pragma once

#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class ClientSession;
class PacketDispatcher;
class IocpEvent;
class GameWorld;

class IocpServer
{
public:
    IocpServer();
    ~IocpServer();

    IocpServer(const IocpServer&) = delete;
    IocpServer& operator=(const IocpServer&) = delete;

    bool Start(unsigned short port);
    void Stop();

    void SendPacket(ClientSession* session, const char* data, int size);

private:
    bool InitializeWinsock();
    bool CreateListenSocket(unsigned short port);
    bool CreateIocpHandle();
    bool LoadExtensionFunctions();

    bool AssociateSocket(SOCKET socket, ULONG_PTR completionKey);

    bool PostAccept();
    bool PostRecv(ClientSession* session);

    void WorkerThreadLoop();

    void HandleAccept(IocpEvent* event, DWORD transferredBytes);
    void HandleRecv(IocpEvent* event, DWORD transferredBytes);
    void HandleSend(IocpEvent* event, DWORD transferredBytes);

    bool DisconnectSession(ClientSession* session);
    void RemoveSession(ClientSession* session);
    void CleanupClosingSessions();

    int GetWorkerThreadCount() const;

private:
    WSADATA wsaData_{};
    bool winsockInitialized_ = false;

    SOCKET listenSocket_ = INVALID_SOCKET;
    HANDLE iocpHandle_ = nullptr;

    LPFN_ACCEPTEX acceptEx_ = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS getAcceptExSockaddrs_ = nullptr;

    std::atomic_bool running_ = false;

    std::vector<std::thread> workerThreads_;

    std::mutex sessionLock_;
    std::unordered_map<int, std::shared_ptr<ClientSession>> sessions_;

    std::mutex closingSessionLock_;
    std::vector<std::shared_ptr<ClientSession>> closingSessions_;

    std::atomic_int nextSessionId_ = 1;

    std::unique_ptr<GameWorld> gameWorld_;
    std::unique_ptr<PacketDispatcher> dispatcher_;
};