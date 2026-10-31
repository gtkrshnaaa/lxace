#include "cli/terminal.hpp"
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <csignal>
#include <atomic>
#include <iostream>

namespace lxace::cli {

static struct termios orig_termios;
static bool raw_mode_enabled = false;
static std::atomic<bool> g_window_resized{false};

static void sigwinch_handler(int) {
    g_window_resized.store(true, std::memory_order_relaxed);
}

void Terminal::install_signal_handlers() {
    struct sigaction sa{};
    sa.sa_handler = sigwinch_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, nullptr);
    std::atexit(Terminal::disable_raw_mode);
}

bool Terminal::check_and_reset_resized() {
    return g_window_resized.exchange(false, std::memory_order_relaxed);
}

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

int Terminal::read_key_timeout(int timeout_ms) {
    if (!is_stdin_ready(timeout_ms)) {
        return -1;
    }
    char c = 0;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) {
        return static_cast<unsigned char>(c);
    }
    return -1;
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
