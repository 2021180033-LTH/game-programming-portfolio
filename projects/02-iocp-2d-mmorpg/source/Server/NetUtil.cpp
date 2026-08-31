#include "NetUtil.h"

#include <Windows.h>

std::string NetUtil::GetWindowsErrorMessage(DWORD errorCode)
{
    char* messageBuffer = nullptr;

    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0,
        nullptr
    );

    std::string message;

    if (size == 0 || messageBuffer == nullptr)
    {
        message = "Unknown Windows error: " + std::to_string(errorCode);
    }
    else
    {
        message.assign(messageBuffer, size);
        LocalFree(messageBuffer);
    }

    return message;
}

std::string NetUtil::GetWsaErrorMessage(int errorCode)
{
    return GetWindowsErrorMessage(static_cast<DWORD>(errorCode));
}

bool NetUtil::IsPendingIoError()
{
    return WSAGetLastError() == WSA_IO_PENDING;
}

void NetUtil::CloseSocket(SOCKET& socket)
{
    if (socket != INVALID_SOCKET)
    {
        closesocket(socket);
        socket = INVALID_SOCKET;
    }
}