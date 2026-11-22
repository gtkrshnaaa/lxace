#include "sys/fs.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

namespace lxace::sys {

std::optional<std::string> FsEngine::read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool FsEngine::write_file(const std::filesystem::path& path, const std::string& content, bool append) {
    std::error_code ec;
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    if (append) {
        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open()) {
            return false;
        }
        file.write(content.data(), static_cast<std::streamsize>(content.size()));
        file.flush();
        return file.good();
    }

    // Write to a sibling temporary file, fsync it, then rename over the target. A crash
    // mid-write can no longer leave a truncated file with the original destroyed.
    std::filesystem::path temp_path = path;
    temp_path += ".tmp.";
    temp_path += std::to_string(static_cast<long long>(::getpid()));

    const int fd = ::open(temp_path.c_str(),
                          O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return false;
    }

    size_t written = 0;
    bool ok = true;
    while (written < content.size()) {
        const ssize_t n = ::write(fd, content.data() + written, content.size() - written);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            ok = false;
            break;
        }
        written += static_cast<size_t>(n);
    }

    if (ok && ::fsync(fd) != 0) {
        ok = false;
    }
    if (::close(fd) != 0) {
        ok = false;
    }

    if (!ok) {
        std::filesystem::remove(temp_path, ec);
        return false;
    }

    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return false;
    }
    return true;
}

bool FsEngine::exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool FsEngine::is_directory(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

std::vector<DirectoryEntry> FsEngine::list_directory(const std::filesystem::path& path, bool recursive) {
    std::vector<DirectoryEntry> entries;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
        return entries;
    }

    auto process_entry = [&entries](const auto& entry) {
        DirectoryEntry de;
        de.name = entry.path().filename().string();
        de.path = entry.path().string();
        std::error_code it_ec;
        de.is_directory = entry.is_directory(it_ec);
        if (!de.is_directory) {
            de.size = entry.file_size(it_ec);
        }
        auto perms = entry.status(it_ec).permissions();
        de.permissions = static_cast<uint32_t>(perms);
        entries.push_back(std::move(de));
    };

    if (recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
            process_entry(entry);
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
            process_entry(entry);
        }
    }
    return entries;
}

bool FsEngine::remove_path(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::remove_all(path, ec) > 0;
}

bool FsEngine::copy_path(const std::filesystem::path& src, const std::filesystem::path& dst) {
    std::error_code ec;
    std::filesystem::copy(src, dst, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

bool FsEngine::create_directories(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::create_directories(path, ec);
}

std::optional<uint32_t> FsEngine::get_permissions(const std::filesystem::path& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(st.st_mode & 07777);
}

bool FsEngine::set_permissions(const std::filesystem::path& path, uint32_t mode) {
    return chmod(path.c_str(), static_cast<mode_t>(mode)) == 0;
}

bool FsEngine::set_owner(const std::filesystem::path& path, uint32_t uid, uint32_t gid, bool follow_symlinks) {
    // An unprivileged process may only chown a file to ids it already owns, so callers must
    // surface the failure instead of assuming success. lchown is used by default so a
    // symlink cannot be used to retarget ownership of a file outside the workspace.
    const int rc = follow_symlinks
        ? ::chown(path.c_str(), static_cast<uid_t>(uid), static_cast<gid_t>(gid))
        : ::lchown(path.c_str(), static_cast<uid_t>(uid), static_cast<gid_t>(gid));
    return rc == 0;
}

std::optional<std::filesystem::path> FsEngine::resolve_safe_path(const std::filesystem::path& base_dir, const std::filesystem::path& target) {
    std::error_code ec;
    auto abs_base = std::filesystem::weakly_canonical(base_dir, ec);
    if (ec) return std::nullopt;

    std::filesystem::path combined = target.is_absolute() ? target : (abs_base / target);

    // Reject any symlink in the path from the base down to the final component. A
    // dangling final symlink previously survived weakly_canonical, passed the prefix
    // check, and then let ofstream write outside the sandbox through the link.
    std::filesystem::path probe = abs_base;
    const auto rel = combined.lexically_relative(abs_base);
    if (rel.empty() && combined != abs_base) {
        return std::nullopt;
    }
    for (const auto& part : rel) {
        if (part == "." || part == "..") {
            continue;
        }
        probe /= part;
        struct stat st{};
        if (::lstat(probe.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
            return std::nullopt;
        }
    }

    auto normalized = std::filesystem::weakly_canonical(combined, ec);
    if (ec) return std::nullopt;

    std::string base_str = abs_base.string();
    std::string norm_str = normalized.string();

    // Ensure normalized path starts with base_str
    if (norm_str.rfind(base_str, 0) == 0) {
        if (norm_str.length() == base_str.length() || norm_str[base_str.length()] == '/' || base_str == "/") {
            return normalized;
        }
    }
    return std::nullopt;
}

} // namespace lxace::sys
