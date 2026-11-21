#include "agent/orchestrator.hpp"
#include <sstream>

namespace lxace::agent {

Orchestrator::Orchestrator(
    std::shared_ptr<core::EventBus> event_bus,
    std::shared_ptr<tool::ToolRegistry> tool_registry,
    std::filesystem::path project_dir
) : event_bus_(std::move(event_bus)),
    tool_registry_(std::move(tool_registry)),
    project_dir_(std::move(project_dir)) {}

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
        const size_t excess = tasks_.size() - MAX_TASK_HISTORY;
        tasks_.erase(tasks_.begin(), tasks_.begin() + static_cast<std::ptrdiff_t>(excess));
    }
}

std::string Orchestrator::submit_prompt(const std::string& prompt) {
    auto done_flag = std::make_shared<std::atomic<bool>>(false);
    auto cancel_token = std::make_shared<std::atomic<bool>>(false);

    std::lock_guard<std::mutex> lock(mutex_);
    reap_finished_workers_locked();

    auto task = std::make_shared<Task>();
    task->id = "task-" + std::to_string(next_task_id_++);
    task->prompt = prompt;
    task->state = TaskState::Pending;
    static const std::string agent_names[] = {"Sub-agent A", "Sub-agent B", "Sub-agent C"};
    task->assigned_agent = agent_names[(next_task_id_ - 1) % 3];

    std::string task_id = task->id;
    std::string agent_name = task->assigned_agent;
    tasks_.push_back(task);

    auto handle = std::make_unique<WorkerHandle>();
    handle->task_id = task_id;
    handle->done = done_flag;
    handle->cancel = cancel_token;
    // The worker owns a shared_ptr to its own task, so trimming the history deque can
    // never destroy a task that is still executing.
    handle->thread = std::thread([this, task, agent_name, done_flag, cancel_token]() {
        SubAgent agent(agent_name, event_bus_, tool_registry_, project_dir_);
        agent.execute_task(*task, *cancel_token);
        done_flag->store(true);
    });

    workers_.push_back(std::move(handle));

    // Enforce the history bound after insertion. Trimming only before the push left the
    // deque one entry over the ceiling, and running tasks are safe to evict because each
    // worker holds its own shared_ptr.
    if (tasks_.size() > MAX_TASK_HISTORY) {
        const size_t excess = tasks_.size() - MAX_TASK_HISTORY;
        tasks_.erase(tasks_.begin(), tasks_.begin() + static_cast<std::ptrdiff_t>(excess));
    }

    return task_id;
}

void Orchestrator::stop_all() {
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
    // Set every live token. Tokens are never cleared here, so a later submit_prompt
    // cannot resurrect a cancelled task.
    for (auto& w : workers_) {
        if (w && w->cancel) {
            w->cancel->store(true);
        }
    }
    for (auto& t : tasks_) {
        if (t->state == TaskState::Pending || t->state == TaskState::Running) {
            t->state = TaskState::Cancelled;
            t->finished_at = std::chrono::system_clock::now();
        }
    }
}

std::vector<Task> Orchestrator::get_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Task> snapshot;
    snapshot.reserve(tasks_.size());
    for (const auto& t : tasks_) {
        snapshot.push_back(*t);
    }
    return snapshot;
}

bool Orchestrator::has_active_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& t : tasks_) {
        if (t->state == TaskState::Running || t->state == TaskState::Pending) {
            return true;
        }
    }
    return false;
}

} // namespace lxace::agent
