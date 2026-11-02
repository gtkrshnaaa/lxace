#pragma once

#include "json/json_value.hpp"
#include "core/event_bus.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <chrono>

namespace lxace::tool {

enum class ToolRiskLevel {
    ReadOnly,
    Mutating,
    Destructive
};

[[nodiscard]] inline std::string tool_risk_to_string(ToolRiskLevel risk) {
    switch (risk) {
        case ToolRiskLevel::ReadOnly: return "read-only";
        case ToolRiskLevel::Mutating: return "mutating";
        case ToolRiskLevel::Destructive: return "destructive";
    }
    return "read-only";
}

struct ToolDefinition {
    std::string name;
    std::string description;
    ToolRiskLevel risk_level{ToolRiskLevel::ReadOnly};
    json::JsonObject parameter_schema;

    [[nodiscard]] json::JsonObject to_json() const {
        json::JsonObject obj;
        obj["name"] = name;
        obj["description"] = description;
        obj["risk_level"] = tool_risk_to_string(risk_level);
        obj["parameters"] = json::JsonValue(parameter_schema);
        return obj;
    }
};

struct ToolResult {
    bool success{false};
    json::JsonValue data;
    std::string error;
    int64_t duration_ms{0};
    bool truncated{false};

    [[nodiscard]] json::JsonObject to_json() const {
        json::JsonObject obj;
        obj["success"] = success;
        obj["data"] = data;
        obj["error"] = error;
        obj["duration_ms"] = static_cast<double>(duration_ms);
        obj["truncated"] = truncated;
        return obj;
    }

    static ToolResult ok(json::JsonValue data, int64_t duration_ms = 0, bool truncated = false) {
        ToolResult res;
        res.success = true;
        res.data = std::move(data);
        res.duration_ms = duration_ms;
        res.truncated = truncated;
        return res;
    }

    static ToolResult fail(std::string err, int64_t duration_ms = 0) {
        ToolResult res;
        res.success = false;
        res.error = std::move(err);
        res.duration_ms = duration_ms;
        return res;
    }
};

struct ToolExecutionContext {
    std::filesystem::path project_dir;
    std::shared_ptr<core::EventBus> event_bus;
};

class Tool {
public:
    virtual ~Tool() = default;
    [[nodiscard]] virtual ToolDefinition definition() const = 0;
    virtual ToolResult execute(const json::JsonObject& args, const ToolExecutionContext& ctx) = 0;
};

} // namespace lxace::tool
