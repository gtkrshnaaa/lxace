#include "tool/tool_net_probe.hpp"
#include "sys/network.hpp"
#include <string>

namespace lxace::tool {

ToolDefinition ToolNetProbe::definition() const {
    ToolDefinition def;
    def.name = "net_probe";
    def.description =
        "Probe network state: check whether a local port is bound, or test TCP "
        "reachability of a host and port with a bounded timeout.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject props;

    json::JsonObject op_p;
    op_p["type"] = "string";
    op_p["enum"] = json::JsonValue(json::JsonArray{
        json::JsonValue("port_in_use"), json::JsonValue("tcp_connect")
    });
    op_p["description"] = "Probe to run";
    props["operation"] = json::JsonValue(op_p);

    json::JsonObject port_p;
    port_p["type"] = "integer";
    port_p["description"] = "Target port (1..65535)";
    props["port"] = json::JsonValue(port_p);

    json::JsonObject host_p;
    host_p["type"] = "string";
    host_p["description"] = "Hostname or address for 'tcp_connect' (default 127.0.0.1)";
    props["host"] = json::JsonValue(host_p);

    json::JsonObject timeout_p;
    timeout_p["type"] = "integer";
    timeout_p["description"] = "Connect timeout in milliseconds (default 2000, max 10000)";
    props["timeout_ms"] = json::JsonValue(timeout_p);

    json::JsonArray req;
    req.push_back(json::JsonValue("operation"));
    req.push_back(json::JsonValue("port"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolNetProbe::execute(const json::JsonObject& args, const ToolExecutionContext&) {
    auto op_it = args.find("operation");
    if (op_it == args.end() || !op_it->second.is_string()) {
        return ToolResult::fail("Missing required string parameter: operation");
    }
    const std::string operation = op_it->second.as_string();

    if (!args.contains("port")) {
        return ToolResult::fail("Missing required parameter: port");
    }
    const int64_t port = args.at("port").as_int(0);
    if (port < 1 || port > 65535) {
        return ToolResult::fail("port must be within 1..65535");
    }

    json::JsonObject data;
    data["operation"] = operation;
    data["port"] = static_cast<double>(port);

    if (operation == "port_in_use") {
        data["in_use"] = sys::NetworkUtils::is_port_in_use(static_cast<uint16_t>(port));
        return ToolResult::ok(json::JsonValue(data));
    }

    if (operation == "tcp_connect") {
        std::string host = "127.0.0.1";
        if (args.contains("host")) {
            const std::string requested = args.at("host").as_string();
            if (!requested.empty()) {
                host = requested;
            }
        }

        int64_t timeout_ms = 2000;
        if (args.contains("timeout_ms")) {
            const int64_t requested = args.at("timeout_ms").as_int(2000);
            if (requested < 100) {
                return ToolResult::fail("timeout_ms must be at least 100");
            }
            timeout_ms = std::min<int64_t>(requested, 10000);
        }

        const bool reachable = sys::NetworkUtils::check_tcp_connection(
            host, static_cast<uint16_t>(port), std::chrono::milliseconds(timeout_ms));

        data["host"] = host;
        data["timeout_ms"] = static_cast<double>(timeout_ms);
        data["reachable"] = reachable;
        return ToolResult::ok(json::JsonValue(data));
    }

    return ToolResult::fail("Unsupported operation: " + operation);
}

} // namespace lxace::tool
