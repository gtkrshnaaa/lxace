#include "core/engine.hpp"
#include "core/version.hpp"
#include "cli/cli_app.hpp"
#include "cli/terminal.hpp"
#include <iostream>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> g_shutdown_requested{false};
static std::shared_ptr<lxace::cli::CliApp> g_cli_app;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        g_shutdown_requested = true;
        if (g_cli_app) {
            g_cli_app->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    std::signal(SIGPIPE, SIG_IGN);
    lxace::cli::Terminal::install_signal_handlers();

    std::filesystem::path project_dir = std::filesystem::current_path();
    int port = 8080;
    bool headless = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project-dir" && i + 1 < argc) {
            project_dir = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            try { port = std::stoi(argv[++i]); } catch (...) {}
        } else if (arg == "--headless") {
            headless = true;
        } else if (arg == "--version" || arg == "-v" || arg == "-V") {
            std::cout << lxace::core::format_version() << "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: lxace [options]\n"
                      << "Options:\n"
                      << "  --project-dir <path>  Target project directory (default: current)\n"
                      << "  --port <port>         HTTP/SSE API server port (default: 8080)\n"
                      << "  --headless            Run API engine without interactive CLI\n"
                      << "  -v, -V, --version     Display version information\n"
                      << "  -h, --help            Show this help message\n";
            return 0;
        }
    }

    auto engine = std::make_shared<lxace::core::Engine>(project_dir, port);
    if (!engine->initialize()) {
        std::cerr << "Failed to initialize LXACE engine.\n";
        return 1;
    }

    engine->start_services();

    if (headless) {
        std::cout << "LXACE running in headless mode on port " << port << " for project: " << project_dir << "\n";
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    } else {
        g_cli_app = std::make_shared<lxace::cli::CliApp>(engine);
        g_cli_app->run();
    }

    engine->stop_services();
    return 0;
}
