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

    bool bind_and_listen(uint16_t port, int backlog = 128);
    [[nodiscard]] int accept_client(std::string* client_ip = nullptr);
    void close_socket();

    [[nodiscard]] int fd() const { return fd_; }
    [[nodiscard]] bool is_valid() const { return fd_ >= 0; }

private:
    int fd_{-1};
};

} // namespace lxace::net
