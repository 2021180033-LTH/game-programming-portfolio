#pragma once

#include <string>

class Logger
{
public:
    enum class Level
    {
        Info = 0,
        Warn = 1,
        Error = 2,
        Off = 3
    };

    static void SetMinLevel(Level level);

    static void Info(const std::string& message);
    static void Warn(const std::string& message);
    static void Error(const std::string& message);

private:
    static bool ShouldPrint(Level level);
    static void Print(const char* level, const std::string& message);
};
