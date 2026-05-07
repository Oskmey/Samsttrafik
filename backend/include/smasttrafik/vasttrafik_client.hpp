#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "smasttrafik/config.hpp"
#include "smasttrafik/http_client.hpp"
#include "smasttrafik/token_bucket.hpp"

namespace smasttrafik {

class UpstreamBackoffError final : public std::runtime_error {
public:
    explicit UpstreamBackoffError(std::string message, std::chrono::system_clock::time_point backoff_until);

    std::chrono::system_clock::time_point backoff_until() const;

private:
    std::chrono::system_clock::time_point backoff_until_;
};

class VasttrafikClient {
public:
    explicit VasttrafikClient(Config config);

    nlohmann::json fetch_stop_areas();
    nlohmann::json fetch_departures(const std::string& stop_area_gid);
    nlohmann::json fetch_departure_details(
        const std::string& stop_area_gid,
        const std::string& details_reference
    );
    nlohmann::json fetch_traffic_situations();

private:
    void respect_global_backoff();
    void set_global_backoff(std::chrono::system_clock::time_point backoff_until);
    nlohmann::json get_json(const std::string& url);
    std::string access_token();
    void authenticate();

    Config config_;
    HttpClient http_;
    TokenBucket limiter_;
    std::string token_;
    std::chrono::system_clock::time_point token_expires_at_;
    std::chrono::system_clock::time_point global_backoff_until_;
    std::mutex token_mutex_;
    std::mutex backoff_mutex_;
};

std::optional<std::chrono::system_clock::time_point> parse_retry_after_header(
    const std::string& value,
    std::chrono::system_clock::time_point now
);

} // namespace smasttrafik
