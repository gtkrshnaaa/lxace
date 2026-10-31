#pragma once

#include "cli/view_base.hpp"
#include <string>

namespace lxace::cli {

class ViewHome : public ViewBase {
public:
    void render(const core::Engine& engine) override;
    ViewState handle_input(core::Engine& engine) override;
};

} // namespace lxace::cli
