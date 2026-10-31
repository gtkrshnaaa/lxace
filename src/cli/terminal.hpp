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
    static int read_key_timeout(int timeout_ms = 100);
    static char read_char();
    static std::string read_line();
    static std::string read_masked_line(char mask_char = '*');
    static void install_signal_handlers();
    static bool check_and_reset_resized();
};

} // namespace lxace::cli
