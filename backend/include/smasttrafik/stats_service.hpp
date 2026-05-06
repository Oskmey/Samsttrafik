#pragma once

#include <memory>

#include "smasttrafik/models.hpp"
#include "smasttrafik/repository.hpp"

namespace smasttrafik {

class StatsService {
public:
    explicit StatsService(std::shared_ptr<Repository> repository);

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
    std::shared_ptr<Repository> repository_;
};

} // namespace smasttrafik
