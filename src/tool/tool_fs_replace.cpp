#include "tool/tool_fs_replace.hpp"
#include "sys/fs.hpp"

namespace lxace::tool {

ToolDefinition ToolFsReplace::definition() const {
    ToolDefinition def;
    def.name = "fs_replace";
    def.description = "Surgically replace an exact target string block with replacement content in a file. Enforces unique matching by default to prevent accidental mutations.";
    def.risk_level = ToolRiskLevel::Mutating;

    json::JsonObject props;
    json::JsonObject path_p;
    path_p["type"] = "string";
    path_p["description"] = "Relative path to target file within workspace";
    props["path"] = json::JsonValue(path_p);

    json::JsonObject target_p;
    target_p["type"] = "string";
    target_p["description"] = "Exact text block to replace";
    props["target"] = json::JsonValue(target_p);

    json::JsonObject repl_p;
    repl_p["type"] = "string";
    repl_p["description"] = "Exact text block to insert";
    props["replacement"] = json::JsonValue(repl_p);

    json::JsonObject mult_p;
    mult_p["type"] = "boolean";
    mult_p["description"] = "Allow replacing multiple occurrences (default: false)";
    props["allow_multiple"] = json::JsonValue(mult_p);

    json::JsonArray req;
    req.push_back(json::JsonValue("path"));
    req.push_back(json::JsonValue("target"));
    req.push_back(json::JsonValue("replacement"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolFsReplace::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    auto path_it = args.find("path");
    auto target_it = args.find("target");
    auto repl_it = args.find("replacement");

    if (path_it == args.end() || target_it == args.end() || repl_it == args.end()) {
        return ToolResult::fail("Missing required parameters (path, target, replacement)");
    }

    std::string rel_path = path_it->second.as_string();
    std::string target = target_it->second.as_string();
    std::string replacement = repl_it->second.as_string();

    if (target.empty()) {
        return ToolResult::fail("Target string cannot be empty");
    }

    bool allow_multiple = false;
    if (auto it = args.find("allow_multiple"); it != args.end()) {
        allow_multiple = it->second.as_bool(false);
    }

    auto safe_path = sys::FsEngine::resolve_safe_path(ctx.project_dir, rel_path);
    if (!safe_path) {
        return ToolResult::fail("Access denied: path outside project directory");
    }

    auto content_opt = sys::FsEngine::read_file(*safe_path);
    if (!content_opt) {
        return ToolResult::fail("File not found or unreadable: " + rel_path);
    }

    std::string content = *content_opt;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find(target, pos)) != std::string::npos) {
        ++count;
        pos += target.length();
    }

    if (count == 0) {
        return ToolResult::fail("Target string not found in " + rel_path);
    }

    if (count > 1 && !allow_multiple) {
        return ToolResult::fail("Target string matches " + std::to_string(count) +
                               " times in " + rel_path + ". Must be unique unless allow_multiple is true.");
    }

    // Perform replacement
    std::string updated;
    updated.reserve(content.size() + (replacement.size() > target.size() ? (replacement.size() - target.size()) * count : 0));
    pos = 0;
    size_t last_pos = 0;
    size_t replaced_count = 0;

    while ((pos = content.find(target, last_pos)) != std::string::npos) {
        updated.append(content, last_pos, pos - last_pos);
        updated.append(replacement);
        last_pos = pos + target.length();
        ++replaced_count;
        if (!allow_multiple) {
            break;
        }
    }
    updated.append(content, last_pos, content.length() - last_pos);

    if (!sys::FsEngine::write_file(*safe_path, updated, false)) {
        return ToolResult::fail("Failed to write updated file: " + rel_path);
    }

    json::JsonObject data;
    data["path"] = rel_path;
    data["replacements_count"] = static_cast<double>(replaced_count);
    data["new_size_bytes"] = static_cast<double>(updated.size());

    return ToolResult::ok(json::JsonValue(data));
}

} // namespace lxace::tool
