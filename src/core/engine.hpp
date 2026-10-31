#pragma once

#include "core/config.hpp"
#include "core/event_bus.hpp"
#include "core/ring_buffer.hpp"
#include "agent/orchestrator.hpp"
#include "net/http_server.hpp"
#include <string>
#include <memory>
#include <filesystem>

namespace lxace::core {

class Engine {
public:
    explicit Engine(std::filesystem::path project_dir, int port = 8080);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initialize();
    void start_services();
    void stop_services();

    [[nodiscard]] const std::filesystem::path& project_dir() const { return project_dir_; }
    [[nodiscard]] std::shared_ptr<ConfigManager> config() const { return config_; }
    [[nodiscard]] std::shared_ptr<EventBus> event_bus() const { return event_bus_; }
    [[nodiscard]] std::shared_ptr<RingBuffer<std::string>> ring_buffer() const { return ring_buffer_; }
    [[nodiscard]] std::shared_ptr<agent::Orchestrator> orchestrator() const { return orchestrator_; }
    [[nodiscard]] std::shared_ptr<net::HttpServer> http_server() const { return http_server_; }

    void submit_prompt(const std::string& prompt);
    void stop_all();

private:
    void register_api_routes();

    std::filesystem::path project_dir_;
    int port_;
    std::shared_ptr<ConfigManager> config_;
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<RingBuffer<std::string>> ring_buffer_;
    std::shared_ptr<agent::Orchestrator> orchestrator_;
    std::shared_ptr<net::SseStreamManager> sse_manager_;
    std::shared_ptr<net::HttpRouter> http_router_;
    std::shared_ptr<net::HttpServer> http_server_;
    size_t event_bus_listener_id_{0};
};

} // namespace lxace::core
