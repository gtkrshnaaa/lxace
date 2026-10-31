#include "cli/cli_app.hpp"
#include "cli/terminal.hpp"
#include <iostream>

namespace lxace::cli {

CliApp::CliApp(std::shared_ptr<core::Engine> engine)
    : engine_(std::move(engine)) {}

void CliApp::stop() {
    running_ = false;
}

void CliApp::run() {
    if (!engine_) return;

    running_ = true;
    current_state_ = ViewState::Home;

    while (running_ && current_state_ != ViewState::Exit) {
        switch (current_state_) {
            case ViewState::Home:
                view_home_.render(*engine_);
                current_state_ = view_home_.handle_input(*engine_);
                break;
            case ViewState::Settings:
                view_settings_.render(*engine_);
                current_state_ = view_settings_.handle_input(*engine_);
                break;
            case ViewState::SettingsBaseUrl:
                ViewSettings::render_base_url(*engine_);
                current_state_ = ViewSettings::handle_base_url(*engine_);
                break;
            case ViewState::SettingsApiKey:
                ViewSettings::render_api_key(*engine_);
                current_state_ = ViewSettings::handle_api_key(*engine_);
                break;
            case ViewState::SettingsModel:
                ViewSettings::render_model(*engine_);
                current_state_ = ViewSettings::handle_model(*engine_);
                break;
            case ViewState::SettingsMaxTokens:
                ViewSettings::render_max_tokens(*engine_);
                current_state_ = ViewSettings::handle_max_tokens(*engine_);
                break;
            case ViewState::SettingsTemperature:
                ViewSettings::render_temperature(*engine_);
                current_state_ = ViewSettings::handle_temperature(*engine_);
                break;
            case ViewState::WritePrompt:
                view_prompt_.render(*engine_);
                current_state_ = view_prompt_.handle_input(*engine_);
                break;
            case ViewState::Exit:
                running_ = false;
                break;
        }
    }

    Terminal::clear_screen();
    std::cout << "LXACE engine exited cleanly.\n";
}

} // namespace lxace::cli
