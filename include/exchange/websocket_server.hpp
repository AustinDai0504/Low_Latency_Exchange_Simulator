#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace exchange {

class WebSocketServer {
public:
    WebSocketServer(std::string host, std::uint16_t port);
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    void start();
    void stop();
    void broadcast_text(const std::string& payload);

private:
    void accept_loop();
    void handle_client(int client_fd);
    bool perform_handshake(int client_fd, const std::string& request);
    void close_client(int client_fd);

    std::string host_;
    std::uint16_t port_{};
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    std::mutex clients_mutex_;
    std::set<int> clients_;
};

} // namespace exchange
