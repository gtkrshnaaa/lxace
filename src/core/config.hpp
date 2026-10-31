#pragma once

#include <string>
#include <mutex>
#include <filesystem>

namespace lxace::core {

struct ConfigData {
    std::string base_url{"https://api.example.com/v1"};
    std::string api_key{""};
    std::string model{"claude-sonnet-4"};
    int max_tokens{8192};
    double temperature{0.3};
    int server_port{8080};
};

class ConfigManager {
public:
    ConfigManager() = default;
    explicit ConfigManager(std::filesystem::path config_file_path);

    [[nodiscard]] ConfigData get() const;
    void update(const ConfigData& data);

    void set_base_url(std::string url);
    void set_api_key(std::string key);
    void set_model(std::string model);
    void set_max_tokens(int tokens);
    void set_temperature(double temp);
    void set_server_port(int port);

    [[nodiscard]] std::string masked_api_key() const;
    [[nodiscard]] std::string masked_base_url() const;
    static std::string mask_key(const std::string& key);
    static std::string mask_url(const std::string& url);

    bool load_from_file(const std::filesystem::path& path);
    bool save_to_file(const std::filesystem::path& path) const;

private:
    mutable std::mutex mutex_;
    ConfigData data_;
    std::filesystem::path file_path_;
};

} // namespace lxace::core
