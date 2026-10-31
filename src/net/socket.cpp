#include "net/socket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace lxace::net {

ServerSocket::ServerSocket() = default;

ServerSocket::~ServerSocket() {
    close_socket();
}

void ServerSocket::close_socket() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool ServerSocket::bind_and_listen(uint16_t port, int backlog) {
    close_socket();

    fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd_ < 0) return false;

    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket();
        return false;
    }

    if (listen(fd_, backlog) < 0) {
        close_socket();
        return false;
    }

    return true;
}

int ServerSocket::accept_client(std::string* client_ip) {
    if (fd_ < 0) return -1;

    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept4(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (client_fd >= 0 && client_ip) {
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str))) {
            *client_ip = ip_str;
        }
    }
    return client_fd;
}

} // namespace lxace::net
