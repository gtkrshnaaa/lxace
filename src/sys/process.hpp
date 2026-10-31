#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace lxace::sys {

struct ProcessResult {
    int exit_code{-1};
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out{false};
    int64_t duration_ms{0};
};

class ProcessRunner {
public:
    static ProcessResult execute(
        const std::string& command,
        const std::string& working_dir = "",
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
        size_t max_output_bytes = 1024 * 1024
    );

    static ProcessResult execute_args(
        const std::vector<std::string>& args,
        const std::string& working_dir = "",
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
        size_t max_output_bytes = 1024 * 1024
    );
};

} // namespace lxace::sys
