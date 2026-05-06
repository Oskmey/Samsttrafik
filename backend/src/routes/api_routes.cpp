#include "smasttrafik/api_routes.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "smasttrafik/repository.hpp"
#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

namespace {

std::string param(const crow::request& req, const char* name, const std::string& fallback = "") {
    const char* value = req.url_params.get(name);
    return value == nullptr ? fallback : std::string(value);
}

int param_int(const crow::request& req, const char* name, int fallback) {
    const char* value = req.url_params.get(name);
    if (value == nullptr) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

crow::response json_response(const Json& payload, int status = 200) {
    crow::response response(status);
    response.set_header("Content-Type", "application/json; charset=utf-8");
    response.set_header("Cache-Control", "no-store");
    response.body = payload.dump();
    return response;
}

Json line_json(const Line& line) {
    return {
        {"gid", line.gid},
        {"designation", line.designation},
        {"shortName", line.short_name},
        {"name", line.name},
        {"transportMode", line.transport_mode},
        {"backgroundColor", line.background_color},
        {"foregroundColor", line.foreground_color},
        {"borderColor", line.border_color},
    };
}

Json stop_json(const StopArea& stop) {
    return {
        {"gid", stop.gid},
        {"name", stop.name},
        {"lat", stop.latitude},
        {"lon", stop.longitude},
    };
}

Json ranking_json(const RankingRow& row) {
    return {
        {"id", row.id},
        {"label", row.label},
        {"route", row.route},
        {"lineDesignation", row.line_designation},
        {"stopName", row.stop_name},
        {"totalDelayMinutes", row.total_delay_minutes},
        {"avgDelayMinutes", row.avg_delay_minutes},
        {"delayedDepartures", row.delayed_departures},
        {"observedDepartures", row.observed_departures},
    };
}

Json incident_json(const Incident& incident) {
    return {
        {"situationNumber", incident.situation_number},
        {"severity", incident.severity},
        {"title", incident.title},
        {"description", incident.description},
        {"startTime", incident.start_time},
        {"endTime", incident.end_time},
    };
}

Json compare_json(const CompareSeries& series) {
    Json points = Json::array();
    for (const auto& point : series.points) {
        points.push_back({
            {"bucketStart", point.bucket_start},
            {"value", point.value},
            {"delayedDepartures", point.delayed_departures},
            {"observedDepartures", point.observed_departures},
        });
    }
    return {
        {"id", series.id},
        {"label", series.label},
        {"points", points},
    };
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string content_type_for(const std::string& path) {
    if (path.ends_with(".js")) {
        return "application/javascript; charset=utf-8";
    }
    if (path.ends_with(".css")) {
        return "text/css; charset=utf-8";
    }
    if (path.ends_with(".svg")) {
        return "image/svg+xml";
    }
    if (path.ends_with(".png")) {
        return "image/png";
    }
    if (path.ends_with(".woff2")) {
        return "font/woff2";
    }
    return "application/octet-stream";
}

} // namespace

void register_api_routes(crow::SimpleApp& app, const Config& config, std::shared_ptr<StatsService> stats) {
    CROW_ROUTE(app, "/api/health")([stats]() {
        const auto coverage = stats->coverage();
        return json_response({
            {"ok", true},
            {"service", "smasttrafik-api"},
            {"mode", coverage.mode},
            {"time", current_rfc3339_local()},
        });
    });

    CROW_ROUTE(app, "/api/meta/coverage")([stats]() {
        const auto coverage = stats->coverage();
        return json_response({
            {"monitoredStopAreas", coverage.monitored_stop_areas},
            {"lines", coverage.lines},
            {"stops", coverage.stops},
            {"observedDepartures", coverage.observed_departures},
            {"earliestObservation", coverage.earliest_observation},
            {"latestObservation", coverage.latest_observation},
            {"mode", coverage.mode},
        });
    });

    CROW_ROUTE(app, "/api/lines")([stats](const crow::request& req) {
        Json rows = Json::array();
        for (const auto& line : stats->lines(param(req, "query"), param(req, "transportMode", "bus"))) {
            rows.push_back(line_json(line));
        }
        return json_response({{"results", rows}});
    });

    CROW_ROUTE(app, "/api/stops")([stats](const crow::request& req) {
        Json rows = Json::array();
        for (const auto& stop : stats->stops(param(req, "query"), param(req, "lineId"))) {
            rows.push_back(stop_json(stop));
        }
        return json_response({{"results", rows}});
    });

    CROW_ROUTE(app, "/api/delays/rankings")([stats](const crow::request& req) {
        RankingQuery query;
        query.entity = param(req, "entity", "line");
        query.period = param(req, "period", "week");
        query.from = param(req, "from");
        query.to = param(req, "to");
        query.sort = param(req, "sort", "total_delay_minutes");
        query.direction = param(req, "direction", "desc");
        query.page = param_int(req, "page", 1);
        query.page_size = param_int(req, "pageSize", 50);
        query.line_ids = split_csv(param(req, "lineIds"));
        query.stop_ids = split_csv(param(req, "stopIds"));

        const auto page = stats->rankings(query);
        Json rows = Json::array();
        for (const auto& row : page.rows) {
            rows.push_back(ranking_json(row));
        }
        return json_response({
            {"results", rows},
            {"page", page.page},
            {"pageSize", page.page_size},
            {"total", page.total},
            {"from", page.from},
            {"to", page.to},
        });
    });

    CROW_ROUTE(app, "/api/delays/compare")([stats](const crow::request& req) {
        CompareQuery query;
        query.entity = param(req, "entity", "line");
        query.ids = split_csv(param(req, "ids"));
        query.from = param(req, "from");
        query.to = param(req, "to");
        query.bucket = param(req, "bucket", "day");
        query.metric = param(req, "metric", "avg_delay_minutes");

        Json series = Json::array();
        for (const auto& item : stats->compare(query)) {
            series.push_back(compare_json(item));
        }
        return json_response({{"series", series}});
    });

    CROW_ROUTE(app, "/api/incidents")([stats](const crow::request& req) {
        Json rows = Json::array();
        for (const auto& incident : stats->incidents(
            param(req, "lineId"),
            param(req, "stopId"),
            param(req, "from"),
            param(req, "to")
        )) {
            rows.push_back(incident_json(incident));
        }
        return json_response({{"results", rows}});
    });

    CROW_ROUTE(app, "/assets/<path>")([](const std::string& path) {
        if (path.find("..") != std::string::npos) {
            return crow::response(400);
        }
        const std::string body = read_file("frontend/dist/assets/" + path);
        if (body.empty()) {
            return crow::response(404);
        }
        crow::response response(body);
        response.set_header("Content-Type", content_type_for(path));
        response.set_header("Cache-Control", "public, max-age=31536000, immutable");
        return response;
    });

    CROW_ROUTE(app, "/")([config]() {
        const std::string html = read_file("frontend/dist/index.html");
        if (!html.empty()) {
            crow::response response(html);
            response.set_header("Content-Type", "text/html; charset=utf-8");
            return response;
        }

        crow::response response(
            "<!doctype html><html><head><title>Smasttrafik API</title></head>"
            "<body><h1>Smasttrafik API</h1><p>Frontend build not found. Try /api/health.</p></body></html>"
        );
        response.set_header("Content-Type", "text/html; charset=utf-8");
        response.set_header("X-Smasttrafik-Port", std::to_string(config.port));
        return response;
    });
}

} // namespace smasttrafik
