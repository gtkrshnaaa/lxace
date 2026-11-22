#include "net/request_reader.hpp"
#include <sys/socket.h>
#include <poll.h>
#include <cerrno>
#include <cctype>

namespace lxace::net {

namespace {

std::string to_lower_copy(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

bool RequestReader::find_header(const std::string& raw, size_t header_end,
                                const std::string& name, std::string& out) {
    const std::string needle = to_lower_copy(name);

    size_t line_start = raw.find("\r\n");
    if (line_start == std::string::npos) {
        return false;
    }
    line_start += 2;

    while (line_start < header_end) {
        size_t line_end = raw.find("\r\n", line_start);
        if (line_end == std::string::npos || line_end > header_end) {
            line_end = header_end;
        }

        const std::string_view line(raw.data() + line_start, line_end - line_start);
        const auto colon = line.find(':');
        if (colon != std::string_view::npos && to_lower_copy(line.substr(0, colon)) == needle) {
            std::string_view value = line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                value.remove_suffix(1);
            }
            out = std::string(value);
            return true;
        }

        line_start = line_end + 2;
    }
    return false;
}

std::string RequestReader::read_request(int client_fd) {
    std::string raw_buffer;
    char chunk[4096];

    while (raw_buffer.size() < kMaxRequestSize) {
        // The descriptor is non-blocking, so wait for readable data instead of reading
        // blindly. A short poll window means a stalled peer cannot pin a worker.
        struct pollfd pfd{};
        pfd.fd = client_fd;
        pfd.events = POLLIN;
        const int poll_status = poll(&pfd, 1, kReadTimeoutSeconds * 1000);
        if (poll_status <= 0) {
            // Timeout or poll failure: stop and let the parsed prefix (if any) be used.
            break;
        }

        const ssize_t bytes_read = recv(client_fd, chunk, sizeof(chunk), 0);
        if (bytes_read == 0) {
            // Orderly shutdown by the peer.
            break;
        }
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Spurious readiness; wait again rather than treating it as EOF.
                continue;
            }
            break;
        }
        raw_buffer.append(chunk, static_cast<size_t>(bytes_read));

        const auto header_end = raw_buffer.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            continue;
        }

        std::string length_value;
        if (!find_header(raw_buffer, header_end, "Content-Length", length_value)) {
            break;
        }

        try {
            const size_t content_len = std::stoul(length_value);
            if (content_len > kMaxRequestSize) {
                break;
            }
            if (raw_buffer.size() >= header_end + 4 + content_len) {
                break;
            }
        } catch (...) {
            break;
        }
    }

    return raw_buffer;
}

} // namespace lxace::net
