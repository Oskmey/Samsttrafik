#include <crow.h>

#include <cstdint>
#include <iostream>
#include <memory>

#include "smasttrafik/api_routes.hpp"
#include "smasttrafik/config.hpp"
#include "smasttrafik/repository.hpp"
#include "smasttrafik/stats_service.hpp"

int main() {
    const auto config = smasttrafik::load_config();
    auto repository = smasttrafik::make_repository(config);
    repository->migrate();

    auto stats = std::make_shared<smasttrafik::StatsService>(repository);

    crow::SimpleApp app;
    smasttrafik::register_api_routes(app, config, stats);

    std::cout << "Smasttrafik API listening on http://" << config.bind_host << ':'
              << config.port << " using " << repository->mode() << " data\n";

    app.bindaddr(config.bind_host)
        .port(static_cast<std::uint16_t>(config.port))
        .multithreaded()
        .run();
}
