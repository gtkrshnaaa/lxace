#pragma once

#include "net/socket.hpp"
#include "net/epoll_reactor.hpp"
#include "net/http_router.hpp"
#include "net/sse_stream.hpp"
#include <thread>
#include <atomic>
#include <memory>

namespace lxace::net {

class HttpServer {
public:
    explicit HttpServer(uint16_t port = 8080);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void set_router(std::shared_ptr<HttpRouter> router);
    void set_sse_manager(std::shared_ptr<SseStreamManager> sse_manager);

    bool start();
    void stop();

    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] uint16_t port() const { return port_; }

private:
    void server_loop();
    void handle_client(int client_fd);

    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    ServerSocket server_socket_;
    std::unique_ptr<EpollReactor> reactor_;
    std::shared_ptr<HttpRouter> router_;
    std::shared_ptr<SseStreamManager> sse_manager_;
};

} // namespace lxace::net
