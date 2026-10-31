#pragma once

#include "cli/view_base.hpp"

namespace lxace::cli {

class ViewPrompt : public ViewBase {
public:
    void render(const core::Engine& engine) override;
    ViewState handle_input(core::Engine& engine) override;
};

} // namespace lxace::cli
