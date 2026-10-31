#pragma once

#include <sys/epoll.h>
#include <vector>
#include <chrono>

namespace lxace::net {

class EpollReactor {
public:
    explicit EpollReactor(int max_events = 64);
    ~EpollReactor();

    EpollReactor(const EpollReactor&) = delete;
    EpollReactor& operator=(const EpollReactor&) = delete;

    bool add_fd(int fd, uint32_t events);
    bool modify_fd(int fd, uint32_t events);
    bool remove_fd(int fd);

    int wait(std::vector<epoll_event>& out_events, std::chrono::milliseconds timeout);

    [[nodiscard]] int fd() const { return epoll_fd_; }
    [[nodiscard]] bool is_valid() const { return epoll_fd_ >= 0; }

private:
    int epoll_fd_{-1};
    int max_events_;
};

} // namespace lxace::net
