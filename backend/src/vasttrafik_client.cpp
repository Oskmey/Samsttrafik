#include "smasttrafik/vasttrafik_client.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

namespace {

time_t portable_timegm(std::tm* tm) {
#if defined(_WIN32)
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

std::string base64_encode(const std::string& input) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    int value = 0;
    int bits = -6;
    for (const unsigned char c : input) {
        value = (value << 8) + c;
        bits += 8;
        while (bits >= 0) {
            output.push_back(alphabet[(value >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) {
        output.push_back(alphabet[((value << 8) >> (bits + 8)) & 0x3F]);
    }
    while (output.size() % 4 != 0) {
        output.push_back('=');
    }
    return output;
}

std::string join_url(const std::string& base, const std::string& path) {
    if (base.empty()) {
        return path;
    }
    if (base.back() == '/' && !path.empty() && path.front() == '/') {
        return base.substr(0, base.size() - 1) + path;
    }
    if (base.back() != '/' && !path.empty() && path.front() != '/') {
        return base + "/" + path;
    }
    return base + path;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string header_value(const HttpResponse& response, const std::string& key) {
    const std::string wanted = lowercase(key);
    for (const auto& [name, value] : response.headers) {
        if (lowercase(name) == wanted) {
            return value;
        }
    }
    return {};
}

} // namespace

UpstreamBackoffError::UpstreamBackoffError(
    std::string message,
    std::chrono::system_clock::time_point backoff_until
) : std::runtime_error(std::move(message)),
    backoff_until_(backoff_until) {}

std::chrono::system_clock::time_point UpstreamBackoffError::backoff_until() const {
    return backoff_until_;
}

VasttrafikClient::VasttrafikClient(Config config)
    : config_(std::move(config)),
      limiter_(config_.upstream_requests_per_minute, config_.upstream_burst_capacity),
      token_expires_at_(std::chrono::system_clock::time_point::min()),
      global_backoff_until_(std::chrono::system_clock::time_point::min()) {}

nlohmann::json VasttrafikClient::fetch_stop_areas() {
    return get_json(join_url(config_.vasttrafik_pr_base_url, "/stop-areas"));
}

nlohmann::json VasttrafikClient::fetch_departures(const std::string& stop_area_gid) {
    std::ostringstream url;
    url << join_url(config_.vasttrafik_pr_base_url, "/stop-areas/")
        << url_encode(stop_area_gid)
        << "/departures?timeSpanInMinutes=" << config_.departure_horizon_minutes
        << "&maxDeparturesPerLineAndDirection=" << config_.max_departures_per_line_direction
        << "&limit=" << config_.departure_limit
        << "&transportModes=bus";
    return get_json(url.str());
}

nlohmann::json VasttrafikClient::fetch_departure_details(
    const std::string& stop_area_gid,
    const std::string& details_reference
) {
    std::ostringstream url;
    url << join_url(config_.vasttrafik_pr_base_url, "/stop-areas/")
        << url_encode(stop_area_gid)
        << "/departures/"
        << url_encode(details_reference)
        << "/details?includes=servicejourneycalls";
    return get_json(url.str());
}

nlohmann::json VasttrafikClient::fetch_traffic_situations() {
    return get_json(join_url(config_.vasttrafik_ts_base_url, "/traffic-situations"));
}

nlohmann::json VasttrafikClient::get_json(const std::string& url) {
    std::exception_ptr last_error;
    for (int attempt = 0; attempt <= config_.upstream_retries; ++attempt) {
        try {
            respect_global_backoff();
            limiter_.wait_for_token();
            const std::string token = access_token();
            const HttpResponse response = http_.get(
                url,
                {
                    {"Accept", "application/json"},
                    {"Accept-Language", config_.accept_language},
                    {"Authorization", "Bearer " + token},
                },
                config_.upstream_timeout_seconds
            );

            if (response.status_code == 429) {
                const auto now = std::chrono::system_clock::now();
                const auto parsed_retry_after = parse_retry_after_header(header_value(response, "Retry-After"), now);
                const auto backoff_until = parsed_retry_after.value_or(now + std::chrono::seconds(60));
                set_global_backoff(backoff_until);
                throw UpstreamBackoffError("Vasttrafik request rate-limited; honoring Retry-After/backoff", backoff_until);
            }
            if (response.status_code >= 500) {
                last_error = std::make_exception_ptr(
                    std::runtime_error("Vasttrafik request failed with HTTP " + std::to_string(response.status_code))
                );
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1) * (attempt + 1)));
                continue;
            }
            if (response.status_code < 200 || response.status_code >= 300) {
                throw std::runtime_error("Vasttrafik request failed with HTTP " + std::to_string(response.status_code));
            }
            return nlohmann::json::parse(response.body);
        } catch (const UpstreamBackoffError&) {
            throw;
        } catch (...) {
            last_error = std::current_exception();
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1)));
        }
    }
    if (last_error) {
        std::rethrow_exception(last_error);
    }
    throw std::runtime_error("Vasttrafik request failed");
}

