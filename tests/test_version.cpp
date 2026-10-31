#include "test_runner.hpp"
#include "core/version.hpp"

using namespace lxace::core;

TEST_CASE(version_constants) {
    ASSERT_TRUE(!VERSION.empty());
    ASSERT_EQ(VERSION, "1.0.0");
    ASSERT_EQ(BUILD_ARCH, "Linux x86_64");
    ASSERT_EQ(CXX_STANDARD, "C++20");
}

TEST_CASE(version_format_strings) {
    std::string full = format_version();
    ASSERT_TRUE(full.find("lxace version 1.0.0") != std::string::npos);
    ASSERT_TRUE(full.find("Linux x86_64") != std::string::npos);
    ASSERT_TRUE(full.find("C++20") != std::string::npos);

    std::string short_ver = format_version_short();
    ASSERT_EQ(short_ver, "1.0.0");
}
