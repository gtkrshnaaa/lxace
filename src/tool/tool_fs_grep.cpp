#include "tool/tool_fs_grep.hpp"
#include "tool/tool_fs_ignore.hpp"
#include "sys/fs.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace lxace::tool {

namespace {

bool is_text_file(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    return ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c" ||
           ext == ".md" || ext == ".txt" || ext == ".json" || ext == ".sh" ||
           ext == ".yml" || ext == ".yaml" || ext == ".cmake" ||
           path.filename() == "CMakeLists.txt" || path.filename() == "Makefile" ||
           path.filename() == "Dockerfile";
}

std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

} // namespace

ToolDefinition ToolFsGrep::definition() const {
    ToolDefinition def;
    def.name = "fs_grep";
    def.description = "Search for string or content pattern across text files in project workspace.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject props;
    json::JsonObject q_p;
    q_p["type"] = "string";
    q_p["description"] = "Substring pattern to search for";
    props["query"] = json::JsonValue(q_p);

    json::JsonObject path_p;
    path_p["type"] = "string";
    path_p["description"] = "Relative directory path to search within (default: root)";
    props["path"] = json::JsonValue(path_p);

    json::JsonObject max_p;
    max_p["type"] = "integer";
    max_p["description"] = "Maximum matches to return (default: 50)";
    props["max_matches"] = json::JsonValue(max_p);

    json::JsonObject case_p;
    case_p["type"] = "boolean";
    case_p["description"] = "Case-sensitive search (default: true)";
    props["case_sensitive"] = json::JsonValue(case_p);

    json::JsonArray req;
    req.push_back(json::JsonValue("query"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolFsGrep::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    auto q_it = args.find("query");
    if (q_it == args.end()) {
        return ToolResult::fail("Missing required parameter: query");
    }
    std::string query = q_it->second.as_string();
    if (query.empty()) {
        return ToolResult::fail("Query string cannot be empty");
    }

    std::string rel_dir = "";
    if (auto it = args.find("path"); it != args.end()) {
        rel_dir = it->second.as_string();
    }

    int64_t max_matches = 50;
    if (auto it = args.find("max_matches"); it != args.end()) {
        max_matches = std::max<int64_t>(1, it->second.as_int(50));
    }

    bool case_sensitive = true;
    if (auto it = args.find("case_sensitive"); it != args.end()) {
        case_sensitive = it->second.as_bool(true);
    }

    auto safe_path = sys::FsEngine::resolve_safe_path(ctx.project_dir, rel_dir);
    if (!safe_path) {
        return ToolResult::fail("Access denied: path outside project directory");
    }

    std::error_code ec;
    if (!std::filesystem::exists(*safe_path, ec) || !std::filesystem::is_directory(*safe_path, ec)) {
        return ToolResult::fail("Directory not found or is a regular file: " + rel_dir);
    }

    std::string query_transformed = case_sensitive ? query : to_lower(query);
    json::JsonArray matches;
    bool truncated = false;

    auto options = std::filesystem::directory_options::skip_permission_denied;
    // Testing `ec` in the loop condition aborted the whole scan on the first unreadable
    // path, which made skip_permission_denied useless. Recovery is handled in the body.
    for (std::filesystem::recursive_directory_iterator it(*safe_path, options, ec), end;
         it != end && !truncated; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        const auto& entry = *it;
        std::string filename = entry.path().filename().string();

        if (entry.is_directory(ec)) {
            if (is_ignored_directory(filename)) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!is_text_file(entry.path())) {
            continue;
        }

        std::ifstream file(entry.path());
        if (!file.is_open()) {
            continue;
        }

        std::string line;
        int64_t line_number = 0;
        auto rel_file = std::filesystem::relative(entry.path(), ctx.project_dir, ec);
        std::string rel_file_str = ec ? entry.path().string() : rel_file.string();

        while (std::getline(file, line)) {
            ++line_number;
            std::string check_line = case_sensitive ? line : to_lower(line);
            if (check_line.find(query_transformed) != std::string::npos) {
                json::JsonObject match;
                match["path"] = rel_file_str;
                match["line_number"] = static_cast<double>(line_number);
                match["line_content"] = line;
                matches.push_back(json::JsonValue(match));

                if (static_cast<int64_t>(matches.size()) >= max_matches) {
                    truncated = true;
                    break;
                }
            }
        }
    }

    json::JsonObject data;
    data["query"] = query;
    data["matches"] = json::JsonValue(matches);
    data["count"] = static_cast<double>(matches.size());
    data["truncated"] = truncated;

    return ToolResult::ok(json::JsonValue(data), 0, truncated);
}

} // namespace lxace::tool
