#pragma once

#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>

#include <vector>
#include <cstddef>
#include <cstring>

class ClientSession;

enum class IoOperation
{
    Accept,
    Recv,
    Send
};

class IocpEvent
{
public:
    IocpEvent(IoOperation operation, std::size_t bufferSize)
        : operation_(operation),
        buffer_(bufferSize)
    {
        Reset();
    }

    void Reset()
    {
        ZeroMemory(&overlapped_, sizeof(overlapped_));

        wsaBuffer_.buf = buffer_.data();
        wsaBuffer_.len = static_cast<ULONG>(buffer_.size());
    }

    OVERLAPPED* GetOverlapped()
    {
        return &overlapped_;
    }

    WSABUF* GetWsaBuffer()
    {
        return &wsaBuffer_;
    }

    char* GetBuffer()
    {
        return buffer_.data();
    }

    const char* GetBuffer() const
    {
        return buffer_.data();
    }

    std::size_t GetBufferSize() const
    {
        return buffer_.size();
    }

    IoOperation GetOperation() const
    {
        return operation_;
    }

    void SetOwner(ClientSession* owner)
    {
        owner_ = owner;
    }

    ClientSession* GetOwner() const
    {
        return owner_;
    }

    void SetAcceptSocket(SOCKET socket)
    {
        acceptSocket_ = socket;
    }

    SOCKET GetAcceptSocket() const
    {
        return acceptSocket_;
    }

    bool CopyFrom(const char* data, int size)
    {
        if (data == nullptr || size <= 0)
        {
            return false;
        }

        if (static_cast<std::size_t>(size) > buffer_.size())
        {
            return false;
        }

        std::memcpy(buffer_.data(), data, size);
        wsaBuffer_.buf = buffer_.data();
        wsaBuffer_.len = static_cast<ULONG>(size);
        return true;
    }

    static IocpEvent* FromOverlapped(OVERLAPPED* overlapped)
    {
        return reinterpret_cast<IocpEvent*>(overlapped);
    }

private:
    OVERLAPPED overlapped_{};

    WSABUF wsaBuffer_{};

    IoOperation operation_;
    std::vector<char> buffer_;

    ClientSession* owner_ = nullptr;

    SOCKET acceptSocket_ = INVALID_SOCKET;
};