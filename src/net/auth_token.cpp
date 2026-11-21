#include "net/http_auth.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>

namespace lxace::net {

namespace {

constexpr size_t kTokenBytes = 32;

std::string read_file_trimmed(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    if (!std::getline(file, line)) {
        return "";
    }
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    return line;
}

bool write_private_file(const std::string& path, const std::string& content) {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        return false;
    }
    size_t written = 0;
    bool ok = true;
    while (written < content.size()) {
        const ssize_t n = write(fd, content.data() + written, content.size() - written);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            ok = false;
            break;
        }
        written += static_cast<size_t>(n);
    }
    if (ok && fsync(fd) != 0) {
        ok = false;
    }
    if (close(fd) != 0) {
        ok = false;
    }
    return ok;
}

} // namespace

std::string HttpAuth::generate_token() {
    unsigned char raw[kTokenBytes];
    size_t filled = 0;

    while (filled < kTokenBytes) {
        const ssize_t n = getrandom(raw + filled, kTokenBytes - filled, 0);
        if (n <= 0) {
            if (errno == EINTR) {
                continue;
            }
            return "";
        }
        filled += static_cast<size_t>(n);
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string token;
    token.reserve(kTokenBytes * 2);
    for (size_t i = 0; i < kTokenBytes; ++i) {
        token.push_back(kHex[(raw[i] >> 4) & 0x0F]);
        token.push_back(kHex[raw[i] & 0x0F]);
    }
    return token;
}

std::string HttpAuth::resolve_token(const std::string& file_path) {
    if (const char* env = std::getenv("LXACE_API_TOKEN"); env != nullptr && env[0] != '\0') {
        return std::string(env);
    }

    const std::string existing = read_file_trimmed(file_path);
    if (!existing.empty()) {
        return existing;
    }

    const std::string token = generate_token();
    if (token.empty()) {
        return "";
    }
    if (!write_private_file(file_path, token + "\n")) {
        return "";
    }
    return token;
}

} // namespace lxace::net
