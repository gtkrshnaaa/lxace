#include "tool/tool_fs_find.hpp"
#include "sys/fs.hpp"
#include <filesystem>
#include <algorithm>

namespace lxace::tool {

namespace {

bool matches_pattern(const std::string& name, const std::string& pattern) {
    if (pattern.empty()) {
        return true;
    }
    if (pattern.rfind("*.", 0) == 0) {
        std::string ext = pattern.substr(1); // e.g. ".cpp"
        if (name.length() >= ext.length()) {
            return name.compare(name.length() - ext.length(), ext.length(), ext) == 0;
        }
        return false;
    }
    if (pattern.back() == '*' && pattern.length() > 1) {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return name.rfind(prefix, 0) == 0;
    }
    return name.find(pattern) != std::string::npos;
}

bool is_ignored_dir(const std::string& dirname) {
    return dirname == ".git" || dirname == "build" || dirname == ".aicodeeditor_context_archive" ||
           dirname == ".aicodeeditor_context_window";
}

} // namespace

ToolDefinition ToolFsFind::definition() const {
    ToolDefinition def;
    def.name = "fs_find";
    def.description = "Recursively find files in project workspace matching filename pattern or extension.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject props;
    json::JsonObject path_p;
    path_p["type"] = "string";
    path_p["description"] = "Relative directory path to search within (default: root)";
    props["path"] = json::JsonValue(path_p);

    json::JsonObject pat_p;
    pat_p["type"] = "string";
    pat_p["description"] = "Filename pattern or extension to match (e.g. '*.cpp', 'test_*', or substring)";
    props["pattern"] = json::JsonValue(pat_p);

    json::JsonObject max_p;
    max_p["type"] = "integer";
    max_p["description"] = "Maximum results to return (default: 100)";
    props["max_results"] = json::JsonValue(max_p);

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolFsFind::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    std::string rel_dir = "";
    if (auto it = args.find("path"); it != args.end()) {
        rel_dir = it->second.as_string();
    }

    std::string pattern = "";
    if (auto it = args.find("pattern"); it != args.end()) {
        pattern = it->second.as_string();
    }

    int64_t max_results = 100;
    if (auto it = args.find("max_results"); it != args.end()) {
        max_results = std::max<int64_t>(1, it->second.as_int(100));
    }

    auto safe_path = sys::FsEngine::resolve_safe_path(ctx.project_dir, rel_dir);
    if (!safe_path) {
        return ToolResult::fail("Access denied: path outside project directory");
    }

    std::error_code ec;
    if (!std::filesystem::exists(*safe_path, ec) || !std::filesystem::is_directory(*safe_path, ec)) {
        return ToolResult::fail("Directory not found or is a regular file: " + rel_dir);
    }

    json::JsonArray matches;
    bool truncated = false;

    auto options = std::filesystem::directory_options::skip_permission_denied;
    for (std::filesystem::recursive_directory_iterator it(*safe_path, options, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        const auto& entry = *it;
        std::string filename = entry.path().filename().string();

        if (entry.is_directory(ec)) {
            if (is_ignored_dir(filename)) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (matches_pattern(filename, pattern)) {
            json::JsonObject match;
            auto rel = std::filesystem::relative(entry.path(), ctx.project_dir, ec);
            match["path"] = ec ? entry.path().string() : rel.string();
            match["size_bytes"] = static_cast<double>(entry.file_size(ec));
            matches.push_back(json::JsonValue(match));

            if (static_cast<int64_t>(matches.size()) >= max_results) {
                truncated = true;
                break;
            }
        }
    }

    json::JsonObject data;
    data["pattern"] = pattern;
    data["search_dir"] = rel_dir;
    data["matches"] = json::JsonValue(matches);
    data["count"] = static_cast<double>(matches.size());
    data["truncated"] = truncated;

    return ToolResult::ok(json::JsonValue(data), 0, truncated);
}

} // namespace lxace::tool
