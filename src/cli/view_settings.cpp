#include "cli/view_settings.hpp"
#include "cli/terminal.hpp"
#include <iostream>

namespace lxace::cli {

void ViewSettings::render(const core::Engine&) {
    Terminal::clear_screen();
    std::cout << "=== SETTINGS ===\n\n"
              << "1. Base URL\n"
              << "2. API Key\n"
              << "3. Model\n"
              << "4. Max Tokens\n"
              << "5. Temperature\n"
              << "6. Back\n\n"
              << "choose menu: " << std::flush;
}

ViewState ViewSettings::handle_input(core::Engine&) {
    Terminal::enable_raw_mode();
    int key = Terminal::read_key_timeout(5000);
    Terminal::disable_raw_mode();

    if (key == -1) return ViewState::Settings;

    switch (key) {
        case '1': return ViewState::SettingsBaseUrl;
        case '2': return ViewState::SettingsApiKey;
        case '3': return ViewState::SettingsModel;
        case '4': return ViewState::SettingsMaxTokens;
        case '5': return ViewState::SettingsTemperature;
        case '6':
        case 'q':
        case 'Q':
        case 27: // ESC
            return ViewState::Home;
        default: return ViewState::Settings;
    }
}

void ViewSettings::render_base_url(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "=== SETTINGS > BASE URL ===\n\n"
              << "Current: " << engine.config()->get().base_url << "\n\n"
              << "Enter new base URL (empty to cancel):\n"
              << ": " << std::flush;
}

ViewState ViewSettings::handle_base_url(core::Engine& engine) {
    std::string input = Terminal::read_line();
    if (!input.empty()) {
        engine.config()->set_base_url(input);
    }
    return ViewState::Settings;
}

void ViewSettings::render_api_key(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "=== SETTINGS > API KEY ===\n\n"
              << "Current: " << engine.config()->masked_api_key() << "\n\n"
              << "Enter new API key (empty to cancel):\n"
              << ": " << std::flush;
}

ViewState ViewSettings::handle_api_key(core::Engine& engine) {
    std::string input = Terminal::read_line();
    if (!input.empty()) {
        engine.config()->set_api_key(input);
    }
    return ViewState::Settings;
}

void ViewSettings::render_model(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "=== SETTINGS > MODEL ===\n\n"
              << "Current: " << engine.config()->get().model << "\n\n"
              << "1. claude-sonnet-4\n"
              << "2. claude-opus-4\n"
              << "3. gpt-4o\n"
              << "4. custom\n"
              << "5. Back\n\n"
              << "choose menu: " << std::flush;
}

ViewState ViewSettings::handle_model(core::Engine& engine) {
    Terminal::enable_raw_mode();
    int key = Terminal::read_key_timeout(5000);
    Terminal::disable_raw_mode();

    if (key == -1) return ViewState::SettingsModel;

    switch (key) {
        case '1':
            engine.config()->set_model("claude-sonnet-4");
            return ViewState::Settings;
        case '2':
            engine.config()->set_model("claude-opus-4");
            return ViewState::Settings;
        case '3':
            engine.config()->set_model("gpt-4o");
            return ViewState::Settings;
        case '4': {
            std::cout << "Enter custom model name: " << std::flush;
            std::string custom_model = Terminal::read_line();
            if (!custom_model.empty()) {
                engine.config()->set_model(custom_model);
            }
            return ViewState::Settings;
        }
        case '5':
        case 'q':
        case 'Q':
        case 27:
            return ViewState::Settings;
        default:
            return ViewState::SettingsModel;
    }
}

void ViewSettings::render_max_tokens(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "=== SETTINGS > MAX TOKENS ===\n\n"
              << "Current: " << engine.config()->get().max_tokens << "\n\n"
              << "Enter new value (empty to cancel):\n"
              << ": " << std::flush;
}

ViewState ViewSettings::handle_max_tokens(core::Engine& engine) {
    std::string input = Terminal::read_line();
    if (!input.empty()) {
        try {
            int val = std::stoi(input);
            if (val > 0) engine.config()->set_max_tokens(val);
        } catch (...) {}
    }
    return ViewState::Settings;
}

void ViewSettings::render_temperature(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "=== SETTINGS > TEMPERATURE ===\n\n"
              << "Current: " << engine.config()->get().temperature << "\n\n"
              << "Enter new value 0.0 - 1.0 (empty to cancel):\n"
              << ": " << std::flush;
}

ViewState ViewSettings::handle_temperature(core::Engine& engine) {
    std::string input = Terminal::read_line();
    if (!input.empty()) {
        try {
            double val = std::stod(input);
            if (val >= 0.0 && val <= 1.0) engine.config()->set_temperature(val);
        } catch (...) {}
    }
    return ViewState::Settings;
}

} // namespace lxace::cli
