#include "test_runner.hpp"
#include "sys/process.hpp"
#include "sys/procfs.hpp"
#include <unistd.h>

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

TEST_CASE(process_privilege_drop_is_inert_by_default) {
    // The default limits must never alter the child identity, otherwise an unprivileged
    // engine would lose its own uid merely by running a command.
    ResourceLimits limits;
    ASSERT_EQ(limits.drop_uid, 0u);
    ASSERT_EQ(limits.drop_gid, 0u);

    auto res = ProcessRunner::execute("id -u", "", std::chrono::milliseconds(5000), 4096, limits);
    ASSERT_EQ(res.exit_code, 0);

    auto expected = ProcessRunner::execute("id -u", "", std::chrono::milliseconds(5000), 4096,
                                           ResourceLimits{});
    ASSERT_EQ(expected.exit_code, 0);
    ASSERT_TRUE(res.stdout_output == expected.stdout_output);
}

TEST_CASE(process_privilege_drop_refuses_when_not_permitted) {
    // A non-root engine cannot chown itself to another uid. The child must refuse to exec
    // rather than silently continuing with full privileges, so the run reports 126.
    if (geteuid() == 0) {
        // Running as root, where the drop would genuinely succeed; nothing to assert here.
        return;
    }

    ResourceLimits limits;
    limits.drop_uid = 10001;
    limits.drop_gid = 10001;

    auto res = ProcessRunner::execute("echo should-not-run", "", std::chrono::milliseconds(5000),
                                      4096, limits);
    ASSERT_EQ(res.exit_code, 126);
    ASSERT_TRUE(res.stdout_output.find("should-not-run") == std::string::npos);
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
