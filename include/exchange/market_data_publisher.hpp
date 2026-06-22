#pragma once

#include "exchange/types.hpp"

#include <functional>
#include <mutex>
#include <vector>

namespace exchange {

class MarketDataPublisher {
public:
    using Subscriber = std::function<void(const MatchResult&)>;

    void subscribe(Subscriber subscriber);
    void publish(const MatchResult& result);

private:
    std::mutex mutex_;
    std::vector<Subscriber> subscribers_;
};

} // namespace exchange
