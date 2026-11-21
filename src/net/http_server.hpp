#pragma once

#include "net/socket.hpp"
#include "net/epoll_reactor.hpp"
#include "net/http_auth.hpp"
#include "net/http_router.hpp"
#include "net/sse_stream.hpp"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lxace::net {

class HttpServer {
public:
    explicit HttpServer(uint16_t port = 8080);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void set_router(std::shared_ptr<HttpRouter> router);
    void set_sse_manager(std::shared_ptr<SseStreamManager> sse_manager);
    void set_bind_address(std::string address) { bind_address_ = std::move(address); }

    bool start();
    void stop();

    [[nodiscard]] bool is_running() const { return running_; }
    [[nodiscard]] uint16_t port() const { return port_; }
    [[nodiscard]] const std::string& bind_address() const { return bind_address_; }
    [[nodiscard]] std::string last_error() const;

private:
    void server_loop();
    void handle_client(int client_fd);
    void dispatch_client(int client_fd);
    void worker_loop();
    void enqueue(int client_fd);
    void join_workers();

    uint16_t port_;
    std::string bind_address_{"127.0.0.1"};
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    ServerSocket server_socket_;
    std::unique_ptr<EpollReactor> reactor_;
    std::shared_ptr<HttpRouter> router_;
    std::shared_ptr<SseStreamManager> sse_manager_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<int> pending_clients_;
    std::vector<std::thread> workers_;
    mutable std::mutex error_mutex_;
    std::string last_error_;
};

} // namespace lxace::net
