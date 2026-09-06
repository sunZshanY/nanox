#include "nanox/editor/TextBuffer.h"

#include <algorithm>
#include <utility>

namespace nanox::editor {

namespace {

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
    current_line().insert(static_cast<std::size_t>(col_), 1, c);
    ++col_;
    dirty_ = true;
}

void TextBuffer::insert_text(const std::string& text) {
    current_line().insert(static_cast<std::size_t>(col_), text);
    col_ += static_cast<int>(text.size());
    dirty_ = true;
}

void TextBuffer::insert_newline() {
    std::string rest = current_line().substr(static_cast<std::size_t>(col_));
    current_line().erase(static_cast<std::size_t>(col_));
    lines_.insert(lines_.begin() + row_ + 1, std::move(rest));
    ++row_;
    col_ = 0;
    dirty_ = true;
}

void TextBuffer::backspace() {
    if (col_ > 0) {
        current_line().erase(static_cast<std::size_t>(col_) - 1, 1);
        --col_;
    } else if (row_ > 0) {
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
        current_line().erase(static_cast<std::size_t>(col_), 1);
    } else if (row_ + 1 < static_cast<int>(lines_.size())) {
        // Join the next line onto the current one.
        current_line() += lines_[static_cast<std::size_t>(row_) + 1];
        lines_.erase(lines_.begin() + row_ + 1);
    } else {
        return;  // nothing to delete at the very end of the buffer
    }
    dirty_ = true;
}

}  // namespace nanox::editor
