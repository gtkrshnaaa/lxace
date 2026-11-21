#include "sys/process.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <csignal>
#include <cerrno>
#include <vector>
#include <chrono>

namespace lxace::sys {

namespace {

void apply_child_limits(const ResourceLimits& limits) {
    if (!limits.enabled) {
        return;
    }

    auto set_limit = [](int resource, uint64_t value) {
        if (value == 0) return;
        struct rlimit rl{};
        rl.rlim_cur = static_cast<rlim_t>(value);
        rl.rlim_max = static_cast<rlim_t>(value);
        // Best effort: an unprivileged process cannot raise a hard limit, and some
        // kernels reject particular resources in restricted namespaces.
        setrlimit(resource, &rl);
    };

    // RLIMIT_NPROC counts every task owned by the real uid, threads included, and it is
    // per-uid rather than per-process-tree. Setting an absolute ceiling therefore fails
    // every fork as soon as the user already runs more tasks than the ceiling (a desktop
    // editor alone holds hundreds of threads). Instead, take a fixed headroom below the
    // inherited soft limit, which still bounds a fork bomb without breaking normal use.
    auto set_process_headroom = [](uint64_t headroom) {
        if (headroom == 0) return;
        struct rlimit current{};
        if (getrlimit(RLIMIT_NPROC, &current) != 0) return;
        if (current.rlim_cur == RLIM_INFINITY || current.rlim_cur <= headroom) return;
        struct rlimit rl{};
        rl.rlim_cur = current.rlim_cur - headroom;
        rl.rlim_max = current.rlim_max;
        if (rl.rlim_cur > rl.rlim_max) rl.rlim_cur = rl.rlim_max;
        setrlimit(RLIMIT_NPROC, &rl);
    };

    set_process_headroom(limits.max_processes);
    set_limit(RLIMIT_AS, limits.max_address_space_bytes);
    set_limit(RLIMIT_NOFILE, limits.max_open_files);
    set_limit(RLIMIT_CPU, limits.max_cpu_seconds);
    set_limit(RLIMIT_FSIZE, limits.max_file_size_bytes);
    set_limit(RLIMIT_CORE, 0);
}

} // namespace

ResourceLimits ProcessRunner::default_limits() {
    return ResourceLimits{};
}

ProcessResult ProcessRunner::execute(
    const std::string& command,
    const std::string& working_dir,
    std::chrono::milliseconds timeout,
    size_t max_output_bytes,
    const ResourceLimits& limits
) {
    std::vector<std::string> args = {"/bin/sh", "-c", command};
    return execute_args(args, working_dir, timeout, max_output_bytes, limits);
}

ProcessResult ProcessRunner::execute_args(
    const std::vector<std::string>& args,
    const std::string& working_dir,
    std::chrono::milliseconds timeout,
    size_t max_output_bytes,
    const ResourceLimits& limits
) {
    ProcessResult result;
    if (args.empty()) {
        result.exit_code = -1;
        return result;
    }

    auto start_time = std::chrono::steady_clock::now();

    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe2(stdout_pipe, O_CLOEXEC) != 0) return result;
    if (pipe2(stderr_pipe, O_CLOEXEC) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child process - create dedicated process group
        setpgid(0, 0);

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Bound the child before it can exhaust host resources. Applied after the
        // descriptor plumbing so the limits cannot block our own dup2 calls.
        apply_child_limits(limits);

        if (!working_dir.empty()) {
            if (chdir(working_dir.c_str()) != 0) {
                _exit(127);
            }
        }

        std::vector<char*> c_args;
        c_args.reserve(args.size() + 1);
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    struct pollfd fds[2];
    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN | POLLHUP;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN | POLLHUP;

    bool stdout_open = true;
    bool stderr_open = true;
    char buffer[4096];

    while (stdout_open || stderr_open) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        );
        if (elapsed >= timeout) {
            result.timed_out = true;
            // Send SIGKILL to the entire process group
            kill(-pid, SIGKILL);
            break;
        }

        int remaining_ms = static_cast<int>((timeout - elapsed).count());
        if (remaining_ms <= 0) remaining_ms = 1;
        if (remaining_ms > 100) remaining_ms = 100;

        int ret = poll(fds, 2, remaining_ms);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (stdout_open && (fds[0].revents & (POLLIN | POLLHUP))) {
            ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                if (result.stdout_output.size() < max_output_bytes) {
                    size_t to_append = std::min<size_t>(static_cast<size_t>(n), max_output_bytes - result.stdout_output.size());
                    result.stdout_output.append(buffer, to_append);
                    if (to_append < static_cast<size_t>(n)) {
                        result.truncated_stdout = true;
                    }
                } else {
                    result.truncated_stdout = true;
                }
            } else if (n == 0 || (fds[0].revents & POLLHUP)) {
                stdout_open = false;
            }
        }

        if (stderr_open && (fds[1].revents & (POLLIN | POLLHUP))) {
            ssize_t n = read(stderr_pipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                if (result.stderr_output.size() < max_output_bytes) {
                    size_t to_append = std::min<size_t>(static_cast<size_t>(n), max_output_bytes - result.stderr_output.size());
                    result.stderr_output.append(buffer, to_append);
                    if (to_append < static_cast<size_t>(n)) {
                        result.truncated_stderr = true;
                    }
                } else {
                    result.truncated_stderr = true;
                }
            } else if (n == 0 || (fds[1].revents & POLLHUP)) {
                stderr_open = false;
            }
        }
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    // Retry on EINTR. A single interrupted waitpid returned -1 with status untouched,
    // which made WIFEXITED(0) true and reported exit code 0 for a command that never ran.
    int status = 0;
    pid_t waited = 0;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited < 0) {
        // Unknown status is a distinct signal to the caller, never "success".
        result.exit_code = -1;
    } else if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    } else {
        result.exit_code = -1;
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    return result;
}

} // namespace lxace::sys
