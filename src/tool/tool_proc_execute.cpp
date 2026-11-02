#include "tool/tool_proc_execute.hpp"
#include "sys/process.hpp"

namespace lxace::tool {

ToolDefinition ToolProcExecute::definition() const {
    ToolDefinition def;
    def.name = "proc_execute";
    def.description = "Execute a command with timeout and resource limits within the project workspace.";
    def.risk_level = ToolRiskLevel::Mutating;

    json::JsonObject props;
    json::JsonObject cmd_p;
    cmd_p["type"] = "string";
    cmd_p["description"] = "Command line string to execute";
    props["command"] = json::JsonValue(cmd_p);

    json::JsonObject to_p;
    to_p["type"] = "integer";
    to_p["description"] = "Timeout in milliseconds (default: 30000)";
    props["timeout_ms"] = json::JsonValue(to_p);

    json::JsonArray req;
    req.push_back(json::JsonValue("command"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolProcExecute::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    auto it = args.find("command");
    if (it == args.end()) {
        return ToolResult::fail("Missing required parameter: command");
    }
    std::string cmd = it->second.as_string();
    if (cmd.empty()) {
        return ToolResult::fail("Command string cannot be empty");
    }

    int64_t timeout_ms = 30000;
    if (auto to_it = args.find("timeout_ms"); to_it != args.end()) {
        timeout_ms = std::clamp<int64_t>(to_it->second.as_int(30000), 100, 300000);
    }

    if (ctx.event_bus) {
        core::Event ev;
        ev.kind = core::EventKind::CommandOutput;
        ev.text = cmd;
        ctx.event_bus->publish(ev);
    }

    auto result = sys::ProcessRunner::execute(
        cmd,
        ctx.project_dir.string(),
        std::chrono::milliseconds(timeout_ms)
    );

    json::JsonObject data;
    data["exit_code"] = static_cast<double>(result.exit_code);
    data["stdout"] = result.stdout_output;
    data["stderr"] = result.stderr_output;
    data["timed_out"] = result.timed_out;
    data["duration_ms"] = static_cast<double>(result.duration_ms);

    return ToolResult::ok(json::JsonValue(data), result.duration_ms);
}

} // namespace lxace::tool
