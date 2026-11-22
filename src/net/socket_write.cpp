#include "net/socket_write.hpp"
#include <sys/socket.h>
#include <cerrno>

namespace lxace::net {

ssize_t send_all(int fd, const char* data, size_t length) {
    size_t written = 0;
    while (written < length) {
        const ssize_t n = send(fd, data + written, length - written, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // The descriptor carries SO_SNDTIMEO, so the kernel gives up on its own
                // rather than spinning here forever.
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }
        written += static_cast<size_t>(n);
    }
    return static_cast<ssize_t>(written);
}

} // namespace lxace::net
