#include "exchange/clock.hpp"
#include "exchange/matching_engine.hpp"
#include "exchange/metrics.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using namespace exchange;

int main(int argc, char** argv) {
    const std::size_t total_orders = argc > 1 ? static_cast<std::size_t>(std::stoull(argv[1])) : 200000;

    MatchingEngine engine("BENCH", 5);
    OrderId next_id = 1;

    for (int i = 0; i < 1000; ++i) {
        engine.submit(NewOrder{next_id++, Side::Buy, OrderType::Limit, 9900 - i % 50, 100, now_nanos()});
        engine.submit(NewOrder{next_id++, Side::Sell, OrderType::Limit, 10100 + i % 50, 100, now_nanos()});
    }

    std::mt19937_64 rng(20260622);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> type_dist(0, 99);
    std::uniform_int_distribution<int> px_dist(-150, 150);
    std::uniform_int_distribution<int> qty_dist(1, 100);

    std::vector<std::uint64_t> latency_samples;
    latency_samples.reserve(total_orders);

    const auto bench_start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < total_orders; ++i) {
        const Side side = side_dist(rng) == 0 ? Side::Buy : Side::Sell;
        const bool is_market = type_dist(rng) < 15;
        const Price price = 10000 + px_dist(rng);
        const Quantity qty = static_cast<Quantity>(qty_dist(rng));
        const auto order = NewOrder{
            next_id++,
            side,
            is_market ? OrderType::Market : OrderType::Limit,
            is_market ? 0 : price,
            qty,
            now_nanos()};

        const auto start = std::chrono::steady_clock::now();
        engine.submit(order);
        const auto end = std::chrono::steady_clock::now();
        latency_samples.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()));
    }
    const auto bench_end = std::chrono::steady_clock::now();

    const double seconds =
        std::chrono::duration<double>(bench_end - bench_start).count();
    const double throughput = static_cast<double>(total_orders) / seconds;
    const auto stats = summarize_latency(std::move(latency_samples));

    std::cout << "orders=" << total_orders << "\n"
              << "elapsed_sec=" << seconds << "\n"
              << "throughput_ops_sec=" << static_cast<std::uint64_t>(throughput) << "\n"
              << "latency_min_ns=" << stats.min_ns << "\n"
              << "latency_avg_ns=" << static_cast<std::uint64_t>(stats.avg_ns) << "\n"
              << "latency_p50_ns=" << stats.p50_ns << "\n"
              << "latency_p99_ns=" << stats.p99_ns << "\n"
              << "latency_max_ns=" << stats.max_ns << "\n";

    return throughput >= 50000.0 ? 0 : 2;
}
