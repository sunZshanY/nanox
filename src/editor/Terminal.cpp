#include "nanox/editor/Terminal.h"

#include <cstdio>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00  // Windows 10+: needed for ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
#include <windows.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <cstdlib>
#else
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

namespace nanox::editor {

namespace {

Key make_key(Key::Kind kind, char ch = 0, unsigned param = 0) {
    Key key;
    key.kind = kind;
    key.ch = ch;
    key.param = param;
    return key;
}

}  // namespace

#ifdef _WIN32

Terminal::Terminal() {
    h_in_ = GetStdHandle(STD_INPUT_HANDLE);
    h_out_ = GetStdHandle(STD_OUTPUT_HANDLE);

    if (GetConsoleMode(h_in_, &old_in_mode_)) {
        in_is_console_ = true;
        // Raw input: no echo, no line buffering, no Ctrl+C interception.
        // Extended scan codes (arrows, etc.) still arrive as 0x00/0xE0
        // prefixed pairs, which read_key() handles.
        DWORD in_mode = old_in_mode_;
        in_mode &= ~(DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                            ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
        in_mode |= ENABLE_EXTENDED_FLAGS;
        SetConsoleMode(h_in_, in_mode);
    } else {
        // Redirected stdin (pipe/file): _getch() would still read the real
        // console, so switch to binary byte reads instead.
        _setmode(_fileno(stdin), _O_BINARY);
    }

    if (GetConsoleMode(h_out_, &old_out_mode_)) {
        // ANSI escape output + no automatic CR insertion.
        DWORD out_mode = old_out_mode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                         DISABLE_NEWLINE_AUTO_RETURN;
        out_mode &= ~(DWORD)ENABLE_WRAP_AT_EOL_OUTPUT;
        SetConsoleMode(h_out_, out_mode);
    }

    // Prevent the CRT from translating '\n' to "\r\n"; the editor emits
    // explicit "\r\n" itself. Also switch the console to UTF-8 so the
    // box-drawing characters used by the TUI render correctly.
    _setmode(_fileno(stdout), _O_BINARY);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    raw_active_ = true;
    get_size(cached_rows_, cached_cols_);
}

Terminal::~Terminal() {
    if (raw_active_) {
        SetConsoleMode(h_in_, old_in_mode_);
        SetConsoleMode(h_out_, old_out_mode_);
    }
}

Key Terminal::read_key() {
    if (!in_is_console_) {
        // Redirected stdin (pipe/file): plain byte reads. No scan codes, no
        // console polling — arrows/F-keys simply do not exist in a file.
        const int c = std::fgetc(stdin);
        if (c == EOF || c < 0) {
            return make_key(Key::Kind::Eof);
        }
        if (c == '\r' || c == '\n') {
            return make_key(Key::Kind::Enter);
        }
        if (c == '\t') {
            return make_key(Key::Kind::Tab);
        }
        if (c == 8 || c == 127) {
            return make_key(Key::Kind::Backspace);
        }
        if (c == 27) {
            return make_key(Key::Kind::Escape);
        }
        if (c >= 1 && c <= 26) {
            return make_key(Key::Kind::Ctrl, static_cast<char>('a' + c - 1));
        }
        return make_key(Key::Kind::Char, static_cast<char>(c & 0xFF));
    }
    if (in_is_console_) {
        // Poll for input so a resize is noticed even without a keypress.
        for (;;) {
            if (_kbhit() != 0) {
                break;
            }
            int rows = 0;
            int cols = 0;
            get_size(rows, cols);
            if (rows != cached_rows_ || cols != cached_cols_) {
                cached_rows_ = rows;
                cached_cols_ = cols;
                return make_key(Key::Kind::Resize);
            }
            Sleep(50);
        }
    }
    const int c = _getch();
    // With redirected (non-console) stdin, MS _getch reports EOF as 0x1A
    // (DOS EOF); without this the editor would spin on it forever.
    if (c < 0 || (c == 0x1A && !in_is_console_)) {
        return make_key(Key::Kind::Eof);
    }
    if (c == 0 || c == 0xE0) {  // extended key, next byte is the scan code
        const int scan = _getch();
        switch (scan) {
            case 72: return make_key(Key::Kind::ArrowUp);
            case 80: return make_key(Key::Kind::ArrowDown);
            case 75: return make_key(Key::Kind::ArrowLeft);
            case 77: return make_key(Key::Kind::ArrowRight);
            case 71: return make_key(Key::Kind::Home);
            case 79: return make_key(Key::Kind::End);
            case 73: return make_key(Key::Kind::PageUp);
            case 81: return make_key(Key::Kind::PageDown);
            case 83: return make_key(Key::Kind::Delete);
            case 133: return make_key(Key::Kind::Function, 0, 11);  // F11
            case 134: return make_key(Key::Kind::Function, 0, 12);  // F12
            default: break;
        }
        if (scan >= 59 && scan <= 68) {  // F1..F10
            return make_key(Key::Kind::Function, 0, static_cast<unsigned>(scan - 58));
        }
        return make_key(Key::Kind::Unknown);
    }
    if (c == '\r' || c == '\n') {
        return make_key(Key::Kind::Enter);
    }
    if (c == '\t') {
        return make_key(Key::Kind::Tab);
    }
    if (c == 8) {
        return make_key(Key::Kind::Backspace);
    }
    if (c == 27) {
        return make_key(Key::Kind::Escape);
    }
    if (c >= 1 && c <= 26) {
        return make_key(Key::Kind::Ctrl, static_cast<char>('a' + c - 1));
    }
    if (c < 0) {
        return make_key(Key::Kind::Eof);
    }
    return make_key(Key::Kind::Char, static_cast<char>(c & 0xFF));
}

bool Terminal::get_size(int& rows, int& cols) const {
    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (GetConsoleScreenBufferInfo(h_out_, &csbi)) {
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        if (rows > 0 && cols > 0) {
            return true;
        }
    }
    // LINES/COLUMNS override, mirroring the POSIX branch: lets tests and
    // headless runs pin the terminal size when output is redirected.
    const char* lines_env = std::getenv("LINES");
    const char* cols_env = std::getenv("COLUMNS");
    if (lines_env != nullptr && cols_env != nullptr) {
        rows = std::atoi(lines_env);
        cols = std::atoi(cols_env);
        if (rows > 0 && cols > 0) {
            return true;
        }
    }
    rows = 24;
    cols = 80;
    return false;
}

#else  // POSIX (Linux / macOS)

Terminal::Terminal() {
    if (tcgetattr(STDIN_FILENO, &old_termios_) != 0) {
        raw_active_ = false;  // not a TTY; keep default behavior
        get_size(cached_rows_, cached_cols_);
        return;
    }

    termios raw = old_termios_;
    // Raw mode: no canonical line buffering, no echo, no signal chars
    // (Ctrl+C arrives as a plain byte and is handled as a key), no XON/XOFF
    // flow control (Ctrl+S/Ctrl+Q must reach the editor).
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL | INPCK | ISTRIP | BRKINT);
    raw.c_oflag &= ~static_cast<tcflag_t>(OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1;   // read() returns as soon as 1 byte is available
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_active_ = true;
    get_size(cached_rows_, cached_cols_);
}

Terminal::~Terminal() {
    if (raw_active_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios_);
    }
}

bool Terminal::try_read_byte(char& out) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 50 * 1000;  // 50 ms: enough for a contiguous ESC sequence
    const int ready = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
    if (ready <= 0) {
        return false;
    }
    return read(STDIN_FILENO, &out, 1) == 1;
}

