#include "test_runner.hpp"
#include "tool/tool_registry.hpp"
#include "sys/fs.hpp"
#include "json/json_parser.hpp"
#include "core/engine.hpp"
#include <filesystem>

using namespace lxace::tool;
using namespace lxace::sys;

TEST_CASE(tool_registry_schemas_and_discovery) {
    ToolRegistry registry;
    auto tools = registry.list_tools();
    ASSERT_TRUE(tools.size() >= 6);

    ASSERT_TRUE(registry.has_tool("fs_view"));
    ASSERT_TRUE(registry.has_tool("fs_replace"));
    ASSERT_TRUE(registry.has_tool("fs_find"));
    ASSERT_TRUE(registry.has_tool("fs_grep"));
    ASSERT_TRUE(registry.has_tool("proc_execute"));
    ASSERT_TRUE(registry.has_tool("sys_metrics"));

    auto schemas = registry.get_schemas();
    ASSERT_EQ(schemas.size(), tools.size());

    for (const auto& s : schemas) {
        ASSERT_TRUE(s.is_object());
        ASSERT_TRUE(s.contains("name"));
        ASSERT_TRUE(s.contains("description"));
        ASSERT_TRUE(s.contains("parameters"));
        ASSERT_TRUE(s.contains("risk_level"));
    }
}

TEST_CASE(tool_fs_view_windowing_and_bounds) {
    std::filesystem::path temp_dir = "/tmp/lxace_test_tool_fs_view";
    FsEngine::create_directories(temp_dir);
    std::filesystem::path test_file = temp_dir / "sample.txt";

    std::string content = "line 1\nline 2\nline 3\nline 4\nline 5\n";
    FsEngine::write_file(test_file, content);

    ToolRegistry registry;
    ToolExecutionContext ctx{temp_dir, nullptr};

    // View range: lines 2 to 4
    lxace::json::JsonObject args;
    args["path"] = lxace::json::JsonValue("sample.txt");
    args["start_line"] = lxace::json::JsonValue(2);
    args["end_line"] = lxace::json::JsonValue(4);

    auto res = registry.execute("fs_view", args, ctx);
    ASSERT_TRUE(res.success);
    ASSERT_TRUE(res.data.is_object());
    ASSERT_EQ(res.data["start_line"].as_double(), 2.0);
    ASSERT_EQ(res.data["total_lines"].as_double(), 5.0);

    std::string view_str = res.data["content"].as_string();
    ASSERT_TRUE(view_str.find("line 2") != std::string::npos);
    ASSERT_TRUE(view_str.find("line 3") != std::string::npos);
    ASSERT_TRUE(view_str.find("line 4") != std::string::npos);
    ASSERT_TRUE(view_str.find("line 1") == std::string::npos);

    // Path traversal check
    lxace::json::JsonObject bad_args;
    bad_args["path"] = lxace::json::JsonValue("../../etc/passwd");
    auto bad_res = registry.execute("fs_view", bad_args, ctx);
    ASSERT_FALSE(bad_res.success);
    ASSERT_TRUE(bad_res.error.find("Access denied") != std::string::npos);

    FsEngine::remove_path(temp_dir);
}

TEST_CASE(tool_fs_replace_surgical_mutation) {
    std::filesystem::path temp_dir = "/tmp/lxace_test_tool_fs_replace";
    FsEngine::create_directories(temp_dir);
    std::filesystem::path test_file = temp_dir / "code.cpp";

    std::string original = "void run() {\n    int value = 42;\n    return;\n}\n";
    FsEngine::write_file(test_file, original);

    ToolRegistry registry;
    ToolExecutionContext ctx{temp_dir, nullptr};

    // Valid unique surgical replace
    lxace::json::JsonObject args;
    args["path"] = lxace::json::JsonValue("code.cpp");
    args["target"] = lxace::json::JsonValue("int value = 42;");
    args["replacement"] = lxace::json::JsonValue("int value = 100;");

    auto res = registry.execute("fs_replace", args, ctx);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.data["replacements_count"].as_double(), 1.0);

    auto read_back = FsEngine::read_file(test_file);
    ASSERT_TRUE(read_back.has_value());
    ASSERT_TRUE(read_back->find("int value = 100;") != std::string::npos);
    ASSERT_TRUE(read_back->find("int value = 42;") == std::string::npos);

    // Target not found failure
    lxace::json::JsonObject not_found_args;
    not_found_args["path"] = lxace::json::JsonValue("code.cpp");
    not_found_args["target"] = lxace::json::JsonValue("non_existent_symbol");
    not_found_args["replacement"] = lxace::json::JsonValue("replacement");

    auto nf_res = registry.execute("fs_replace", not_found_args, ctx);
    ASSERT_FALSE(nf_res.success);
    ASSERT_TRUE(nf_res.error.find("Target string not found") != std::string::npos);

    FsEngine::remove_path(temp_dir);
}

