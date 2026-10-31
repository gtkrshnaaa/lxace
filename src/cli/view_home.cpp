#include "cli/view_home.hpp"
#include "cli/terminal.hpp"
#include "core/version.hpp"
#include <iostream>
#include <cstdio>

namespace lxace::cli {

void ViewHome::render(const core::Engine& engine) {
    auto size = Terminal::get_size();
    last_rows_ = size.rows;
    last_cols_ = size.cols;
    last_rendered_version_ = engine.ring_buffer()->version();

    std::string frame;
    frame.reserve(4096);
    frame.append("\033[H");

    bool is_busy = engine.orchestrator() && engine.orchestrator()->has_active_tasks();
    std::string status_badge = is_busy ? "[STATUS: BUSY]" : "[STATUS: IDLE]";

    frame.append("================================================================================\n");
    frame.append("  LXACE ENGINE v");
    frame.append(core::VERSION);
    frame.append("                 ");
    frame.append(status_badge);
    frame.append("   PORT: ");
    frame.append(std::to_string(engine.port()));
    frame.append("\n");
    frame.append("================================================================================\n");
    frame.append("  Project : ").append(engine.project_dir().string()).append("\n");
    frame.append("  Model   : ").append(engine.config()->get().model)
         .append(" (Tokens: ").append(std::to_string(engine.config()->get().max_tokens))
         .append(", Temp: ");
    char temp_buf[16];
    std::snprintf(temp_buf, sizeof(temp_buf), "%.2f", engine.config()->get().temperature);
    frame.append(temp_buf).append(")\n");
    frame.append("  API     : http://127.0.0.1:").append(std::to_string(engine.port())).append("/api/v1\n");
    frame.append("────────────────────────────────────────────────────────────────────────────────\n");
    frame.append("  [1] Settings       Configure API keys, model endpoints, and parameters\n");
    frame.append("  [2] Write Prompt   Dispatch autonomous task to engine worker queue\n");
    frame.append("  [3] Stop All       Terminate running sub-agents and worker threads\n");
    frame.append("  [4] Exit           Gracefully shutdown engine and background services\n");

    if (!notification_.empty()) {
        frame.append("────────────────────────────────────────────────────────────────────────────────\n");
        frame.append("  ").append(notification_.format()).append("\n");
    }

    frame.append("────────────────────────────────────────────────────────────────────────────────\n");
    frame.append("  LIVE ENGINE LOG STREAM\n");
    frame.append("────────────────────────────────────────────────────────────────────────────────\n");

    int header_lines = notification_.empty() ? 14 : 16;
    int max_log_lines = size.rows - header_lines - 2;
    if (max_log_lines < 2) max_log_lines = 2;

    auto logs = engine.ring_buffer()->tail(static_cast<size_t>(max_log_lines));
    if (logs.empty()) {
        frame.append("  (No logs recorded yet. Engine ready for commands.)\n");
    } else {
        for (const auto& line : logs) {
            frame.append("  ").append(line).append("\n");
        }
    }

    frame.append("────────────────────────────────────────────────────────────────────────────────\n");
    frame.append("  Select option [1-4], [S]ettings, [P]rompt, [K]ill, [Q]uit: ");
    frame.append("\033[J");

    std::cout << frame << std::flush;
}

ViewState ViewHome::handle_input(core::Engine& engine) {
    Terminal::enable_raw_mode();

    while (true) {
        int key = Terminal::read_key_timeout(60);
        if (key != -1) {
            Terminal::disable_raw_mode();
            switch (key) {
                case '1':
                case 's':
                case 'S':
                    return ViewState::Settings;
                case '2':
                case 'p':
                case 'P':
                    return ViewState::WritePrompt;
                case '3':
                case 'k':
                case 'K':
                    engine.stop_all();
                    set_notification(UiNotification::Type::Warning, "All running tasks and worker threads stopped");
                    render(engine);
                    Terminal::enable_raw_mode();
                    break;
                case '4':
                case 'q':
                case 'Q':
                case 3: // Ctrl+C
                case 4: // Ctrl+D
                    return ViewState::Exit;
                case '?':
                case 'h':
                case 'H':
                    set_notification(UiNotification::Type::Info, "Keys: 1/S (Settings), 2/P (Prompt), 3/K (Stop All), 4/Q (Exit)");
                    render(engine);
                    Terminal::enable_raw_mode();
                    break;
                default:
                    Terminal::enable_raw_mode();
                    break;
            }
        } else {
            bool resized = Terminal::check_and_reset_resized();
            auto current_size = Terminal::get_size();
            if (resized || current_size.rows != last_rows_ || current_size.cols != last_cols_ ||
                engine.ring_buffer()->version() != last_rendered_version_) {
                render(engine);
            }
        }
    }
}

} // namespace lxace::cli
