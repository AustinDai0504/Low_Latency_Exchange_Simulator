#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace exchange {

using OrderId = std::uint64_t;
using Price = std::int64_t;     // 价格用整数 tick 表示，避免浮点误差
using Quantity = std::uint64_t;
using TimestampNanos = std::uint64_t;

enum class Side { Buy, Sell };
enum class OrderType { Limit, Market };

struct NewOrder {
    OrderId id{};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    Price price{};
    Quantity quantity{};
    TimestampNanos timestamp{};
};

struct CancelOrder {
    OrderId id{};
    TimestampNanos timestamp{};
};

struct Trade {
    OrderId resting_order_id{};
    OrderId aggressor_order_id{};
    Price price{};
    Quantity quantity{};
    TimestampNanos timestamp{};
};

struct BookLevel {
    Price price{};
    Quantity quantity{};
};

struct BookSnapshot {
    std::string symbol;
    std::uint64_t sequence{};
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
};

struct MatchResult {
    bool accepted{false};
    bool cancelled{false};
    std::string message;
    std::vector<Trade> trades;
    BookSnapshot snapshot;
};

inline const char* to_string(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

inline const char* to_string(OrderType type) {
    return type == OrderType::Limit ? "LIMIT" : "MARKET";
}

} // namespace exchange
