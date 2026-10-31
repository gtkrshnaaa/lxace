#pragma once

#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lxace::core {

enum class EventKind {
    Insight,
    Action,
    Message,
    CommandOutput,
    System
};

[[nodiscard]] inline std::string event_kind_to_string(EventKind kind) {
    switch (kind) {
        case EventKind::Insight: return "Insight";
        case EventKind::Action: return "Action";
        case EventKind::Message: return "Message";
        case EventKind::CommandOutput: return "CommandOutput";
        case EventKind::System: return "System";
    }
    return "Unknown";
}

struct Event {
    std::string id;
    std::string agent_id;
    EventKind kind{EventKind::System};
    std::string text;
    std::string details;
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};

    [[nodiscard]] std::string format_time() const {
        std::time_t t = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);
        std::ostringstream ss;
        ss << std::setfill('0')
           << std::setw(2) << tm_buf.tm_hour << ":"
           << std::setw(2) << tm_buf.tm_min << ":"
           << std::setw(2) << tm_buf.tm_sec;
        return ss.str();
    }

    [[nodiscard]] std::string format_cli_line() const {
        std::string time_str = format_time();
        std::ostringstream ss;
        if (!agent_id.empty() && kind == EventKind::Action) {
            ss << "[" << time_str << "] " << agent_id << ": " << text;
        } else if (kind == EventKind::Insight) {
            ss << "[" << time_str << "] Insight: " << text;
        } else if (kind == EventKind::Message) {
            ss << "[" << time_str << "] Message: " << text;
        } else if (kind == EventKind::CommandOutput) {
            ss << "[" << time_str << "] Running: " << text;
        } else {
            ss << "[" << time_str << "] " << text;
        }
        return ss.str();
    }
};

} // namespace lxace::core
