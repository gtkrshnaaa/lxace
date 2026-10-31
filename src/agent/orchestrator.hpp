#pragma once

#include "agent/task.hpp"
#include "agent/sub_agent.hpp"
#include "core/event_bus.hpp"
#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>

namespace lxace::agent {

class Orchestrator {
public:
    explicit Orchestrator(std::shared_ptr<core::EventBus> event_bus);
    ~Orchestrator();

    Orchestrator(const Orchestrator&) = delete;
    Orchestrator& operator=(const Orchestrator&) = delete;

    std::string submit_prompt(const std::string& prompt);
    void stop_all();

    [[nodiscard]] std::vector<Task> get_tasks() const;
    [[nodiscard]] bool has_active_tasks() const;

private:
    std::shared_ptr<core::EventBus> event_bus_;
    mutable std::mutex mutex_;
    std::vector<Task> tasks_;
    std::atomic<bool> cancel_flag_{false};
    std::vector<std::thread> worker_threads_;
    size_t next_task_id_{1};
};

} // namespace lxace::agent
