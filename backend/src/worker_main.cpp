#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "smasttrafik/config.hpp"
#include "smasttrafik/repository.hpp"
#include "smasttrafik/traffic_collector.hpp"

namespace {

volatile std::sig_atomic_t keep_running = 1;

void handle_signal(int) {
    keep_running = 0;
}

} // namespace

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const auto config = smasttrafik::load_config();
    auto repository = smasttrafik::make_repository(config);
    repository->migrate();

    if (!repository->is_persistent()) {
        std::cerr << "smasttrafik-worker is running with the demo repository; live data will not be persisted.\n";
    }

    smasttrafik::TrafficCollector collector(config, repository);
    collector.sync_stop_areas_once();

    while (keep_running != 0) {
        collector.collect_all_once();
        for (int elapsed = 0; keep_running != 0 && elapsed < config.poll_interval_seconds; ++elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    std::cout << "smasttrafik-worker stopped\n";
    return 0;
}
