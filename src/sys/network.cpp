#include "sys/network.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>

namespace lxace::sys {

bool NetworkUtils::is_port_in_use(uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return true;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int res = bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    close(sock);
    return res < 0;
}

bool NetworkUtils::check_tcp_connection(const std::string& host, uint16_t port, std::chrono::milliseconds timeout) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        return false;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return false;
    }

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int conn_res = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (conn_res == 0) {
        close(sock);
        return true;
    }

    if (errno != EINPROGRESS) {
        close(sock);
        return false;
    }

    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLOUT;

    int poll_res = poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (poll_res <= 0) {
        close(sock);
        return false;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    close(sock);

    return so_error == 0;
}

} // namespace lxace::sys
