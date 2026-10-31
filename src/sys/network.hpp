#pragma once

#include <string>
#include <chrono>
#include <cstdint>

namespace lxace::sys {

class NetworkUtils {
public:
    static bool is_port_in_use(uint16_t port);
    static bool check_tcp_connection(const std::string& host, uint16_t port, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));
};

} // namespace lxace::sys
