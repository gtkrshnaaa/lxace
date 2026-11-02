#include "agent/sub_agent.hpp"
#include "tool/tool_registry.hpp"
#include <thread>
#include <chrono>
#include <algorithm>

namespace lxace::agent {

SubAgent::SubAgent(std::string name, std::shared_ptr<core::EventBus> event_bus,
                   std::shared_ptr<tool::ToolRegistry> tool_registry,
                   std::filesystem::path project_dir)
    : name_(std::move(name)),
      event_bus_(std::move(event_bus)),
      tool_registry_(std::move(tool_registry)),
      project_dir_(std::move(project_dir)) {}

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

    if (tool_registry_) {
        tool::ToolExecutionContext ctx{project_dir_, event_bus_};
        auto metrics_res = tool_registry_->execute("sys_metrics", {}, ctx);
        if (metrics_res.success && metrics_res.data.is_object()) {
            double cpus = metrics_res.data["cpu_50_percent_cap"].as_double();
            emit_insight("Operating under 50% CPU allocation cap: " + std::to_string(static_cast<int>(cpus)) + " cores.");
        }

        if (cancel_flag) {
            task.state = TaskState::Cancelled;
            return;
        }

        std::string lower_prompt = task.prompt;
        for (char& c : lower_prompt) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        if (lower_prompt.find("find") != std::string::npos || lower_prompt.find("files") != std::string::npos) {
            json::JsonObject find_args;
            find_args["max_results"] = json::JsonValue(10);
            auto find_res = tool_registry_->execute("fs_find", find_args, ctx);
            if (find_res.success) {
                emit_action("Located workspace files via native fs_find.");
            }
        } else if (lower_prompt.find("search") != std::string::npos || lower_prompt.find("grep") != std::string::npos) {
            json::JsonObject grep_args;
            grep_args["query"] = json::JsonValue("Engine");
            grep_args["max_matches"] = json::JsonValue(5);
            auto grep_res = tool_registry_->execute("fs_grep", grep_args, ctx);
            if (grep_res.success) {
                emit_action("Scanned workspace patterns via native fs_grep.");
            }
        }
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (cancel_flag) {
        task.state = TaskState::Cancelled;
        return;
    }

    emit_message("Completed native tool execution for task: " + task.prompt);
    task.state = TaskState::Succeeded;
    task.finished_at = std::chrono::system_clock::now();
}

} // namespace lxace::agent
