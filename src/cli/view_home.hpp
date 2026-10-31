#pragma once

#include "cli/view_base.hpp"
#include <string>

namespace lxace::cli {

class ViewHome : public ViewBase {
public:
    void render(const core::Engine& engine) override;
    ViewState handle_input(core::Engine& engine) override;

private:
    uint64_t last_rendered_version_{0};
    int last_rows_{0};
    int last_cols_{0};
};

} // namespace lxace::cli
