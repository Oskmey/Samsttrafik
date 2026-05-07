#include "smasttrafik/traffic_collector.hpp"

#include <chrono>
#include <iostream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

namespace {

const Json* object_at(const Json& value, const std::initializer_list<const char*> keys) {
    const Json* current = &value;
    for (const char* key : keys) {
        if (!current->is_object() || !current->contains(key)) {
            return nullptr;
        }
        current = &(*current)[key];
    }
    return current;
}

std::string string_at(const Json& value, const std::initializer_list<const char*> keys, const std::string& fallback = "") {
    const Json* item = object_at(value, keys);
    if (item == nullptr || item->is_null()) {
        return fallback;
    }
    if (item->is_string()) {
        return item->get<std::string>();
    }
    if (item->is_number_integer()) {
        return std::to_string(item->get<long long>());
    }
    return fallback;
}

double double_at(const Json& value, const std::initializer_list<const char*> keys, double fallback = 0.0) {
    const Json* item = object_at(value, keys);
    if (item == nullptr || item->is_null()) {
        return fallback;
    }
    if (item->is_number()) {
        return item->get<double>();
    }
    if (item->is_string()) {
        try {
            return std::stod(item->get<std::string>());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

bool bool_at(const Json& value, const std::initializer_list<const char*> keys, bool fallback = false) {
    const Json* item = object_at(value, keys);
    if (item == nullptr || item->is_null()) {
        return fallback;
    }
    if (item->is_boolean()) {
        return item->get<bool>();
    }
    return fallback;
}

std::vector<Json> array_items(const Json& payload) {
    if (payload.is_array()) {
        return payload.get<std::vector<Json>>();
    }
    for (const char* key : {"results", "departures", "stopAreas", "serviceJourneyCalls", "calls", "trafficSituations"}) {
        if (payload.contains(key) && payload[key].is_array()) {
            return payload[key].get<std::vector<Json>>();
        }
    }
    return {};
}

Line parse_line(const Json& source) {
    const Json* line = object_at(source, {"line"});
    if (line == nullptr) {
        line = object_at(source, {"serviceJourney", "line"});
    }
    if (line == nullptr) {
        line = &source;
    }

    Line result;
    result.gid = string_at(*line, {"gid"}, string_at(*line, {"id"}));
    result.designation = string_at(*line, {"designation"}, string_at(*line, {"shortName"}));
    result.short_name = string_at(*line, {"shortName"}, result.designation);
    result.name = string_at(*line, {"name"}, result.designation);
    result.transport_mode = string_at(*line, {"transportMode"}, "bus");
    result.background_color = string_at(*line, {"backgroundColor"});
    result.foreground_color = string_at(*line, {"foregroundColor"});
    result.border_color = string_at(*line, {"borderColor"});
    return result;
}

StopPoint parse_stop_point(const Json& source) {
    const Json* stop_point = object_at(source, {"stopPoint"});
    if (stop_point == nullptr) {
        stop_point = object_at(source, {"plannedStopPoint"});
    }
    if (stop_point == nullptr) {
        stop_point = object_at(source, {"realtimeStopPoint"});
    }
    if (stop_point == nullptr) {
        stop_point = &source;
    }

    StopPoint result;
    result.gid = string_at(*stop_point, {"gid"}, string_at(*stop_point, {"id"}));
    result.stop_area_gid = string_at(*stop_point, {"stopArea", "gid"}, string_at(*stop_point, {"stopAreaGid"}));
    result.name = string_at(*stop_point, {"name"}, string_at(*stop_point, {"stopArea", "name"}));
    result.platform = string_at(*stop_point, {"platform"}, string_at(*stop_point, {"platformName"}));
    result.latitude = double_at(*stop_point, {"latitude"}, double_at(*stop_point, {"lat"}));
    result.longitude = double_at(*stop_point, {"longitude"}, double_at(*stop_point, {"lon"}));
    return result;
}

ServiceJourney parse_service_journey(const Json& source, const std::string& planned_departure_at) {
    const Json* service_journey = object_at(source, {"serviceJourney"});
    if (service_journey == nullptr) {
        service_journey = &source;
    }

    ServiceJourney result;
    result.gid = string_at(*service_journey, {"gid"}, string_at(*service_journey, {"id"}, string_at(source, {"serviceJourneyGid"})));
    result.line_gid = string_at(*service_journey, {"line", "gid"}, string_at(source, {"line", "gid"}));
    result.direction = string_at(*service_journey, {"direction"}, string_at(source, {"direction"}));

    const auto parsed = parse_rfc3339(planned_departure_at);
    result.traffic_day = parsed ? traffic_day_for(*parsed) : string_at(source, {"trafficDay"});
    return result;
}

CollectedDeparture parse_call(const Json& call) {
    CollectedDeparture result;
    result.line = parse_line(call);
    result.stop_point = parse_stop_point(call);
    result.realtime_stop_point = result.stop_point;
    result.planned_departure_at = string_at(call, {"plannedDepartureTime"}, string_at(call, {"plannedTime"}));
    result.estimated_departure_at = string_at(call, {"estimatedDepartureTime"}, string_at(call, {"estimatedTime"}, result.planned_departure_at));
    result.service_journey = parse_service_journey(call, result.planned_departure_at);
    if (result.service_journey.line_gid.empty()) {
        result.service_journey.line_gid = result.line.gid;
    }
    result.details_reference = string_at(call, {"detailsReference"});
    result.delay_seconds = delay_seconds_between(result.planned_departure_at, result.estimated_departure_at);
    result.is_cancelled = bool_at(call, {"isCancelled"}, bool_at(call, {"cancelled"}));
    result.is_part_cancelled = bool_at(call, {"isPartCancelled"}, bool_at(call, {"partCancelled"}));
    result.raw = call;
    return result;
}

std::vector<StopArea> parse_stop_areas(const Json& payload) {
    std::vector<StopArea> stops;
    for (const auto& item : array_items(payload)) {
        StopArea stop;
        stop.gid = string_at(item, {"gid"}, string_at(item, {"id"}));
        stop.name = string_at(item, {"name"});
        stop.latitude = double_at(item, {"latitude"}, double_at(item, {"lat"}));
        stop.longitude = double_at(item, {"longitude"}, double_at(item, {"lon"}));
        if (!stop.gid.empty() && !stop.name.empty()) {
            stops.push_back(std::move(stop));
        }
    }
    return stops;
}

void apply_backoff(FetchRun& run, const UpstreamBackoffError& error) {
    run.status_code = 429;
    run.error = error.what();
    run.backoff_until = format_rfc3339_local(error.backoff_until());
}

bool apply_persisted_backoff(const std::shared_ptr<Repository>& repository, FetchRun& run) {
    const std::string backoff_until = repository->active_backoff_until();
    const auto parsed = parse_rfc3339(backoff_until);
    if (parsed && std::chrono::system_clock::now() < *parsed) {
        run.status_code = 429;
        run.error = "Vasttrafik persisted backoff is active";
        run.backoff_until = backoff_until;
        return true;
    }
    return false;
}

} // namespace

TrafficCollector::TrafficCollector(Config config, std::shared_ptr<Repository> repository)
    : config_(std::move(config)),
      repository_(std::move(repository)),
      client_(config_) {}

void TrafficCollector::sync_stop_areas_once() {
    FetchRun run;
    run.endpoint = "/stop-areas";
    try {
        if (apply_persisted_backoff(repository_, run)) {
            std::cerr << "stop-area sync skipped until " << run.backoff_until << '\n';
        } else {
            const auto payload = client_.fetch_stop_areas();
            const auto stops = parse_stop_areas(payload);
            repository_->upsert_stop_areas(stops);
            run.status_code = 200;
            run.rows_fetched = static_cast<int>(stops.size());
        }
    } catch (const UpstreamBackoffError& error) {
        apply_backoff(run, error);
        std::cerr << "stop-area sync backed off until " << run.backoff_until << ": " << run.error << '\n';
    } catch (const std::exception& error) {
        run.status_code = 0;
        run.error = error.what();
        std::cerr << "stop-area sync failed: " << run.error << '\n';
    }
    repository_->record_fetch_run(run);
}

void TrafficCollector::collect_departures_once() {
    for (const auto& stop_area_gid : config_.monitored_stop_areas) {
        FetchRun run;
        run.endpoint = "/stop-areas/" + stop_area_gid + "/departures";
        try {
            if (apply_persisted_backoff(repository_, run)) {
                std::cerr << "departure collection skipped for " << stop_area_gid << " until " << run.backoff_until << '\n';
            } else {
                const auto payload = client_.fetch_departures(stop_area_gid);
                auto departures = parse_departures(payload);
                run.rows_fetched = static_cast<int>(departures.size());

                if (config_.fetch_departure_details) {
                    std::vector<CollectedDeparture> detailed;
                    int detail_calls_remaining = config_.max_detail_calls_per_cycle;
                    for (const auto& seed : departures) {
                        if (seed.details_reference.empty() || detail_calls_remaining <= 0) {
                            detailed.push_back(seed);
                            continue;
                        }
                        try {
                            --detail_calls_remaining;
                            const auto details = client_.fetch_departure_details(stop_area_gid, seed.details_reference);
                            auto parsed = parse_departure_details(details, seed);
                            detailed.insert(detailed.end(), parsed.begin(), parsed.end());
                        } catch (const UpstreamBackoffError&) {
                            throw;
                        } catch (const std::exception& error) {
                            std::cerr << "departure details failed for " << seed.details_reference << ": " << error.what() << '\n';
                            detailed.push_back(seed);
                        }
                    }
                    departures = std::move(detailed);
                }

                repository_->upsert_departures(departures);
                run.status_code = 200;
                run.rows_fetched = static_cast<int>(departures.size());
            }
        } catch (const UpstreamBackoffError& error) {
            apply_backoff(run, error);
            std::cerr << "departure collection backed off for " << stop_area_gid << " until " << run.backoff_until << ": " << run.error << '\n';
        } catch (const std::exception& error) {
            run.status_code = 0;
            run.error = error.what();
            std::cerr << "departure collection failed for " << stop_area_gid << ": " << run.error << '\n';
        }
        repository_->record_fetch_run(run);
    }
}

void TrafficCollector::collect_traffic_situations_once() {
    FetchRun run;
    run.endpoint = "/traffic-situations";
    try {
        if (apply_persisted_backoff(repository_, run)) {
            std::cerr << "traffic-situation collection skipped until " << run.backoff_until << '\n';
        } else {
            const auto payload = client_.fetch_traffic_situations();
            repository_->upsert_traffic_situations(payload);
            run.status_code = 200;
            run.rows_fetched = static_cast<int>(array_items(payload).size());
        }
    } catch (const UpstreamBackoffError& error) {
        apply_backoff(run, error);
        std::cerr << "traffic-situation collection backed off until " << run.backoff_until << ": " << run.error << '\n';
    } catch (const std::exception& error) {
        run.status_code = 0;
        run.error = error.what();
        std::cerr << "traffic-situation collection failed: " << run.error << '\n';
    }
    repository_->record_fetch_run(run);
}

void TrafficCollector::collect_all_once() {
    collect_departures_once();
    collect_traffic_situations_once();
}

std::vector<CollectedDeparture> TrafficCollector::parse_departures(const Json& payload) {
    std::vector<CollectedDeparture> result;
    for (const auto& item : array_items(payload)) {
        auto departure = parse_call(item);
        if (!departure.service_journey.gid.empty() && !departure.stop_point.gid.empty() && !departure.planned_departure_at.empty()) {
            result.push_back(std::move(departure));
        }
    }
    return result;
}

std::vector<CollectedDeparture> TrafficCollector::parse_departure_details(
    const Json& payload,
    const CollectedDeparture& seed
) {
    std::vector<CollectedDeparture> result;
    const auto calls = array_items(payload);
    if (calls.empty()) {
        result.push_back(seed);
        return result;
    }

    for (const auto& call : calls) {
        auto departure = parse_call(call);
        if (departure.line.gid.empty()) {
            departure.line = seed.line;
        }
        if (departure.service_journey.gid.empty()) {
            departure.service_journey = seed.service_journey;
        }
        if (departure.service_journey.line_gid.empty()) {
            departure.service_journey.line_gid = departure.line.gid;
        }
        if (departure.details_reference.empty()) {
            departure.details_reference = seed.details_reference;
        }
        if (!departure.service_journey.gid.empty() && !departure.stop_point.gid.empty() && !departure.planned_departure_at.empty()) {
            result.push_back(std::move(departure));
        }
    }
    return result;
}

} // namespace smasttrafik
