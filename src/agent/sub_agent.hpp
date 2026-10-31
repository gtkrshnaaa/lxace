#pragma once

#include "agent/task.hpp"
#include "core/event_bus.hpp"
#include <string>
#include <memory>
#include <atomic>
#include <functional>

namespace lxace::agent {

class SubAgent {
public:
    SubAgent(std::string name, std::shared_ptr<core::EventBus> event_bus);
    ~SubAgent() = default;

    SubAgent(const SubAgent&) = delete;
    SubAgent& operator=(const SubAgent&) = delete;

    [[nodiscard]] const std::string& name() const { return name_; }

    void emit_action(const std::string& text, const std::string& details = "");
    void emit_insight(const std::string& text, const std::string& details = "");
    void emit_message(const std::string& text, const std::string& details = "");
    void emit_command(const std::string& command);

    void execute_task(Task& task, const std::atomic<bool>& cancel_flag);

private:
    std::string name_;
    std::shared_ptr<core::EventBus> event_bus_;
};

} // namespace lxace::agent
