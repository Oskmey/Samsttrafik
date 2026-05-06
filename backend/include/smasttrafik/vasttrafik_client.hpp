#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "smasttrafik/config.hpp"
#include "smasttrafik/http_client.hpp"
#include "smasttrafik/token_bucket.hpp"

namespace smasttrafik {

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
    nlohmann::json get_json(const std::string& url);
    std::string access_token();
    void authenticate();

    Config config_;
    HttpClient http_;
    TokenBucket limiter_;
    std::string token_;
    std::chrono::system_clock::time_point token_expires_at_;
    std::mutex token_mutex_;
};

} // namespace smasttrafik
