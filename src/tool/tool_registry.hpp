#pragma once

#include "tool/tool_definition.hpp"
#include <map>
#include <vector>
#include <memory>
#include <mutex>

namespace lxace::tool {

class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry() = default;

    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    void register_tool(std::shared_ptr<Tool> tool);
    [[nodiscard]] std::shared_ptr<Tool> get_tool(const std::string& name) const;
    [[nodiscard]] std::vector<ToolDefinition> list_tools() const;
    [[nodiscard]] json::JsonArray get_schemas() const;
    [[nodiscard]] bool has_tool(const std::string& name) const;

    ToolResult execute(
        const std::string& name,
        const json::JsonObject& args,
        const ToolExecutionContext& ctx
    );

    void register_default_tools();

private:
    [[nodiscard]] std::string validate_arguments(
        const ToolDefinition& def,
        const json::JsonObject& args
    ) const;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Tool>> tools_;
};

} // namespace lxace::tool
