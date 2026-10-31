#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <unistd.h>

namespace lxace::sys {

struct ProcessMetrics {
    pid_t pid{0};
    std::string state;
    uint64_t virtual_memory_bytes{0};
    uint64_t resident_memory_bytes{0};
    uint64_t user_time_ticks{0};
    uint64_t kernel_time_ticks{0};
};

struct SystemMemoryInfo {
    uint64_t total_bytes{0};
    uint64_t free_bytes{0};
    uint64_t available_bytes{0};
};

class ProcfsReader {
public:
    static std::optional<ProcessMetrics> get_process_metrics(pid_t pid);
    static std::optional<SystemMemoryInfo> get_system_memory();
    static bool is_process_alive(pid_t pid);
};

} // namespace lxace::sys
