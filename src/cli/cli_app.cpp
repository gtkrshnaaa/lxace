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
                if (current_state_ == ViewState::Home && !view_settings_.notification().empty()) {
                    view_home_.set_notification(view_settings_.notification().type, view_settings_.notification().text);
                    view_settings_.clear_notification();
                }
                break;
            case ViewState::SettingsBaseUrl:
                view_settings_.render_base_url(*engine_);
                current_state_ = view_settings_.handle_base_url(*engine_);
                break;
            case ViewState::SettingsApiKey:
                view_settings_.render_api_key(*engine_);
                current_state_ = view_settings_.handle_api_key(*engine_);
                break;
            case ViewState::SettingsModel:
                view_settings_.render_model(*engine_);
                current_state_ = view_settings_.handle_model(*engine_);
                break;
            case ViewState::SettingsMaxTokens:
                view_settings_.render_max_tokens(*engine_);
                current_state_ = view_settings_.handle_max_tokens(*engine_);
                break;
            case ViewState::SettingsTemperature:
                view_settings_.render_temperature(*engine_);
                current_state_ = view_settings_.handle_temperature(*engine_);
                break;
            case ViewState::WritePrompt:
                view_prompt_.render(*engine_);
                current_state_ = view_prompt_.handle_input(*engine_);
                if (!view_prompt_.notification().empty()) {
                    view_home_.set_notification(view_prompt_.notification().type, view_prompt_.notification().text);
                    view_prompt_.clear_notification();
                }
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
