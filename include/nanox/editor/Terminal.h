#pragma once

#include <cstdint>
#include <string_view>

#ifndef _WIN32
#include <termios.h>
#endif

namespace nanox::editor {

// A single input event, normalized across platforms.
//
// Ctrl keys arrive as kind == Ctrl with `ch` set to the lowercase letter
// ('a'..'z'), e.g. Ctrl+S is {Ctrl, 's'}. The punctuation controls carry their
// base character instead: Ctrl+\ is {Ctrl, '\\'}, Ctrl+_ is {Ctrl, '_'} (which
// is also what Ctrl+/ sends). Alt keys arrive as kind == Alt with the lowercase
// letter or digit, e.g. Alt+U is {Alt, 'u'}, Alt+6 is {Alt, '6'}. This is
// identical on Windows and POSIX, so the rest of the editor never needs
// platform code.
//
// Escape and Alt are unambiguous here even though POSIX sends both as a bare
// ESC byte: see read_escape_sequence(), which disambiguates with a short poll.
struct Key {
    enum class Kind : std::uint8_t {
        None,
        Char,     // printable character, in `ch`
        Enter,
        Tab,
        Backspace,
        Delete,
        ArrowUp,
        ArrowDown,
        ArrowLeft,
        ArrowRight,
        Home,
        End,
        PageUp,
        PageDown,
        Function, // F1..F12, number in `param`
        Ctrl,     // control combination, base character in `ch`
        Alt,      // Alt + letter/digit, lowercase character in `ch`
        Escape,
        Unknown,  // a recognized-but-unhandled sequence
        Resize,   // terminal size changed while waiting for input
        Eof,      // input stream ended
    };

    Kind kind = Kind::None;
    char ch = 0;
    unsigned param = 0;  // Function: F-key number (1-12)
};

// Small terminal abstraction that isolates every platform difference:
//   * Windows:  console API (_getch scan codes) + ANSI output via
//               ENABLE_VIRTUAL_TERMINAL_PROCESSING.
//   * POSIX:    termios raw mode + ANSI output.
//
// The constructor switches the terminal into raw mode; the destructor restores
// the previous settings. Rendering always uses ANSI escape sequences, which
// both platforms support.
class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    // Blocks until one input event is available and returns it. On a TTY,
    // input is polled while waiting, so a terminal resize is reported as
    // {Kind::Resize} even when no key was pressed.
    Key read_key();

    void write(std::string_view text);
    void flush();

    // Queries the terminal size. Returns false (with a 24x80 fallback) when
    // the size cannot be determined (e.g. output is redirected to a file).
    bool get_size(int& rows, int& cols) const;

private:
#ifndef _WIN32
    bool try_read_byte(char& out);
    Key read_escape_sequence();
#endif

#ifdef _WIN32
    void* h_in_ = nullptr;    // HANDLE
    void* h_out_ = nullptr;   // HANDLE
    unsigned long old_in_mode_ = 0;
    unsigned long old_out_mode_ = 0;
    bool in_is_console_ = false;  // stdin is a console (not redirected)
#else
    termios old_termios_{};
#endif
    bool raw_active_ = false;
    int cached_rows_ = 0;  // last size seen by read_key's resize poll
    int cached_cols_ = 0;
};

}  // namespace nanox::editor
