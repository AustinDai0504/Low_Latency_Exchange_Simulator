#include "exchange/json.hpp"

namespace exchange {

namespace {

void append_levels(std::ostringstream& os, const std::vector<BookLevel>& levels) {
    os << "[";
    for (std::size_t i = 0; i < levels.size(); ++i) {
        if (i > 0) {
            os << ",";
        }
        os << "{\"price\":" << levels[i].price << ",\"qty\":" << levels[i].quantity << "}";
    }
    os << "]";
}

} // namespace

std::string snapshot_to_json(const BookSnapshot& snapshot) {
    std::ostringstream os;
    os << "{\"type\":\"book\",\"symbol\":\"" << snapshot.symbol << "\",\"seq\":" << snapshot.sequence
       << ",\"bids\":";
    append_levels(os, snapshot.bids);
    os << ",\"asks\":";
    append_levels(os, snapshot.asks);
    os << "}";
    return os.str();
}

std::string trades_to_json(const std::vector<Trade>& trades) {
    std::ostringstream os;
    os << "[";
    for (std::size_t i = 0; i < trades.size(); ++i) {
        if (i > 0) {
            os << ",";
        }
        const auto& t = trades[i];
        os << "{\"resting\":" << t.resting_order_id << ",\"aggressor\":" << t.aggressor_order_id
           << ",\"price\":" << t.price << ",\"qty\":" << t.quantity
           << ",\"ts\":" << t.timestamp << "}";
    }
    os << "]";
    return os.str();
}

std::string market_update_to_json(const MatchResult& result) {
    std::ostringstream os;
    os << "{\"type\":\"update\",\"accepted\":" << (result.accepted ? "true" : "false")
       << ",\"cancelled\":" << (result.cancelled ? "true" : "false")
       << ",\"message\":\"" << result.message << "\",\"trades\":" << trades_to_json(result.trades)
       << ",\"book\":" << snapshot_to_json(result.snapshot) << "}";
    return os.str();
}

} // namespace exchange
