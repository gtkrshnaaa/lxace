#pragma once

#include "core/engine.hpp"
#include <string>
#include <utility>

namespace lxace::cli {

enum class ViewState {
    Home,
    Settings,
    SettingsBaseUrl,
    SettingsApiKey,
    SettingsModel,
    SettingsMaxTokens,
    SettingsTemperature,
    WritePrompt,
    Exit
};

struct UiNotification {
    enum class Type { None, Info, Success, Warning, Error };
    Type type{Type::None};
    std::string text;

    void clear() {
        type = Type::None;
        text.clear();
    }

    [[nodiscard]] bool empty() const {
        return text.empty() || type == Type::None;
    }

    [[nodiscard]] std::string format() const {
        if (empty()) return "";
        std::string prefix;
        switch (type) {
            case Type::Success: prefix = "[OK] "; break;
            case Type::Warning: prefix = "[WARN] "; break;
            case Type::Error:   prefix = "[ERROR] "; break;
            case Type::Info:
            default:            prefix = "[INFO] "; break;
        }
        return prefix + text;
    }
};

class ViewBase {
public:
    virtual ~ViewBase() = default;
    virtual void render(const core::Engine& engine) = 0;
    virtual ViewState handle_input(core::Engine& engine) = 0;

    void set_notification(UiNotification::Type type, std::string text) {
        notification_.type = type;
        notification_.text = std::move(text);
    }

    void clear_notification() {
        notification_.clear();
    }

    [[nodiscard]] const UiNotification& notification() const {
        return notification_;
    }

protected:
    UiNotification notification_;
};

} // namespace lxace::cli
