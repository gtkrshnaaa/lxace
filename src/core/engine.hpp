#pragma once

#include "core/config.hpp"
#include "core/event_bus.hpp"
#include "core/ring_buffer.hpp"
#include "agent/orchestrator.hpp"
#include "net/http_server.hpp"
#include "tool/tool_registry.hpp"
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
    // Returns false when the API listener could not bind, so startup can fail loudly.
    bool start_services();
    void stop_services();

    [[nodiscard]] const std::filesystem::path& project_dir() const { return project_dir_; }
    [[nodiscard]] int port() const { return port_; }
    [[nodiscard]] std::string last_error() const { return last_error_; }
    [[nodiscard]] const std::string& api_token() const { return api_token_; }
    [[nodiscard]] const std::string& bind_address() const { return bind_address_; }
    [[nodiscard]] std::shared_ptr<ConfigManager> config() const { return config_; }
    [[nodiscard]] std::shared_ptr<EventBus> event_bus() const { return event_bus_; }
    [[nodiscard]] std::shared_ptr<RingBuffer<std::string>> ring_buffer() const { return ring_buffer_; }
    [[nodiscard]] std::shared_ptr<agent::Orchestrator> orchestrator() const { return orchestrator_; }
    [[nodiscard]] std::shared_ptr<net::HttpServer> http_server() const { return http_server_; }
    [[nodiscard]] std::shared_ptr<tool::ToolRegistry> tool_registry() const { return tool_registry_; }

    void submit_prompt(const std::string& prompt);
    void stop_all();

private:
    void register_api_routes();
    void configure_auth();

    std::filesystem::path project_dir_;
    int port_;
    std::string bind_address_{"127.0.0.1"};
    std::string api_token_;
    std::string last_error_;
    std::shared_ptr<ConfigManager> config_;
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<RingBuffer<std::string>> ring_buffer_;
    std::shared_ptr<agent::Orchestrator> orchestrator_;
    std::shared_ptr<tool::ToolRegistry> tool_registry_;
    std::shared_ptr<net::SseStreamManager> sse_manager_;
    std::shared_ptr<net::HttpRouter> http_router_;
    std::shared_ptr<net::HttpServer> http_server_;
    size_t event_bus_listener_id_{0};
};

} // namespace lxace::core
