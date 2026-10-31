#include "cli/terminal.hpp"
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <iostream>

namespace lxace::cli {

static struct termios orig_termios;
static bool raw_mode_enabled = false;

TerminalSize Terminal::get_size() {
    TerminalSize size;
    struct winsize ws{};
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        size.rows = ws.ws_row;
        size.cols = ws.ws_col;
    }
    return size;
}

void Terminal::clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void Terminal::move_cursor_home() {
    std::cout << "\033[H" << std::flush;
}

void Terminal::enable_raw_mode() {
    if (raw_mode_enabled) return;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == 0) {
        struct termios raw = orig_termios;
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        raw_mode_enabled = true;
    }
}

void Terminal::disable_raw_mode() {
    if (!raw_mode_enabled) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_mode_enabled = false;
}

bool Terminal::is_stdin_ready(int timeout_ms) {
    struct pollfd pfd{};
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    return poll(&pfd, 1, timeout_ms) > 0;
}

char Terminal::read_char() {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return c;
    }
    return 0;
}

std::string Terminal::read_line() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

} // namespace lxace::cli
