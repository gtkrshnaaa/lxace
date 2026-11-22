#include "tool/tool_fs_mutate.hpp"
#include "sys/fs.hpp"
#include <filesystem>

namespace lxace::tool {

namespace {

constexpr uint32_t MAX_MODE = 07777;

std::optional<std::filesystem::path> resolve(
    const ToolExecutionContext& ctx,
    const json::JsonObject& args,
    const char* key
) {
    auto it = args.find(key);
    if (it == args.end() || !it->second.is_string()) {
        return std::nullopt;
    }
    return sys::FsEngine::resolve_safe_path(ctx.project_dir, it->second.as_string());
}

ToolResult denied(const char* key) {
    return ToolResult::fail(std::string("Access denied: '") + key +
                            "' resolves outside the project directory or crosses a symlink");
}

} // namespace

ToolDefinition ToolFsMutate::definition() const {
    ToolDefinition def;
    def.name = "fs_mutate";
    def.description =
        "Mutate the project filesystem: remove, copy, mkdir, chmod, chown, or stat a path. "
        "Every path is resolved inside the project root and must not cross a symlink.";
    def.risk_level = ToolRiskLevel::Destructive;

    json::JsonObject props;

    json::JsonObject op_p;
    op_p["type"] = "string";
    op_p["enum"] = json::JsonValue(json::JsonArray{
        json::JsonValue("remove"), json::JsonValue("copy"), json::JsonValue("mkdir"),
        json::JsonValue("chmod"), json::JsonValue("chown"), json::JsonValue("stat")
    });
    op_p["description"] = "Operation to perform";
    props["operation"] = json::JsonValue(op_p);

    json::JsonObject path_p;
    path_p["type"] = "string";
    path_p["description"] = "Target path relative to the project root";
    props["path"] = json::JsonValue(path_p);

    json::JsonObject dst_p;
    dst_p["type"] = "string";
    dst_p["description"] = "Destination path for 'copy', relative to the project root";
    props["destination"] = json::JsonValue(dst_p);

    json::JsonObject mode_p;
    mode_p["type"] = "integer";
    mode_p["description"] = "Octal permission mode for 'chmod' (0..4095), e.g. 420 means 0644";
    props["mode"] = json::JsonValue(mode_p);

    json::JsonObject uid_p;
    uid_p["type"] = "integer";
    uid_p["description"] = "Numeric owner uid for 'chown' (use -1 to leave unchanged)";
    props["uid"] = json::JsonValue(uid_p);

    json::JsonObject gid_p;
    gid_p["type"] = "integer";
    gid_p["description"] = "Numeric group gid for 'chown' (use -1 to leave unchanged)";
    props["gid"] = json::JsonValue(gid_p);

    json::JsonArray req;
    req.push_back(json::JsonValue("operation"));
    req.push_back(json::JsonValue("path"));

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    schema["required"] = json::JsonValue(req);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolFsMutate::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    auto op_it = args.find("operation");
    if (op_it == args.end() || !op_it->second.is_string()) {
        return ToolResult::fail("Missing required string parameter: operation");
    }
    const std::string operation = op_it->second.as_string();

    auto target = resolve(ctx, args, "path");
    if (!target) {
        return denied("path");
    }

    json::JsonObject data;
    data["operation"] = operation;
    data["path"] = target->string();

    if (operation == "remove") {
        if (*target == std::filesystem::weakly_canonical(ctx.project_dir)) {
            return ToolResult::fail("Refusing to remove the project root");
        }
        if (!sys::FsEngine::exists(*target)) {
            return ToolResult::fail("Path does not exist: " + target->string());
        }
        if (!sys::FsEngine::remove_path(*target)) {
            return ToolResult::fail("Failed to remove: " + target->string());
        }
        data["removed"] = true;
        return ToolResult::ok(json::JsonValue(data));
    }

    if (operation == "copy") {
        auto dest = resolve(ctx, args, "destination");
        if (!dest) {
            return denied("destination");
        }
        if (!sys::FsEngine::exists(*target)) {
            return ToolResult::fail("Source does not exist: " + target->string());
        }
        if (!sys::FsEngine::copy_path(*target, *dest)) {
            return ToolResult::fail("Failed to copy to: " + dest->string());
        }
        data["destination"] = dest->string();
        data["copied"] = true;
        return ToolResult::ok(json::JsonValue(data));
    }

    if (operation == "mkdir") {
        // create_directories reports false when the directory already existed, which is
        // not an error for this tool, so existence is checked separately.
        const bool created = sys::FsEngine::create_directories(*target);
        if (!created && !sys::FsEngine::is_directory(*target)) {
            return ToolResult::fail("Failed to create directory: " + target->string());
        }
        data["created"] = created;
        data["is_directory"] = sys::FsEngine::is_directory(*target);
        return ToolResult::ok(json::JsonValue(data));
    }

    if (operation == "chmod") {
        auto mode_it = args.find("mode");
        if (mode_it == args.end() || !mode_it->second.is_number()) {
            return ToolResult::fail("chmod requires an integer 'mode' parameter");
        }
        const int64_t mode = mode_it->second.as_int(-1);
        if (mode < 0 || mode > static_cast<int64_t>(MAX_MODE)) {
            return ToolResult::fail("mode must be within 0..4095 (octal 0..07777)");
        }
        if (!sys::FsEngine::exists(*target)) {
            return ToolResult::fail("Path does not exist: " + target->string());
        }
        if (!sys::FsEngine::set_permissions(*target, static_cast<uint32_t>(mode))) {
            return ToolResult::fail("Failed to chmod: " + target->string());
        }
        data["mode"] = static_cast<double>(mode);
        return ToolResult::ok(json::JsonValue(data));
    }

    if (operation == "chown") {
        const int64_t uid = args.contains("uid") ? args.at("uid").as_int(-1) : -1;
        const int64_t gid = args.contains("gid") ? args.at("gid").as_int(-1) : -1;
        if (uid < -1 || gid < -1) {
            return ToolResult::fail("uid and gid must be -1 (unchanged) or a non-negative id");
        }
        if (!sys::FsEngine::exists(*target)) {
            return ToolResult::fail("Path does not exist: " + target->string());
        }
        if (!sys::FsEngine::set_owner(*target,
                                      static_cast<uint32_t>(uid),
                                      static_cast<uint32_t>(gid))) {
            return ToolResult::fail(
                "Failed to chown: " + target->string() +
                " (an unprivileged engine may only change ownership within its own uid)");
        }
        data["uid"] = static_cast<double>(uid);
        data["gid"] = static_cast<double>(gid);
        return ToolResult::ok(json::JsonValue(data));
    }

    if (operation == "stat") {
        if (!sys::FsEngine::exists(*target)) {
            return ToolResult::fail("Path does not exist: " + target->string());
        }
        auto mode = sys::FsEngine::get_permissions(*target);
        data["exists"] = true;
        data["is_directory"] = sys::FsEngine::is_directory(*target);
        data["mode"] = mode ? json::JsonValue(static_cast<double>(*mode)) : json::JsonValue(nullptr);
        return ToolResult::ok(json::JsonValue(data));
    }

    return ToolResult::fail("Unsupported operation: " + operation);
}

} // namespace lxace::tool
