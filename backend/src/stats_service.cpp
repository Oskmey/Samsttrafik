#include "smasttrafik/stats_service.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

namespace {

int clamp_page_size(int value) {
    return std::clamp(value, 1, 200);
}

bool is_allowed_sort(const std::string& sort) {
    return sort == "total_delay_minutes"
        || sort == "avg_delay_minutes"
        || sort == "delayed_departures"
        || sort == "route";
}

} // namespace

StatsService::StatsService(std::shared_ptr<Repository> repository)
    : repository_(std::move(repository)) {}

Coverage StatsService::coverage() {
    return repository_->coverage();
}

std::vector<Line> StatsService::lines(const std::string& query, const std::string& transport_mode) {
    return repository_->find_lines(query, transport_mode);
}

std::vector<StopArea> StatsService::stops(const std::string& query, const std::string& line_id) {
    return repository_->find_stops(query, line_id);
}

RankingPage StatsService::rankings(RankingQuery query) {
    if (query.entity != "line" && query.entity != "route" && query.entity != "stop") {
        query.entity = "line";
    }
    if (!is_allowed_sort(query.sort)) {
        query.sort = "total_delay_minutes";
    }
    if (query.direction != "asc") {
        query.direction = "desc";
    }
    query.page = std::max(1, query.page);
    query.page_size = clamp_page_size(query.page_size);
    const auto range = resolve_period(query.period, query.from, query.to);
    query.from = range.from;
    query.to = range.to;
    return repository_->rankings(query);
}

std::vector<CompareSeries> StatsService::compare(CompareQuery query) {
    if (query.entity != "line" && query.entity != "stop") {
        query.entity = "line";
    }
    if (query.bucket != "hour" && query.bucket != "day" && query.bucket != "week") {
        query.bucket = "day";
    }
    if (query.metric != "avg_delay_minutes"
        && query.metric != "total_delay_minutes"
        && query.metric != "delayed_departures") {
        query.metric = "avg_delay_minutes";
    }
    const auto range = resolve_period("custom", query.from, query.to);
    query.from = range.from;
    query.to = range.to;
    return repository_->compare(query);
}

std::vector<Incident> StatsService::incidents(
    const std::string& line_id,
    const std::string& stop_id,
    const std::string& from,
    const std::string& to
) {
    const auto range = resolve_period("custom", from, to);
    return repository_->incidents(line_id, stop_id, range.from, range.to);
}

} // namespace smasttrafik
