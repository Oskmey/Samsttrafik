#include "smasttrafik/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>

namespace smasttrafik {

namespace {

std::string getenv_or(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }
    return value;
}

int getenv_int(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

bool getenv_bool(const char* name, bool fallback) {
    std::string value = getenv_or(name, fallback ? "true" : "false");
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

} // namespace

std::vector<std::string> split_csv(const std::string& value) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const auto first = item.find_first_not_of(" \t\r\n");
        const auto last = item.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
            parts.push_back(item.substr(first, last - first + 1));
        }
    }
    return parts;
}

Config load_config() {
    Config config;
    config.bind_host = getenv_or("SMASTTRAFIK_BIND_HOST", config.bind_host);
    config.port = getenv_int("SMASTTRAFIK_PORT", config.port);
    config.database_url = getenv_or("DATABASE_URL", getenv_or("SMASTTRAFIK_DATABASE_URL", ""));
    config.migrations_dir = getenv_or("SMASTTRAFIK_MIGRATIONS_DIR", config.migrations_dir);

    config.vasttrafik_client_id = getenv_or("VASTTRAFIK_CLIENT_ID", "");
    config.vasttrafik_client_secret = getenv_or("VASTTRAFIK_CLIENT_SECRET", "");
    config.vasttrafik_token_url = getenv_or("VASTTRAFIK_TOKEN_URL", config.vasttrafik_token_url);
    config.vasttrafik_pr_base_url = getenv_or("VASTTRAFIK_PR_BASE_URL", config.vasttrafik_pr_base_url);
    config.vasttrafik_ts_base_url = getenv_or("VASTTRAFIK_TS_BASE_URL", config.vasttrafik_ts_base_url);
    config.accept_language = getenv_or("SMASTTRAFIK_ACCEPT_LANGUAGE", config.accept_language);

    config.upstream_requests_per_minute = getenv_int("SMASTTRAFIK_UPSTREAM_REQUESTS_PER_MINUTE", config.upstream_requests_per_minute);
    config.upstream_timeout_seconds = getenv_int("SMASTTRAFIK_UPSTREAM_TIMEOUT_SECONDS", config.upstream_timeout_seconds);
    config.upstream_retries = getenv_int("SMASTTRAFIK_UPSTREAM_RETRIES", config.upstream_retries);
    config.poll_interval_seconds = getenv_int("SMASTTRAFIK_POLL_INTERVAL_SECONDS", config.poll_interval_seconds);
    config.departure_horizon_minutes = getenv_int("SMASTTRAFIK_DEPARTURE_HORIZON_MINUTES", config.departure_horizon_minutes);
    config.max_departures_per_line_direction = getenv_int(
        "SMASTTRAFIK_MAX_DEPARTURES_PER_LINE_DIRECTION",
        config.max_departures_per_line_direction
    );
    config.fetch_departure_details = getenv_bool("SMASTTRAFIK_FETCH_DEPARTURE_DETAILS", config.fetch_departure_details);

    const std::string default_stop_areas = "9021014001760000,9021014003980000,9021014000020000";
    config.monitored_stop_areas = split_csv(getenv_or("SMASTTRAFIK_MONITORED_STOP_AREAS", default_stop_areas));

#ifdef SMASTTRAFIK_ENABLE_POSTGRES
    config.use_demo_repository = getenv_bool("SMASTTRAFIK_USE_DEMO_REPOSITORY", config.database_url.empty());
#else
    config.use_demo_repository = true;
#endif

    return config;
}

} // namespace smasttrafik
