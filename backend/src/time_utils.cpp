#include "smasttrafik/time_utils.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace smasttrafik {

namespace {

time_t portable_timegm(std::tm* tm) {
#if defined(_WIN32)
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

std::string local_offset(const std::tm& tm) {
#if defined(__linux__) || defined(__APPLE__)
    const long offset = tm.tm_gmtoff;
#else
    const long offset = 0;
#endif
    const char sign = offset >= 0 ? '+' : '-';
    const long abs_offset = offset >= 0 ? offset : -offset;
    const long hours = abs_offset / 3600;
    const long minutes = (abs_offset % 3600) / 60;
    std::ostringstream out;
    out << sign << std::setw(2) << std::setfill('0') << hours << ':'
        << std::setw(2) << std::setfill('0') << minutes;
    return out.str();
}

DateRange day_range(std::chrono::system_clock::time_point now, int days_back) {
    std::time_t current = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&current, &local);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    const auto start = std::chrono::system_clock::from_time_t(std::mktime(&local)) - std::chrono::hours(24 * days_back);
    const auto end = now;
    return {format_rfc3339_local(start), format_rfc3339_local(end)};
}

} // namespace

std::optional<std::chrono::system_clock::time_point> parse_rfc3339(const std::string& value) {
    if (value.size() < 19) {
        return std::nullopt;
    }

    std::tm tm{};
    std::istringstream stream(value.substr(0, 19));
    stream >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (stream.fail()) {
        return std::nullopt;
    }

    std::size_t zone_pos = value.find_first_of("Z+-", 19);
    int offset_seconds = 0;
    if (zone_pos != std::string::npos && value[zone_pos] != 'Z') {
        if (zone_pos + 5 >= value.size()) {
            return std::nullopt;
        }
        const int sign = value[zone_pos] == '-' ? -1 : 1;
        try {
            const int hours = std::stoi(value.substr(zone_pos + 1, 2));
            const int minutes = std::stoi(value.substr(zone_pos + 4, 2));
            offset_seconds = sign * ((hours * 3600) + (minutes * 60));
        } catch (...) {
            return std::nullopt;
        }
    }

    const time_t utc_seconds = portable_timegm(&tm) - offset_seconds;
    return std::chrono::system_clock::from_time_t(utc_seconds);
}

std::string format_rfc3339_local(std::chrono::system_clock::time_point value) {
    std::time_t time = std::chrono::system_clock::to_time_t(value);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%dT%H:%M:%S") << local_offset(local);
    return out.str();
}

std::string current_rfc3339_local() {
    return format_rfc3339_local(std::chrono::system_clock::now());
}

std::string traffic_day_for(std::chrono::system_clock::time_point value) {
    const auto shifted = value - std::chrono::hours(4);
    std::time_t time = std::chrono::system_clock::to_time_t(shifted);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

int delay_seconds_between(const std::string& planned, const std::string& estimated) {
    if (planned.empty() || estimated.empty()) {
        return 0;
    }
    const auto planned_time = parse_rfc3339(planned);
    const auto estimated_time = parse_rfc3339(estimated);
    if (!planned_time || !estimated_time) {
        return 0;
    }
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(*estimated_time - *planned_time).count());
}

DateRange resolve_period(const std::string& period, const std::string& from, const std::string& to) {
    if (period == "custom" && !from.empty() && !to.empty()) {
        return {from, to};
    }

    const auto now = std::chrono::system_clock::now();
    if (period == "today") {
        return day_range(now, 0);
    }
    if (period == "week") {
        return day_range(now, 6);
    }
    if (period == "month") {
        return day_range(now, 29);
    }
    if (period == "year") {
        return day_range(now, 364);
    }
    return day_range(now, 6);
}

} // namespace smasttrafik
