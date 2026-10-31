#include "sys/fs.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

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

    std::ios_base::openmode mode = std::ios::binary | (append ? std::ios::app : std::ios::trunc);
    std::ofstream file(path, mode);
    if (!file.is_open()) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
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

} // namespace lxace::sys
