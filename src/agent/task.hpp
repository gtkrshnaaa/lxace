#pragma once

#include <string>
#include <chrono>

namespace lxace::agent {

enum class TaskState {
    Pending,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

[[nodiscard]] inline std::string task_state_to_string(TaskState state) {
    switch (state) {
        case TaskState::Pending: return "Pending";
        case TaskState::Running: return "Running";
        case TaskState::Succeeded: return "Succeeded";
        case TaskState::Failed: return "Failed";
        case TaskState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

struct Task {
    std::string id;
    std::string prompt;
    TaskState state{TaskState::Pending};
    std::string assigned_agent{"Sub-agent A"};
    std::string result_summary;
    std::chrono::system_clock::time_point created_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point finished_at{};
};

} // namespace lxace::agent
