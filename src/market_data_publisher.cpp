#include "exchange/market_data_publisher.hpp"

namespace exchange {

void MarketDataPublisher::subscribe(Subscriber subscriber) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.push_back(std::move(subscriber));
}

void MarketDataPublisher::publish(const MatchResult& result) {
    std::vector<Subscriber> subscribers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers = subscribers_;
    }

    for (const auto& subscriber : subscribers) {
        subscriber(result);
    }
}

} // namespace exchange
