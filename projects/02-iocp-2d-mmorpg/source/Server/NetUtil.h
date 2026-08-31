#pragma once

#include <string>
#include <WinSock2.h>

class NetUtil
{
public:
    static std::string GetWindowsErrorMessage(DWORD errorCode);
    static std::string GetWsaErrorMessage(int errorCode = WSAGetLastError());

    static bool IsPendingIoError();
    static void CloseSocket(SOCKET& socket);
};