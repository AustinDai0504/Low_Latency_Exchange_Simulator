#pragma once

#include "exchange/order_book.hpp"

#include <mutex>
#include <string>

namespace exchange {

class MatchingEngine {
public:
    explicit MatchingEngine(std::string symbol, std::size_t snapshot_depth = 10);

    MatchResult submit(const NewOrder& order);
    MatchResult cancel(const CancelOrder& cancel);
    BookSnapshot snapshot() const;

private:
    mutable std::mutex mutex_;
    OrderBook book_;
};

} // namespace exchange
