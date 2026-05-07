#pragma once

#include <memory>
#include <string>
#include <vector>

#include "smasttrafik/config.hpp"
#include "smasttrafik/models.hpp"

namespace smasttrafik {

class Repository {
public:
    virtual ~Repository() = default;

    virtual bool is_persistent() const = 0;
    virtual std::string mode() const = 0;
    virtual void migrate() = 0;

    virtual Coverage coverage() = 0;
    virtual std::vector<Line> find_lines(const std::string& query, const std::string& transport_mode) = 0;
    virtual std::vector<StopArea> find_stops(const std::string& query, const std::string& line_id) = 0;
    virtual RankingPage rankings(const RankingQuery& query) = 0;
    virtual std::vector<CompareSeries> compare(const CompareQuery& query) = 0;
    virtual std::vector<Incident> incidents(
        const std::string& line_id,
        const std::string& stop_id,
        const std::string& from,
        const std::string& to
    ) = 0;

    virtual void record_fetch_run(const FetchRun& run) = 0;
    virtual std::string active_backoff_until() = 0;
    virtual void upsert_stop_areas(const std::vector<StopArea>& stops) = 0;
    virtual void upsert_departures(const std::vector<CollectedDeparture>& departures) = 0;
    virtual void upsert_traffic_situations(const Json& situations) = 0;
};

std::shared_ptr<Repository> make_demo_repository(const Config& config);
std::shared_ptr<Repository> make_repository(const Config& config);

} // namespace smasttrafik
