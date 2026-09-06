#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nanox::editor {

// An editable text buffer: a sequence of lines plus a cursor (row, col).
//
// Design note: this class is deliberately free of any terminal/UI knowledge.
// The TUI (Editor) only *reads* the buffer and *applies* key events; the buffer
// owns all editing invariants (cursor clamping, line joins/splits, dirty
// tracking). Because it has no I/O dependencies, it can be unit-tested on all
// three platforms without a terminal.
//
// Cursor convention: row/col are 0-based; col ranges from 0 to line length
// (so col == line.size() means "just past the last character").
class TextBuffer {
public:
    TextBuffer();

    // Builds a buffer from file/text content. Lines are split on '\n', a
    // single trailing newline does not produce a phantom last line, and tab
    // characters are expanded to 4 spaces (the editor itself never stores
    // tabs; this keeps column math exact on every terminal).
    static TextBuffer from_string(const std::string& text);

    // Serializes the buffer back to text (LF line endings, one trailing '\n'
    // unless the buffer is empty). from_string(to_string()) round-trips.
    std::string to_string() const;

    int row() const { return row_; }
    int col() const { return col_; }
    std::size_t line_count() const { return lines_.size(); }
    const std::vector<std::string>& lines() const { return lines_; }

    // Sets the cursor to an arbitrary position (clamped into range).
    void set_cursor(int row, int col);

    void move_up();
    void move_down();
    void move_left();
    void move_right();
    void move_home();
    void move_end();
    void move_page_up(int n);
    void move_page_down(int n);

    void insert_char(char c);
    void insert_text(const std::string& text);
    void insert_newline();
    void backspace();
    void delete_char();

    bool dirty() const { return dirty_; }
    void mark_clean() { dirty_ = false; }

private:
    void clamp_cursor();
    std::string& current_line() { return lines_[static_cast<std::size_t>(row_)]; }
    const std::string& current_line() const { return lines_[static_cast<std::size_t>(row_)]; }

    std::vector<std::string> lines_;
    int row_ = 0;
    int col_ = 0;
    bool dirty_ = false;
};

}  // namespace nanox::editor
