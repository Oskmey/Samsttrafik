#include "smasttrafik/repository.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

namespace {

bool contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

class DemoRepository final : public Repository {
public:
    explicit DemoRepository(Config config) : config_(std::move(config)) {
        lines_ = {
            {"9011014500100000", "X1", "X1", "Partille centrum - Torslanda", "bus", "#FFFF50", "#D400A2", "#D400A2"},
            {"9011014500200000", "X2", "X2", "Frölunda torg - Mölnlycke", "bus", "#FFFF50", "#D400A2", "#D400A2"},
            {"9011014501600000", "16", "16", "Eketrägatan - Högsbohöjd", "bus", "#0072bc", "#ffffff", "#00558a"},
            {"9011014506000000", "60", "60", "Masthugget - Redbergsplatsen", "bus", "#f59e0b", "#111827", "#b45309"},
            {"9011014502500000", "25", "25", "Länsmansgården - Balltorp", "bus", "#10b981", "#052e2b", "#047857"},
            {"9011014501900000", "19", "19", "Backa - Fredriksdal", "bus", "#ef4444", "#ffffff", "#b91c1c"},
        };
        stops_ = {
            {"9021014001760000", "Brunnsparken", 57.7069, 11.9675},
            {"9021014003980000", "Korsvägen", 57.6972, 11.9865},
            {"9021014000020000", "Göteborg Centralstation", 57.7093, 11.9737},
            {"9021014004870000", "Järntorget", 57.6999, 11.9520},
        };
        incidents_ = {
            {"DEMO-1001", "normal", "Omlagd körväg vid Korsvägen", "Påverkar flera busshållplatser i riktning mot centrum.", current_rfc3339_local(), ""},
            {"DEMO-1002", "slight", "Tät trafik genom centrum", "Risk för några minuters försening på innerstadslinjer.", current_rfc3339_local(), ""},
        };
    }

    bool is_persistent() const override { return false; }
    std::string mode() const override { return "demo"; }
    void migrate() override {}

    Coverage coverage() override {
        std::lock_guard<std::mutex> lock(mutex_);
        Coverage coverage;
        coverage.monitored_stop_areas = static_cast<int>(config_.monitored_stop_areas.size());
        coverage.lines = static_cast<int>(lines_.size());
        coverage.stops = static_cast<int>(stops_.size());
        coverage.observed_departures = 142;
        coverage.earliest_observation = resolve_period("week", "", "").from;
        coverage.latest_observation = current_rfc3339_local();
        coverage.mode = mode();
        return coverage;
    }

    std::vector<Line> find_lines(const std::string& query, const std::string& transport_mode) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Line> result;
        for (const auto& line : lines_) {
            if (!transport_mode.empty() && line.transport_mode != transport_mode) {
                continue;
            }
            if (contains_ci(line.designation + " " + line.name, query)) {
                result.push_back(line);
            }
        }
        return result;
    }

