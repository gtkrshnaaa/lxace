#include "tool/tool_fs_watch.hpp"
#include "sys/fs.hpp"
#include "sys/watcher.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <vector>

namespace lxace::tool {

namespace {

const char* kind_to_string(sys::WatchEventKind kind) {
    switch (kind) {
        case sys::WatchEventKind::Created: return "created";
        case sys::WatchEventKind::Modified: return "modified";
        case sys::WatchEventKind::Deleted: return "deleted";
    }
    return "unknown";
}

} // namespace

ToolDefinition ToolFsWatch::definition() const {
    ToolDefinition def;
    def.name = "fs_watch";
    def.description =
        "Observe a project directory for a bounded window and return the filesystem events "
        "collected. Blocks for at most timeout_ms, so it always terminates.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject props;

    json::JsonObject path_p;
    path_p["type"] = "string";
    path_p["description"] = "Directory to watch, relative to the project root (default: root)";
    props["path"] = json::JsonValue(path_p);

    json::JsonObject timeout_p;
    timeout_p["type"] = "integer";
    timeout_p["description"] = "Observation window in milliseconds (default 2000, max 30000)";
    props["timeout_ms"] = json::JsonValue(timeout_p);

    json::JsonObject max_p;
    max_p["type"] = "integer";
    max_p["description"] = "Stop early after this many events (default 100, max 1000)";
    props["max_events"] = json::JsonValue(max_p);

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolFsWatch::execute(const json::JsonObject& args, const ToolExecutionContext& ctx) {
    std::string rel = "";
    if (args.contains("path")) {
        rel = args.at("path").as_string();
    }

    int64_t timeout_ms = 2000;
    if (args.contains("timeout_ms")) {
        const int64_t requested = args.at("timeout_ms").as_int(2000);
        if (requested < 1) {
            return ToolResult::fail("timeout_ms must be at least 1");
        }
        timeout_ms = std::min<int64_t>(requested, 30000);
    }

    int64_t max_events = 100;
    if (args.contains("max_events")) {
        const int64_t requested = args.at("max_events").as_int(100);
        if (requested < 1) {
            return ToolResult::fail("max_events must be at least 1");
        }
        max_events = std::min<int64_t>(requested, 1000);
    }

    auto target = sys::FsEngine::resolve_safe_path(ctx.project_dir, rel);
    if (!target) {
        return ToolResult::fail("Access denied: path resolves outside the project directory");
    }
    if (!sys::FsEngine::is_directory(*target)) {
        return ToolResult::fail("Watch target is not a directory: " + target->string());
    }

    sys::FsWatcher watcher;
    if (!watcher.add_watch(*target)) {
        return ToolResult::fail("Failed to register an inotify watch on: " + target->string());
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::vector<sys::WatchEvent> events;
    bool done = false;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const size_t cap = static_cast<size_t>(max_events);

    watcher.start([&](const sys::WatchEvent& event) {
        std::lock_guard<std::mutex> lock(mtx);
        if (done || events.size() >= cap) {
            return;
        }
        events.push_back(event);
        if (events.size() >= cap) {
            done = true;
            cv.notify_all();
        }
    });

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_until(lock, deadline, [&] { return done; });
        done = true;
    }

    // stop() joins the worker thread, so no callback can run after this returns and no
    // captured reference can dangle.
    watcher.stop();

    json::JsonArray out;
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& event : events) {
            json::JsonObject item;
            item["kind"] = kind_to_string(event.kind);
            item["filename"] = event.filename;
            item["path"] = (target->lexically_normal() / event.filename).string();
            out.push_back(json::JsonValue(item));
        }
    }

    json::JsonObject data;
    data["watched_path"] = target->string();
    data["events"] = json::JsonValue(out);
    data["count"] = static_cast<double>(out.size());
    data["timeout_ms"] = static_cast<double>(timeout_ms);
    data["max_events"] = static_cast<double>(max_events);

    return ToolResult::ok(json::JsonValue(data));
}

} // namespace lxace::tool
