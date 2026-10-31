#include "cli/view_home.hpp"
#include "cli/terminal.hpp"
#include <iostream>

namespace lxace::cli {

void ViewHome::render(const core::Engine& engine) {
    auto size = Terminal::get_size();
    last_rows_ = size.rows;
    last_cols_ = size.cols;
    last_rendered_version_ = engine.ring_buffer()->version();

    std::string frame;
    frame.reserve(4096);
    frame.append("\033[H");

    frame.append("=== LXACE ===\n");
    frame.append("Project: ");
    frame.append(engine.project_dir().string());
    frame.append("\n\n");

    frame.append("1. Settings\n");
    frame.append("2. Write Prompt\n");
    frame.append("3. Stop All\n");
    frame.append("4. Exit\n\n");

    frame.append("────────────────────────────────────────\n");
    frame.append("BACKGROUND WORKING STATUS\n");

    int max_log_lines = size.rows - 13;
    if (max_log_lines < 2) max_log_lines = 2;

    auto logs = engine.ring_buffer()->tail(static_cast<size_t>(max_log_lines));
    for (const auto& line : logs) {
        frame.append(line);
        frame.append("\n");
    }

    frame.append("────────────────────────────────────────\n\n");
    frame.append("choose menu: ");
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
                case '1': return ViewState::Settings;
                case '2': return ViewState::WritePrompt;
                case '3':
                    engine.stop_all();
                    render(engine);
                    Terminal::enable_raw_mode();
                    break;
                case '4':
                case 'q':
                case 'Q':
                case 3: // Ctrl+C
                case 4: // Ctrl+D
                    return ViewState::Exit;
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