Key Terminal::read_escape_sequence() {
    // After ESC we may have "[" (CSI) or "O" (SS3) followed by a final byte,
    // e.g. ESC [ A = ArrowUp, ESC [ 3 ~ = Delete, ESC [ 15 ~ = F5.
    char next = 0;
    if (!try_read_byte(next)) {
        return make_key(Key::Kind::Escape);
    }
    if (next == 'O') {
        char final = 0;
        if (!try_read_byte(final)) {
            return make_key(Key::Kind::Escape);
        }
        switch (final) {
            case 'A': return make_key(Key::Kind::ArrowUp);
            case 'B': return make_key(Key::Kind::ArrowDown);
            case 'C': return make_key(Key::Kind::ArrowRight);
            case 'D': return make_key(Key::Kind::ArrowLeft);
            case 'H': return make_key(Key::Kind::Home);
            case 'F': return make_key(Key::Kind::End);
            case 'P': return make_key(Key::Kind::Function, 0, 1);  // F1
            case 'Q': return make_key(Key::Kind::Function, 0, 2);  // F2
            case 'R': return make_key(Key::Kind::Function, 0, 3);  // F3
            case 'S': return make_key(Key::Kind::Function, 0, 4);  // F4
            default:  return make_key(Key::Kind::Unknown);
        }
    }
    if (next != '[') {
        return make_key(Key::Kind::Escape);  // Alt+key: treat as plain Escape
    }

    char param = 0;
    if (!try_read_byte(param)) {
        return make_key(Key::Kind::Escape);
    }
    switch (param) {
        case 'A': return make_key(Key::Kind::ArrowUp);
        case 'B': return make_key(Key::Kind::ArrowDown);
        case 'C': return make_key(Key::Kind::ArrowRight);
        case 'D': return make_key(Key::Kind::ArrowLeft);
        case 'H': return make_key(Key::Kind::Home);
        case 'F': return make_key(Key::Kind::End);
        default: break;
    }

    // Digit sequence terminated by '~': ESC [ <n> ~  (Home, F-keys, ...).
    if (param < '0' || param > '9') {
        return make_key(Key::Kind::Unknown);
    }
    std::string digits(1, param);
    for (int i = 0; i < 3; ++i) {
        char c = 0;
        if (!try_read_byte(c)) {
            return make_key(Key::Kind::Unknown);
        }
        if (c == '~') {
            break;
        }
        if ((c >= '0' && c <= '9') || c == ';') {
            digits += c;
            continue;
        }
        return make_key(Key::Kind::Unknown);
    }

    const int n = std::atoi(digits.c_str());
    switch (n) {
        case 1:
        case 7:  return make_key(Key::Kind::Home);
        case 3:  return make_key(Key::Kind::Delete);
        case 4:
        case 8:  return make_key(Key::Kind::End);
        case 5:  return make_key(Key::Kind::PageUp);
        case 6:  return make_key(Key::Kind::PageDown);
        case 11:
        case 12:
        case 13:
        case 14: return make_key(Key::Kind::Function, 0, static_cast<unsigned>(n - 10));
        case 15: return make_key(Key::Kind::Function, 0, 5);
        case 17: return make_key(Key::Kind::Function, 0, 6);
        case 18: return make_key(Key::Kind::Function, 0, 7);
        case 19: return make_key(Key::Kind::Function, 0, 8);
        case 20: return make_key(Key::Kind::Function, 0, 9);
        case 21: return make_key(Key::Kind::Function, 0, 10);
        case 23: return make_key(Key::Kind::Function, 0, 11);
        case 24: return make_key(Key::Kind::Function, 0, 12);
        default: return make_key(Key::Kind::Unknown);
    }
}

