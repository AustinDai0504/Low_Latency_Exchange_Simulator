#include "exchange/clock.hpp"
#include "exchange/json.hpp"
#include "exchange/market_data_publisher.hpp"
#include "exchange/matching_engine.hpp"
#include "exchange/websocket_server.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>

using namespace exchange;

namespace {

NewOrder make_limit(OrderId id, Side side, Price price, Quantity qty) {
    return NewOrder{id, side, OrderType::Limit, price, qty, now_nanos()};
}

NewOrder make_market(OrderId id, Side side, Quantity qty) {
    return NewOrder{id, side, OrderType::Market, 0, qty, now_nanos()};
}

void print_help() {
    std::cout << "Commands:\n"
              << "  buy <price> <qty>\n"
              << "  sell <price> <qty>\n"
              << "  mbuy <qty>\n"
              << "  msell <qty>\n"
              << "  cancel <order_id>\n"
              << "  snapshot\n"
              << "  quit\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::uint16_t port = argc > 1 ? static_cast<std::uint16_t>(std::stoi(argv[1])) : 9002;

    MatchingEngine engine("SIM-USD", 10);
    MarketDataPublisher publisher;
    WebSocketServer server("0.0.0.0", port);

    publisher.subscribe([&](const MatchResult& result) {
        server.broadcast_text(market_update_to_json(result));
    });

    server.start();
    std::cout << "Exchange simulator started: ws://127.0.0.1:" << port << "\n";
    print_help();

    std::atomic<bool> running{true};
    std::atomic<OrderId> next_id{1};

    // 预热订单簿，让 WebSocket 客户端连接后能马上看到盘口变化。
    for (int i = 0; i < 5; ++i) {
        publisher.publish(engine.submit(make_limit(next_id++, Side::Buy, 9990 - i * 5, 100 + i * 10)));
        publisher.publish(engine.submit(make_limit(next_id++, Side::Sell, 10010 + i * 5, 100 + i * 10)));
    }

    std::thread simulator([&] {
        std::mt19937_64 rng(42);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<int> px_dist(-30, 30);
        std::uniform_int_distribution<int> qty_dist(1, 20);

        while (running) {
            const Side side = side_dist(rng) == 0 ? Side::Buy : Side::Sell;
            const Price px = 10000 + px_dist(rng);
            const Quantity qty = static_cast<Quantity>(qty_dist(rng));
            publisher.publish(engine.submit(make_limit(next_id++, side, px, qty)));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd == "quit" || cmd == "exit") {
            break;
        }
        if (cmd == "help") {
            print_help();
            continue;
        }
        if (cmd == "snapshot") {
            std::cout << snapshot_to_json(engine.snapshot()) << "\n";
            continue;
        }
        if (cmd == "cancel") {
            OrderId id = 0;
            in >> id;
            const auto result = engine.cancel(CancelOrder{id, now_nanos()});
            publisher.publish(result);
            std::cout << market_update_to_json(result) << "\n";
            continue;
        }

        MatchResult result;
        if (cmd == "buy" || cmd == "sell") {
            Price price = 0;
            Quantity qty = 0;
            in >> price >> qty;
            result = engine.submit(make_limit(next_id++, cmd == "buy" ? Side::Buy : Side::Sell, price, qty));
        } else if (cmd == "mbuy" || cmd == "msell") {
            Quantity qty = 0;
            in >> qty;
            result = engine.submit(make_market(next_id++, cmd == "mbuy" ? Side::Buy : Side::Sell, qty));
        } else {
            std::cout << "unknown command\n";
            continue;
        }

        publisher.publish(result);
        std::cout << market_update_to_json(result) << "\n";
    }

    running = false;
    if (simulator.joinable()) {
        simulator.join();
    }
    server.stop();
    return 0;
}
