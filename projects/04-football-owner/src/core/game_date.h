#pragma once

#include <compare>
#include <optional>
#include <string>

namespace football_owner {

struct GameDate {
    int year {1970};
    int month {1};
    int day {1};

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::string toIsoString() const;
    [[nodiscard]] GameDate addDays(int dayCount) const;
    [[nodiscard]] GameDate addMonths(int monthCount) const;
    [[nodiscard]] int daysUntil(const GameDate& other) const;

    static std::optional<GameDate> fromIsoString(const std::string& value);

    auto operator<=>(const GameDate&) const = default;
};

} // namespace football_owner
