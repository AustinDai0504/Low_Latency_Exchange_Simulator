#pragma once

#include "exchange/types.hpp"

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace exchange {

class OrderBook {
public:
    explicit OrderBook(std::string symbol, std::size_t snapshot_depth = 10);

    MatchResult add_order(const NewOrder& order);
    MatchResult cancel_order(const CancelOrder& cancel);

    BookSnapshot snapshot() const;
    std::uint64_t sequence() const { return sequence_; }
    const std::string& symbol() const { return symbol_; }

private:
    struct RestingOrder {
        OrderId id{};
        Side side{Side::Buy};
        Price price{};
        Quantity remaining{};
        TimestampNanos timestamp{};
        bool active{true};
    };

    using OrderPtr = std::shared_ptr<RestingOrder>;
    using BidBook = std::map<Price, std::deque<OrderPtr>, std::greater<Price>>;
    using AskBook = std::map<Price, std::deque<OrderPtr>>;

    struct OrderLocation {
        Side side{Side::Buy};
        Price price{};
        OrderPtr order;
    };

    bool can_match(Side aggressor_side, Price aggressor_price, OrderType type) const;
    void match_buy(const NewOrder& aggressor, Quantity& remaining, std::vector<Trade>& trades);
    void match_sell(const NewOrder& aggressor, Quantity& remaining, std::vector<Trade>& trades);
    void insert_limit_order(const NewOrder& order, Quantity remaining);

    void prune_empty_levels();
    static Quantity visible_quantity(const std::deque<OrderPtr>& queue);

    std::string symbol_;
    std::size_t snapshot_depth_{10};
    std::uint64_t sequence_{0};

    // map 按价格排序，deque 保留同价位 FIFO 队列，即 price-time priority
    BidBook bids_;
    AskBook asks_;
    std::unordered_map<OrderId, OrderLocation> order_index_;
};

} // namespace exchange
