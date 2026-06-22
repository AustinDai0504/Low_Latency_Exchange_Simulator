#include "exchange/clock.hpp"
#include "exchange/matching_engine.hpp"

#include <cassert>
#include <iostream>

using namespace exchange;

int main() {
    MatchingEngine engine("TEST", 10);

    auto r1 = engine.submit(NewOrder{1, Side::Sell, OrderType::Limit, 10100, 10, now_nanos()});
    auto r2 = engine.submit(NewOrder{2, Side::Sell, OrderType::Limit, 10100, 20, now_nanos()});
    auto r3 = engine.submit(NewOrder{3, Side::Buy, OrderType::Limit, 10100, 25, now_nanos()});

    assert(r1.accepted);
    assert(r2.accepted);
    assert(r3.accepted);
    assert(r3.trades.size() == 2);
    assert(r3.trades[0].resting_order_id == 1); // 同价位先到先成交
    assert(r3.trades[0].quantity == 10);
    assert(r3.trades[1].resting_order_id == 2);
    assert(r3.trades[1].quantity == 15);

    auto shot = engine.snapshot();
    assert(shot.asks.size() == 1);
    assert(shot.asks[0].price == 10100);
    assert(shot.asks[0].quantity == 5);

    auto r4 = engine.submit(NewOrder{4, Side::Buy, OrderType::Limit, 10000, 50, now_nanos()});
    assert(r4.accepted);
    assert(engine.cancel(CancelOrder{4, now_nanos()}).cancelled);

    shot = engine.snapshot();
    assert(shot.bids.empty());

    auto r5 = engine.submit(NewOrder{5, Side::Buy, OrderType::Market, 0, 10, now_nanos()});
    assert(r5.accepted);
    assert(r5.trades.size() == 1);
    assert(r5.trades[0].quantity == 5);
    assert(engine.snapshot().asks.empty());

    std::cout << "order_book_tests passed\n";
    return 0;
}
