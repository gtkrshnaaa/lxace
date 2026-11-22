#include "net/sse_stream.hpp"
#include "net/socket_write.hpp"
#include "json/json_value.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <algorithm>
#include <cerrno>
#include <regex>

namespace lxace::net {

namespace {

std::string redact_secret_values(const std::string& text) {
    // Redacts anything that looks like an assigned credential before it reaches a
    // subscriber. Covers JSON pairs, TOKEN=value shell style, and bare API keys.
    static const std::regex kJsonSecret(
        R"RX(("(?:api[_-]?key|token|secret|password|authorization)"\s*:\s*")([^"]*)("))RX",
        std::regex::icase);
    static const std::regex kShellSecret(
        R"RX((\b(?:API[_-]?KEY|TOKEN|SECRET|PASSWORD)=)(\S+))RX",
        std::regex::icase);
    static const std::regex kBareKey(
        R"RX(\b(sk-[A-Za-z0-9_\-]{8,}))RX");

    std::string out = std::regex_replace(text, kJsonSecret, "$1***$3");
    out = std::regex_replace(out, kShellSecret, "$1***");
    out = std::regex_replace(out, kBareKey, "sk-***");
    return out;
}

} // namespace

SseStreamManager::~SseStreamManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int fd : client_fds_) {
        close(fd);
    }
    client_fds_.clear();
}

void SseStreamManager::add_client(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (client_fds_.size() >= MAX_CLIENTS) {
            // Close oldest client to prevent file descriptor exhaustion
            int oldest_fd = client_fds_.front();
            close(oldest_fd);
            client_fds_.erase(client_fds_.begin());
        }
        client_fds_.push_back(client_fd);
    }

    // Initial SSE header. The CORS origin is applied by the router policy, never a wildcard.
    std::string initial_header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        ": connected\n\n";

    send_all(client_fd, initial_header);
}

void SseStreamManager::remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(client_fds_.begin(), client_fds_.end(), client_fd);
    if (it != client_fds_.end()) {
        close(*it);
        client_fds_.erase(it);
    }
}

size_t SseStreamManager::client_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return client_fds_.size();
}

void SseStreamManager::broadcast(const core::Event& event) {
    json::JsonObject obj;
    obj["id"] = event.id;
    obj["agent_id"] = event.agent_id;
    obj["kind"] = core::event_kind_to_string(event.kind);
    obj["text"] = redact_secret_values(event.text);
    obj["details"] = redact_secret_values(event.details);
    obj["time"] = event.format_time();

    json::JsonValue val(obj);
    std::string sse_payload = "data: " + val.serialize(false) + "\n\n";
    broadcast_raw(sse_payload);
}

void SseStreamManager::broadcast_raw(const std::string& data) {
    // Snapshot under the lock, then send outside it. Holding the mutex across a blocking
    // send() stalled every producer for up to SO_SNDTIMEO per slow subscriber.
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        targets = client_fds_;
    }

    std::vector<int> dead_fds;
    for (int fd : targets) {
        ssize_t sent = send(fd, data.data(), data.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent < 0) {
            dead_fds.push_back(fd);
        }
    }

    if (dead_fds.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (int dead_fd : dead_fds) {
        close(dead_fd);
        client_fds_.erase(std::remove(client_fds_.begin(), client_fds_.end(), dead_fd), client_fds_.end());
    }
}

size_t SseStreamManager::reap_disconnected() {
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        targets = client_fds_;
    }

    std::vector<int> dead_fds;
    for (int fd : targets) {
        // A zero-length non-blocking recv distinguishes an orderly hangup from
        // "no data available yet" without disturbing the stream.
        char probe = 0;
        const ssize_t n = recv(fd, &probe, 1, MSG_PEEK | MSG_DONTWAIT);
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            dead_fds.push_back(fd);
        }
    }

    if (dead_fds.empty()) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (int dead_fd : dead_fds) {
        close(dead_fd);
        client_fds_.erase(std::remove(client_fds_.begin(), client_fds_.end(), dead_fd), client_fds_.end());
    }
    return dead_fds.size();
}

} // namespace lxace::net
