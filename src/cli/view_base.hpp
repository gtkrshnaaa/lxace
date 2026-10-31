#pragma once

#include "core/engine.hpp"

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

class ViewBase {
public:
    virtual ~ViewBase() = default;
    virtual void render(const core::Engine& engine) = 0;
    virtual ViewState handle_input(core::Engine& engine) = 0;
};

} // namespace lxace::cli
