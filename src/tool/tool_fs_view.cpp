#include "tool/tool_fs_view.hpp"
#include "sys/fs.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace lxace::tool {

ToolDefinition ToolFsView::definition() const {
    ToolDefinition def;
    def.name = "fs_view";
    def.description = "View contents of a file within the project boundary with line numbering and range windowing.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject props;
    json::JsonObject path_p;
    path_p["type"] = "string";
    path_p["description"] = "Relative path to file within workspace";
    props["path"] = json::JsonValue(path_p);

    json::JsonObject start_p;
    start_p["type"] = "integer";
    start_p["description"] = "1-based starting line number (default: 1)";
    props["start_line"] = json::JsonValue(start_p);

    json::JsonObject end_p;
    end_p["type"] = "integer";
    end_p["description"] = "1-based ending line number (default: start_line + 100)";
    props["end_line"] = json::JsonValue(end_p);

    json::JsonObject max_b;
    max_b["type"] = "integer";
    max_b["description"] = "Maximum content bytes to return (default: 65536)";
    props["max_bytes"] = json::JsonValue(max_b);

    json::JsonArray req;
    req.push_back(json::JsonValue("path"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolFsView::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    auto path_it = args.find("path");
    if (path_it == args.end()) {
        return ToolResult::fail("Missing required parameter: path");
    }
    std::string rel_path = path_it->second.as_string();

    auto safe_path = sys::FsEngine::resolve_safe_path(ctx.project_dir, rel_path);
    if (!safe_path) {
        return ToolResult::fail("Access denied: path outside project directory");
    }

    if (!sys::FsEngine::exists(*safe_path) || sys::FsEngine::is_directory(*safe_path)) {
        return ToolResult::fail("File not found or is a directory: " + rel_path);
    }

    int64_t start_line = 1;
    if (auto it = args.find("start_line"); it != args.end()) {
        start_line = std::max<int64_t>(1, it->second.as_int(1));
    }

    int64_t end_line = start_line + 100;
    if (auto it = args.find("end_line"); it != args.end()) {
        end_line = std::max<int64_t>(start_line, it->second.as_int(end_line));
    }

    int64_t max_bytes = 65536;
    if (auto it = args.find("max_bytes"); it != args.end()) {
        max_bytes = std::max<int64_t>(1024, it->second.as_int(65536));
    }

    std::ifstream file(*safe_path);
    if (!file.is_open()) {
        return ToolResult::fail("Failed to open file: " + rel_path);
    }

    std::string line;
    int64_t current_line = 0;
    std::ostringstream ss;
    int64_t bytes_written = 0;
    bool truncated = false;
    int64_t lines_included = 0;

    while (std::getline(file, line)) {
        ++current_line;
        if (current_line < start_line) {
            continue;
        }
        if (current_line > end_line) {
            // Keep counting lines if needed, or break after recording total
            break;
        }

        std::ostringstream line_ss;
        line_ss << std::setw(5) << current_line << " | " << line << "\n";
        std::string formatted = line_ss.str();

        if (bytes_written + static_cast<int64_t>(formatted.size()) > max_bytes) {
            truncated = true;
            break;
        }

        ss << formatted;
        bytes_written += static_cast<int64_t>(formatted.size());
        ++lines_included;
    }

    // Fast-forward to count total lines in file if not completed
    while (std::getline(file, line)) {
        ++current_line;
    }

    json::JsonObject data;
    data["path"] = rel_path;
    data["start_line"] = static_cast<double>(start_line);
    data["end_line"] = static_cast<double>(start_line + lines_included - (lines_included > 0 ? 1 : 0));
    data["total_lines"] = static_cast<double>(current_line);
    data["content"] = ss.str();
    data["truncated"] = truncated;

    return ToolResult::ok(json::JsonValue(data), 0, truncated);
}

} // namespace lxace::tool
