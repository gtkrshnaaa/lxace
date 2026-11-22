#include "test_runner.hpp"
#include "tool/tool_registry.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace lxace;

namespace {

namespace fs = std::filesystem;

fs::path make_workspace(const std::string& name) {
    auto dir = fs::temp_directory_path() / ("lxace_cap_" + name);
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

json::JsonValue string_arg(const char* value) {
    return json::JsonValue(std::string(value));
}

json::JsonValue int_arg(int64_t value) {
    return json::JsonValue(value);
}

} // namespace

TEST_CASE(capability_tools_are_registered) {
    lxace::tool::ToolRegistry registry;

    ASSERT_TRUE(registry.has_tool("fs_mutate"));
    ASSERT_TRUE(registry.has_tool("fs_watch"));
    ASSERT_TRUE(registry.has_tool("proc_list"));
    ASSERT_TRUE(registry.has_tool("proc_signal"));
    ASSERT_TRUE(registry.has_tool("net_probe"));

    const auto definitions = registry.list_tools();
    ASSERT_TRUE(definitions.size() >= 11);
}

TEST_CASE(capability_risk_levels_gate_destructive_tools) {
    lxace::tool::ToolRegistry registry;

    auto mutate = registry.get_tool("fs_mutate")->definition();
    auto signal_tool = registry.get_tool("proc_signal")->definition();
    auto probe = registry.get_tool("net_probe")->definition();
    auto list = registry.get_tool("proc_list")->definition();

    ASSERT_TRUE(mutate.risk_level == lxace::tool::ToolRiskLevel::Destructive);
    ASSERT_TRUE(signal_tool.risk_level == lxace::tool::ToolRiskLevel::Destructive);
    ASSERT_TRUE(probe.risk_level == lxace::tool::ToolRiskLevel::ReadOnly);
    ASSERT_TRUE(list.risk_level == lxace::tool::ToolRiskLevel::ReadOnly);
}

TEST_CASE(fs_mutate_creates_and_removes_within_workspace) {
    const auto dir = make_workspace("mutate");
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;
    ctx.project_dir = dir;

    json::JsonObject mkdir_args;
    mkdir_args["operation"] = string_arg("mkdir");
    mkdir_args["path"] = string_arg("nested/deep");
    auto created = registry.execute("fs_mutate", mkdir_args, ctx);
    ASSERT_TRUE(created.success);
    ASSERT_TRUE(fs::is_directory(dir / "nested/deep"));

    json::JsonObject remove_args;
    remove_args["operation"] = string_arg("remove");
    remove_args["path"] = string_arg("nested");
    auto removed = registry.execute("fs_mutate", remove_args, ctx);
    ASSERT_TRUE(removed.success);
    ASSERT_FALSE(fs::exists(dir / "nested"));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE(fs_mutate_rejects_escape_and_root_removal) {
    const auto dir = make_workspace("mutate_guard");
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;
    ctx.project_dir = dir;

    json::JsonObject escape;
    escape["operation"] = string_arg("remove");
    escape["path"] = string_arg("../../etc/passwd");
    auto denied = registry.execute("fs_mutate", escape, ctx);
    ASSERT_FALSE(denied.success);

    json::JsonObject root;
    root["operation"] = string_arg("remove");
    root["path"] = string_arg(".");
    auto refused = registry.execute("fs_mutate", root, ctx);
    ASSERT_FALSE(refused.success);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE(fs_mutate_chmod_rejects_out_of_range_mode) {
    const auto dir = make_workspace("mutate_mode");
    {
        std::ofstream file(dir / "target.txt");
        file << "x";
    }

    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;
    ctx.project_dir = dir;

    json::JsonObject bad;
    bad["operation"] = string_arg("chmod");
    bad["path"] = string_arg("target.txt");
    bad["mode"] = int_arg(65535);
    auto rejected = registry.execute("fs_mutate", bad, ctx);
    ASSERT_FALSE(rejected.success);

    json::JsonObject good;
    good["operation"] = string_arg("chmod");
    good["path"] = string_arg("target.txt");
    good["mode"] = int_arg(0600);
    auto applied = registry.execute("fs_mutate", good, ctx);
    ASSERT_TRUE(applied.success);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE(proc_list_returns_processes_ordered_by_memory) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["limit"] = int_arg(5);
    auto result = registry.execute("proc_list", args, ctx);
    ASSERT_TRUE(result.success);

    const auto& data = result.data.as_object();
    ASSERT_TRUE(data.at("count").as_int() >= 1);
    ASSERT_TRUE(data.at("processes").is_array());

    const auto& rows = data.at("processes").as_array();
    for (size_t i = 1; i < rows.size(); ++i) {
        const auto prev = rows[i - 1].as_object().at("resident_memory_bytes").as_double();
        const auto curr = rows[i].as_object().at("resident_memory_bytes").as_double();
        ASSERT_TRUE(prev >= curr);
    }
}

TEST_CASE(proc_list_rejects_non_positive_pid) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["pid"] = int_arg(0);
    auto result = registry.execute("proc_list", args, ctx);
    ASSERT_FALSE(result.success);
}

TEST_CASE(proc_signal_refuses_self_and_kill) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject self;
    self["pid"] = int_arg(static_cast<int64_t>(::getpid()));
    self["signal"] = string_arg("SIGTERM");
    auto refused = registry.execute("proc_signal", self, ctx);
    ASSERT_FALSE(refused.success);

    // SIGKILL is rejected by the schema enum gate before the tool body runs, which is the
    // earlier of the two defenses. The assertion checks the rejection names the parameter
    // rather than pinning the message to one specific layer.
    json::JsonObject kill_attempt;
    kill_attempt["pid"] = int_arg(1);
    kill_attempt["signal"] = string_arg("SIGKILL");
    auto blocked = registry.execute("proc_signal", kill_attempt, ctx);
    ASSERT_FALSE(blocked.success);
    ASSERT_TRUE(blocked.error.find("signal") != std::string::npos);
}

TEST_CASE(proc_signal_rejects_unknown_pid) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["pid"] = int_arg(999999);
    args["signal"] = string_arg("SIGTERM");
    auto result = registry.execute("proc_signal", args, ctx);
    ASSERT_FALSE(result.success);
}

