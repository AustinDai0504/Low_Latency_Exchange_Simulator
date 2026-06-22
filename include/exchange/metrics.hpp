#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace exchange {

struct LatencyStats {
    std::uint64_t min_ns{};
    std::uint64_t p50_ns{};
    std::uint64_t p99_ns{};
    std::uint64_t max_ns{};
    double avg_ns{};
};

inline LatencyStats summarize_latency(std::vector<std::uint64_t> samples) {
    LatencyStats stats{};
    if (samples.empty()) {
        return stats;
    }
    std::sort(samples.begin(), samples.end());
    const auto pick = [&](double q) {
        const auto idx = static_cast<std::size_t>((samples.size() - 1) * q);
        return samples[idx];
    };
    stats.min_ns = samples.front();
    stats.p50_ns = pick(0.50);
    stats.p99_ns = pick(0.99);
    stats.max_ns = samples.back();
    const auto total = std::accumulate(samples.begin(), samples.end(), std::uint64_t{0});
    stats.avg_ns = static_cast<double>(total) / static_cast<double>(samples.size());
    return stats;
}

} // namespace exchange
