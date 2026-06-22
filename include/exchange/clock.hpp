#pragma once

#include "exchange/types.hpp"

#include <chrono>

namespace exchange {

inline TimestampNanos now_nanos() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<TimestampNanos>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace exchange
