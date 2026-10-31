#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <filesystem>

namespace lxace::sys {

enum class WatchEventKind {
    Created,
    Modified,
    Deleted
};

struct WatchEvent {
    WatchEventKind kind;
    std::string path;
    std::string filename;
};

using WatchCallback = std::function<void(const WatchEvent&)>;

class FsWatcher {
public:
    FsWatcher();
    ~FsWatcher();

    FsWatcher(const FsWatcher&) = delete;
    FsWatcher& operator=(const FsWatcher&) = delete;

    bool add_watch(const std::filesystem::path& path);
    void start(WatchCallback callback);
    void stop();

    [[nodiscard]] bool is_running() const { return running_; }

private:
    void watch_loop();

    int inotify_fd_{-1};
    std::vector<int> watch_descriptors_;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    WatchCallback callback_;
};

} // namespace lxace::sys
