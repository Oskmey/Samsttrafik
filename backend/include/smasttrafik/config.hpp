#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace smasttrafik {

struct Config {
    std::string bind_host = "0.0.0.0";
    int port = 8080;

    std::string database_url;
    std::string migrations_dir = "backend/migrations";

    std::string vasttrafik_client_id;
    std::string vasttrafik_client_secret;
    std::string vasttrafik_token_url = "https://ext-api.vasttrafik.se/token";
    std::string vasttrafik_pr_base_url = "https://ext-api.vasttrafik.se/pr/v4";
    std::string vasttrafik_ts_base_url = "https://ext-api.vasttrafik.se/ts/v1";
    std::string accept_language = "sv";

    int upstream_requests_per_minute = 20;
    int upstream_burst_capacity = 1;
    int upstream_timeout_seconds = 20;
    int upstream_retries = 3;
    int poll_interval_seconds = 600;
    int departure_horizon_minutes = 60;
    int max_departures_per_line_direction = 2;
    int departure_limit = 40;
    int max_detail_calls_per_cycle = 12;
    int max_custom_range_days = 366;
    bool fetch_departure_details = false;
    bool store_raw_payloads = false;

    std::vector<std::string> monitored_stop_areas;
    bool use_demo_repository = true;
};

Config load_config();

std::vector<std::string> split_csv(const std::string& value);

} // namespace smasttrafik
