#pragma once

#include <string_view>

namespace lxace::tool {

// Directories that must never be traversed by filesystem tools. `.lxace` holds the
// engine state directory, including `config.json` with a plaintext API key, so scanning
// it leaked secrets into tool results and the SSE broadcast.
[[nodiscard]] inline bool is_ignored_directory(std::string_view dirname) {
    return dirname == ".git" || dirname == ".lxace" || dirname == "build" ||
           dirname == ".aicodeeditor_context_archive" ||
           dirname == ".aicodeeditor_context_window";
}

} // namespace lxace::tool
