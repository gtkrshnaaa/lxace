#pragma once

#include "agent/task.hpp"
#include "core/event_bus.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <filesystem>

namespace lxace::tool { class ToolRegistry; }

namespace lxace::agent {

class SubAgent {
public:
    SubAgent(std::string name, std::shared_ptr<core::EventBus> event_bus,
             std::shared_ptr<tool::ToolRegistry> tool_registry = nullptr,
             std::filesystem::path project_dir = "");
    ~SubAgent() = default;

    SubAgent(const SubAgent&) = delete;
    SubAgent& operator=(const SubAgent&) = delete;

    [[nodiscard]] const std::string& name() const { return name_; }

    void emit_action(const std::string& text, const std::string& details = "");
    void emit_insight(const std::string& text, const std::string& details = "");
    void emit_message(const std::string& text, const std::string& details = "");
    void emit_command(const std::string& command);

    // Executes the task and records the terminal state on `task` (Succeeded, Failed, or
    // Cancelled). State is the single source of truth, so callers never have to reconcile
    // a separate boolean with it. cancel_token is per task, so cancelling one task never
    // clears another task's signal.
    void execute_task(Task& task, const std::atomic<bool>& cancel_token);

private:
    std::string name_;
    std::shared_ptr<core::EventBus> event_bus_;
    std::shared_ptr<tool::ToolRegistry> tool_registry_;
    std::filesystem::path project_dir_;
};

} // namespace lxace::agent
