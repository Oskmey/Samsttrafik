#include "smasttrafik/token_bucket.hpp"

#include <algorithm>
#include <thread>

namespace smasttrafik {

TokenBucket::TokenBucket(int tokens_per_minute)
    : capacity_(std::max(1, tokens_per_minute)),
      tokens_(static_cast<double>(std::max(1, tokens_per_minute))),
      last_refill_(std::chrono::steady_clock::now()) {}

void TokenBucket::wait_for_token() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto now = std::chrono::steady_clock::now();
            const double elapsed_seconds = std::chrono::duration<double>(now - last_refill_).count();
            const double refill_per_second = static_cast<double>(capacity_) / 60.0;
            tokens_ = std::min(static_cast<double>(capacity_), tokens_ + (elapsed_seconds * refill_per_second));
            last_refill_ = now;

            if (tokens_ >= 1.0) {
                tokens_ -= 1.0;
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

} // namespace smasttrafik
