#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace smasttrafik {

using Json = nlohmann::json;

struct Config;

struct Line {
    std::string gid;
    std::string designation;
    std::string short_name;
    std::string name;
    std::string transport_mode;
    std::string background_color;
    std::string foreground_color;
    std::string border_color;
};

struct StopArea {
    std::string gid;
    std::string name;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct StopPoint {
    std::string gid;
    std::string stop_area_gid;
    std::string name;
    std::string platform;
    double latitude = 0.0;
    double longitude = 0.0;
};

struct ServiceJourney {
    std::string gid;
    std::string traffic_day;
    std::string line_gid;
    std::string direction;
};

struct CollectedDeparture {
    Line line;
    StopPoint stop_point;
    StopPoint realtime_stop_point;
    ServiceJourney service_journey;
    std::string details_reference;
    std::string planned_departure_at;
    std::string estimated_departure_at;
    int delay_seconds = 0;
    bool is_cancelled = false;
    bool is_part_cancelled = false;
    Json raw = Json::object();
};

struct FetchRun {
    std::string endpoint;
    int status_code = 0;
    long latency_ms = 0;
    int rows_fetched = 0;
    std::string error;
};

struct RankingQuery {
    std::string entity = "line";
    std::string period = "week";
    std::string from;
    std::string to;
    std::string sort = "total_delay_minutes";
    std::string direction = "desc";
    int page = 1;
    int page_size = 50;
    std::vector<std::string> line_ids;
    std::vector<std::string> stop_ids;
};

struct RankingRow {
    std::string id;
    std::string label;
    std::string route;
    std::string line_designation;
    std::string stop_name;
    double total_delay_minutes = 0.0;
    double avg_delay_minutes = 0.0;
    int delayed_departures = 0;
    int observed_departures = 0;
};

struct RankingPage {
    std::vector<RankingRow> rows;
    int page = 1;
    int page_size = 50;
    int total = 0;
    std::string from;
    std::string to;
};

struct CompareQuery {
    std::string entity = "line";
    std::vector<std::string> ids;
    std::string from;
    std::string to;
    std::string bucket = "day";
    std::string metric = "avg_delay_minutes";
};

struct TrendPoint {
    std::string bucket_start;
    double value = 0.0;
    int delayed_departures = 0;
    int observed_departures = 0;
};

struct CompareSeries {
    std::string id;
    std::string label;
    std::vector<TrendPoint> points;
};

struct Incident {
    std::string situation_number;
    std::string severity;
    std::string title;
    std::string description;
    std::string start_time;
    std::string end_time;
};

struct Coverage {
    int monitored_stop_areas = 0;
    int lines = 0;
    int stops = 0;
    int observed_departures = 0;
    std::string earliest_observation;
    std::string latest_observation;
    std::string mode;
};

} // namespace smasttrafik