TEST_CASE(net_probe_reports_port_state) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["operation"] = string_arg("port_in_use");
    args["port"] = int_arg(1);
    auto result = registry.execute("net_probe", args, ctx);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.data.as_object().count("in_use") == 1);
}

TEST_CASE(net_probe_rejects_out_of_range_port) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["operation"] = string_arg("port_in_use");
    args["port"] = int_arg(70000);
    auto result = registry.execute("net_probe", args, ctx);
    ASSERT_FALSE(result.success);
}

TEST_CASE(fs_watch_returns_within_timeout) {
    const auto dir = make_workspace("watch");
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;
    ctx.project_dir = dir;

    json::JsonObject args;
    args["timeout_ms"] = int_arg(150);
    auto result = registry.execute("fs_watch", args, ctx);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.data.as_object().at("events").is_array());

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE(fs_watch_rejects_escape) {
    const auto dir = make_workspace("watch_guard");
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;
    ctx.project_dir = dir;

    json::JsonObject args;
    args["path"] = string_arg("../..");
    args["timeout_ms"] = int_arg(50);
    auto result = registry.execute("fs_watch", args, ctx);
    ASSERT_FALSE(result.success);

    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE(validation_rejects_wrong_argument_type) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["command"] = int_arg(42);
    auto result = registry.execute("proc_execute", args, ctx);
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error.find("must be of type string") != std::string::npos);
}

TEST_CASE(validation_rejects_non_integer_for_integer_param) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["operation"] = string_arg("port_in_use");
    args["port"] = json::JsonValue(1.5);
    auto result = registry.execute("net_probe", args, ctx);
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error.find("must be of type integer") != std::string::npos);
}

TEST_CASE(validation_rejects_value_outside_enum) {
    lxace::tool::ToolRegistry registry;
    lxace::tool::ToolExecutionContext ctx;

    json::JsonObject args;
    args["operation"] = string_arg("obliterate");
    args["port"] = int_arg(80);
    auto result = registry.execute("net_probe", args, ctx);
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.error.find("must be one of") != std::string::npos);
}
