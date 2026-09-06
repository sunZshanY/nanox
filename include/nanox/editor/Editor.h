#pragma once

#include "nanox/Diagnostic.h"
#include "nanox/FileTree.h"
#include "nanox/editor/Terminal.h"
#include "nanox/editor/TextBuffer.h"

#include <cstddef>
#include <string>
#include <vector>

namespace nanox::editor {

// Full-screen IDE-style TUI for NanoX source code.
//
// Layout (see docs/editor.md):
//   ┌ top border ┐
//   │ title bar: NanoX 0.1.0 | project / file | ● status │
//   ├ project tree ┬ editor tabs ┤
//   │ tree rows    │ code + line numbers (or "No file opened") │
//   ├─┴─┤
//   │ OUTPUT / REPL panel (auto-height: 1 row when empty) │
//   ├─┤
//   │ F1..F6 function bar (or prompt) │
//   └ bottom border ┘
//
// Width invariant: every row is emitted as exactly `cols` cells (see the
// comment in Editor::render), so the outer border, the tree/editor separator
// and the panel separators always line up. Each frame starts with a home +
// erase (ESC[H ESC[J) and ends at the bottom-right cell without a trailing
// newline, so resizes never leave stale cells or scroll the screen.
//
// All terminal differences live in Terminal; all editing invariants live in
// TextBuffer; the project model lives in FileTree/Project. Editor only wires
// them together and owns the panel/focus state.
class Editor {
public:
    // `workspace` is the project root (shown in the tree and title bar).
    // `open_file` (optional) is loaded into the first tab; empty = untitled.
    Editor(std::string workspace, std::string open_file, std::string initial_text);
    ~Editor();

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    // Runs the main loop until the user quits. Returns the process exit code.
    int run();

private:
    enum class Focus { Explorer, Editor, Output };
    enum class OutputMode { Normal, Repl };
    enum class PromptKind { None, SaveAs, ConfirmQuit, ConfirmClose };

    struct OpenFile {
        std::string path;   // empty = untitled buffer
        TextBuffer buffer;
        int first_row = 0;  // per-file viewport
        int first_col = 0;
    };

    struct Span {
        std::size_t begin = 0;   // column of first highlighted character
        std::size_t end = 0;     // one past the last highlighted character
        const char* color = nullptr;  // ANSI color sequence (nullptr = default)
    };

    void analyze();   // re-lex the current buffer: diagnostics + color spans
    void render();
    void render_help();
    int desired_output_body_rows() const;

    void handle_ctrl_key(char ch);
    void handle_function_key(unsigned n);
    void handle_prompt_key(const Key& key);
    void editor_key(const Key& key);
    void explorer_key(const Key& key);
    void output_key(const Key& key);

    void start_prompt(PromptKind kind, std::string prompt);
    void finish_prompt();
    void note(const std::string& line);
    TextBuffer& buffer_of_current() { return files_[current_file_].buffer; }

    bool save_file(OpenFile& file);
    void save_current();
    void request_quit();
    void close_tab(std::size_t index);
    void do_close_tab(std::size_t index);
    void open_file(const std::string& path);
    void switch_tab(int delta);
    bool any_dirty() const;
    std::string display_path(const std::string& path) const;

    void do_build();
    void do_run();
    void enter_repl();
    void leave_repl();
    void repl_submit_line();

    std::string workspace_;
    FileTree tree_;
    Terminal terminal_;

    std::vector<OpenFile> files_;
    std::size_t current_file_ = 0;

    std::size_t tree_selected_ = 0;  // index into the visible-node list
    int tree_scroll_ = 0;

    std::vector<std::string> output_lines_;
    int output_scroll_ = 0;
    std::vector<std::string> repl_output_;
    std::string repl_input_;
    std::string repl_buffer_;        // accumulated multi-line REPL input
    bool repl_continuation_ = false;

    Focus focus_ = Focus::Editor;
    OutputMode output_mode_ = OutputMode::Normal;
    bool last_build_failed_ = false;

    bool help_visible_ = false;
    bool quit_ = false;

    bool prompt_mode_ = false;
    PromptKind prompt_kind_ = PromptKind::None;
    std::string prompt_text_;
    std::string prompt_input_;
    std::size_t pending_close_ = 0;

    // Per-render lexer state for the current file.
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::vector<Span>> spans_;
    std::size_t token_count_ = 0;

    int content_rows_ = 0;
    int output_body_rows_ = 1;
};

}  // namespace nanox::editor
