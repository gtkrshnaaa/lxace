#include "cli/view_home.hpp"
#include "cli/terminal.hpp"
#include <iostream>

namespace lxace::cli {

void ViewHome::render(const core::Engine& engine) {
    Terminal::clear_screen();

    std::cout << "=== LXACE ===\n"
              << "Project: " << engine.project_dir().string() << "\n\n"
              << "1. Settings\n"
              << "2. Write Prompt\n"
              << "3. Stop All\n"
              << "4. Exit\n\n"
              << "────────────────────────────────────────\n"
              << "BACKGROUND WORKING STATUS\n";

    auto size = Terminal::get_size();
    // Fixed lines count: header(8) + top divider & title(2) + bottom divider & prompt(3) = 13 lines
    int max_log_lines = size.rows - 13;
    if (max_log_lines < 2) max_log_lines = 2;

    auto logs = engine.ring_buffer()->tail(static_cast<size_t>(max_log_lines));
    for (const auto& line : logs) {
        std::cout << line << "\n";
    }

    std::cout << "────────────────────────────────────────\n\n"
              << "choose menu: " << std::flush;
}

ViewState ViewHome::handle_input(core::Engine& engine) {
    std::string input = Terminal::read_line();
    if (input.empty()) return ViewState::Home;

    char c = input[0];
    switch (c) {
        case '1': return ViewState::Settings;
        case '2': return ViewState::WritePrompt;
        case '3':
            engine.stop_all();
            return ViewState::Home;
        case '4': return ViewState::Exit;
        default: return ViewState::Home;
    }
}

} // namespace lxace::cli
