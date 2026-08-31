#include "core/game_date.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace football_owner {
namespace {

std::chrono::sys_days toSysDays(const GameDate& value)
{
    const auto date = std::chrono::year {value.year}
        / std::chrono::month {static_cast<unsigned>(value.month)}
        / std::chrono::day {static_cast<unsigned>(value.day)};

    if (!date.ok()) {
        throw std::invalid_argument("Invalid game date: " + value.toIsoString());
    }

    return std::chrono::sys_days {date};
}

GameDate fromSysDays(const std::chrono::sys_days value)
{
    const std::chrono::year_month_day date {value};
    return {
        static_cast<int>(date.year()),
        static_cast<int>(static_cast<unsigned>(date.month())),
        static_cast<int>(static_cast<unsigned>(date.day()))
    };
}

} // namespace

bool GameDate::isValid() const
{
    const auto value = std::chrono::year {year}
        / std::chrono::month {static_cast<unsigned>(month)}
        / std::chrono::day {static_cast<unsigned>(day)};
    return value.ok();
}

std::string GameDate::toIsoString() const
{
    char buffer[11] {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    return buffer;
}

GameDate GameDate::addDays(const int dayCount) const
{
    return fromSysDays(toSysDays(*this) + std::chrono::days {dayCount});
}

GameDate GameDate::addMonths(const int monthCount) const
{
    const auto source = std::chrono::year {year}
        / std::chrono::month {static_cast<unsigned>(month)}
        / std::chrono::day {static_cast<unsigned>(day)};
    if (!source.ok()) {
        throw std::invalid_argument("Invalid game date: " + toIsoString());
    }

    const auto sourceYearMonth = std::chrono::year_month {
        source.year(),
        source.month()
    };
    const auto targetYearMonth = sourceYearMonth + std::chrono::months {monthCount};
    const auto targetLastDay = std::chrono::year_month_day_last {
        targetYearMonth / std::chrono::last
    }.day();
    const auto targetDay = std::chrono::day {
        std::min(
            static_cast<unsigned>(source.day()),
            static_cast<unsigned>(targetLastDay))
    };
    return fromSysDays(std::chrono::sys_days {targetYearMonth / targetDay});
}

int GameDate::daysUntil(const GameDate& other) const
{
    return static_cast<int>((toSysDays(other) - toSysDays(*this)).count());
}

std::optional<GameDate> GameDate::fromIsoString(const std::string& value)
{
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return std::nullopt;
    }

    try {
        const GameDate result {
            std::stoi(value.substr(0, 4)),
            std::stoi(value.substr(5, 2)),
            std::stoi(value.substr(8, 2))
        };
        return result.isValid() ? std::optional<GameDate> {result} : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace football_owner
