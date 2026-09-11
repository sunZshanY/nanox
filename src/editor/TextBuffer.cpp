#include "nanox/editor/TextBuffer.h"

#include <algorithm>
#include <utility>

namespace nanox::editor {

namespace {

// Undo history is capped so a long session cannot grow without bound; the
// oldest steps are dropped first.
constexpr std::size_t kMaxUndoDepth = 512;
// Sentinel for "the save point was dropped from the history", meaning the
// buffer can no longer be proven clean.
constexpr std::size_t kUnreachable = static_cast<std::size_t>(-1);

void expand_tabs(std::string& line) {
    std::size_t pos = 0;
    while ((pos = line.find('\t', pos)) != std::string::npos) {
        line.replace(pos, 1, "    ");
        pos += 4;
    }
}

}  // namespace

TextBuffer::TextBuffer() {
    lines_.push_back(std::string());
}

TextBuffer TextBuffer::from_string(const std::string& text) {
    TextBuffer buffer;
    buffer.lines_.clear();

    std::size_t start = 0;
    while (true) {
        const std::size_t newline = text.find('\n', start);
        const std::size_t length =
            (newline == std::string::npos) ? std::string::npos : newline - start;
        std::string line = text.substr(start, length);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // normalize CRLF input
        }
        expand_tabs(line);
        buffer.lines_.push_back(std::move(line));

        if (newline == std::string::npos) {
            break;
        }
        start = newline + 1;
    }

    // "abc\n" splits into ["abc", ""] — the trailing phantom line is dropped.
    if (buffer.lines_.size() > 1 && buffer.lines_.back().empty() && text.back() == '\n') {
        buffer.lines_.pop_back();
    }

    if (buffer.lines_.empty()) {
        buffer.lines_.push_back(std::string());
    }
    return buffer;
}

std::string TextBuffer::to_string() const {
    std::string out;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (i > 0) {
            out += '\n';
        }
        out += lines_[i];
    }
    if (!out.empty()) {
        out += '\n';
    }
    return out;
}

void TextBuffer::clamp_cursor() {
    row_ = std::clamp(row_, 0, static_cast<int>(lines_.size()) - 1);
    col_ = std::clamp(col_, 0, static_cast<int>(lines_[static_cast<std::size_t>(row_)].size()));
}

void TextBuffer::set_cursor(int row, int col) {
    row_ = row;
    col_ = col;
    clamp_cursor();
}

void TextBuffer::move_up() {
    if (row_ > 0) {
        --row_;
    }
    clamp_cursor();
}

void TextBuffer::move_down() {
    if (row_ + 1 < static_cast<int>(lines_.size())) {
        ++row_;
    }
    clamp_cursor();
}

void TextBuffer::move_left() {
    if (col_ > 0) {
        --col_;
    } else if (row_ > 0) {
        --row_;
        col_ = static_cast<int>(current_line().size());
    }
}

void TextBuffer::move_right() {
    if (col_ < static_cast<int>(current_line().size())) {
        ++col_;
    } else if (row_ + 1 < static_cast<int>(lines_.size())) {
        ++row_;
        col_ = 0;
    }
}

void TextBuffer::move_home() {
    col_ = 0;
}

void TextBuffer::move_end() {
    col_ = static_cast<int>(current_line().size());
}

void TextBuffer::move_page_up(int n) {
    row_ = std::max(0, row_ - std::max(1, n));
    clamp_cursor();
}

void TextBuffer::move_page_down(int n) {
    row_ = std::min(static_cast<int>(lines_.size()) - 1, row_ + std::max(1, n));
    clamp_cursor();
}

void TextBuffer::insert_char(char c) {
    // A character typed right where the previous one left off joins the same
    // undo unit, so a typed word undoes in one step.
    const bool continues = typing_ && row_ == typing_row_ && col_ == typing_col_;
    record_undo(continues);

    current_line().insert(static_cast<std::size_t>(col_), 1, c);
    ++col_;
    typing_ = true;
    typing_row_ = row_;
    typing_col_ = col_;
    dirty_ = true;
}

