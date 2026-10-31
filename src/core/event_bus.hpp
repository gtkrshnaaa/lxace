#pragma once

#include "core/event.hpp"
#include <functional>
#include <vector>
#include <mutex>
#include <memory>

namespace lxace::core {

using EventCallback = std::function<void(const Event&)>;

class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    size_t subscribe(EventCallback callback);
    void unsubscribe(size_t listener_id);
    void publish(const Event& event);

private:
    struct Listener {
        size_t id;
        EventCallback callback;
    };

    mutable std::mutex mutex_;
    std::vector<Listener> listeners_;
    size_t next_id_{1};
};

} // namespace lxace::core