Key Terminal::read_key() {
    if (raw_active_) {
        // Poll for input so a resize is noticed even without a keypress.
        for (;;) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 200 * 1000;  // 200 ms poll interval
            const int ready = select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout);
            if (ready > 0) {
                break;
            }
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return make_key(Key::Kind::Eof);
            }
            int rows = 0;
            int cols = 0;
            get_size(rows, cols);
            if (rows != cached_rows_ || cols != cached_cols_) {
                cached_rows_ = rows;
                cached_cols_ = cols;
                return make_key(Key::Kind::Resize);
            }
        }
    }
    unsigned char c = 0;
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return make_key(Key::Kind::Eof);
    }
    if (c == '\r' || c == '\n') {
        return make_key(Key::Kind::Enter);
    }
    if (c == '\t') {
        return make_key(Key::Kind::Tab);
    }
    if (c == 127 || c == 8) {  // DEL or Ctrl+H are both Backspace
        return make_key(Key::Kind::Backspace);
    }
    if (c >= 1 && c <= 26) {
        return make_key(Key::Kind::Ctrl, static_cast<char>('a' + c - 1));
    }
    if (c == 27) {
        return read_escape_sequence();
    }
    return make_key(Key::Kind::Char, static_cast<char>(c));
}

bool Terminal::get_size(int& rows, int& cols) const {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
        return true;
    }
    const char* lines_env = std::getenv("LINES");
    const char* cols_env = std::getenv("COLUMNS");
    if (lines_env != nullptr && cols_env != nullptr) {
        rows = std::atoi(lines_env);
        cols = std::atoi(cols_env);
        if (rows > 0 && cols > 0) {
            return true;
        }
    }
    rows = 24;
    cols = 80;
    return false;
}

#endif

void Terminal::write(std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
}

void Terminal::flush() {
    std::fflush(stdout);
}

}  // namespace nanox::editor
