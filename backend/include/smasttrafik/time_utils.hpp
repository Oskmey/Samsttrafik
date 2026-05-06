#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace smasttrafik {

struct DateRange {
    std::string from;
    std::string to;
};

std::optional<std::chrono::system_clock::time_point> parse_rfc3339(const std::string& value);
std::string format_rfc3339_local(std::chrono::system_clock::time_point value);
std::string current_rfc3339_local();
std::string traffic_day_for(std::chrono::system_clock::time_point value);
int delay_seconds_between(const std::string& planned, const std::string& estimated);
DateRange resolve_period(const std::string& period, const std::string& from, const std::string& to);

} // namespace smasttrafik
