#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace lxace::net {

// Reads one HTTP request from a connected descriptor, honoring Content-Length with a
// case-insensitive header lookup (RFC 9110). Returns an empty string when the peer
// closes or the request exceeds the size ceiling.
//
// Accepted client descriptors are non-blocking, so EAGAIN is a normal "body not here
// yet" signal rather than end-of-stream. Treating it as EOF truncated POST bodies and
// closed the connection with no response at all.
class RequestReader {
public:
    static constexpr size_t kMaxRequestSize = 10 * 1024 * 1024;
    static constexpr int kReadTimeoutSeconds = 5;

    [[nodiscard]] static std::string read_request(int client_fd);

    // Case-insensitive lookup of a header inside a raw header block ending at header_end.
    [[nodiscard]] static bool find_header(const std::string& raw, size_t header_end,
                                          const std::string& name, std::string& out);
};

} // namespace lxace::net
