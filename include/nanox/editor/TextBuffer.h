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

    // --- line-level editing (nano ^K, Vim dd / yy / o / O / D) --------------
    // delete_current_line() and yank_line() hand the affected text back so the
    // editor can keep it in the clipboard; the buffer owns no clipboard itself.
    std::string delete_current_line();
    std::string yank_line() const;
    void delete_to_end_of_line();
    void insert_line_above();
    void insert_line_below();
    void insert_line_at(int index, std::string text);  // linewise paste

    // --- search (nano ^W / ^\) -------------------------------------------------
    // Both are plain substring, case-sensitive, and never cross a line break
    // (a needle containing '\n' is rejected).
    // find_forward() moves the cursor to the first match at or after it and
    // wraps to the top when there is none below; returns false (cursor
    // unchanged) when the needle is absent from the whole buffer.
    bool find_forward(const std::string& needle);
    // Replaces every match at or after the cursor. Returns the count; the
    // cursor lands on the first replacement. One undo step for the whole pass.
    std::size_t replace_all_forward(const std::string& needle,
                                    const std::string& replacement);

    // --- undo / redo ---------------------------------------------------------
    // Snapshot based. Consecutive character inserts coalesce into a single undo
    // unit, so undoing a typed word takes one step rather than one per letter;
    // any other edit starts a new unit. History is capped (oldest dropped).
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    void undo();
    void redo();

    bool dirty() const { return dirty_; }
    void mark_clean();

private:
    // The state a change replaced; restoring one is an undo step.
    struct Snapshot {
        std::vector<std::string> lines;
        int row = 0;
        int col = 0;
    };

    void record_undo(bool continues_typing);
    // True when the buffer is back at the state mark_clean() last recorded.
    bool at_saved_state() const;

    void clamp_cursor();
    std::string& current_line() { return lines_[static_cast<std::size_t>(row_)]; }
    const std::string& current_line() const { return lines_[static_cast<std::size_t>(row_)]; }

    std::vector<std::string> lines_;
    int row_ = 0;
    int col_ = 0;
    bool dirty_ = false;

    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    std::size_t saved_depth_ = 0;  // undo depth when the buffer was last clean
    bool typing_ = false;          // the previous edit was a character insert
    int typing_row_ = -1;          // ...at this position, so the next one joins it
    int typing_col_ = -1;
};

}  // namespace nanox::editor