void TextBuffer::insert_text(const std::string& text) {
    record_undo(false);
    current_line().insert(static_cast<std::size_t>(col_), text);
    col_ += static_cast<int>(text.size());
    dirty_ = true;
}

void TextBuffer::insert_newline() {
    record_undo(false);
    std::string rest = current_line().substr(static_cast<std::size_t>(col_));
    current_line().erase(static_cast<std::size_t>(col_));
    lines_.insert(lines_.begin() + row_ + 1, std::move(rest));
    ++row_;
    col_ = 0;
    dirty_ = true;
}

void TextBuffer::backspace() {
    if (col_ > 0) {
        record_undo(false);
        current_line().erase(static_cast<std::size_t>(col_) - 1, 1);
        --col_;
    } else if (row_ > 0) {
        record_undo(false);
        // Join the current line onto the previous one.
        col_ = static_cast<int>(lines_[static_cast<std::size_t>(row_) - 1].size());
        lines_[static_cast<std::size_t>(row_) - 1] += current_line();
        lines_.erase(lines_.begin() + row_);
        --row_;
    } else {
        return;  // nothing to delete at the very start of the buffer
    }
    dirty_ = true;
}

void TextBuffer::delete_char() {
    if (col_ < static_cast<int>(current_line().size())) {
        record_undo(false);
        current_line().erase(static_cast<std::size_t>(col_), 1);
    } else if (row_ + 1 < static_cast<int>(lines_.size())) {
        record_undo(false);
        // Join the next line onto the current one.
        current_line() += lines_[static_cast<std::size_t>(row_) + 1];
        lines_.erase(lines_.begin() + row_ + 1);
    } else {
        return;  // nothing to delete at the very end of the buffer
    }
    dirty_ = true;
}

// ---------------------------------------------------------------------------
// Line-level editing
// ---------------------------------------------------------------------------

std::string TextBuffer::yank_line() const {
    return lines_[static_cast<std::size_t>(row_)];
}

std::string TextBuffer::delete_current_line() {
    if (lines_.size() == 1) {
        // Never leave the buffer with no line at all: clear it instead.
        if (lines_[0].empty()) {
            return {};  // already empty, nothing to cut and nothing to undo
        }
        record_undo(false);
        std::string removed = std::move(lines_[0]);
        lines_[0].clear();
        col_ = 0;
        dirty_ = true;
        return removed;
    }

    record_undo(false);
    std::string removed = lines_[static_cast<std::size_t>(row_)];
    lines_.erase(lines_.begin() + row_);
    clamp_cursor();
    dirty_ = true;
    return removed;
}

void TextBuffer::delete_to_end_of_line() {
    if (col_ >= static_cast<int>(current_line().size())) {
        return;  // nothing to the right of the cursor
    }
    record_undo(false);
    current_line().erase(static_cast<std::size_t>(col_));
    dirty_ = true;
}

void TextBuffer::insert_line_above() {
    record_undo(false);
    lines_.insert(lines_.begin() + row_, std::string());
    col_ = 0;
    dirty_ = true;
}

void TextBuffer::insert_line_below() {
    record_undo(false);
    lines_.insert(lines_.begin() + row_ + 1, std::string());
    ++row_;
    col_ = 0;
    dirty_ = true;
}

