#include "nanox/editor/Editor.h"

#include "nanox/Lexer.h"
#include "nanox/Project.h"
#include "nanox/TokenKind.h"
#include "nanox/editor/CommandParser.h"
#include "nanox/editor/DisplayWidth.h"
#include "nanox/editor/Keymap.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>

namespace nanox::editor {

namespace fs = std::filesystem;

namespace {

// Layout constants. Rows used: top border, title, separator, header (tabs),
// content rows, separator, output header, output_body_rows_ body rows,
// separator, function bar, bottom border = content + output_body_rows_ + 9.
// The output body grows with its content: 1 row ("No output") when empty,
// kMinOutputRows..kMaxOutputRows otherwise.
constexpr int kMinOutputRows = 4;
constexpr int kMaxOutputRows = 12;
constexpr int kMinContentRows = 4;
constexpr int kMinRows = 16;
constexpr int kMinCols = 40;

// ANSI colors.
constexpr const char* kReset = "\x1b[0m";
constexpr const char* kInverse = "\x1b[7m";
constexpr const char* kColorKeyword = "\x1b[94m";     // bright blue
constexpr const char* kColorString = "\x1b[32m";      // green
constexpr const char* kColorNumber = "\x1b[33m";      // yellow
constexpr const char* kColorInvalid = "\x1b[31;1m";   // bright red
constexpr const char* kColorGutter = "\x1b[90m";      // dim gray
constexpr const char* kColorErrorLine = "\x1b[31;1m"; // bright red
constexpr const char* kColorGreen = "\x1b[32m";
constexpr const char* kColorYellow = "\x1b[33m";
constexpr const char* kColorRed = "\x1b[31;1m";
constexpr const char* kColorCyan = "\x1b[36m";

const char* color_for(TokenKind kind) {
    switch (kind) {
        case TokenKind::Fn:
        case TokenKind::Let:
        case TokenKind::If:
        case TokenKind::Else:
        case TokenKind::While:
        case TokenKind::Return:
        case TokenKind::Int:
        case TokenKind::Float:
        case TokenKind::Bool:
        case TokenKind::String:
        case TokenKind::Void:
        case TokenKind::True:
        case TokenKind::False:
            return kColorKeyword;
        case TokenKind::StringLiteral:
            return kColorString;
        case TokenKind::IntegerLiteral:
        case TokenKind::FloatLiteral:
            return kColorNumber;
        case TokenKind::Invalid:
            return kColorInvalid;
        default:
            return nullptr;
    }
}

void append_repeat(std::string& out, const std::string& unit, int n) {
    for (int i = 0; i < n; ++i) {
        out += unit;
    }
}

// Moves `end` backwards until it no longer splits a UTF-8 sequence. The editor
// viewport budgets a line in bytes, so the cut can land inside a multi-byte
// character; emitting the leading bytes would make the terminal print a
// replacement glyph and break the row width.
std::size_t floor_utf8_boundary(const std::string& s, std::size_t end) {
    if (end > s.size()) {
        end = s.size();
    }
    while (end > 0 && end < s.size() &&
           (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80) {
        --end;
    }
    return end;
}

// Parses a plain non-negative decimal, used by the "line,column" prompt.
// Deliberately not std::stoi: a rejected input must be reported, not thrown.
bool parse_decimal(const std::string& text, int& out) {
    if (text.empty() || text.size() > 9) {
        return false;
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

std::string cell(const std::string& text, int width) {
    std::string out = truncate_to_width(text, static_cast<std::size_t>(std::max(0, width)));
    append_repeat(out, " ", std::max(0, width - static_cast<int>(display_width(out))));
    return out;
}

const char* severity_name(Severity severity) {
    return severity == Severity::Error ? "error" : "warning";
}

}  // namespace

Editor::Editor(std::string workspace, std::string open_file, std::string initial_text,
               EditingMode mode)
    : workspace_(std::move(workspace)), tree_(FileTree::scan(workspace_)) {
    set_editing_mode(mode);
    OpenFile first;
    if (!open_file.empty()) {
        first.path = open_file;
        first.buffer = TextBuffer::from_string(initial_text);
    }
    files_.push_back(std::move(first));
}

Editor::~Editor() = default;

int Editor::run() {
    quit_ = false;
    while (!quit_) {
        analyze();
        if (help_visible_) {
            render_help();
        } else {
            render();
        }
        const Key key = terminal_.read_key();
        if (key.kind == Key::Kind::Eof) {
            break;  // input stream ended (e.g. redirected stdin)
        }
        if (key.kind == Key::Kind::Resize) {
            continue;  // re-render with the new terminal size
        }
        if (help_visible_) {
            help_visible_ = false;
            continue;
        }
        if (prompt_mode_) {
            handle_prompt_key(key);
            continue;
        }
        if (key.kind == Key::Kind::Function) {
            handle_function_key(key.param);
            continue;
        }
        // The editing keymap is live only while the editor pane has focus; the
        // tree and output panels keep their own plain navigation keys, and the
        // global ^S/^Q/^T shortcuts below still work from any pane.
        if (focus_ == Focus::Editor) {
            feed_editor_key(key);
            continue;
        }
        if (key.kind == Key::Kind::Ctrl) {
            handle_ctrl_key(key.ch);
            continue;
        }
        if (key.kind == Key::Kind::Escape) {
            if (output_mode_ == OutputMode::Repl) {
                leave_repl();
            }
            continue;
        }
        switch (focus_) {
            case Focus::Explorer: explorer_key(key); break;
            case Focus::Output:   output_key(key);   break;
            case Focus::Editor:   editor_key(key);   break;
        }
    }
    // Restore a visible cursor on a fresh line: frames end at the bottom-right
    // cell (no trailing newline), so the shell prompt needs a clean line.
    terminal_.write("\x1b[?25h\r\n");
    terminal_.flush();
    return 0;
}

void Editor::analyze() {
    diagnostics_.clear();
    const TextBuffer& buffer = files_[current_file_].buffer;
    spans_.assign(buffer.line_count(), {});

    const std::string text = buffer.to_string();
    const std::string diag_name =
        files_[current_file_].path.empty() ? std::string("(untitled)") : files_[current_file_].path;

    DiagnosticEngine diag(diag_name);
    Lexer lexer(text, diag_name, diag);

    token_count_ = 0;
    while (true) {
        const Token token = lexer.next();
        if (token.is_eof()) {
            break;
        }
        ++token_count_;

        const char* color = color_for(token.kind);
        if (color == nullptr) {
            continue;
        }
        const std::size_t line_idx = static_cast<std::size_t>(token.location.line - 1);
        if (line_idx >= spans_.size()) {
            continue;
        }
        const std::size_t begin = static_cast<std::size_t>(token.location.column - 1);
        spans_[line_idx].push_back(Span{begin, begin + token.lexeme.size(), color});
    }
    diagnostics_ = diag.diagnostics();
}

int Editor::desired_output_body_rows() const {
    if (output_mode_ == OutputMode::Repl) {
        return std::clamp(static_cast<int>(repl_output_.size()) + 1,
                          kMinOutputRows, kMaxOutputRows);
    }
    if (output_lines_.empty()) {
        return 1;  // just the "No output" placeholder
    }
    return std::clamp(static_cast<int>(output_lines_.size()),
                      kMinOutputRows, kMaxOutputRows);
}

void Editor::render() {
    int rows = 0;
    int cols = 0;
    terminal_.get_size(rows, cols);

    std::string out;
    out.reserve(static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols) + 1024);
    out += "\x1b[?25l";  // hide cursor while redrawing
    out += "\x1b[H";     // move to home
    out += "\x1b[J";     // erase stale content below/right (resize leftovers)

    if (rows < kMinRows || cols < kMinCols) {
        out += "terminal too small (need at least 40x16)";
        terminal_.write(out);
        terminal_.flush();
        return;
    }

    const int inner_w = cols - 2;
    output_body_rows_ = std::min(
        desired_output_body_rows(),
        std::max(1, rows - 9 - kMinContentRows));  // keep room for the editor
    content_rows_ = rows - output_body_rows_ - 9;
    const int explorer_w = std::clamp(cols / 4, 12, 24);
    // Width invariant: every row is exactly `cols` cells long.
    //   top/bottom:  1 + inner_w + 1 = cols
    //   title:       2 + 11 + 1 + avail + 1 + status + 2 = cols
    //   split rows:  1 + explorer_w + 1 + editor_w + 1 = cols
    //   body rows:   2 + (inner_w - 1) + 1 = cols
    const int editor_w = cols - explorer_w - 3;

    OpenFile& cur = files_[current_file_];
    TextBuffer& buffer = cur.buffer;
    const bool no_file = cur.path.empty() && !buffer.dirty();

    const std::size_t gutter_w =
        std::max<std::size_t>(std::to_string(buffer.line_count()).size() + 1, 2);
    const int text_w = editor_w - static_cast<int>(gutter_w);

    // --- scrolling: keep the cursor inside the visible window ---------------
    if (buffer.row() < cur.first_row) {
        cur.first_row = buffer.row();
    }
    if (buffer.row() >= cur.first_row + content_rows_) {
        cur.first_row = buffer.row() - content_rows_ + 1;
    }
    if (buffer.col() < cur.first_col) {
        cur.first_col = buffer.col();
    }
    if (text_w > 1 && buffer.col() >= cur.first_col + text_w) {
        cur.first_col = buffer.col() - text_w + 1;
    }
    cur.first_row = std::max(0, cur.first_row);
    cur.first_col = std::max(0, cur.first_col);
    if (cur.first_row > static_cast<int>(buffer.line_count()) - 1) {
        cur.first_row = static_cast<int>(buffer.line_count()) - 1;
    }

    // --- tree selection and scroll -------------------------------------------
    const std::vector<std::size_t> vis = tree_.visible();
    if (!vis.empty() && tree_selected_ >= vis.size()) {
        tree_selected_ = vis.size() - 1;
    }
    if (static_cast<int>(tree_selected_) < tree_scroll_) {
        tree_scroll_ = static_cast<int>(tree_selected_);
    }
    if (static_cast<int>(tree_selected_) >= tree_scroll_ + content_rows_) {
        tree_scroll_ = static_cast<int>(tree_selected_) - content_rows_ + 1;
    }
    tree_scroll_ = std::max(0, tree_scroll_);

    // --- title bar -------------------------------------------------------------
    const char* status_color = kColorGreen;
    std::string status_text = "Ready";
    if (any_dirty()) {
        status_color = kColorYellow;
        status_text = "Modified";
    } else if (last_build_failed_) {
        status_color = kColorRed;
        status_text = "Errors";
    }
    const std::string status = "● " + status_text;

    out += "┌";
    append_repeat(out, "─", inner_w);
    out += "┐\r\n";

    out += "│ ";
    // The editing state sits next to the version: NORMAL / INSERT / COMMAND for
    // Vim and Hybrid, EDITOR for nano (which has no modes).
    const std::string mode_text =
        std::string("[") + std::string(editor_mode_name(editing_mode_, editor_mode_)) + "]";
    const std::string prefix = "NanoX 0.1.0 " + mode_text + " ";
    out += prefix;
    std::string project_name = fs::path(workspace_).filename().string();
    if (project_name.empty()) {
        project_name = workspace_;  // e.g. workspace is "C:\" or "/"
    }
    const std::string file_name =
        cur.path.empty() ? "(untitled)" : fs::path(cur.path).filename().string();
    const std::string mid = project_name + " / " + file_name;
    // Row = "│ " + prefix + mid + padding + " " + status + " │" = cols = inner_w
    // + 2, so mid and its padding share inner_w - 3 - prefix - status cells.
    // Widths are cells, not bytes ('●' is 1 cell / 3 bytes).
    int avail = inner_w - 3 - static_cast<int>(display_width(prefix)) -
                static_cast<int>(display_width(status));
    if (avail < 0) {
        avail = 0;
    }
    const std::string shown = truncate_to_width(mid, static_cast<std::size_t>(avail));
    out += shown;
    append_repeat(out, " ", avail - static_cast<int>(display_width(shown)));
    out += " ";
    out += status_color;
    out += status;
    out += kReset;
    out += " │\r\n";

    // --- separator + header row -----------------------------------------------
    out += "├";
    append_repeat(out, "─", explorer_w);
    out += "┬";
    append_repeat(out, "─", editor_w);
    out += "┤\r\n";

    out += "│";
    out += kInverse;
    std::string project_header = "PROJECT";
    if (static_cast<int>(vis.size()) > content_rows_) {
        // Scroll indicator: first visible row / total rows (e.g. "6/42").
        const std::string indicator =
            " " + std::to_string(tree_scroll_ + 1) + "/" + std::to_string(vis.size());
        if (static_cast<int>(project_header.size() + indicator.size()) <= explorer_w) {
            project_header += indicator;
        }
    }
    out += cell(project_header, explorer_w);
    out += kReset;
    out += "│";

    std::string tabs;
    int tabs_w = 0;
    const int tab_space = editor_w;
    for (std::size_t i = 0; i < files_.size(); ++i) {
        std::string tab = " ";
        tab += files_[i].path.empty() ? "(untitled)" : fs::path(files_[i].path).filename().string();
        if (files_[i].buffer.dirty()) {
            tab += "*";
        }
        tab += " ";
        if (tabs_w + static_cast<int>(display_width(tab)) > tab_space) {
            // The ellipsis counts against the tab strip too; if even it does
            // not fit, stop without it rather than overflow the row.
            if (tabs_w + 2 <= tab_space) {
                tabs += " …";
                tabs_w += 2;
            }
            break;
        }
        if (i == current_file_) {
            tabs += kInverse;
            tabs += tab;
            tabs += kReset;
        } else {
            tabs += tab;
        }
        tabs_w += static_cast<int>(display_width(tab));
    }
    out += tabs;
    out += cell("", tab_space - tabs_w);
    out += "│\r\n";

    // --- content rows ----------------------------------------------------------
    std::vector<bool> error_line(buffer.line_count(), false);
    for (const Diagnostic& d : diagnostics_) {
        if (d.location.line >= 1 &&
            static_cast<std::size_t>(d.location.line - 1) < error_line.size()) {
            error_line[static_cast<std::size_t>(d.location.line - 1)] = true;
        }
    }

    const auto& lines = buffer.lines();

    for (int i = 0; i < content_rows_; ++i) {
        out += "│";

        // Explorer cell.
        const int vi = tree_scroll_ + i;
        if (vi >= 0 && vi < static_cast<int>(vis.size())) {
            const TreeNode& node = tree_.nodes()[vis[static_cast<std::size_t>(vi)]];
            std::string cell_text;
            append_repeat(cell_text, "  ", node.depth);
            cell_text += node.is_dir ? (node.expanded ? "▾ " : "▸ ") : "  ";
            // Deep nesting must never push the cell past the panel width.
            if (display_width(cell_text) > static_cast<std::size_t>(explorer_w)) {
                cell_text = truncate_to_width(cell_text, static_cast<std::size_t>(explorer_w));
            }
            const std::string name = truncate_to_width(
                node.name,
                static_cast<std::size_t>(std::max(
                    0, explorer_w - static_cast<int>(display_width(cell_text)))));
            const bool is_nx = !node.is_dir && node.name.size() >= 3 &&
                               node.name.substr(node.name.size() - 3) == ".nx";
            const bool selected = vi == static_cast<int>(tree_selected_);
            if (selected) {
                out += kInverse;
            }
            out += cell_text;
            if (is_nx) {
                out += kColorCyan;
            }
            out += name;
            if (is_nx) {
                out += kReset;
            }
            out += cell("", std::max(0, explorer_w -
                                          static_cast<int>(display_width(cell_text) +
                                                           display_width(name))));
            if (selected) {
                out += kReset;
            }
        } else {
            out += cell("", explorer_w);
        }

        out += "│";

        // Editor cell.
        const int li = cur.first_row + i;
        if (no_file) {
            if (i == content_rows_ / 2) {
                // No file opened: centered placeholder instead of a blank panel.
                const std::string msg = "No file opened";
                const int pad = std::max(0, (editor_w - static_cast<int>(msg.size())) / 2);
                out += kColorGutter;
                out += cell(std::string(static_cast<std::size_t>(pad), ' ') + msg, editor_w);
                out += kReset;
            } else {
                out += cell("", editor_w);
            }
        } else if (li < 0 || li >= static_cast<int>(lines.size())) {
            out += cell("", editor_w);
        } else {
            const bool is_current = li == buffer.row();
            if (is_current) {
                out += kInverse;
            } else if (error_line[static_cast<std::size_t>(li)]) {
                out += kColorErrorLine;
            } else {
                out += kColorGutter;
            }
            std::string number = std::to_string(li + 1);
            if (number.size() < gutter_w - 1) {
                number.insert(0, gutter_w - 1 - number.size(), ' ');
            }
            out += number;
            out += " ";
            out += kReset;

            const std::string& line = lines[static_cast<std::size_t>(li)];
            const std::size_t from = static_cast<std::size_t>(cur.first_col);
            // text_w budgets the viewport in bytes, so the cut can land inside
            // a multi-byte character; pull it back to a character boundary so
            // the row never ends on a partial sequence.
            std::size_t to =
                std::min(line.size(), from + static_cast<std::size_t>(std::max(0, text_w)));
            to = floor_utf8_boundary(line, to);
            if (to < from) {
                to = from;
            }
            std::size_t p = from;

            const std::vector<Span>* spans = nullptr;
            if (static_cast<std::size_t>(li) < spans_.size()) {
                spans = &spans_[static_cast<std::size_t>(li)];
            }
            if (spans != nullptr) {
                for (const Span& s : *spans) {
                    if (s.end <= p) {
                        continue;
                    }
                    if (s.begin >= to) {
                        break;
                    }
                    const std::size_t b = std::max(s.begin, p);
                    const std::size_t e = std::min(s.end, to);
                    if (b > p) {
                        // Uncolored text between the cursor position and this
                        // span must still be printed.
                        out.append(line, p, b - p);
                        p = b;
                    }
                    if (b < e) {
                        if (s.color != nullptr) {
                            out += s.color;
                        }
                        out.append(line, b, e - b);
                        if (s.color != nullptr) {
                            out += kReset;
                        }
                        p = e;
                    }
                }
            }
            std::string rest;
            if (p < to) {
                rest = line.substr(p, to - p);
            }
            out += rest;
            // Pad to the panel width using the printed text's display width,
            // not its byte count: a CJK line is fewer cells than bytes, and
            // counting bytes would leave the right border short.
            std::size_t printed_w = 0;
            if (to > from) {
                printed_w = display_width(std::string_view(line).substr(from, to - from));
            }
            const int used = static_cast<int>(gutter_w) + static_cast<int>(printed_w);
            out += cell("", std::max(0, editor_w - used));
        }
        out += "│\r\n";
    }

    // --- output panel ------------------------------------------------------------
    out += "├";
    append_repeat(out, "─", explorer_w);
    out += "┴";
    append_repeat(out, "─", editor_w);
    out += "┤\r\n";

    const std::string output_header =
        output_mode_ == OutputMode::Repl ? " REPL" : " OUTPUT";
    out += "│";
    out += kInverse;
    out += cell(output_header, inner_w);
    out += kReset;
    out += "│\r\n";

    std::vector<std::string> body_lines;
    bool show_no_output = false;
    if (output_mode_ == OutputMode::Repl) {
        body_lines = repl_output_;
    } else {
        body_lines = output_lines_;
        show_no_output = body_lines.empty();
    }
    const int max_scroll =
        std::max(0, static_cast<int>(body_lines.size()) - output_body_rows_);
    output_scroll_ = std::clamp(output_scroll_, 0, max_scroll);

    for (int i = 0; i < output_body_rows_; ++i) {
        out += "│ ";
        const int line_idx = output_scroll_ + i;
        std::string body;
        if (output_mode_ == OutputMode::Repl && i == output_body_rows_ - 1) {
            body = "> " + repl_input_;
        } else if (show_no_output) {
            body = i == 0 ? "No output" : "";
        } else if (line_idx >= 0 && line_idx < static_cast<int>(body_lines.size())) {
            body = body_lines[static_cast<std::size_t>(line_idx)];
        }
        if (show_no_output && i == 0) {
            out += kColorGutter;
            out += cell(body, inner_w - 1);
            out += kReset;
        } else {
            out += cell(body, inner_w - 1);
        }
        out += "│\r\n";
    }

    out += "├";
    append_repeat(out, "─", inner_w);
    out += "┤\r\n";

    // --- function bar / prompt ------------------------------------------------------
    std::string bar;
    if (prompt_mode_) {
        bar = prompt_text_ + " " + prompt_input_;
    } else {
        // Never hardcoded here: the hint follows the editing mode and state.
        bar = footer_text();
    }
    out += "│ ";
    out += cell(bar, inner_w - 1);
    out += "│\r\n";

    out += "└";
    append_repeat(out, "─", inner_w);
    out += "┘";  // no trailing newline: writing it at the last row would scroll

    // --- cursor ----------------------------------------------------------------------
    char position[32];
    bool cursor_shown = false;
    if (prompt_mode_) {
        std::snprintf(position, sizeof(position), "\x1b[%d;%dH",
                      content_rows_ + output_body_rows_ + 8,
                      4 + static_cast<int>(prompt_text_.size()) +
                          static_cast<int>(prompt_input_.size()));
        cursor_shown = true;
    } else if (focus_ == Focus::Editor && !no_file) {
        std::snprintf(position, sizeof(position), "\x1b[%d;%dH",
                      5 + (buffer.row() - cur.first_row),
                      3 + explorer_w + static_cast<int>(gutter_w) +
                          (buffer.col() - cur.first_col));
        cursor_shown = true;
    } else if (output_mode_ == OutputMode::Repl && focus_ == Focus::Output) {
        std::snprintf(position, sizeof(position), "\x1b[%d;%dH",
                      content_rows_ + output_body_rows_ + 6,
                      4 + static_cast<int>(repl_input_.size()));
        cursor_shown = true;
    }

    if (cursor_shown) {
        out += position;
        out += "\x1b[?25h";
    }

    terminal_.write(out);
    terminal_.flush();
}

void Editor::render_help() {
    int rows = 0;
    int cols = 0;
    terminal_.get_size(rows, cols);
    (void)rows;
    (void)cols;

    std::string out;
    out += "\x1b[?25l\x1b[H\x1b[2J";

    std::vector<std::string> lines = {
        "NanoX 0.1.0 — Help",
        "",
        std::string("Editing mode: ") + std::string(editing_mode_name(editing_mode_)) +
            "   state: " + std::string(editor_mode_name(editing_mode_, editor_mode_)),
        "F8 cycles Vim -> Nano -> Hybrid, or use  :set mode <vim|nano|hybrid>",
        "",
        "Global",
        "  F1 Help        F4 Run         F6 Output",
        "  F2 File tree   F5 REPL        F8 Editing mode",
        "  F3 Build       ^Q / ^C Quit",
        "",
    };

    // The key list is generated from the live keymap, so the help can never
    // drift from the bindings that actually exist in this mode.
    const std::vector<std::pair<std::string, std::string>> bindings =
        keymap_.bindings_for(editing_mode_, editor_mode_);
    if (bindings.empty()) {
        lines.push_back("  (no keys bound in this state)");
    } else {
        lines.push_back(std::string("Keys in ") +
                        std::string(editor_mode_name(editing_mode_, editor_mode_)) + ":");
        for (const std::pair<std::string, std::string>& binding : bindings) {
            const std::size_t pad =
                binding.first.size() < 14 ? 14 - binding.first.size() : 1;
            lines.push_back("  " + binding.first + std::string(pad, ' ') + binding.second);
        }
    }

    lines.push_back("");
    lines.push_back("Explorer: Up/Down select, Enter open, Right expand, Left collapse");
    lines.push_back("Output:   PgUp/PgDn scroll");
    lines.push_back("REPL:     Enter lexes the line, Esc leaves the REPL");
    lines.push_back("");
    lines.push_back("Phase 1: 'Build' runs the Lexer over every *.nx file in the project.");
    lines.push_back("Run, the AST viewer and the IR viewer arrive with later phases.");
    lines.push_back("");
    lines.push_back("press any key to close");

    for (const std::string& line : lines) {
        out += line;
        out += "\x1b[K\r\n";
    }

    terminal_.write(out);
    terminal_.flush();
}

// ---------------------------------------------------------------------------
// Key handling
// ---------------------------------------------------------------------------

void Editor::handle_ctrl_key(char ch) {
    switch (ch) {
        case 's': save_current(); break;
        case 'q':
        case 'c': request_quit(); break;
        case 't': switch_tab(1); break;
        case 'w': close_tab(current_file_); break;
        default: break;
    }
}

void Editor::handle_function_key(unsigned n) {
    // Function keys stay global (they work from every pane), but they still go
    // through the command layer rather than calling actions directly.
    switch (n) {
        case 1: execute(Command::Help); break;
        case 2: execute(Command::ToggleExplorer); break;
        case 3: execute(Command::Build); break;
        case 4: execute(Command::Run); break;
        case 5: execute(Command::Repl); break;
        case 6: execute(Command::ToggleOutput); break;
        case 8: execute(Command::CycleEditingMode); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Command layer
// ---------------------------------------------------------------------------

std::string Editor::footer_text() const {
    return Keymap::footer_hint(editing_mode_, editor_mode_);
}

void Editor::set_editing_mode(EditingMode mode) {
    editing_mode_ = mode;
    // Vim opens in NORMAL (that is what makes it Vim); Nano has no other state
    // to be in; Hybrid opens ready to type and leaves via ESC.
    editor_mode_ =
        (mode == EditingMode::Vim) ? EditorMode::Normal : EditorMode::Insert;
    keymap_.reset_pending();
}

void Editor::cycle_editing_mode() {
    switch (editing_mode_) {
        case EditingMode::Vim:    set_editing_mode(EditingMode::Nano);   break;
        case EditingMode::Nano:   set_editing_mode(EditingMode::Hybrid); break;
        case EditingMode::Hybrid: set_editing_mode(EditingMode::Vim);    break;
    }
    note("editing mode: " + std::string(editing_mode_name(editing_mode_)));
}

void Editor::set_editor_mode(EditorMode mode) {
    // Nano never leaves its single editing state.
    editor_mode_ = (editing_mode_ == EditingMode::Nano) ? EditorMode::Insert : mode;
    keymap_.reset_pending();
}

void Editor::feed_editor_key(const Key& key) {
    const KeyChord chord = KeyChord::from_key(key);
    Command command = Command::None;

    switch (keymap_.feed(editing_mode_, editor_mode_, chord, command)) {
        case Keymap::Match::Pending:
            return;  // first half of "dd"/"yy": nothing to do yet
        case Keymap::Match::Complete:
            execute(command);
            return;
        case Keymap::Match::None:
            break;
    }

    // An unbound key still types and moves the cursor while editing; in NORMAL
    // it is deliberately inert, which is the entire point of the mode.
    if (editing_mode_ == EditingMode::Nano || editor_mode_ == EditorMode::Insert) {
        editor_key(key);
    }
}

void Editor::execute(Command command) {
    // Note: buffer_of_current() is re-fetched per branch rather than hoisted,
    // because close/quit commands can erase the tab it would point at.
    switch (command) {
        case Command::None:
            break;

        // --- files, tabs, application ----------------------------------------
        case Command::Save:
            save_current();
            break;
        case Command::WriteOut:
            start_prompt(PromptKind::WriteOut, "File Name to Write:",
                         files_[current_file_].path);
            break;
        case Command::Quit:
            request_quit();
            break;
        case Command::ForceQuit:
            quit_ = true;
            break;
        case Command::SaveAndQuit:
            save_current();
            if (!any_dirty()) {
                quit_ = true;
            }
            break;
        case Command::CloseTab:
            close_current_tab_or_quit();
            break;
        case Command::NextTab:
            switch_tab(1);
            break;
        case Command::CycleEditingMode:
            cycle_editing_mode();
            break;
        case Command::SetEditingMode:
            break;  // reached only via ":" / ":set mode", which carries the name

        // --- panels -----------------------------------------------------------
        case Command::Help:
            help_visible_ = true;
            break;
        case Command::Build:
            do_build();
            break;
        case Command::Run:
            do_run();
            break;
        case Command::Repl:
            enter_repl();
            break;
        case Command::ToggleOutput:
            focus_ = focus_ == Focus::Output ? Focus::Editor : Focus::Output;
            break;
        case Command::ToggleExplorer:
            focus_ = focus_ == Focus::Explorer ? Focus::Editor : Focus::Explorer;
            break;

        // --- editing ----------------------------------------------------------
        case Command::Undo:
            buffer_of_current().undo();
            break;
        case Command::Redo:
            buffer_of_current().redo();
            break;
        case Command::CutLine:
        case Command::DeleteLine:
            cut_line();
            break;
        case Command::CopyLine:
        case Command::YankLine:
            copy_line();
            break;
        case Command::Paste:
        case Command::PasteBefore:
            paste_clipboard(false);
            break;
        case Command::PasteAfter:
            paste_clipboard(true);
            break;
        case Command::Search:
            start_prompt(PromptKind::Search, "Search:");
            break;
        case Command::Replace:
            start_prompt(PromptKind::ReplaceSearch, "Search:");
            break;
        case Command::GoToLine:
            start_prompt(PromptKind::GoToLine, "Go To Line (line,column):");
            break;

        // --- mode transitions --------------------------------------------------
        case Command::EnterNormal:
            set_editor_mode(EditorMode::Normal);
            break;
        case Command::EnterCommandMode:
            // start_prompt first: it records the mode to return to, which must
            // be the one we are leaving, not Command itself.
            start_prompt(PromptKind::ExCommand, ":");
            set_editor_mode(EditorMode::Command);
            break;
        case Command::VimInsert:
            set_editor_mode(EditorMode::Insert);
            break;
        case Command::VimAppend:
            buffer_of_current().move_right();
            set_editor_mode(EditorMode::Insert);
            break;
        case Command::VimInsertLineStart:
            buffer_of_current().move_home();
            set_editor_mode(EditorMode::Insert);
            break;
        case Command::VimAppendLineEnd:
            buffer_of_current().move_end();
            set_editor_mode(EditorMode::Insert);
            break;
        case Command::VimOpenBelow:
            buffer_of_current().insert_line_below();
            set_editor_mode(EditorMode::Insert);
            break;
        case Command::VimOpenAbove:
            buffer_of_current().insert_line_above();
            set_editor_mode(EditorMode::Insert);
            break;

        // --- motion ------------------------------------------------------------
        case Command::MoveLeft:  buffer_of_current().move_left();  break;
        case Command::MoveDown:  buffer_of_current().move_down();  break;
        case Command::MoveUp:    buffer_of_current().move_up();    break;
        case Command::MoveRight: buffer_of_current().move_right(); break;
        case Command::LineStart: buffer_of_current().move_home();  break;
        case Command::LineEnd:   buffer_of_current().move_end();   break;

        // --- operators ----------------------------------------------------------
        case Command::DeleteChar:
            buffer_of_current().delete_char();
            break;
        case Command::DeleteToEnd:
            buffer_of_current().delete_to_end_of_line();
            break;
    }
}

void Editor::run_ex_command(const std::string& line) {
    const ParsedCommand parsed = parse_ex_command(line);
    if (!parsed.ok) {
        if (!line.empty()) {
            note("error: not a command: " + line);
        }
        return;
    }
    // ":set mode" and ":w <file>" carry an argument the generic dispatch has no
    // place for, so they are handled here rather than in execute().
    if (parsed.command == Command::SetEditingMode) {
        const std::optional<EditingMode> mode = editing_mode_from_string(parsed.arg);
        if (mode.has_value()) {
            set_editing_mode(*mode);
            note("editing mode: " + std::string(editing_mode_name(*mode)));
        }
        return;
    }
    if (parsed.command == Command::Save && !parsed.arg.empty()) {
        write_out_to(parsed.arg);
        return;
    }
    execute(parsed.command);
}

// --- editing actions used by execute() -------------------------------------

void Editor::close_current_tab_or_quit() {
    // ^X means "leave" in nano. With no other tab open there is nothing left to
    // edit, so it quits rather than leaving an empty buffer behind.
    if (files_.size() <= 1) {
        request_quit();
        return;
    }
    close_tab(current_file_);
}

void Editor::cut_line() {
    clipboard_.text = buffer_of_current().delete_current_line();
    clipboard_.linewise = true;
    note("cut 1 line");
}

void Editor::copy_line() {
    clipboard_.text = buffer_of_current().yank_line();
    clipboard_.linewise = true;
    note("copied 1 line");
}

void Editor::paste_clipboard(bool after) {
    // An untouched register is empty and not linewise; a cut empty line is
    // empty but linewise, and must still paste a blank line.
    if (clipboard_.text.empty() && !clipboard_.linewise) {
        note("clipboard is empty");
        return;
    }

    TextBuffer& buffer = buffer_of_current();
    if (clipboard_.linewise) {
        const int at = buffer.row() + (after ? 1 : 0);
        buffer.insert_line_at(at, clipboard_.text);
        buffer.set_cursor(at, 0);
        return;
    }
    if (after) {
        buffer.move_right();  // Vim's p pastes after the character under the cursor
    }
    buffer.insert_text(clipboard_.text);
}

void Editor::search_for(const std::string& needle) {
    if (needle.empty()) {
        note("search: empty pattern");
        return;
    }
    last_search_ = needle;
    note(buffer_of_current().find_forward(needle) ? "found: " + needle
                                                  : "not found: " + needle);
}

void Editor::replace_all(const std::string& needle, const std::string& replacement) {
    if (needle.empty()) {
        note("replace: empty search pattern");
        return;
    }
    last_search_ = needle;
    const std::size_t count =
        buffer_of_current().replace_all_forward(needle, replacement);
    note("replaced " + std::to_string(count) + " occurrence(s) of \"" + needle +
         "\" with \"" + replacement + "\"");
}

void Editor::goto_line(const std::string& text) {
    const std::size_t comma = text.find(',');
    const std::string row_text = text.substr(0, comma);
    const std::string col_text =
        (comma == std::string::npos) ? std::string() : text.substr(comma + 1);

    int row = 0;
    int col = 1;
    if (!parse_decimal(row_text, row) ||
        (!col_text.empty() && !parse_decimal(col_text, col))) {
        note("go to line: expected \"line,column\"");
        return;
    }
    // Input is 1-based ("line 1" is the first line) and the buffer is 0-based.
    // The cursor is clamped, so an out-of-range line lands on the last one.
    buffer_of_current().set_cursor(row - 1, col - 1);
}

void Editor::write_out_to(const std::string& path) {
    if (path.empty()) {
        return;
    }
    OpenFile& file = files_[current_file_];
    const std::string previous = file.path;
    // The path has to be set before saving (that is what save_file writes to),
    // but a failed write must not leave the buffer pointing at a new target.
    file.path = path;
    if (!save_file(file)) {
        file.path = previous;
        return;
    }
    tree_ = FileTree::scan(workspace_);  // a new file may have appeared
}

void Editor::editor_key(const Key& key) {
    using K = Key::Kind;
    switch (key.kind) {
        case K::Char:      buffer_of_current().insert_char(key.ch); break;
        case K::Enter:     buffer_of_current().insert_newline(); break;
        case K::Tab:       buffer_of_current().insert_text("    "); break;
        case K::Backspace: buffer_of_current().backspace(); break;
        case K::Delete:    buffer_of_current().delete_char(); break;
        case K::ArrowUp:   buffer_of_current().move_up(); break;
        case K::ArrowDown: buffer_of_current().move_down(); break;
        case K::ArrowLeft: buffer_of_current().move_left(); break;
        case K::ArrowRight: buffer_of_current().move_right(); break;
        case K::Home:      buffer_of_current().move_home(); break;
        case K::End:       buffer_of_current().move_end(); break;
        case K::PageUp: {
            const int n = std::max(1, content_rows_);
            buffer_of_current().move_page_up(n);
            files_[current_file_].first_row = buffer_of_current().row();
            break;
        }
        case K::PageDown: {
            const int n = std::max(1, content_rows_);
            buffer_of_current().move_page_down(n);
            files_[current_file_].first_row = buffer_of_current().row();
            break;
        }
        default: break;
    }
}

void Editor::explorer_key(const Key& key) {
    using K = Key::Kind;
    const std::vector<std::size_t> vis = tree_.visible();
    if (vis.empty()) {
        return;
    }
    const std::size_t sel = tree_selected_;

    switch (key.kind) {
        case K::ArrowUp:
            if (sel > 0) {
                --tree_selected_;
            }
            break;
        case K::ArrowDown:
            if (sel + 1 < vis.size()) {
                ++tree_selected_;
            }
            break;
        case K::Home: tree_selected_ = 0; break;
        case K::End: tree_selected_ = vis.size() - 1; break;
        case K::PageUp:
            tree_selected_ = (sel > static_cast<std::size_t>(content_rows_))
                                 ? sel - static_cast<std::size_t>(content_rows_)
                                 : 0;
            break;
        case K::PageDown:
            tree_selected_ = std::min(vis.size() - 1,
                                      sel + static_cast<std::size_t>(content_rows_));
            break;
        case K::Enter: {
            const TreeNode& node = tree_.nodes()[vis[sel]];
            if (node.is_dir) {
                tree_.toggle_expanded(vis[sel]);
            } else {
                open_file(node.path);
            }
            break;
        }
        case K::ArrowRight: {
            const TreeNode& node = tree_.nodes()[vis[sel]];
            if (node.is_dir) {
                if (!node.expanded) {
                    tree_.toggle_expanded(vis[sel]);
                }
            } else {
                open_file(node.path);
            }
            break;
        }
        case K::ArrowLeft: {
            const TreeNode& node = tree_.nodes()[vis[sel]];
            if (node.is_dir && node.expanded) {
                tree_.toggle_expanded(vis[sel]);
            } else {
                for (std::size_t i = sel; i > 0; --i) {
                    if (tree_.nodes()[vis[i]].depth < node.depth) {
                        tree_selected_ = i;
                        break;
                    }
                }
            }
            break;
        }
        default: break;
    }
}

void Editor::output_key(const Key& key) {
    using K = Key::Kind;
    if (output_mode_ == OutputMode::Repl) {
        switch (key.kind) {
            case K::Char:
                if (key.ch >= 0x20) {
                    repl_input_ += key.ch;
                }
                break;
            case K::Backspace:
                if (!repl_input_.empty()) {
                    repl_input_.pop_back();
                }
                break;
            case K::Enter: repl_submit_line(); break;
            case K::PageUp: output_scroll_ -= 3; break;
            case K::PageDown: output_scroll_ += 3; break;
            default: break;
        }
    } else {
        switch (key.kind) {
            case K::PageUp: output_scroll_ -= 3; break;
            case K::PageDown: output_scroll_ += 3; break;
            case K::Home: output_scroll_ = 0; break;
            case K::End: output_scroll_ = 1000000; break;
            default: break;
        }
    }
}

void Editor::start_prompt(PromptKind kind, std::string prompt, std::string prefill) {
    if (!prompt_mode_) {
        // Remember where we were: a prompt is a detour, not a mode change.
        prompt_return_mode_ = editor_mode_;
    }
    prompt_kind_ = kind;
    prompt_text_ = std::move(prompt);
    prompt_input_ = std::move(prefill);
    prompt_mode_ = true;
}

void Editor::finish_prompt() {
    prompt_mode_ = false;
    prompt_kind_ = PromptKind::None;
    prompt_input_.clear();
    // Back to the state that opened the prompt (NORMAL after ":", INSERT after
    // nano's ^W); set_editor_mode keeps nano pinned to its single state.
    set_editor_mode(prompt_return_mode_);
}

void Editor::handle_prompt_key(const Key& key) {
    using K = Key::Kind;
    switch (key.kind) {
        case K::Enter: {
            const std::string input = prompt_input_;
            const bool yes = !input.empty() && (input[0] == 'y' || input[0] == 'Y');
            // Read the kind BEFORE finish_prompt() resets it to None. Switching
            // on prompt_kind_ after that call always matched None, which is why
            // SaveAs never wrote and Ctrl+Q could not quit a modified buffer.
            const PromptKind kind = prompt_kind_;
            finish_prompt();

            switch (kind) {
                case PromptKind::ConfirmQuit:
                    quit_ = yes;
                    break;
                case PromptKind::ConfirmClose:
                    if (yes) {
                        do_close_tab(pending_close_);
                    }
                    break;
                case PromptKind::SaveAs:
                case PromptKind::WriteOut:
                    // Same action: "save as" and nano's ^O both name the target
                    // then write through the one path that handles failure.
                    write_out_to(input);
                    break;
                case PromptKind::Search:
                    search_for(input);
                    break;
                case PromptKind::ReplaceSearch:
                    // Ask for the replacement before touching the buffer.
                    pending_replace_ = input;
                    start_prompt(PromptKind::ReplaceWith, "Replace with:");
                    break;
                case PromptKind::ReplaceWith:
                    replace_all(pending_replace_, input);
                    pending_replace_.clear();
                    break;
                case PromptKind::GoToLine:
                    goto_line(input);
                    break;
                case PromptKind::ExCommand:
                    run_ex_command(input);
                    break;
                case PromptKind::None:
                    break;
            }
            break;
        }
        case K::Escape:
            finish_prompt();
            break;
        case K::Backspace:
            if (!prompt_input_.empty()) {
                prompt_input_.pop_back();
            }
            break;
        case K::Char:
            if (key.ch >= 0x20) {
                prompt_input_ += key.ch;
            }
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

bool Editor::any_dirty() const {
    for (const OpenFile& file : files_) {
        if (file.buffer.dirty()) {
            return true;
        }
    }
    return false;
}

std::string Editor::display_path(const std::string& path) const {
    std::error_code ec;
    const fs::path rel = fs::relative(fs::path(path), fs::path(workspace_), ec);
    return ec ? path : rel.string();
}

void Editor::note(const std::string& line) {
    output_lines_.push_back(line);
}

bool Editor::save_file(OpenFile& file) {
    std::ofstream out(file.path, std::ios::binary | std::ios::trunc);
    if (!out) {
        note("error: cannot write file: " + file.path);
        return false;
    }
    out << file.buffer.to_string();
    out.close();
    if (!out.good()) {
        note("error: cannot write file: " + file.path);
        return false;
    }
    file.buffer.mark_clean();
    note("saved: " + display_path(file.path));
    return true;
}

void Editor::save_current() {
    OpenFile& file = files_[current_file_];
    if (file.path.empty()) {
        start_prompt(PromptKind::SaveAs, "save as:");
        return;
    }
    save_file(file);
}

void Editor::request_quit() {
    if (!any_dirty()) {
        quit_ = true;
        return;
    }
    start_prompt(PromptKind::ConfirmQuit, "unsaved changes! quit anyway? (y/N)");
}

void Editor::switch_tab(int delta) {
    const int n = static_cast<int>(files_.size());
    if (n <= 1) {
        return;
    }
    current_file_ = static_cast<std::size_t>(
        (static_cast<int>(current_file_) + delta + n) % n);
}

void Editor::close_tab(std::size_t index) {
    if (index >= files_.size()) {
        return;
    }
    if (files_[index].buffer.dirty()) {
        pending_close_ = index;
        start_prompt(PromptKind::ConfirmClose, "file modified, close anyway? (y/N)");
        return;
    }
    do_close_tab(index);
}

void Editor::do_close_tab(std::size_t index) {
    if (index >= files_.size()) {
        return;
    }
    files_.erase(std::next(files_.begin(), static_cast<std::ptrdiff_t>(index)));
    if (files_.empty()) {
        files_.push_back(OpenFile{});
    }
    if (current_file_ >= files_.size()) {
        current_file_ = files_.size() - 1;
    }
    focus_ = Focus::Editor;
}

void Editor::open_file(const std::string& path) {
    for (std::size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].path == path) {
            current_file_ = i;
            focus_ = Focus::Editor;
            return;
        }
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        note("error: cannot open file: " + display_path(path));
        return;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    TextBuffer text = TextBuffer::from_string(buffer.str());

    // Reuse a pristine untitled tab instead of piling up empty tabs.
    if (files_.size() == 1 && files_[0].path.empty() && !files_[0].buffer.dirty()) {
        files_[0].path = path;
        files_[0].buffer = std::move(text);
        current_file_ = 0;
    } else {
        OpenFile file;
        file.path = path;
        file.buffer = std::move(text);
        files_.push_back(std::move(file));
        current_file_ = files_.size() - 1;
    }
    focus_ = Focus::Editor;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void Editor::do_build() {
    const Project project(workspace_);
    const ProjectAnalysis result = project.lex_all();

    output_lines_.clear();
    output_lines_.push_back("NanoX build (Phase 1: lexer) — " +
                            std::to_string(result.files.size()) + " file(s)");

    for (const FileAnalysis& file : result.files) {
        bool has_error = false;
        for (const Diagnostic& d : file.diagnostics) {
            if (d.severity == Severity::Error) {
                has_error = true;
            }
        }
        output_lines_.push_back("  " + display_path(file.path) + "  " +
                                (has_error ? "ERROR"
                                           : "OK (" + std::to_string(file.token_count) +
                                                 " tokens)"));
    }

    if (result.success()) {
        output_lines_.push_back("✓ Build succeeded — " +
                                std::to_string(result.total_tokens()) + " token(s)");
    } else {
        output_lines_.push_back("✗ Build failed — " +
                                std::to_string(result.total_errors()) + " error(s)");
        for (const FileAnalysis& file : result.files) {
            for (const Diagnostic& d : file.diagnostics) {
                output_lines_.push_back("  " + display_path(file.path) + ":" +
                                        std::to_string(d.location.line) + ":" +
                                        std::to_string(d.location.column) + ": " +
                                        severity_name(d.severity) + ": " + d.message);
            }
        }
    }

    if (any_dirty()) {
        output_lines_.push_back("note: unsaved editor changes are not included in the build");
    }

    char timing[48];
    std::snprintf(timing, sizeof(timing), "Finished in %.2fs", result.elapsed_seconds);
    output_lines_.push_back(timing);

    last_build_failed_ = !result.success();
    output_mode_ = OutputMode::Normal;
    output_scroll_ = 0;
    focus_ = Focus::Output;
}

void Editor::do_run() {
    output_lines_.clear();
    output_lines_.push_back("NanoX run");
    output_lines_.push_back("✗ not available yet — the interpreter arrives in a later phase.");
    output_mode_ = OutputMode::Normal;
    output_scroll_ = 0;
    focus_ = Focus::Output;
}

void Editor::enter_repl() {
    output_mode_ = OutputMode::Repl;
    repl_output_.clear();
    repl_input_.clear();
    repl_buffer_.clear();
    repl_continuation_ = false;
    output_scroll_ = 0;
    focus_ = Focus::Output;
}

void Editor::leave_repl() {
    output_mode_ = OutputMode::Normal;
}

void Editor::repl_submit_line() {
    repl_output_.push_back("> " + repl_input_);
    repl_buffer_ += repl_input_;
    repl_buffer_ += '\n';

    DiagnosticEngine diag("<repl>");
    Lexer lexer(repl_buffer_, "<repl>", diag);
    int depth = 0;
    while (true) {
        const Token token = lexer.next();
        if (token.is_eof()) {
            break;
        }
        if (token.is(TokenKind::LeftBrace)) {
            ++depth;
        } else if (token.is(TokenKind::RightBrace)) {
            --depth;
        }
        std::string line = "  " + std::to_string(token.location.line) + ":" +
                           std::to_string(token.location.column) + "  " +
                           token_kind_name(token.kind);
        if (!token.lexeme.empty()) {
            line += "  '" + std::string(token.lexeme) + "'";
        }
        repl_output_.push_back(line);
    }
    for (const Diagnostic& d : diag.diagnostics()) {
        repl_output_.push_back("  <repl>:" + std::to_string(d.location.line) + ":" +
                               std::to_string(d.location.column) + ": " +
                               severity_name(d.severity) + ": " + d.message);
    }

    if (depth > 0) {
        repl_continuation_ = true;
    } else {
        repl_buffer_.clear();
        repl_continuation_ = false;
    }
    repl_input_.clear();
    output_scroll_ = 1000000;
}

}  // namespace nanox::editor
