#include "sys/procfs.hpp"
#include <fstream>
#include <sstream>
#include <csignal>

namespace lxace::sys {

bool ProcfsReader::is_process_alive(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

std::optional<ProcessMetrics> ProcfsReader::get_process_metrics(pid_t pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    ProcessMetrics metrics;
    metrics.pid = pid;

    // Fields in /proc/[pid]/stat:
    // pid (comm) state ppid pgrp session tty_nr tpgid flags minflt cminflt majflt cmajflt utime stime ... vsize rss
    std::string comm;
    int ppid, pgrp, session, tty_nr, tpgid;
    unsigned int flags;
    uint64_t minflt, cminflt, majflt, cmajflt;

    file >> metrics.pid >> comm >> metrics.state
         >> ppid >> pgrp >> session >> tty_nr >> tpgid
         >> flags >> minflt >> cminflt >> majflt >> cmajflt
         >> metrics.user_time_ticks >> metrics.kernel_time_ticks;

    // Skip to vsize (field 23) and rss (field 24)
    int64_t cutime, cstime, priority, nice, num_threads, itrealvalue;
    unsigned long long starttime;
    uint64_t vsize;
    int64_t rss;

    file >> cutime >> cstime >> priority >> nice >> num_threads >> itrealvalue
         >> starttime >> vsize >> rss;

    if (!file.fail()) {
        metrics.virtual_memory_bytes = vsize;
        long page_size = sysconf(_SC_PAGESIZE);
        metrics.resident_memory_bytes = static_cast<uint64_t>(rss > 0 ? rss * page_size : 0);
        return metrics;
    }
    return std::nullopt;
}

std::optional<SystemMemoryInfo> ProcfsReader::get_system_memory() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return std::nullopt;
    }

    SystemMemoryInfo info;
    std::string line;
    int found = 0;

    while (std::getline(file, line) && found < 3) {
        std::istringstream iss(line);
        std::string key;
        uint64_t val = 0;
        std::string unit;

        if (iss >> key >> val >> unit) {
            uint64_t bytes = val * 1024;
            if (key == "MemTotal:") {
                info.total_bytes = bytes;
                ++found;
            } else if (key == "MemFree:") {
                info.free_bytes = bytes;
                ++found;
            } else if (key == "MemAvailable:") {
                info.available_bytes = bytes;
                ++found;
            }
        }
    }

    if (found >= 2) {
        return info;
    }
    return std::nullopt;
}

uint32_t ProcfsReader::get_cpu_cores() {
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (cores > 0) ? static_cast<uint32_t>(cores) : 1;
}

uint32_t ProcfsReader::get_50_percent_cpu_cap() {
    uint32_t cores = get_cpu_cores();
    return std::max<uint32_t>(1, cores / 2);
}

uint64_t ProcfsReader::get_50_percent_memory_cap() {
    auto mem = get_system_memory();
    return mem ? (mem->total_bytes / 2) : 0;
}

} // namespace lxace::sys
