#include "test_runner.hpp"
#include "sys/process.hpp"
#include "sys/procfs.hpp"

using namespace lxace::sys;

TEST_CASE(process_simple_echo) {
    auto res = ProcessRunner::execute("echo 'lxace runner'");
    ASSERT_EQ(res.exit_code, 0);
    ASSERT_FALSE(res.timed_out);
    ASSERT_TRUE(res.stdout_output.find("lxace runner") != std::string::npos);
}

TEST_CASE(process_exit_code_propagation) {
    auto res = ProcessRunner::execute("exit 42");
    ASSERT_EQ(res.exit_code, 42);
    ASSERT_FALSE(res.timed_out);
}

TEST_CASE(process_timeout_enforcement) {
    auto res = ProcessRunner::execute("sleep 2", "", std::chrono::milliseconds(150));
    ASSERT_TRUE(res.timed_out);
}

TEST_CASE(procfs_resource_50_percent_caps) {
    uint32_t total_cpus = ProcfsReader::get_cpu_cores();
    ASSERT_TRUE(total_cpus >= 1);

    uint32_t cpu_cap = ProcfsReader::get_50_percent_cpu_cap();
    ASSERT_TRUE(cpu_cap >= 1);
    ASSERT_TRUE(cpu_cap <= total_cpus);

    uint64_t mem_cap = ProcfsReader::get_50_percent_memory_cap();
    ASSERT_TRUE(mem_cap > 0);
}
