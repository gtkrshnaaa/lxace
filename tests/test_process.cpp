#include "test_runner.hpp"
#include "sys/process.hpp"

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
