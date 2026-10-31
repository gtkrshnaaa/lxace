#include "net/http_server.hpp"
#include "net/http_parser.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <vector>

namespace lxace::net {

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

bool HttpServer::start() {
    if (running_) return true;

    if (!server_socket_.bind_and_listen(port_)) {
        return false;
    }

    reactor_ = std::make_unique<EpollReactor>(128);
    if (!reactor_->is_valid() || !reactor_->add_fd(server_socket_.fd(), EPOLLIN)) {
        server_socket_.close_socket();
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&HttpServer::server_loop, this);
    return true;
}

void HttpServer::stop() {
    if (!running_) return;
    running_ = false;

    server_socket_.close_socket();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void HttpServer::server_loop() {
    std::vector<epoll_event> events;

    while (running_) {
        int nfds = reactor_->wait(events, std::chrono::milliseconds(100));
        if (nfds <= 0) continue;

        for (const auto& ev : events) {
            if (ev.data.fd == server_socket_.fd()) {
                // Incoming client connection
                int client_fd = server_socket_.accept_client();
                if (client_fd >= 0) {
                    handle_client(client_fd);
                }
            }
        }
    }
}

void HttpServer::handle_client(int client_fd) {
    struct timeval tv{};
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    std::string raw_buffer;
    char chunk[4096];
    constexpr size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024; // 10MB

    while (raw_buffer.size() < MAX_REQUEST_SIZE) {
        ssize_t bytes_read = recv(client_fd, chunk, sizeof(chunk), 0);
        if (bytes_read > 0) {
            raw_buffer.append(chunk, static_cast<size_t>(bytes_read));
            // Check if we have complete headers
            auto header_end = raw_buffer.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                // Check content length
                auto cl_pos = raw_buffer.find("Content-Length:");
                if (cl_pos == std::string::npos) {
                    cl_pos = raw_buffer.find("content-length:");
                }
                if (cl_pos != std::string::npos && cl_pos < header_end) {
                    size_t val_start = cl_pos + 15;
                    size_t val_end = raw_buffer.find("\r\n", val_start);
                    if (val_end != std::string::npos) {
                        try {
                            size_t content_len = std::stoul(raw_buffer.substr(val_start, val_end - val_start));
                            if (raw_buffer.size() >= header_end + 4 + content_len) {
                                break;
                            }
                        } catch (...) {
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
        } else {
            break;
        }
    }

    if (raw_buffer.empty()) {
        close(client_fd);
        return;
    }

    auto req = HttpParser::parse(raw_buffer);
    if (!req) {
        HttpResponse bad_req;
        bad_req.status_code = 400;
        bad_req.status_text = "Bad Request";
        bad_req.body = "{\"error\": \"Malformed HTTP request\"}";
        std::string res_str = bad_req.to_http_string();
        send(client_fd, res_str.data(), res_str.size(), MSG_NOSIGNAL);
        close(client_fd);
        return;
    }

    if (req->path == "/api/events" && req->method == HttpMethod::Get) {
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
        res.status_code = 500;
        res.status_text = "Internal Server Error";
    }

    std::string res_str = res.to_http_string();
    send(client_fd, res_str.data(), res_str.size(), MSG_NOSIGNAL);
    close(client_fd);
}

} // namespace lxace::net
