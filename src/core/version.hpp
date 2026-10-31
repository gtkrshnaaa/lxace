#pragma once

#include <string>
#include <string_view>

namespace lxace::core {

inline constexpr std::string_view VERSION = "1.0.0";
inline constexpr std::string_view BUILD_ARCH = "Linux x86_64";
inline constexpr std::string_view CXX_STANDARD = "C++20";

[[nodiscard]] inline std::string format_version() {
    return "lxace version " + std::string(VERSION) + " (" + std::string(BUILD_ARCH) + ", " + std::string(CXX_STANDARD) + ")";
}

[[nodiscard]] inline std::string format_version_short() {
    return std::string(VERSION);
}

} // namespace lxace::core
