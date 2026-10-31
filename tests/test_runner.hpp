#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <chrono>

namespace lxace::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void register_test(std::string name, std::function<void()> fn) {
        tests_.push_back({std::move(name), std::move(fn)});
    }

    int run_all() {
        int passed = 0;
        int failed = 0;
        auto start = std::chrono::steady_clock::now();

        std::cout << "Running " << tests_.size() << " tests...\n";

        for (const auto& test : tests_) {
            try {
                test.fn();
                std::cout << "  [PASS] " << test.name << "\n";
                ++passed;
            } catch (const std::exception& e) {
                std::cout << "  [FAIL] " << test.name << ": " << e.what() << "\n";
                ++failed;
            } catch (...) {
                std::cout << "  [FAIL] " << test.name << ": Unknown exception\n";
                ++failed;
            }
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start
        ).count();

        std::cout << "\nResults: " << passed << " passed, " << failed << " failed in "
                  << duration << " ms.\n";

        return failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

#define TEST_CASE(name) \
    static void test_func_##name(); \
    namespace { \
        struct AutoReg_##name { \
            AutoReg_##name() { \
                ::lxace::test::TestRegistry::instance().register_test(#name, test_func_##name); \
            } \
        } auto_reg_##name; \
    } \
    static void test_func_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error("Assertion failed: " #expr " at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            throw std::runtime_error("Assertion failed: " #a " == " #b " at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            throw std::runtime_error("Assertion failed: " #a " != " #b " at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

} // namespace lxace::test
