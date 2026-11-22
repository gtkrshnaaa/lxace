#include "tool/tool_proc_signal.hpp"
#include "sys/procfs.hpp"
#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>
#include <unistd.h>

namespace lxace::tool {

namespace {

struct SignalSpec {
    const char* name;
    int number;
};

// Deliberately narrow: only signals whose effect is observable and bounded. SIGKILL and
// SIGSTOP can strand a process or the host in an unrecoverable state, so an autonomous
// caller must escalate to SIGTERM and let the operator terminate explicitly if needed.
constexpr SignalSpec ALLOWED[] = {
    {"SIGHUP", SIGHUP},
    {"SIGINT", SIGINT},
    {"SIGTERM", SIGTERM},
    {"SIGUSR1", SIGUSR1},
    {"SIGUSR2", SIGUSR2},
    {"SIGCONT", SIGCONT},
    {"SIGSTOP", SIGSTOP},
};

const SignalSpec* find_signal(const std::string& name) {
    for (const auto& spec : ALLOWED) {
        if (name == spec.name) {
            return &spec;
        }
    }
    return nullptr;
}

} // namespace

ToolDefinition ToolProcSignal::definition() const {
    ToolDefinition def;
    def.name = "proc_signal";
    def.description =
        "Send an allowlisted signal to a process. Supported: SIGHUP, SIGINT, SIGTERM, "
        "SIGUSR1, SIGUSR2, SIGCONT, SIGSTOP. SIGKILL is intentionally not offered.";
    def.risk_level = ToolRiskLevel::Destructive;

    json::JsonObject props;

    json::JsonObject pid_p;
    pid_p["type"] = "integer";
    pid_p["description"] = "Target process id (must be positive)";
    props["pid"] = json::JsonValue(pid_p);

    json::JsonObject sig_p;
    sig_p["type"] = "string";
    sig_p["enum"] = json::JsonValue(json::JsonArray{
        json::JsonValue("SIGHUP"), json::JsonValue("SIGINT"), json::JsonValue("SIGTERM"),
        json::JsonValue("SIGUSR1"), json::JsonValue("SIGUSR2"), json::JsonValue("SIGCONT"),
        json::JsonValue("SIGSTOP")
    });
    sig_p["description"] = "Signal name from the allowlist";
    props["signal"] = json::JsonValue(sig_p);

    json::JsonArray req;
    req.push_back(json::JsonValue("pid"));
    req.push_back(json::JsonValue("signal"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolProcSignal::execute(const json::JsonObject& args, const ToolExecutionContext&) {
    if (!args.contains("pid")) {
        return ToolResult::fail("Missing required parameter: pid");
    }
    const int64_t raw_pid = args.at("pid").as_int(0);
    if (raw_pid <= 0) {
        return ToolResult::fail("pid must be a positive integer; process groups and pid 0 are refused");
    }
    const pid_t pid = static_cast<pid_t>(raw_pid);

    // Signalling the engine itself would terminate the control plane mid-request.
    if (pid == ::getpid()) {
        return ToolResult::fail("Refusing to signal the engine process itself");
    }

    if (!args.contains("signal")) {
        return ToolResult::fail("Missing required parameter: signal");
    }
    const std::string signal_name = args.at("signal").as_string();
    const SignalSpec* spec = find_signal(signal_name);
    if (spec == nullptr) {
        return ToolResult::fail(
            "Unsupported signal '" + signal_name +
            "'. Allowed: SIGHUP, SIGINT, SIGTERM, SIGUSR1, SIGUSR2, SIGCONT, SIGSTOP");
    }

    const bool was_alive = sys::ProcfsReader::is_process_alive(pid);
    if (!was_alive) {
        return ToolResult::fail("No such process: " + std::to_string(raw_pid));
    }

    if (::kill(pid, spec->number) != 0) {
        return ToolResult::fail("kill failed: " + std::string(std::strerror(errno)));
    }

    json::JsonObject data;
    data["pid"] = static_cast<double>(raw_pid);
    data["signal"] = signal_name;
    data["delivered"] = true;
    data["was_alive"] = was_alive;

    return ToolResult::ok(json::JsonValue(data));
}

} // namespace lxace::tool
