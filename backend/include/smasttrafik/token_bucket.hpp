#pragma once

#include <chrono>
#include <mutex>

namespace smasttrafik {

class TokenBucket {
public:
    explicit TokenBucket(int tokens_per_minute, int burst_capacity = 1);

    void wait_for_token();

private:
    int refill_per_minute_;
    int capacity_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};

} // namespace smasttrafik
