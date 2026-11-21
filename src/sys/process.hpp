#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace lxace::sys {

struct ResourceLimits {
    bool enabled{true};
    // RLIMIT_NPROC headroom: how many additional tasks this uid may create beyond the
    // inherited soft limit. Expressed as headroom because RLIMIT_NPROC counts all tasks
    // owned by the uid, so an absolute value breaks ordinary commands on a busy desktop.
    uint64_t max_processes{256};
    // RLIMIT_AS: caps address space so a memory hog cannot OOM the host.
    uint64_t max_address_space_bytes{1024ULL * 1024ULL * 1024ULL};
    // RLIMIT_NOFILE: caps open descriptors.
    uint64_t max_open_files{1024};
    // RLIMIT_CPU: wall-clock-independent CPU seconds ceiling.
    uint64_t max_cpu_seconds{60};
    // RLIMIT_FSIZE: caps a single file write so a runaway command cannot fill the disk.
    uint64_t max_file_size_bytes{512ULL * 1024ULL * 1024ULL};
};

struct ProcessResult {
    int exit_code{-1};
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out{false};
    // Set when captured bytes exceeded the ceiling, so callers can never confuse
    // complete output with silently clamped output.
    bool truncated_stdout{false};
    bool truncated_stderr{false};
    int64_t duration_ms{0};
};

class ProcessRunner {
public:
    static ProcessResult execute(
        const std::string& command,
        const std::string& working_dir = "",
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
        size_t max_output_bytes = 1024 * 1024,
        const ResourceLimits& limits = ResourceLimits{}
    );

    static ProcessResult execute_args(
        const std::vector<std::string>& args,
        const std::string& working_dir = "",
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
        size_t max_output_bytes = 1024 * 1024,
        const ResourceLimits& limits = ResourceLimits{}
    );

    [[nodiscard]] static ResourceLimits default_limits();
};

} // namespace lxace::sys
