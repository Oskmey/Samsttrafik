#pragma once

#include <memory>

#include <crow.h>

#include "smasttrafik/config.hpp"
#include "smasttrafik/stats_service.hpp"

namespace smasttrafik {

void register_api_routes(crow::SimpleApp& app, const Config& config, std::shared_ptr<StatsService> stats);

} // namespace smasttrafik
