#pragma once

#include "tool/tool_definition.hpp"

namespace lxace::tool {

class ToolFsReplace : public Tool {
public:
    [[nodiscard]] ToolDefinition definition() const override;
    ToolResult execute(const json::JsonObject& args, const ToolExecutionContext& ctx) override;
};

} // namespace lxace::tool
