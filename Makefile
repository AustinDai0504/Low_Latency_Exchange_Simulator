CXX ?= c++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -Wpedantic -Iinclude -pthread

SRC := src/order_book.cpp src/matching_engine.cpp src/market_data_publisher.cpp src/json.cpp src/websocket_server.cpp
CORE_SRC := src/order_book.cpp src/matching_engine.cpp src/market_data_publisher.cpp src/json.cpp

.PHONY: all clean test benchmark

all: build/exchange_server build/benchmark build/order_book_tests

build:
	mkdir -p build

build/exchange_server: $(SRC) apps/exchange_server.cpp | build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/benchmark: $(CORE_SRC) apps/benchmark.cpp | build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/order_book_tests: $(CORE_SRC) tests/order_book_tests.cpp | build
	$(CXX) $(CXXFLAGS) $^ -o $@

test: build/order_book_tests
	./build/order_book_tests

benchmark: build/benchmark
	./build/benchmark

clean:
	rm -rf build
