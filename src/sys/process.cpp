#include "sys/process.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <csignal>
#include <vector>
#include <chrono>

namespace lxace::sys {

ProcessResult ProcessRunner::execute(
    const std::string& command,
    const std::string& working_dir,
    std::chrono::milliseconds timeout,
    size_t max_output_bytes
) {
    std::vector<std::string> args = {"/bin/sh", "-c", command};
    return execute_args(args, working_dir, timeout, max_output_bytes);
}

ProcessResult ProcessRunner::execute_args(
    const std::vector<std::string>& args,
    const std::string& working_dir,
    std::chrono::milliseconds timeout,
    size_t max_output_bytes
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
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

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
            kill(pid, SIGKILL);
            break;
        }

        int remaining_ms = static_cast<int>((timeout - elapsed).count());
        if (remaining_ms <= 0) remaining_ms = 1;
        if (remaining_ms > 100) remaining_ms = 100;

        int ret = poll(fds, 2, remaining_ms);
        if (ret < 0) break;

        if (stdout_open && (fds[0].revents & (POLLIN | POLLHUP))) {
            ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                if (result.stdout_output.size() < max_output_bytes) {
                    size_t to_append = std::min<size_t>(static_cast<size_t>(n), max_output_bytes - result.stdout_output.size());
                    result.stdout_output.append(buffer, to_append);
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
                }
            } else if (n == 0 || (fds[1].revents & POLLHUP)) {
                stderr_open = false;
            }
        }
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_code = 128 + WTERMSIG(status);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    return result;
}

} // namespace lxace::sys
