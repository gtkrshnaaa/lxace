#include "cli/view_prompt.hpp"
#include "cli/terminal.hpp"
#include <iostream>

namespace lxace::cli {

void ViewPrompt::render(const core::Engine& engine) {
    Terminal::clear_screen();
    std::string frame;
    frame.reserve(1024);
    frame.append("================================================================================\n");
    frame.append("  LXACE ENGINE - TASK PROMPT DISPATCHER\n");
    frame.append("================================================================================\n");
    frame.append("  Project : ").append(engine.project_dir().string()).append("\n");
    frame.append("  Model   : ").append(engine.config()->get().model).append("\n\n");
    frame.append("  Enter task instructions for the autonomous engine.\n");
    frame.append("  Press [Enter] to submit, or [Esc] / empty to cancel and return.\n");
    frame.append("────────────────────────────────────────────────────────────────────────────────\n\n");
    frame.append("  Prompt: ");
    std::cout << frame << std::flush;
}

ViewState ViewPrompt::handle_input(core::Engine& engine) {
    bool cancelled = false;
    std::string prompt = Terminal::read_interactive_line(&cancelled);
    if (!cancelled && !prompt.empty()) {
        engine.submit_prompt(prompt);
        std::string preview = prompt.size() > 40 ? prompt.substr(0, 37) + "..." : prompt;
        set_notification(UiNotification::Type::Success, "Prompt submitted: \"" + preview + "\"");
    } else {
        set_notification(UiNotification::Type::Info, "Prompt entry cancelled");
    }
    return ViewState::Home;
}

} // namespace lxace::cli
