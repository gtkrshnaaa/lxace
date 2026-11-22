#include "tool/tool_proc_list.hpp"
#include "sys/procfs.hpp"
#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <vector>

namespace lxace::tool {

namespace {

struct Row {
    pid_t pid{0};
    std::string state;
    uint64_t rss{0};
    uint64_t vsize{0};
};

bool parse_pid(const char* name, pid_t& pid) {
    if (name == nullptr || *name == '\0') {
        return false;
    }
    for (const char* p = name; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    errno = 0;
    const long value = std::strtol(name, nullptr, 10);
    if (errno != 0 || value <= 0) {
        return false;
    }
    pid = static_cast<pid_t>(value);
    return true;
}

} // namespace

ToolDefinition ToolProcList::definition() const {
    ToolDefinition def;
    def.name = "proc_list";
    def.description =
        "List live processes with state, resident memory, and virtual memory, ordered by "
        "resident memory descending. Use this to identify resource-hungry processes.";
    def.risk_level = ToolRiskLevel::ReadOnly;

    json::JsonObject props;

    json::JsonObject limit_p;
    limit_p["type"] = "integer";
    limit_p["description"] = "Maximum rows to return (default 20, clamped to 1..500)";
    props["limit"] = json::JsonValue(limit_p);

    json::JsonObject pid_p;
    pid_p["type"] = "integer";
    pid_p["description"] = "Optional single pid to inspect instead of a full listing";
    props["pid"] = json::JsonValue(pid_p);

    json::JsonObject schema;
    schema["type"] = "object";
    schema["properties"] = json::JsonValue(props);
    def.parameter_schema = schema;

    return def;
}

ToolResult ToolProcList::execute(const json::JsonObject& args, const ToolExecutionContext&) {
    if (args.contains("pid")) {
        const int64_t raw = args.at("pid").as_int(0);
        if (raw <= 0) {
            return ToolResult::fail("pid must be a positive integer");
        }
        auto metrics = sys::ProcfsReader::get_process_metrics(static_cast<pid_t>(raw));
        if (!metrics) {
            return ToolResult::fail("No such process or metrics unreadable: " + std::to_string(raw));
        }
        json::JsonObject proc;
        proc["pid"] = static_cast<double>(metrics->pid);
        proc["state"] = metrics->state;
        proc["resident_memory_bytes"] = static_cast<double>(metrics->resident_memory_bytes);
        proc["virtual_memory_bytes"] = static_cast<double>(metrics->virtual_memory_bytes);
        proc["user_time_ticks"] = static_cast<double>(metrics->user_time_ticks);
        proc["kernel_time_ticks"] = static_cast<double>(metrics->kernel_time_ticks);

        json::JsonObject data;
        data["process"] = json::JsonValue(proc);
        return ToolResult::ok(json::JsonValue(data));
    }

    int64_t limit = 20;
    if (args.contains("limit")) {
        const int64_t requested = args.at("limit").as_int(20);
        if (requested < 1) {
            return ToolResult::fail("limit must be at least 1");
        }
        limit = std::min<int64_t>(requested, 500);
    }

    DIR* dir = ::opendir("/proc");
    if (dir == nullptr) {
        return ToolResult::fail("Failed to open /proc");
    }

    std::vector<Row> rows;
    while (const dirent* entry = ::readdir(dir)) {
        pid_t pid = 0;
        if (!parse_pid(entry->d_name, pid)) {
            continue;
        }
        auto metrics = sys::ProcfsReader::get_process_metrics(pid);
        if (!metrics) {
            continue;
        }
        rows.push_back(Row{pid, metrics->state, metrics->resident_memory_bytes,
                           metrics->virtual_memory_bytes});
    }
    ::closedir(dir);

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.rss != b.rss) {
            return a.rss > b.rss;
        }
        return a.pid < b.pid;
    });

    const size_t emit = std::min<size_t>(rows.size(), static_cast<size_t>(limit));
    json::JsonArray out;
    for (size_t i = 0; i < emit; ++i) {
        json::JsonObject proc;
        proc["pid"] = static_cast<double>(rows[i].pid);
        proc["state"] = rows[i].state;
        proc["resident_memory_bytes"] = static_cast<double>(rows[i].rss);
        proc["virtual_memory_bytes"] = static_cast<double>(rows[i].vsize);
        out.push_back(json::JsonValue(proc));
    }

    json::JsonObject data;
    data["processes"] = json::JsonValue(out);
    data["count"] = static_cast<double>(emit);
    data["total_scanned"] = static_cast<double>(rows.size());
    data["truncated"] = rows.size() > emit;

    return ToolResult::ok(json::JsonValue(data), 0, rows.size() > emit);
}

} // namespace lxace::tool