void VasttrafikClient::respect_global_backoff() {
    std::lock_guard<std::mutex> lock(backoff_mutex_);
    const auto now = std::chrono::system_clock::now();
    if (now < global_backoff_until_) {
        throw UpstreamBackoffError("Vasttrafik global backoff is active", global_backoff_until_);
    }
}

void VasttrafikClient::set_global_backoff(std::chrono::system_clock::time_point backoff_until) {
    std::lock_guard<std::mutex> lock(backoff_mutex_);
    if (backoff_until > global_backoff_until_) {
        global_backoff_until_ = backoff_until;
    }
}

std::string VasttrafikClient::access_token() {
    std::lock_guard<std::mutex> lock(token_mutex_);
    const auto now = std::chrono::system_clock::now();
    if (!token_.empty() && now + std::chrono::minutes(2) < token_expires_at_) {
        return token_;
    }
    authenticate();
    return token_;
}

void VasttrafikClient::authenticate() {
    if (config_.vasttrafik_client_id.empty() || config_.vasttrafik_client_secret.empty()) {
        throw std::runtime_error("VASTTRAFIK_CLIENT_ID and VASTTRAFIK_CLIENT_SECRET are required for live collection");
    }

    const std::string credentials = base64_encode(config_.vasttrafik_client_id + ":" + config_.vasttrafik_client_secret);
    const HttpResponse response = http_.post_form(
        config_.vasttrafik_token_url,
        "grant_type=client_credentials",
        {
            {"Accept", "application/json"},
            {"Authorization", "Basic " + credentials},
            {"Content-Type", "application/x-www-form-urlencoded"},
        },
        config_.upstream_timeout_seconds
    );

    if (response.status_code < 200 || response.status_code >= 300) {
        throw std::runtime_error("Vasttrafik token request failed with HTTP " + std::to_string(response.status_code));
    }

    const auto payload = nlohmann::json::parse(response.body);
    token_ = payload.value("access_token", "");
    const int expires_in = payload.value("expires_in", 3600);
    token_expires_at_ = std::chrono::system_clock::now() + std::chrono::seconds(expires_in);
    if (token_.empty()) {
        throw std::runtime_error("Vasttrafik token response did not contain access_token");
    }
}

std::optional<std::chrono::system_clock::time_point> parse_retry_after_header(
    const std::string& value,
    std::chrono::system_clock::time_point now
) {
    if (value.empty()) {
        return std::nullopt;
    }

    if (std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch); })) {
        try {
            return now + std::chrono::seconds(std::stoll(value));
        } catch (...) {
            return std::nullopt;
        }
    }

    std::tm tm{};
    std::istringstream stream(value);
    stream >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    if (stream.fail()) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(portable_timegm(&tm));
}

} // namespace smasttrafik
