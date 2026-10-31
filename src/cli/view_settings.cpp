#include "cli/view_settings.hpp"
#include "cli/terminal.hpp"
#include <iostream>
#include <cstdio>

namespace lxace::cli {

void ViewSettings::render(const core::Engine& engine) {
    Terminal::clear_screen();
    std::string frame;
    frame.reserve(2048);

    frame.append("================================================================================\n");
    frame.append("  LXACE CONFIGURATION SETTINGS\n");
    frame.append("================================================================================\n");
    frame.append("  [1] Base URL      : ").append(engine.config()->masked_base_url()).append("\n");
    frame.append("  [2] API Key       : ").append(engine.config()->masked_api_key()).append("\n");
    frame.append("  [3] Model         : ").append(engine.config()->get().model).append("\n");
    frame.append("  [4] Max Tokens    : ").append(std::to_string(engine.config()->get().max_tokens)).append("\n");

    char temp_buf[32];
    std::snprintf(temp_buf, sizeof(temp_buf), "%.2f", engine.config()->get().temperature);
    frame.append("  [5] Temperature   : ").append(temp_buf).append("\n");
    frame.append("  [6] Back to Main Menu\n");
    frame.append("────────────────────────────────────────────────────────────────────────────────\n");

    if (!notification_.empty()) {
        frame.append("  ").append(notification_.format()).append("\n");
        frame.append("────────────────────────────────────────────────────────────────────────────────\n");
    }

    frame.append("  Select setting [1-6], [B]ack, or [Q]uit: ");
    std::cout << frame << std::flush;
}

ViewState ViewSettings::handle_input(core::Engine&) {
    Terminal::enable_raw_mode();
    int key = -1;
    while (key == -1) {
        key = Terminal::read_key_timeout(100);
        if (Terminal::check_and_reset_resized()) {
            break;
        }
    }
    Terminal::disable_raw_mode();

    if (key == -1) return ViewState::Settings;

    switch (key) {
        case '1':
        case 'u':
        case 'U':
            return ViewState::SettingsBaseUrl;
        case '2':
        case 'k':
        case 'K':
            return ViewState::SettingsApiKey;
        case '3':
        case 'm':
        case 'M':
            return ViewState::SettingsModel;
        case '4':
        case 't':
        case 'T':
            return ViewState::SettingsMaxTokens;
        case '5':
        case 'e':
        case 'E':
            return ViewState::SettingsTemperature;
        case '6':
        case 'b':
        case 'B':
        case 'q':
        case 'Q':
        case 27: // ESC
            return ViewState::Home;
        default:
            return ViewState::Settings;
    }
}

void ViewSettings::render_base_url(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "================================================================================\n"
              << "  LXACE CONFIGURATION > BASE URL\n"
              << "================================================================================\n"
              << "  Current Base URL: " << engine.config()->masked_base_url() << "\n\n"
              << "  Enter new base URL (empty or [Esc] to cancel):\n"
              << "  URL: " << std::flush;
}

ViewState ViewSettings::handle_base_url(core::Engine& engine) {
    std::string input = Terminal::read_masked_line('*');
    if (!input.empty()) {
        engine.config()->set_base_url(input);
        set_notification(UiNotification::Type::Success, "Base URL updated successfully");
    } else {
        set_notification(UiNotification::Type::Info, "Base URL unchanged");
    }
    return ViewState::Settings;
}

void ViewSettings::render_api_key(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "================================================================================\n"
              << "  LXACE CONFIGURATION > API KEY\n"
              << "================================================================================\n"
              << "  Current API Key: " << engine.config()->masked_api_key() << "\n\n"
              << "  Enter new API key (input is masked with '*', empty or [Esc] to cancel):\n"
              << "  Key: " << std::flush;
}

ViewState ViewSettings::handle_api_key(core::Engine& engine) {
    std::string input = Terminal::read_masked_line('*');
    if (!input.empty()) {
        engine.config()->set_api_key(input);
        set_notification(UiNotification::Type::Success, "API Key updated and masked");
    } else {
        set_notification(UiNotification::Type::Info, "API Key unchanged");
    }
    return ViewState::Settings;
}

void ViewSettings::render_model(const core::Engine& engine) {
    Terminal::clear_screen();
    const auto& current_model = engine.config()->get().model;
    std::string frame;
    frame.reserve(1024);

    frame.append("================================================================================\n");
    frame.append("  LXACE CONFIGURATION > SELECT MODEL\n");
    frame.append("================================================================================\n");
    frame.append("  Active Model: ").append(current_model).append("\n\n");

    auto append_option = [&](char num, const std::string& name) {
        frame.append("  [").append(1, num).append("] ").append(name);
        if (current_model == name) {
            frame.append("  (currently active)");
        }
        frame.append("\n");
    };

    append_option('1', "claude-3-5-sonnet-20241022");
    append_option('2', "claude-sonnet-4");
    append_option('3', "claude-opus-4");
    append_option('4', "gpt-4o");
    append_option('5', "o1");
    frame.append("  [6] Custom Model\n");
    frame.append("  [7] Back to Settings\n");
    frame.append("────────────────────────────────────────────────────────────────────────────────\n");
    frame.append("  Select option [1-7], or [B]ack: ");
    std::cout << frame << std::flush;
}

ViewState ViewSettings::handle_model(core::Engine& engine) {
    Terminal::enable_raw_mode();
    int key = -1;
    while (key == -1) {
        key = Terminal::read_key_timeout(100);
        if (Terminal::check_and_reset_resized()) break;
    }
    Terminal::disable_raw_mode();

    if (key == -1) return ViewState::SettingsModel;

    switch (key) {
        case '1':
            engine.config()->set_model("claude-3-5-sonnet-20241022");
            set_notification(UiNotification::Type::Success, "Model set to claude-3-5-sonnet-20241022");
            return ViewState::Settings;
        case '2':
            engine.config()->set_model("claude-sonnet-4");
            set_notification(UiNotification::Type::Success, "Model set to claude-sonnet-4");
            return ViewState::Settings;
        case '3':
            engine.config()->set_model("claude-opus-4");
            set_notification(UiNotification::Type::Success, "Model set to claude-opus-4");
            return ViewState::Settings;
        case '4':
            engine.config()->set_model("gpt-4o");
            set_notification(UiNotification::Type::Success, "Model set to gpt-4o");
            return ViewState::Settings;
        case '5':
            engine.config()->set_model("o1");
            set_notification(UiNotification::Type::Success, "Model set to o1");
            return ViewState::Settings;
        case '6':
        case 'c':
        case 'C': {
            std::cout << "\nEnter custom model name (empty or [Esc] to cancel): " << std::flush;
            bool cancelled = false;
            std::string custom_model = Terminal::read_interactive_line(&cancelled);
            if (!cancelled && !custom_model.empty()) {
                engine.config()->set_model(custom_model);
                set_notification(UiNotification::Type::Success, "Model set to " + custom_model);
            } else {
                set_notification(UiNotification::Type::Info, "Model selection unchanged");
            }
            return ViewState::Settings;
        }
        case '7':
        case 'b':
        case 'B':
        case 'q':
        case 'Q':
        case 27:
            set_notification(UiNotification::Type::Info, "Model selection unchanged");
            return ViewState::Settings;
        default:
            return ViewState::SettingsModel;
    }
}

void ViewSettings::render_max_tokens(const core::Engine& engine) {
    Terminal::clear_screen();
    std::cout << "================================================================================\n"
              << "  LXACE CONFIGURATION > MAX TOKENS\n"
              << "================================================================================\n"
              << "  Current Max Tokens: " << engine.config()->get().max_tokens << "\n\n"
              << "  Enter positive integer limit (empty or [Esc] to cancel):\n"
              << "  Limit: " << std::flush;
}

ViewState ViewSettings::handle_max_tokens(core::Engine& engine) {
    bool cancelled = false;
    std::string input = Terminal::read_interactive_line(&cancelled);
    if (!cancelled && !input.empty()) {
        try {
            int val = std::stoi(input);
            if (val > 0) {
                engine.config()->set_max_tokens(val);
                set_notification(UiNotification::Type::Success, "Max tokens set to " + std::to_string(val));
            } else {
                set_notification(UiNotification::Type::Error, "Max tokens must be a positive integer (> 0)");
            }
        } catch (...) {
            set_notification(UiNotification::Type::Error, "Invalid number for max tokens");
        }
    } else {
        set_notification(UiNotification::Type::Info, "Max tokens unchanged");
    }
    return ViewState::Settings;
}

void ViewSettings::render_temperature(const core::Engine& engine) {
    Terminal::clear_screen();
    char temp_buf[32];
    std::snprintf(temp_buf, sizeof(temp_buf), "%.2f", engine.config()->get().temperature);
    std::cout << "================================================================================\n"
              << "  LXACE CONFIGURATION > TEMPERATURE\n"
              << "================================================================================\n"
              << "  Current Temperature: " << temp_buf << "\n\n"
              << "  Enter floating-point value between 0.0 and 1.0 (empty or [Esc] to cancel):\n"
              << "  Temperature: " << std::flush;
}

ViewState ViewSettings::handle_temperature(core::Engine& engine) {
    bool cancelled = false;
    std::string input = Terminal::read_interactive_line(&cancelled);
    if (!cancelled && !input.empty()) {
        try {
            double val = std::stod(input);
            if (val >= 0.0 && val <= 1.0) {
                engine.config()->set_temperature(val);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.2f", val);
                set_notification(UiNotification::Type::Success, "Temperature set to " + std::string(buf));
            } else {
                set_notification(UiNotification::Type::Error, "Temperature must be between 0.0 and 1.0");
            }
        } catch (...) {
            set_notification(UiNotification::Type::Error, "Invalid decimal number for temperature");
        }
    } else {
        set_notification(UiNotification::Type::Info, "Temperature unchanged");
    }
    return ViewState::Settings;
}

} // namespace lxace::cli