void TextBuffer::insert_line_at(int index, std::string text) {
    record_undo(false);
    const int at = std::clamp(index, 0, static_cast<int>(lines_.size()));
    lines_.insert(lines_.begin() + at, std::move(text));
    dirty_ = true;
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

bool TextBuffer::find_forward(const std::string& needle) {
    if (needle.empty() || needle.find('\n') != std::string::npos) {
        return false;
    }

    // Scan from the cursor to the end, then wrap to the top: nano's search
    // wraps, and stopping at the bottom would make it useless near the end.
    const std::size_t from_row = static_cast<std::size_t>(row_);
    const std::size_t from_col = static_cast<std::size_t>(col_);
    for (std::size_t pass = 0; pass < 2; ++pass) {
        const std::size_t begin_row = (pass == 0) ? from_row : 0;
        const std::size_t end_row = (pass == 0) ? lines_.size() : from_row + 1;
        for (std::size_t i = begin_row; i < end_row && i < lines_.size(); ++i) {
            const std::size_t start = (pass == 0 && i == from_row) ? from_col : 0;
            const std::size_t at = lines_[i].find(needle, start);
            if (at == std::string::npos) {
                continue;
            }
            row_ = static_cast<int>(i);
            col_ = static_cast<int>(at);
            return true;
        }
    }
    return false;
}

std::size_t TextBuffer::replace_all_forward(const std::string& needle,
                                            const std::string& replacement) {
    if (needle.empty() || needle.find('\n') != std::string::npos) {
        return 0;
    }

    std::size_t count = 0;
    int first_row = 0;
    int first_col = 0;

    const std::size_t from_row = static_cast<std::size_t>(row_);
    for (std::size_t i = from_row; i < lines_.size(); ++i) {
        const std::size_t start = (i == from_row) ? static_cast<std::size_t>(col_) : 0;
        std::size_t pos = lines_[i].find(needle, start);
        while (pos != std::string::npos) {
            if (count == 0) {
                record_undo(false);  // snapshot before the first change
                first_row = static_cast<int>(i);
                first_col = static_cast<int>(pos);
            }
            lines_[i].replace(pos, needle.size(), replacement);
            // Skip past the replacement so a needled inside it cannot re-match.
            pos = lines_[i].find(needle, pos + replacement.size());
            ++count;
        }
    }

    if (count > 0) {
        row_ = first_row;
        col_ = first_col;
        clamp_cursor();
        dirty_ = true;
    }
    return count;
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

void TextBuffer::record_undo(bool continues_typing) {
    if (continues_typing) {
        return;  // this typing group's snapshot is already on the stack
    }
    if (undo_.size() >= kMaxUndoDepth) {
        undo_.erase(undo_.begin());
        // Dropping the oldest entry shifts every depth down by one. If the save
        // point was that entry (or was already lost), the buffer can no longer
        // be proven clean -- so it stays "modified" rather than risking a
        // silently clean-looking buffer with unsaved changes.
        saved_depth_ = (saved_depth_ == kUnreachable || saved_depth_ == 0)
                           ? kUnreachable
                           : saved_depth_ - 1;
    }
    undo_.push_back(Snapshot{lines_, row_, col_});
    redo_.clear();
    typing_ = false;
}

bool TextBuffer::at_saved_state() const {
    return saved_depth_ != kUnreachable && undo_.size() == saved_depth_;
}

void TextBuffer::mark_clean() {
    dirty_ = false;
    saved_depth_ = undo_.size();
}

void TextBuffer::undo() {
    if (undo_.empty()) {
        return;
    }
    redo_.push_back(Snapshot{lines_, row_, col_});
    Snapshot previous = std::move(undo_.back());
    undo_.pop_back();

    lines_ = std::move(previous.lines);
    row_ = previous.row;
    col_ = previous.col;
    clamp_cursor();
    typing_ = false;
    dirty_ = !at_saved_state();
}

void TextBuffer::redo() {
    if (redo_.empty()) {
        return;
    }
    undo_.push_back(Snapshot{lines_, row_, col_});
    Snapshot next = std::move(redo_.back());
    redo_.pop_back();

    lines_ = std::move(next.lines);
    row_ = next.row;
    col_ = next.col;
    clamp_cursor();
    typing_ = false;
    dirty_ = !at_saved_state();
}

}  // namespace nanox::editor
