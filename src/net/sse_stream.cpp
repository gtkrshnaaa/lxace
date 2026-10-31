#include "net/sse_stream.hpp"
#include "json/json_value.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <algorithm>

namespace lxace::net {

SseStreamManager::~SseStreamManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int fd : client_fds_) {
        close(fd);
    }
    client_fds_.clear();
}

void SseStreamManager::add_client(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    client_fds_.push_back(client_fd);

    // Initial SSE header
    std::string initial_header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n"
        ": connected\n\n";

    send(client_fd, initial_header.data(), initial_header.size(), MSG_NOSIGNAL);
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
    obj["text"] = event.text;
    obj["details"] = event.details;
    obj["time"] = event.format_time();

    json::JsonValue val(obj);
    std::string sse_payload = "data: " + val.serialize(false) + "\n\n";
    broadcast_raw(sse_payload);
}

void SseStreamManager::broadcast_raw(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int> dead_fds;

    for (int fd : client_fds_) {
        ssize_t sent = send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            dead_fds.push_back(fd);
        }
    }

    for (int dead_fd : dead_fds) {
        close(dead_fd);
        client_fds_.erase(std::remove(client_fds_.begin(), client_fds_.end(), dead_fd), client_fds_.end());
    }
}

} // namespace lxace::net
