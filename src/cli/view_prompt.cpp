#include "cli/view_prompt.hpp"
#include "cli/terminal.hpp"
#include <iostream>

namespace lxace::cli {

void ViewPrompt::render(const core::Engine&) {
    Terminal::clear_screen();
    std::cout << "write your prompt here.\n"
              << ": " << std::flush;
}

ViewState ViewPrompt::handle_input(core::Engine& engine) {
    std::string prompt = Terminal::read_line();
    if (!prompt.empty()) {
        engine.submit_prompt(prompt);
    }
    return ViewState::Home;
}

} // namespace lxace::cli
