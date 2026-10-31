#include "core/config.hpp"
#include <fstream>
#include <sstream>

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
    // Simple key-value / json fallback loader
    std::string line;
    while (std::getline(file, line)) {
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
            try { data_.temperature = std::stod(val); } catch (...) {}
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

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "{\n"
         << "  \"base_url\": \"" << data_.base_url << "\",\n"
         << "  \"api_key\": \"" << data_.api_key << "\",\n"
         << "  \"model\": \"" << data_.model << "\",\n"
         << "  \"max_tokens\": " << data_.max_tokens << ",\n"
         << "  \"temperature\": " << data_.temperature << ",\n"
         << "  \"server_port\": " << data_.server_port << "\n"
         << "}\n";
    return true;
}

} // namespace lxace::core