TEST_CASE(tool_fs_find_and_grep) {
    std::filesystem::path temp_dir = "/tmp/lxace_test_tool_fs_find_grep";
    FsEngine::create_directories(temp_dir / "sub");
    FsEngine::write_file(temp_dir / "main.cpp", "int main() { return 0; }\n");
    FsEngine::write_file(temp_dir / "sub" / "helper.hpp", "// Helper header file\nclass Helper {};\n");
    FsEngine::write_file(temp_dir / "README.md", "# Documentation\nHelper notes\n");

    ToolRegistry registry;
    ToolExecutionContext ctx{temp_dir, nullptr};

    // fs_find test
    lxace::json::JsonObject find_args;
    find_args["pattern"] = lxace::json::JsonValue("*.hpp");
    auto find_res = registry.execute("fs_find", find_args, ctx);
    ASSERT_TRUE(find_res.success);
    ASSERT_EQ(find_res.data["count"].as_double(), 1.0);

    // fs_grep test
    lxace::json::JsonObject grep_args;
    grep_args["query"] = lxace::json::JsonValue("Helper");
    auto grep_res = registry.execute("fs_grep", grep_args, ctx);
    ASSERT_TRUE(grep_res.success);
    ASSERT_TRUE(grep_res.data["count"].as_double() >= 2.0);

    FsEngine::remove_path(temp_dir);
}

TEST_CASE(tool_proc_execute_and_sys_metrics) {
    ToolRegistry registry;
    ToolExecutionContext ctx{std::filesystem::current_path(), nullptr};

    // proc_execute
    lxace::json::JsonObject proc_args;
    proc_args["command"] = lxace::json::JsonValue("echo 'engine tool test'");
    auto proc_res = registry.execute("proc_execute", proc_args, ctx);
    ASSERT_TRUE(proc_res.success);
    ASSERT_EQ(proc_res.data["exit_code"].as_double(), 0.0);
    ASSERT_TRUE(proc_res.data["stdout"].as_string().find("engine tool test") != std::string::npos);

    // sys_metrics
    auto metrics_res = registry.execute("sys_metrics", {}, ctx);
    ASSERT_TRUE(metrics_res.success);
    ASSERT_TRUE(metrics_res.data["host_cpu_cores"].as_double() >= 1.0);
    ASSERT_TRUE(metrics_res.data["cpu_50_percent_cap"].as_double() >= 1.0);
}

TEST_CASE(tool_validation_and_route_integration) {
    std::filesystem::path temp_dir = std::filesystem::current_path();
    lxace::core::Engine engine(temp_dir, 9191);
    ASSERT_TRUE(engine.initialize());

    // Validation failure on missing parameter
    ToolRegistry registry;
    ToolExecutionContext ctx{temp_dir, nullptr};
    lxace::json::JsonObject empty_args;
    auto fail_res = registry.execute("fs_view", empty_args, ctx);
    ASSERT_FALSE(fail_res.success);
    ASSERT_TRUE(fail_res.error.find("Missing required parameter") != std::string::npos);

    // Calling non-existent tool
    auto nf_res = registry.execute("unregistered_tool", empty_args, ctx);
    ASSERT_FALSE(nf_res.success);
    ASSERT_TRUE(nf_res.error.find("Tool not found") != std::string::npos);
}
