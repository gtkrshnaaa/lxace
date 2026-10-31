#include "core/event_bus.hpp"
#include <algorithm>

namespace lxace::core {

size_t EventBus::subscribe(EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t id = next_id_++;
    listeners_.push_back(Listener{id, std::move(callback)});
    return id;
}

void EventBus::unsubscribe(size_t listener_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
                       [listener_id](const Listener& l) { return l.id == listener_id; }),
        listeners_.end());
}

void EventBus::publish(const Event& event) {
    std::vector<EventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks.reserve(listeners_.size());
        for (const auto& listener : listeners_) {
            callbacks.push_back(listener.callback);
        }
    }
    for (const auto& cb : callbacks) {
        if (cb) {
            cb(event);
        }
    }
}

} // namespace lxace::core
