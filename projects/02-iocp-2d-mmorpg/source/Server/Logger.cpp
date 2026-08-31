#include "Logger.h"

#include <atomic>
#include <iostream>
#include <mutex>

namespace
{
    std::mutex g_logMutex;

#ifdef NDEBUG
    std::atomic_int g_minLogLevel{ static_cast<int>(Logger::Level::Warn) };
#else
    std::atomic_int g_minLogLevel{ static_cast<int>(Logger::Level::Info) };
#endif
}

void Logger::SetMinLevel(Level level)
{
    g_minLogLevel.store(static_cast<int>(level));
}

bool Logger::ShouldPrint(Level level)
{
    return static_cast<int>(level) >= g_minLogLevel.load();
}

void Logger::Info(const std::string& message)
{
    if (!ShouldPrint(Level::Info))
    {
        return;
    }

    Print("INFO", message);
}

void Logger::Warn(const std::string& message)
{
    if (!ShouldPrint(Level::Warn))
    {
        return;
    }

    Print("WARN", message);
}

void Logger::Error(const std::string& message)
{
    if (!ShouldPrint(Level::Error))
    {
        return;
    }

    Print("ERROR", message);
}

void Logger::Print(const char* level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::cout << "[" << level << "] " << message << '\n';
}