    std::vector<StopArea> find_stops(const std::string& query, const std::string&) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<StopArea> result;
        for (const auto& stop : stops_) {
            if (contains_ci(stop.name, query)) {
                result.push_back(stop);
            }
        }
        return result;
    }

    RankingPage rankings(const RankingQuery& query) override {
        std::lock_guard<std::mutex> lock(mutex_);
        RankingPage page;
        const auto range = resolve_period(query.period, query.from, query.to);
        page.from = range.from;
        page.to = range.to;
        page.page = query.page;
        page.page_size = query.page_size;
        if (query.entity == "stop") {
            page.rows = {
                {"9021014001760000", "Brunnsparken", "Brunnsparken", "", "Brunnsparken", 241.0, 6.5, 37, 104},
                {"9021014003980000", "Korsvägen", "Korsvägen", "", "Korsvägen", 188.0, 5.7, 33, 92},
                {"9021014000020000", "Göteborg Centralstation", "Göteborg Centralstation", "", "Göteborg Centralstation", 156.5, 4.9, 32, 86},
                {"9021014004870000", "Järntorget", "Järntorget", "", "Järntorget", 99.0, 3.4, 29, 81},
            };
        } else if (query.entity == "route") {
            page.rows = {
                {"9011014500100000:torslanda", "X1 mot Torslanda", "X1 mot Torslanda", "X1", "", 181.5, 9.1, 20, 42},
                {"9011014506000000:centrum", "60 mot Redbergsplatsen", "60 mot Redbergsplatsen", "60", "", 153.0, 8.1, 19, 46},
                {"9011014501600000:hogsbo", "16 mot Högsbohöjd", "16 mot Högsbohöjd", "16", "", 122.5, 6.8, 18, 44},
                {"9011014506000000:masthugget", "60 mot Masthugget", "60 mot Masthugget", "60", "", 125.0, 6.6, 19, 50},
                {"9011014501900000:fredriksdal", "19 mot Fredriksdal", "19 mot Fredriksdal", "19", "", 86.0, 4.8, 18, 47},
            };
        } else {
            page.rows = {
                {"9011014500100000", "X1", "X1 Partille centrum - Torslanda", "X1", "", 304.5, 8.7, 35, 83},
                {"9011014500200000", "X2", "X2 Frölunda torg - Mölnlycke", "X2", "", 246.0, 7.9, 31, 78},
                {"9011014506000000", "60", "60 Masthugget - Redbergsplatsen", "60", "", 278.0, 7.3, 38, 96},
                {"9011014501600000", "16", "16 Eketrägatan - Högsbohöjd", "16", "", 216.5, 6.2, 35, 88},
                {"9011014501900000", "19", "19 Backa - Fredriksdal", "19", "", 164.0, 4.8, 34, 91},
                {"9011014502500000", "25", "25 Länsmansgården - Balltorp", "25", "", 119.5, 3.9, 31, 77},
            };
        }

        const auto sort = query.sort;
        std::sort(page.rows.begin(), page.rows.end(), [&](const RankingRow& a, const RankingRow& b) {
            if (sort == "avg_delay_minutes") {
                return a.avg_delay_minutes > b.avg_delay_minutes;
            }
            if (sort == "delayed_departures") {
                return a.delayed_departures > b.delayed_departures;
            }
            if (sort == "route") {
                return a.route < b.route;
            }
            return a.total_delay_minutes > b.total_delay_minutes;
        });
        if (query.direction == "asc") {
            std::reverse(page.rows.begin(), page.rows.end());
        }
        page.total = static_cast<int>(page.rows.size());
        const int offset = std::max(0, (query.page - 1) * query.page_size);
        if (offset >= static_cast<int>(page.rows.size())) {
            page.rows.clear();
        } else {
            const int limit = std::min(static_cast<int>(page.rows.size()), offset + query.page_size);
            page.rows = std::vector<RankingRow>(page.rows.begin() + offset, page.rows.begin() + limit);
        }
        return page;
    }

    std::vector<CompareSeries> compare(const CompareQuery& query) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CompareSeries> series;
        const auto range = (!query.from.empty() && !query.to.empty())
            ? resolve_period("custom", query.from, query.to)
            : resolve_period("week", "", "");
        const auto start = parse_rfc3339(range.from).value_or(std::chrono::system_clock::now());
        const std::vector<std::string> ids = query.ids.empty()
            ? std::vector<std::string>{"9011014506000000", "9011014501600000"}
            : query.ids;

        for (std::size_t i = 0; i < ids.size(); ++i) {
            CompareSeries item;
            item.id = ids[i];
            const auto line = std::find_if(lines_.begin(), lines_.end(), [&](const Line& candidate) {
                return candidate.gid == ids[i];
            });
            item.label = line == lines_.end() ? ids[i] : line->designation;
            for (int day = 0; day < 7; ++day) {
                TrendPoint point;
                point.bucket_start = format_rfc3339_local(start + std::chrono::hours(24 * day));
                point.value = 3.0 + static_cast<double>((day + static_cast<int>(i)) % 5) + (i * 0.6);
                point.delayed_departures = 7 + ((day + static_cast<int>(i)) % 4);
                point.observed_departures = 22 + day;
                item.points.push_back(point);
            }
            series.push_back(item);
        }
        return series;
    }

    std::vector<Incident> incidents(const std::string&, const std::string&, const std::string&, const std::string&) override {
        std::lock_guard<std::mutex> lock(mutex_);
        return incidents_;
    }

    void record_fetch_run(const FetchRun&) override {}

    std::string active_backoff_until() override { return {}; }

    void upsert_stop_areas(const std::vector<StopArea>& stops) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& stop : stops) {
            const auto found = std::find_if(stops_.begin(), stops_.end(), [&](const StopArea& existing) {
                return existing.gid == stop.gid;
            });
            if (found == stops_.end()) {
                stops_.push_back(stop);
            } else {
                *found = stop;
            }
        }
    }

    void upsert_departures(const std::vector<CollectedDeparture>&) override {}
    void upsert_traffic_situations(const Json&) override {}

private:
    Config config_;
    std::vector<Line> lines_;
    std::vector<StopArea> stops_;
    std::vector<Incident> incidents_;
    std::mutex mutex_;
};

} // namespace

std::shared_ptr<Repository> make_demo_repository(const Config& config) {
    return std::make_shared<DemoRepository>(config);
}

#ifndef SMASTTRAFIK_ENABLE_POSTGRES
std::shared_ptr<Repository> make_repository(const Config& config) {
    return make_demo_repository(config);
}
#endif

} // namespace smasttrafik
