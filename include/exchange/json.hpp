#pragma once

#include "exchange/types.hpp"

#include <sstream>
#include <string>

namespace exchange {

std::string snapshot_to_json(const BookSnapshot& snapshot);
std::string trades_to_json(const std::vector<Trade>& trades);
std::string market_update_to_json(const MatchResult& result);

} // namespace exchange
