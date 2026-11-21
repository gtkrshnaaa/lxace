#pragma once

#include <cstdint>
#include <string>

namespace lxace::net {

class ServerSocket {
public:
    ServerSocket();
    ~ServerSocket();

    ServerSocket(const ServerSocket&) = delete;
    ServerSocket& operator=(const ServerSocket&) = delete;

    // bind_address defaults to loopback. Pass "0.0.0.0" to expose the listener remotely,
    // which requires bearer-token authentication to be configured first.
    bool bind_and_listen(uint16_t port, int backlog = 128, const std::string& bind_address = "127.0.0.1");
    [[nodiscard]] int accept_client(std::string* client_ip = nullptr);
    void close_socket();

    [[nodiscard]] int fd() const { return fd_; }
    [[nodiscard]] bool is_valid() const { return fd_ >= 0; }

private:
    int fd_{-1};
};

} // namespace lxace::net
