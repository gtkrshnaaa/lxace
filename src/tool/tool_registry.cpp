#include "tool/tool_registry.hpp"
#include "tool/tool_fs_view.hpp"
#include "tool/tool_fs_replace.hpp"
#include "tool/tool_fs_find.hpp"
#include "tool/tool_fs_grep.hpp"
#include "tool/tool_proc_execute.hpp"
#include "tool/tool_sys_metrics.hpp"
#include <chrono>

namespace lxace::tool {

ToolRegistry::ToolRegistry() {
    register_default_tools();
}

void ToolRegistry::register_default_tools() {
    register_tool(std::make_shared<ToolFsView>());
    register_tool(std::make_shared<ToolFsReplace>());
    register_tool(std::make_shared<ToolFsFind>());
    register_tool(std::make_shared<ToolFsGrep>());
    register_tool(std::make_shared<ToolProcExecute>());
    register_tool(std::make_shared<ToolSysMetrics>());
}

void ToolRegistry::register_tool(std::shared_ptr<Tool> tool) {
    if (!tool) return;
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[tool->definition().name] = std::move(tool);
}

std::shared_ptr<Tool> ToolRegistry::get_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ToolRegistry::has_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.find(name) != tools_.end();
}

std::vector<ToolDefinition> ToolRegistry::list_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolDefinition> list;
    list.reserve(tools_.size());
    for (const auto& [name, tool] : tools_) {
        list.push_back(tool->definition());
    }
    return list;
}

json::JsonArray ToolRegistry::get_schemas() const {
    std::lock_guard<std::mutex> lock(mutex_);
    json::JsonArray arr;
    for (const auto& [name, tool] : tools_) {
        arr.push_back(json::JsonValue(tool->definition().to_json()));
    }
    return arr;
}

std::string ToolRegistry::validate_arguments(
    const ToolDefinition& def,
    const json::JsonObject& args
) const {
    auto req_it = def.parameter_schema.find("required");
    if (req_it != def.parameter_schema.end() && req_it->second.is_array()) {
        for (const auto& field_val : req_it->second.as_array()) {
            if (field_val.is_string()) {
                const std::string& req_key = field_val.as_string();
                if (args.find(req_key) == args.end()) {
                    return "Missing required parameter: " + req_key;
                }
            }
        }
    }
    return "";
}

ToolResult ToolRegistry::execute(
    const std::string& name,
    const json::JsonObject& args,
    const ToolExecutionContext& ctx
) {
    std::shared_ptr<Tool> tool;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        if (it == tools_.end()) {
            return ToolResult::fail("Tool not found: " + name);
        }
        tool = it->second;
    }

    auto validation_err = validate_arguments(tool->definition(), args);
    if (!validation_err.empty()) {
        return ToolResult::fail(validation_err);
    }

    auto start_time = std::chrono::steady_clock::now();
    auto result = tool->execute(args, ctx);
    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (ctx.event_bus) {
        core::Event ev;
        ev.kind = core::EventKind::Action;
        ev.agent_id = "ToolRunner";
        ev.text = "Tool " + name + (result.success ? " succeeded" : " failed: " + result.error);
        ctx.event_bus->publish(ev);
    }

    return result;
}

} // namespace lxace::tool
