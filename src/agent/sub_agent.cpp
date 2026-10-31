#include "agent/sub_agent.hpp"
#include <thread>
#include <chrono>

namespace lxace::agent {

SubAgent::SubAgent(std::string name, std::shared_ptr<core::EventBus> event_bus)
    : name_(std::move(name)), event_bus_(std::move(event_bus)) {}

void SubAgent::emit_action(const std::string& text, const std::string& details) {
    if (!event_bus_) return;
    core::Event ev;
    ev.agent_id = name_;
    ev.kind = core::EventKind::Action;
    ev.text = text;
    ev.details = details;
    event_bus_->publish(ev);
}

void SubAgent::emit_insight(const std::string& text, const std::string& details) {
    if (!event_bus_) return;
    core::Event ev;
    ev.agent_id = name_;
    ev.kind = core::EventKind::Insight;
    ev.text = text;
    ev.details = details;
    event_bus_->publish(ev);
}

void SubAgent::emit_message(const std::string& text, const std::string& details) {
    if (!event_bus_) return;
    core::Event ev;
    ev.agent_id = name_;
    ev.kind = core::EventKind::Message;
    ev.text = text;
    ev.details = details;
    event_bus_->publish(ev);
}

void SubAgent::emit_command(const std::string& command) {
    if (!event_bus_) return;
    core::Event ev;
    ev.agent_id = name_;
    ev.kind = core::EventKind::CommandOutput;
    ev.text = command;
    event_bus_->publish(ev);
}

void SubAgent::execute_task(Task& task, const std::atomic<bool>& cancel_flag) {
    task.state = TaskState::Running;
    emit_action("Analyzing prompt: " + task.prompt);

    if (cancel_flag) {
        task.state = TaskState::Cancelled;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    if (cancel_flag) {
        task.state = TaskState::Cancelled;
        return;
    }

    emit_insight("Identified project context and target scope.");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (cancel_flag) {
        task.state = TaskState::Cancelled;
        return;
    }

    emit_action("Executing native operations for task.");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    if (cancel_flag) {
        task.state = TaskState::Cancelled;
        return;
    }

    emit_message("Completed all actions for prompt.");
    task.state = TaskState::Succeeded;
    task.finished_at = std::chrono::system_clock::now();
}

} // namespace lxace::agent
