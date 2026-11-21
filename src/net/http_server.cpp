#include "net/http_server.hpp"
#include "net/http_parser.hpp"
#include "net/request_reader.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <chrono>
#include <vector>

namespace lxace::net {

namespace {
constexpr size_t kWorkerCount = 4;
} // namespace

HttpServer::HttpServer(uint16_t port) : port_(port) {
    router_ = std::make_shared<HttpRouter>();
    sse_manager_ = std::make_shared<SseStreamManager>();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::set_router(std::shared_ptr<HttpRouter> router) {
    router_ = std::move(router);
}

void HttpServer::set_sse_manager(std::shared_ptr<SseStreamManager> sse_manager) {
    sse_manager_ = std::move(sse_manager);
}

std::string HttpServer::last_error() const {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

bool HttpServer::start() {
    if (running_) return true;

    if (!server_socket_.bind_and_listen(port_, 128, bind_address_)) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "failed to bind " + bind_address_ + ":" + std::to_string(port_);
        return false;
    }

    reactor_ = std::make_unique<EpollReactor>(128);
    if (!reactor_->is_valid() || !reactor_->add_fd(server_socket_.fd(), EPOLLIN)) {
        server_socket_.close_socket();
        std::lock_guard<std::mutex> lock(error_mutex_);
        last_error_ = "failed to register listener with epoll";
        return false;
    }

    running_ = true;

    // A fixed worker pool keeps long-running requests off the acceptance thread, so a
    // slow /api/exec can no longer starve /api/stop or the SSE stream.
    workers_.reserve(kWorkerCount);
    for (size_t i = 0; i < kWorkerCount; ++i) {
        workers_.emplace_back(&HttpServer::worker_loop, this);
    }

    server_thread_ = std::thread(&HttpServer::server_loop, this);
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    server_socket_.close_socket();
    queue_cv_.notify_all();

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    join_workers();
}

void HttpServer::join_workers() {
    for (auto& w : workers_) {
        if (w.joinable()) {
            w.join();
        }
    }
    workers_.clear();
}

void HttpServer::enqueue(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        pending_clients_.push_back(client_fd);
    }
    queue_cv_.notify_one();
}

void HttpServer::worker_loop() {
    for (;;) {
        int client_fd = -1;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() { return !running_ || !pending_clients_.empty(); });
            if (!running_ && pending_clients_.empty()) {
                return;
            }
            client_fd = pending_clients_.front();
            pending_clients_.pop_front();
        }
        if (client_fd >= 0) {
            handle_client(client_fd);
        }
    }
}

void HttpServer::server_loop() {
    std::vector<epoll_event> events;
    auto last_reap = std::chrono::steady_clock::now();

    while (running_) {
        int nfds = reactor_->wait(events, std::chrono::milliseconds(100));
        if (nfds < 0) continue;

        // Reclaim hung-up SSE subscribers on a slow timer. broadcast() only reaps on a
        // failed send, so an idle stream kept closed descriptors until the next event.
        const auto now = std::chrono::steady_clock::now();
        if (sse_manager_ && now - last_reap >= std::chrono::milliseconds(1000)) {
            sse_manager_->reap_disconnected();
            last_reap = now;
        }

        if (nfds == 0) continue;

        for (const auto& ev : events) {
            if (ev.data.fd == server_socket_.fd()) {
                int client_fd = server_socket_.accept_client();
                if (client_fd >= 0) {
                    enqueue(client_fd);
                }
            }
        }
    }

    // Drain descriptors accepted during shutdown so none is leaked.
    std::lock_guard<std::mutex> lock(queue_mutex_);
    for (int fd : pending_clients_) {
        close(fd);
    }
    pending_clients_.clear();
}

void HttpServer::handle_client(int client_fd) {
    struct timeval tv{};
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    const std::string raw_buffer = RequestReader::read_request(client_fd);

    if (raw_buffer.empty()) {
        close(client_fd);
        return;
    }

    auto req = HttpParser::parse(raw_buffer);
    if (!req) {
        auto bad_req = HttpResponse::json(400, "{\"error\": \"Malformed HTTP request\"}");
        std::string res_str = bad_req.to_http_string();
        send(client_fd, res_str.data(), res_str.size(), MSG_NOSIGNAL);
        close(client_fd);
        return;
    }

    if (req->path == "/api/events" && req->method == HttpMethod::Get) {
        // The SSE endpoint is dispatched before the router, so it must repeat the auth
        // gate. Otherwise it was the one unauthenticated path into the event stream.
        if (router_) {
            const auto& policy = router_->auth_policy();
            int auth_status = 200;
            if (policy.require_token && !HttpAuth::authorize(*req, policy, auth_status)) {
                HttpResponse denied = HttpResponse::json(auth_status, "{\"error\": \"Unauthorized\"}");
                denied.headers["WWW-Authenticate"] = "Bearer realm=\"lxace\"";
                HttpAuth::apply_cors(*req, policy, denied);
                const std::string res_str = denied.to_http_string();
                send(client_fd, res_str.data(), res_str.size(), MSG_NOSIGNAL);
                close(client_fd);
                return;
            }
            // Preflight for the stream must pass the same origin allowlist.
            const std::string origin = req->get_header("Origin");
            if (!origin.empty() && !policy.allows_origin(origin)) {
                const auto forbidden = HttpResponse::json(403, "{\"error\": \"Origin not allowed\"}");
                const std::string res_str = forbidden.to_http_string();
                send(client_fd, res_str.data(), res_str.size(), MSG_NOSIGNAL);
                close(client_fd);
                return;
            }
        }
        // Hand off client to SSE manager (does not close fd)
        if (sse_manager_) {
            sse_manager_->add_client(client_fd);
        } else {
            close(client_fd);
        }
        return;
    }

    HttpResponse res;
    if (router_) {
        res = router_->route(*req);
    } else {
        res = HttpResponse::json(500, "{\"error\": \"No router configured\"}");
    }

    std::string res_str = res.to_http_string();
    send(client_fd, res_str.data(), res_str.size(), MSG_NOSIGNAL);
    close(client_fd);
}

} // namespace lxace::net
