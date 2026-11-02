#include "tool/tool_sys_metrics.hpp"
#include "sys/procfs.hpp"

namespace lxace::tool {

ToolDefinition ToolSysMetrics::definition() const {
    ToolDefinition def;
    def.name = "sys_metrics";
    def.description = "Inspect host CPU cores, memory limits, and the active 50% resource cap.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(json::JsonObject{});
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolSysMetrics::execute(const json::JsonObject&, const ToolExecutionContext&) {
    uint32_t total_cpus = sys::ProcfsReader::get_cpu_cores();
    uint32_t cpu_50_cap = sys::ProcfsReader::get_50_percent_cpu_cap();
    auto sys_mem = sys::ProcfsReader::get_system_memory();
    uint64_t total_mem = sys_mem ? sys_mem->total_bytes : 0;
    uint64_t mem_50_cap = sys::ProcfsReader::get_50_percent_memory_cap();

    json::JsonObject data;
    data["host_cpu_cores"] = static_cast<double>(total_cpus);
    data["cpu_50_percent_cap"] = static_cast<double>(cpu_50_cap);
    data["host_total_memory_bytes"] = static_cast<double>(total_mem);
    data["memory_50_percent_cap_bytes"] = static_cast<double>(mem_50_cap);

    return ToolResult::ok(json::JsonValue(data));
}

} // namespace lxace::tool
