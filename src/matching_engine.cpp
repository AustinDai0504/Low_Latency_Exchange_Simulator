#include "exchange/matching_engine.hpp"

#include <utility>

namespace exchange {

MatchingEngine::MatchingEngine(std::string symbol, std::size_t snapshot_depth)
    : book_(std::move(symbol), snapshot_depth) {}

MatchResult MatchingEngine::submit(const NewOrder& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    return book_.add_order(order);
}

MatchResult MatchingEngine::cancel(const CancelOrder& cancel) {
    std::lock_guard<std::mutex> lock(mutex_);
    return book_.cancel_order(cancel);
}

BookSnapshot MatchingEngine::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return book_.snapshot();
}

} // namespace exchange
