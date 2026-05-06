#pragma once

#include <memory>

#include "smasttrafik/config.hpp"
#include "smasttrafik/repository.hpp"
#include "smasttrafik/vasttrafik_client.hpp"

namespace smasttrafik {

class TrafficCollector {
public:
    TrafficCollector(Config config, std::shared_ptr<Repository> repository);

    void sync_stop_areas_once();
    void collect_departures_once();
    void collect_traffic_situations_once();
    void collect_all_once();

private:
    std::vector<CollectedDeparture> parse_departures(const Json& payload);
    std::vector<CollectedDeparture> parse_departure_details(
        const Json& payload,
        const CollectedDeparture& seed
    );

    Config config_;
    std::shared_ptr<Repository> repository_;
    VasttrafikClient client_;
};

} // namespace smasttrafik
