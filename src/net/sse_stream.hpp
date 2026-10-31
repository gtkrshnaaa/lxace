#pragma once

#include "core/event.hpp"
#include <vector>
#include <mutex>
#include <string>

namespace lxace::net {

class SseStreamManager {
public:
    SseStreamManager() = default;
    ~SseStreamManager();

    SseStreamManager(const SseStreamManager&) = delete;
    SseStreamManager& operator=(const SseStreamManager&) = delete;

    void add_client(int client_fd);
    void remove_client(int client_fd);
    void broadcast(const core::Event& event);
    void broadcast_raw(const std::string& data);

    [[nodiscard]] size_t client_count() const;

private:
    mutable std::mutex mutex_;
    std::vector<int> client_fds_;
};

} // namespace lxace::net
