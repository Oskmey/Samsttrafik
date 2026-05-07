#pragma once

#include <memory>

#include "smasttrafik/models.hpp"
#include "smasttrafik/repository.hpp"
#include "smasttrafik/time_utils.hpp"

namespace smasttrafik {

class StatsService {
public:
    explicit StatsService(std::shared_ptr<Repository> repository, int max_custom_range_days = 366);

    Coverage coverage();
    std::vector<Line> lines(const std::string& query, const std::string& transport_mode);
    std::vector<StopArea> stops(const std::string& query, const std::string& line_id);
    RankingPage rankings(RankingQuery query);
    std::vector<CompareSeries> compare(CompareQuery query);
    std::vector<Incident> incidents(
        const std::string& line_id,
        const std::string& stop_id,
        const std::string& from,
        const std::string& to
    );

private:
    DateRange clamp_range(DateRange range) const;

    std::shared_ptr<Repository> repository_;
    int max_custom_range_days_;
};

} // namespace smasttrafik
