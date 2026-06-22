#include "exchange/websocket_server.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace exchange {

namespace {

std::uint32_t left_rotate(std::uint32_t value, std::uint32_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

std::array<std::uint8_t, 20> sha1(std::string_view input) {
    std::uint64_t bit_len = static_cast<std::uint64_t>(input.size()) * 8;
    std::vector<std::uint8_t> msg(input.begin(), input.end());
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<std::uint8_t>((bit_len >> (i * 8)) & 0xff));
    }

    std::uint32_t h0 = 0x67452301;
    std::uint32_t h1 = 0xefcdab89;
    std::uint32_t h2 = 0x98badcfe;
    std::uint32_t h3 = 0x10325476;
    std::uint32_t h4 = 0xc3d2e1f0;

    for (std::size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        std::array<std::uint32_t, 80> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            w[i] = (msg[chunk + i * 4] << 24) | (msg[chunk + i * 4 + 1] << 16) |
                   (msg[chunk + i * 4 + 2] << 8) | msg[chunk + i * 4 + 3];
        }
        for (std::size_t i = 16; i < 80; ++i) {
            w[i] = left_rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (std::size_t i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdc;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6;
            }
            const auto temp = left_rotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = left_rotate(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    std::array<std::uint8_t, 20> out{};
    const std::array<std::uint32_t, 5> words{h0, h1, h2, h3, h4};
    for (std::size_t i = 0; i < words.size(); ++i) {
        out[i * 4] = static_cast<std::uint8_t>((words[i] >> 24) & 0xff);
        out[i * 4 + 1] = static_cast<std::uint8_t>((words[i] >> 16) & 0xff);
        out[i * 4 + 2] = static_cast<std::uint8_t>((words[i] >> 8) & 0xff);
        out[i * 4 + 3] = static_cast<std::uint8_t>(words[i] & 0xff);
    }
    return out;
}

std::string base64_encode(const std::uint8_t* data, std::size_t len) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (std::size_t i = 0; i < len; i += 3) {
        const std::uint32_t octet_a = data[i];
        const std::uint32_t octet_b = i + 1 < len ? data[i + 1] : 0;
        const std::uint32_t octet_c = i + 2 < len ? data[i + 2] : 0;
        const std::uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(table[(triple >> 18) & 0x3f]);
        out.push_back(table[(triple >> 12) & 0x3f]);
        out.push_back(i + 1 < len ? table[(triple >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < len ? table[triple & 0x3f] : '=');
    }
    return out;
}

std::string websocket_accept_key(const std::string& client_key) {
    static constexpr std::string_view guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto digest = sha1(client_key + std::string(guid));
    return base64_encode(digest.data(), digest.size());
}

std::string header_value(const std::string& request, const std::string& name) {
    std::istringstream stream(request);
    std::string line;
    const std::string prefix = name + ":";
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind(prefix, 0) == 0) {
            auto value = line.substr(prefix.size());
            while (!value.empty() && value.front() == ' ') {
                value.erase(value.begin());
            }
            return value;
        }
    }
    return {};
}

bool send_all(int fd, const std::uint8_t* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        const auto n = ::send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::vector<std::uint8_t> text_frame(const std::string& payload) {
    std::vector<std::uint8_t> frame;
    frame.push_back(0x81); // FIN + text
    if (payload.size() <= 125) {
        frame.push_back(static_cast<std::uint8_t>(payload.size()));
    } else if (payload.size() <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<std::uint8_t>((payload.size() >> 8) & 0xff));
        frame.push_back(static_cast<std::uint8_t>(payload.size() & 0xff));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<std::uint8_t>((payload.size() >> (i * 8)) & 0xff));
        }
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

} // namespace

WebSocketServer::WebSocketServer(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    if (running_) {
        return;
    }

    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("socket() failed");
    }

    int yes = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = host_ == "0.0.0.0" ? INADDR_ANY : ::inet_addr(host_.c_str());

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(server_fd_);
        server_fd_ = -1;
        throw std::runtime_error("bind() failed: " + std::string(std::strerror(errno)));
    }
    if (::listen(server_fd_, 64) < 0) {
        ::close(server_fd_);
        server_fd_ = -1;
        throw std::runtime_error("listen() failed");
    }

    running_ = true;
    accept_thread_ = std::thread(&WebSocketServer::accept_loop, this);
}

void WebSocketServer::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    if (server_fd_ >= 0) {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int fd : clients_) {
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }
    clients_.clear();
}

void WebSocketServer::broadcast_text(const std::string& payload) {
    const auto frame = text_frame(payload);
    std::vector<int> dead_clients;

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (int fd : clients_) {
        if (!send_all(fd, frame.data(), frame.size())) {
            dead_clients.push_back(fd);
        }
    }
    for (int fd : dead_clients) {
        ::close(fd);
        clients_.erase(fd);
    }
}

void WebSocketServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        const int client_fd = ::accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &len);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "accept() failed\n";
            }
            continue;
        }
        std::thread(&WebSocketServer::handle_client, this, client_fd).detach();
    }
}

void WebSocketServer::handle_client(int client_fd) {
    std::array<char, 4096> buffer{};
    const auto n = ::recv(client_fd, buffer.data(), buffer.size(), 0);
    if (n <= 0) {
        ::close(client_fd);
        return;
    }

    std::string request(buffer.data(), static_cast<std::size_t>(n));
    if (!perform_handshake(client_fd, request)) {
        ::close(client_fd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.insert(client_fd);
    }

    // 服务端只广播行情；这里读取客户端帧是为了及时发现断开连接。
    while (running_) {
        std::array<std::uint8_t, 2> hdr{};
        const auto got = ::recv(client_fd, hdr.data(), hdr.size(), 0);
        if (got <= 0) {
            break;
        }
        if ((hdr[0] & 0x0f) == 0x8) {
            break;
        }
    }
    close_client(client_fd);
}

bool WebSocketServer::perform_handshake(int client_fd, const std::string& request) {
    const auto key = header_value(request, "Sec-WebSocket-Key");
    if (key.empty()) {
        return false;
    }

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << websocket_accept_key(key) << "\r\n\r\n";
    const auto text = response.str();
    return send_all(client_fd, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

void WebSocketServer::close_client(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(client_fd);
    ::shutdown(client_fd, SHUT_RDWR);
    ::close(client_fd);
}

} // namespace exchange
