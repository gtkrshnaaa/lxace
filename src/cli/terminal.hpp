#pragma once

#include <cstdint>
#include <string>

namespace lxace::cli {

struct TerminalSize {
    int rows{24};
    int cols{80};
};

class Terminal {
public:
    static TerminalSize get_size();
    static void clear_screen();
    static void move_cursor_home();
    static void enable_raw_mode();
    static void disable_raw_mode();
    static bool is_stdin_ready(int timeout_ms = 100);
    static char read_char();
    static std::string read_line();
};

} // namespace lxace::cli
