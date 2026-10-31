#pragma once

#include "core/engine.hpp"
#include "cli/view_base.hpp"
#include "cli/view_home.hpp"
#include "cli/view_settings.hpp"
#include "cli/view_prompt.hpp"
#include <memory>
#include <atomic>

namespace lxace::cli {

class CliApp {
public:
    explicit CliApp(std::shared_ptr<core::Engine> engine);
    ~CliApp() = default;

    void run();
    void stop();

private:
    std::shared_ptr<core::Engine> engine_;
    std::atomic<bool> running_{false};
    ViewState current_state_{ViewState::Home};

    ViewHome view_home_;
    ViewSettings view_settings_;
    ViewPrompt view_prompt_;
};

} // namespace lxace::cli
