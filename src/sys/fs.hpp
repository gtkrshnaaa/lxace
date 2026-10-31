#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>
#include <sys/types.h>

namespace lxace::sys {

struct DirectoryEntry {
    std::string name;
    std::string path;
    bool is_directory{false};
    uintmax_t size{0};
    uint32_t permissions{0};
};

class FsEngine {
public:
    static std::optional<std::string> read_file(const std::filesystem::path& path);
    static bool write_file(const std::filesystem::path& path, const std::string& content, bool append = false);
    static bool exists(const std::filesystem::path& path);
    static bool is_directory(const std::filesystem::path& path);
    static std::vector<DirectoryEntry> list_directory(const std::filesystem::path& path, bool recursive = false);
    static bool remove_path(const std::filesystem::path& path);
    static bool copy_path(const std::filesystem::path& src, const std::filesystem::path& dst);
    static bool create_directories(const std::filesystem::path& path);
    static std::optional<uint32_t> get_permissions(const std::filesystem::path& path);
    static bool set_permissions(const std::filesystem::path& path, uint32_t mode);
    static std::optional<std::filesystem::path> resolve_safe_path(const std::filesystem::path& base_dir, const std::filesystem::path& target);
};

} // namespace lxace::sys
