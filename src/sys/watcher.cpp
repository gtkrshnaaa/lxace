#include "sys/watcher.hpp"
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>

namespace lxace::sys {

FsWatcher::FsWatcher() {
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
}

FsWatcher::~FsWatcher() {
    stop();
    if (inotify_fd_ >= 0) {
        for (int wd : watch_descriptors_) {
            inotify_rm_watch(inotify_fd_, wd);
        }
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
}

bool FsWatcher::add_watch(const std::filesystem::path& path) {
    if (inotify_fd_ < 0) return false;
    uint32_t mask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
    int wd = inotify_add_watch(inotify_fd_, path.c_str(), mask);
    if (wd < 0) return false;
    watch_descriptors_.push_back(wd);
    return true;
}

void FsWatcher::start(WatchCallback callback) {
    if (running_ || inotify_fd_ < 0) return;
    callback_ = std::move(callback);
    running_ = true;
    worker_thread_ = std::thread(&FsWatcher::watch_loop, this);
}

void FsWatcher::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void FsWatcher::watch_loop() {
    constexpr size_t BUF_LEN = 4096;
    alignas(struct inotify_event) char buffer[BUF_LEN];

    struct pollfd pfd{};
    pfd.fd = inotify_fd_;
    pfd.events = POLLIN;

    while (running_) {
        int poll_ret = poll(&pfd, 1, 200);
        if (poll_ret <= 0) continue;

        ssize_t bytes_read = read(inotify_fd_, buffer, BUF_LEN);
        if (bytes_read <= 0) continue;

        ssize_t i = 0;
        while (i < bytes_read) {
            auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            if (event->len > 0) {
                WatchEvent we;
                we.filename = event->name;
                if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
                    we.kind = WatchEventKind::Created;
                } else if (event->mask & IN_MODIFY) {
                    we.kind = WatchEventKind::Modified;
                } else if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
                    we.kind = WatchEventKind::Deleted;
                }

                if (callback_) {
                    callback_(we);
                }
            }
            i += static_cast<ssize_t>(sizeof(struct inotify_event) + event->len);
        }
    }
}

} // namespace lxace::sys
