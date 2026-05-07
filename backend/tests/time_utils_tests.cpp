#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "smasttrafik/time_utils.hpp"
#include "smasttrafik/vasttrafik_client.hpp"

namespace {

void test_rfc3339_offsets() {
    const auto sweden = smasttrafik::parse_rfc3339("2026-05-06T12:30:00+02:00");
    const auto utc = smasttrafik::parse_rfc3339("2026-05-06T10:30:00Z");
    assert(sweden.has_value());
    assert(utc.has_value());
    assert(*sweden == *utc);
}

void test_delay_calculation() {
    assert(smasttrafik::delay_seconds_between(
        "2026-05-06T12:00:00+02:00",
        "2026-05-06T12:07:30+02:00"
    ) == 450);

    assert(smasttrafik::delay_seconds_between(
        "2026-05-06T12:00:00+02:00",
        "2026-05-06T11:59:00+02:00"
    ) == -60);

    assert(smasttrafik::delay_seconds_between("bad", "2026-05-06T12:00:00+02:00") == 0);
    assert(smasttrafik::delay_seconds_between("2026-05-06T12:00:00+02:00", "") == 0);
}

void test_traffic_day_boundary() {
    const auto early = smasttrafik::parse_rfc3339("2026-05-06T03:30:00+02:00");
    const auto later = smasttrafik::parse_rfc3339("2026-05-06T04:30:00+02:00");
    assert(early.has_value());
    assert(later.has_value());
    assert(smasttrafik::traffic_day_for(*early) == "2026-05-05");
    assert(smasttrafik::traffic_day_for(*later) == "2026-05-06");
}

void test_custom_period() {
    const auto range = smasttrafik::resolve_period(
        "custom",
        "2026-05-01T00:00:00+02:00",
        "2026-05-06T00:00:00+02:00"
    );
    assert(range.from == "2026-05-01T00:00:00+02:00");
    assert(range.to == "2026-05-06T00:00:00+02:00");
}

void test_retry_after_seconds() {
    const auto now = std::chrono::system_clock::from_time_t(1000);
    const auto parsed = smasttrafik::parse_retry_after_header("45", now);
    assert(parsed.has_value());
    assert(std::chrono::duration_cast<std::chrono::seconds>(*parsed - now).count() == 45);
}

void test_retry_after_http_date() {
    const auto now = std::chrono::system_clock::from_time_t(0);
    const auto parsed = smasttrafik::parse_retry_after_header("Wed, 06 May 2026 10:30:00 GMT", now);
    const auto expected = smasttrafik::parse_rfc3339("2026-05-06T10:30:00Z");
    assert(parsed.has_value());
    assert(expected.has_value());
    assert(*parsed == *expected);
}

} // namespace

int main() {
    setenv("TZ", "Europe/Stockholm", 1);
    tzset();

    test_rfc3339_offsets();
    test_delay_calculation();
    test_traffic_day_boundary();
    test_custom_period();
    test_retry_after_seconds();
    test_retry_after_http_date();
    std::cout << "time_utils tests passed\n";
}
