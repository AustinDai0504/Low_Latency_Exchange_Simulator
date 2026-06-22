#include "exchange/order_book.hpp"

#include <utility>

namespace exchange {

OrderBook::OrderBook(std::string symbol, std::size_t snapshot_depth)
    : symbol_(std::move(symbol)), snapshot_depth_(snapshot_depth) {}

MatchResult OrderBook::add_order(const NewOrder& order) {
    MatchResult result;
    result.snapshot.symbol = symbol_;

    if (order.quantity == 0) {
        result.message = "rejected: quantity must be positive";
        result.snapshot = snapshot();
        return result;
    }
    if (order.type == OrderType::Limit && order.price <= 0) {
        result.message = "rejected: limit price must be positive";
        result.snapshot = snapshot();
        return result;
    }
    if (order_index_.find(order.id) != order_index_.end()) {
        result.message = "rejected: duplicated order id";
        result.snapshot = snapshot();
        return result;
    }

    Quantity remaining = order.quantity;
    if (order.side == Side::Buy) {
        match_buy(order, remaining, result.trades);
    } else {
        match_sell(order, remaining, result.trades);
    }

    if (remaining > 0 && order.type == OrderType::Limit) {
        insert_limit_order(order, remaining);
    }

    prune_empty_levels();
    ++sequence_;
    result.accepted = true;
    result.message = "accepted";
    result.snapshot = snapshot();
    return result;
}

MatchResult OrderBook::cancel_order(const CancelOrder& cancel) {
    MatchResult result;
    result.snapshot.symbol = symbol_;

    auto found = order_index_.find(cancel.id);
    if (found == order_index_.end() || !found->second.order->active) {
        result.message = "cancel rejected: order not found";
        result.snapshot = snapshot();
        return result;
    }

    // 撤单不在线性队列中间删除，直接标记 inactive，撮合或快照时跳过。
    found->second.order->active = false;
    found->second.order->remaining = 0;
    order_index_.erase(found);
    prune_empty_levels();

    ++sequence_;
    result.accepted = true;
    result.cancelled = true;
    result.message = "cancelled";
    result.snapshot = snapshot();
    return result;
}

BookSnapshot OrderBook::snapshot() const {
    BookSnapshot shot;
    shot.symbol = symbol_;
    shot.sequence = sequence_;

    for (const auto& [price, queue] : bids_) {
        const auto qty = visible_quantity(queue);
        if (qty > 0) {
            shot.bids.push_back({price, qty});
            if (shot.bids.size() >= snapshot_depth_) {
                break;
            }
        }
    }

    for (const auto& [price, queue] : asks_) {
        const auto qty = visible_quantity(queue);
        if (qty > 0) {
            shot.asks.push_back({price, qty});
            if (shot.asks.size() >= snapshot_depth_) {
                break;
            }
        }
    }

    return shot;
}

bool OrderBook::can_match(Side aggressor_side, Price aggressor_price, OrderType type) const {
    if (aggressor_side == Side::Buy) {
        if (asks_.empty()) {
            return false;
        }
        return type == OrderType::Market || asks_.begin()->first <= aggressor_price;
    }

    if (bids_.empty()) {
        return false;
    }
    return type == OrderType::Market || bids_.begin()->first >= aggressor_price;
}

void OrderBook::match_buy(const NewOrder& aggressor, Quantity& remaining, std::vector<Trade>& trades) {
    while (remaining > 0 && can_match(Side::Buy, aggressor.price, aggressor.type)) {
        auto level_it = asks_.begin();
        auto& queue = level_it->second;

        while (!queue.empty() && (!queue.front()->active || queue.front()->remaining == 0)) {
            queue.pop_front();
        }
        if (queue.empty()) {
            asks_.erase(level_it);
            continue;
        }

        auto resting = queue.front();
        const Quantity fill_qty = std::min(remaining, resting->remaining);
        remaining -= fill_qty;
        resting->remaining -= fill_qty;

        trades.push_back({resting->id, aggressor.id, resting->price, fill_qty, aggressor.timestamp});

        if (resting->remaining == 0) {
            resting->active = false;
            order_index_.erase(resting->id);
            queue.pop_front();
        }
    }
}

void OrderBook::match_sell(const NewOrder& aggressor, Quantity& remaining, std::vector<Trade>& trades) {
    while (remaining > 0 && can_match(Side::Sell, aggressor.price, aggressor.type)) {
        auto level_it = bids_.begin();
        auto& queue = level_it->second;

        while (!queue.empty() && (!queue.front()->active || queue.front()->remaining == 0)) {
            queue.pop_front();
        }
        if (queue.empty()) {
            bids_.erase(level_it);
            continue;
        }

        auto resting = queue.front();
        const Quantity fill_qty = std::min(remaining, resting->remaining);
        remaining -= fill_qty;
        resting->remaining -= fill_qty;

        trades.push_back({resting->id, aggressor.id, resting->price, fill_qty, aggressor.timestamp});

        if (resting->remaining == 0) {
            resting->active = false;
            order_index_.erase(resting->id);
            queue.pop_front();
        }
    }
}

void OrderBook::insert_limit_order(const NewOrder& order, Quantity remaining) {
    auto resting = std::make_shared<RestingOrder>();
    resting->id = order.id;
    resting->side = order.side;
    resting->price = order.price;
    resting->remaining = remaining;
    resting->timestamp = order.timestamp;

    if (order.side == Side::Buy) {
        bids_[order.price].push_back(resting);
    } else {
        asks_[order.price].push_back(resting);
    }

    order_index_[order.id] = {order.side, order.price, resting};
}

void OrderBook::prune_empty_levels() {
    for (auto it = bids_.begin(); it != bids_.end();) {
        auto& queue = it->second;
        while (!queue.empty() && (!queue.front()->active || queue.front()->remaining == 0)) {
            queue.pop_front();
        }
        if (queue.empty()) {
            it = bids_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = asks_.begin(); it != asks_.end();) {
        auto& queue = it->second;
        while (!queue.empty() && (!queue.front()->active || queue.front()->remaining == 0)) {
            queue.pop_front();
        }
        if (queue.empty()) {
            it = asks_.erase(it);
        } else {
            ++it;
        }
    }
}

Quantity OrderBook::visible_quantity(const std::deque<OrderPtr>& queue) {
    Quantity total = 0;
    for (const auto& order : queue) {
        if (order->active) {
            total += order->remaining;
        }
    }
    return total;
}

} // namespace exchange
