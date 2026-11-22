#ifndef LXACE_NET_SOCKET_WRITE_HPP
#define LXACE_NET_SOCKET_WRITE_HPP

#include <string>

namespace lxace::net {

// Sends the whole buffer, resuming across partial writes and EINTR. A single send()
// call can return short on a non-blocking descriptor or once the socket buffer fills,
// which silently truncated every response larger than the buffer before the caller
// closed the socket. Returns the number of bytes written, or -1 on error.
ssize_t send_all(int fd, const char* data, size_t length);

inline ssize_t send_all(int fd, const std::string& payload) {
    return send_all(fd, payload.data(), payload.size());
}

} // namespace lxace::net

#endif
