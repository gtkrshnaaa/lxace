#include "core/config.hpp"
#include "json/json_parser.hpp"
#include "json/json_writer.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cmath>

namespace lxace::core {

ConfigManager::ConfigManager(std::filesystem::path config_file_path)
    : file_path_(std::move(config_file_path)) {
    if (!file_path_.empty() && std::filesystem::exists(file_path_)) {
        load_from_file(file_path_);
    }
}

ConfigData ConfigManager::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

void ConfigManager::update(const ConfigData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

void ConfigManager::set_base_url(std::string url) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.base_url = std::move(url);
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

void ConfigManager::set_api_key(std::string key) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.api_key = std::move(key);
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

void ConfigManager::set_model(std::string model) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.model = std::move(model);
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

void ConfigManager::set_max_tokens(int tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.max_tokens = tokens;
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

void ConfigManager::set_temperature(double temp) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.temperature = temp;
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

void ConfigManager::set_server_port(int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.server_port = port;
    if (!file_path_.empty()) {
        save_to_file(file_path_);
    }
}

std::string ConfigManager::mask_key(const std::string& key) {
    if (key.empty()) {
        return "(none)";
    }
    if (key.length() <= 8) {
        return "sk-****";
    }
    std::string prefix = key.substr(0, 3);
    std::string suffix = key.substr(key.length() - 4);
    std::string stars(std::max<size_t>(4, key.length() - 7), '*');
    return prefix + stars + suffix;
}

std::string ConfigManager::masked_api_key() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mask_key(data_.api_key);
}

std::string ConfigManager::mask_url(const std::string& url) {
    if (url.empty()) return "(none)";
    if (url.length() <= 8) return "https://****";

    auto scheme_pos = url.find("://");
    size_t prefix_len = (scheme_pos != std::string::npos) ? scheme_pos + 3 : 4;
    if (prefix_len >= url.length()) prefix_len = 0;

    std::string prefix = url.substr(0, prefix_len);
    std::string suffix = (url.length() > prefix_len + 5) ? url.substr(url.length() - 3) : "";
    size_t star_count = (url.length() > prefix.length() + suffix.length())
                        ? (url.length() - prefix.length() - suffix.length())
                        : 4;

    return prefix + std::string(std::max<size_t>(4, star_count), '*') + suffix;
}

std::string ConfigManager::masked_base_url() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mask_url(data_.base_url);
}

bool ConfigManager::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string raw = buffer.str();

    // Parse as real JSON first. The previous line-based loader split on the first colon,
    // so any URL was truncated at "https:" and every later field silently reset.
    if (auto parsed = json::JsonParser::parse(raw); parsed && parsed->is_object()) {
        const auto& obj = parsed->as_object();
        auto read_str = [&obj](const char* key, std::string& out) {
            auto it = obj.find(key);
            if (it != obj.end() && it->second.is_string()) {
                out = it->second.as_string();
            }
        };
        read_str("base_url", data_.base_url);
        read_str("api_key", data_.api_key);
        read_str("model", data_.model);

        if (auto it = obj.find("max_tokens"); it != obj.end() && it->second.is_number()) {
            const double v = it->second.as_double();
            if (std::isfinite(v) && v >= 1.0 && v <= 1000000.0) {
                data_.max_tokens = static_cast<int>(v);
            }
        }
        if (auto it = obj.find("temperature"); it != obj.end() && it->second.is_number()) {
            const double v = it->second.as_double();
            if (std::isfinite(v) && v >= 0.0 && v <= 2.0) {
                data_.temperature = v;
            }
        }
        if (auto it = obj.find("server_port"); it != obj.end() && it->second.is_number()) {
            const double v = it->second.as_double();
            if (std::isfinite(v) && v >= 1.0 && v <= 65535.0) {
                data_.server_port = static_cast<int>(v);
            }
        }
        return true;
    }

    // Legacy key: value format, only for files written before JSON serialization.
    std::istringstream stream(raw);
    std::string line;
    while (std::getline(stream, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\r\n\"'");
            size_t end = s.find_last_not_of(" \t\r\n\"',");
            if (start == std::string::npos) s.clear();
            else s = s.substr(start, end - start + 1);
        };
        trim(key);
        trim(val);

        if (key == "base_url") data_.base_url = val;
        else if (key == "api_key") data_.api_key = val;
        else if (key == "model") data_.model = val;
        else if (key == "max_tokens") {
            try { data_.max_tokens = std::stoi(val); } catch (...) {}
        }
        else if (key == "temperature") {
            try {
                const double parsed_temp = std::stod(val);
                if (std::isfinite(parsed_temp) && parsed_temp >= 0.0 && parsed_temp <= 2.0) {
                    data_.temperature = parsed_temp;
                }
            } catch (...) {}
        }
        else if (key == "server_port") {
            try { data_.server_port = std::stoi(val); } catch (...) {}
        }
    }
    return true;
}

bool ConfigManager::save_to_file(const std::filesystem::path& path) const {
    std::error_code ec;
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    // Serialize with real JSON escaping. Raw concatenation turned any quote inside
    // api_key or base_url into a file the loader could never read back.
    json::JsonObject obj;
    obj["base_url"] = data_.base_url;
    obj["api_key"] = data_.api_key;
    obj["model"] = data_.model;
    obj["max_tokens"] = static_cast<int64_t>(data_.max_tokens);
    obj["temperature"] = data_.temperature;
    obj["server_port"] = static_cast<int64_t>(data_.server_port);
    const std::string payload = json::JsonWriter::write(json::JsonValue(obj), true) + "\n";

    // Same-directory temporary file, private mode from creation, fsync then rename.
    std::filesystem::path temp_path = path;
    temp_path += ".tmp.";
    temp_path += std::to_string(static_cast<long long>(::getpid()));

    const int fd = ::open(temp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        return false;
    }

    size_t written = 0;
    bool ok = true;
    while (written < payload.size()) {
        const ssize_t n = ::write(fd, payload.data() + written, payload.size() - written);
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

    // Explicitly enforce 0600 in case an existing file had wider permissions.
    ::chmod(temp_path.c_str(), 0600);

    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return false;
    }
    return true;
}

} // namespace lxace::core
