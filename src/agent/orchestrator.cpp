#include "agent/orchestrator.hpp"
#include <sstream>

namespace lxace::agent {

Orchestrator::Orchestrator(std::shared_ptr<core::EventBus> event_bus)
    : event_bus_(std::move(event_bus)) {}

Orchestrator::~Orchestrator() {
    stop_all();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& w : workers_) {
        if (w && w->thread.joinable()) {
            w->thread.join();
        }
    }
    workers_.clear();
}

void Orchestrator::reap_finished_workers_locked() {
    auto it = workers_.begin();
    while (it != workers_.end()) {
        if ((*it)->done->load()) {
            if ((*it)->thread.joinable()) {
                (*it)->thread.join();
            }
            it = workers_.erase(it);
        } else {
            ++it;
        }
    }

    if (tasks_.size() > MAX_TASK_HISTORY) {
        size_t excess = tasks_.size() - MAX_TASK_HISTORY;
        tasks_.erase(tasks_.begin(), tasks_.begin() + static_cast<std::ptrdiff_t>(excess));
    }
}

std::string Orchestrator::submit_prompt(const std::string& prompt) {
    cancel_flag_ = false;

    Task task;
    std::string task_id;
    std::string agent_name;
    auto done_flag = std::make_shared<std::atomic<bool>>(false);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        reap_finished_workers_locked();

        task.id = "task-" + std::to_string(next_task_id_++);
        task.prompt = prompt;
        task.state = TaskState::Pending;
        static const std::string agent_names[] = {"Sub-agent A", "Sub-agent B", "Sub-agent C"};
        task.assigned_agent = agent_names[tasks_.size() % 3];
        tasks_.push_back(task);

        task_id = task.id;
        agent_name = task.assigned_agent;

        auto handle = std::make_unique<WorkerHandle>();
        handle->done = done_flag;
        handle->thread = std::thread([this, task_id, agent_name, done_flag]() {
            SubAgent agent(agent_name, event_bus_);
            Task local_task;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                for (auto& t : tasks_) {
                    if (t.id == task_id) {
                        local_task = t;
                        break;
                    }
                }
            }

            agent.execute_task(local_task, cancel_flag_);

            {
                std::lock_guard<std::mutex> lk(mutex_);
                for (auto& t : tasks_) {
                    if (t.id == task_id) {
                        t.state = local_task.state;
                        t.finished_at = local_task.finished_at;
                        break;
                    }
                }
            }
            done_flag->store(true);
        });

        workers_.push_back(std::move(handle));
    }

    return task_id;
}

void Orchestrator::stop_all() {
    cancel_flag_ = true;
    if (event_bus_) {
        core::Event ev1;
        ev1.kind = core::EventKind::System;
        ev1.text = "Stopping all sub-agents...";
        event_bus_->publish(ev1);

        core::Event ev2;
        ev2.kind = core::EventKind::System;
        ev2.text = "All tasks stopped.";
        event_bus_->publish(ev2);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& t : tasks_) {
        if (t.state == TaskState::Pending || t.state == TaskState::Running) {
            t.state = TaskState::Cancelled;
            t.finished_at = std::chrono::system_clock::now();
        }
    }
}

std::vector<Task> Orchestrator::get_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_;
}

bool Orchestrator::has_active_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& t : tasks_) {
        if (t.state == TaskState::Running || t.state == TaskState::Pending) {
            return true;
        }
    }
    return false;
}

} // namespace lxace::agent
