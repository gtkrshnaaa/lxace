#pragma once

#include "cli/view_base.hpp"

namespace lxace::cli {

class ViewSettings : public ViewBase {
public:
    void render(const core::Engine& engine) override;
    ViewState handle_input(core::Engine& engine) override;

    static void render_base_url(const core::Engine& engine);
    static ViewState handle_base_url(core::Engine& engine);

    static void render_api_key(const core::Engine& engine);
    static ViewState handle_api_key(core::Engine& engine);

    static void render_model(const core::Engine& engine);
    static ViewState handle_model(core::Engine& engine);

    static void render_max_tokens(const core::Engine& engine);
    static ViewState handle_max_tokens(core::Engine& engine);

    static void render_temperature(const core::Engine& engine);
    static ViewState handle_temperature(core::Engine& engine);
};

} // namespace lxace::cli
