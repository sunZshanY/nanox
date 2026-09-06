# NanoX IDE (TUI editor) design

The `nanox` CLI is a full-screen, IDE-style terminal UI for NanoX development.
This document explains the architecture and design decisions.

## Why a built-in editor?

Phase 1 produces only the Lexer, so the CLI cannot run programs yet. The IDE
makes the Lexer's output visible and testable by a human: live syntax
highlighting, per-project builds, and inline diagnostics. No third-party TUI
library (ncurses, ...) is used — NanoX stays dependency-free and builds on
Linux, Windows, and macOS with just CMake + a C++17 compiler.

## Layout

```
┌──────────────────────────────────────────────────────────────────────┐
│ NanoX 0.1.0                  nanox / main.nx               ● Ready │
├──────────────┬───────────────────────────────────────────────────────┤
│ PROJECT 1/6  │  main.nx *                                             │
│              │  1  fn main() {                                        │
│ ▾ nanox      │  2      let name = "NanoX";                            │
│   src/       │  3      println(name);                                 │
│   examples/  │  4  }                                                  │
│   CMakeLists │                                                        │
├──────────────┴───────────────────────────────────────────────────────┤
│ OUTPUT                                                               │
│  ✓ Build succeeded — 53 token(s)                                     │
│  Finished in 0.42s                                                   │
├──────────────────────────────────────────────────────────────────────┤
│ F1 Help  F2 Files  F3 Build  F4 Run  F5 REPL  F6 Output  ^Q Quit     │
└──────────────────────────────────────────────────────────────────────┘
```

- **Title bar** — version, project name and current file (`nanox / main.nx`),
  status dot: `● Ready` (green) / `● Modified` (yellow) / `● Errors` (red).
- **PROJECT panel** — browsable file tree (expand/collapse, `.nx` files in
  cyan, scroll indicator `first/total` in the header when the tree is longer
  than the panel). `nanox` accepts a file (parent dir becomes the project) or
  a directory.
- **Editor tabs** — multiple open files (`^T` next tab, `^W` close, dirty
  marker `*`), line numbers, syntax highlighting from the Lexer. With no
  file open the panel shows a dimmed `No file opened` placeholder.
- **OUTPUT panel** — build results and diagnostics; doubles as the REPL. The
  panel is auto-height: one `No output` row when empty, up to 12 rows while
  a build prints results (scrollable with PgUp/PgDn in the OUTPUT focus).
- **Function bar** — F1..F6 shortcuts; prompts (save as, quit confirmation)
  appear here too.

### Project root discovery

`nanox` walks up from the start directory looking for `nanox.toml` (falling
back to `CMakeLists.txt`) and opens that directory as the project. Running
`nanox` from inside `build/` therefore opens the real project instead of
showing CMake artifacts; `build/`, `out/`, `cmake-build-*` and dot-folders
are never displayed in the tree anyway.

### Resize handling

`read_key()` polls for input while waiting, so a terminal resize is reported
as a `Resize` key event and the layout re-renders immediately (both Windows
and POSIX). Every frame starts with home + erase (`ESC[H ESC[J`), draws every
row at exactly the terminal width, and ends at the bottom-right cell without
a trailing newline — so shrinking/growing never leaves stale cells, double
borders, or a scrolled-off frame.

### Layout invariants

All panel widths derive from `rows`/`cols`; there are no fixed coordinates.
For a terminal of `cols` cells (`inner_w = cols - 2`):

- explorer panel: `explorer_w = clamp(cols / 4, 12, 24)`; editor takes the rest
- every row is exactly `cols` cells: split rows are
  `1 + explorer_w + 1 + editor_w + 1`, body rows are `2 + (inner_w - 1) + 1`
- explorer cells are hard-clamped to `explorer_w`, so deep nesting can never
  push the separator out of line

`LINES`/`COLUMNS` environment variables override the size on both platforms
when the console size cannot be queried (redirected output), which also lets
tests pin the layout at any terminal size.

## Architecture: four layers

```
Editor      (src/editor/Editor.cpp)   panel/focus state, key loop, rendering
   │
   ├── Terminal  (src/editor/Terminal.cpp)  platform abstraction
   │        raw mode, key events (incl. F1-F12), ANSI output, terminal size
   │
   ├── TextBuffer (src/editor/TextBuffer.cpp)  per-file editing logic
   │        lines + cursor, inserts/deletes, cursor clamping, dirty flag
   │
   └── Project model (src/Project.cpp, src/FileTree.cpp)  project domain
            *.nx discovery, tree browsing, Phase-1 build (lexer pass)
```

### TextBuffer — pure editing logic

One `TextBuffer` per open file. Owns every editing invariant (cursor clamping,
line joins/splits, dirty tracking) and has zero I/O dependencies, so it is
fully unit-tested (`tests/test_buffer.cpp`).

### Terminal — the only platform-specific file

| Concern        | Windows                                          | Linux / macOS                     |
|----------------|--------------------------------------------------|-----------------------------------|
| Raw mode       | `SetConsoleMode` (no echo/line/processed input)  | `termios` raw mode                |
| Key events     | `_getch()` + scan codes (F1-F12, arrows, ...)    | `read()` bytes + `ESC` sequences  |
| Redirected stdin | binary byte reads (no scan codes)              | same as normal reads              |
| Resize events  | `_kbhit()` poll + size compare                  | `select()` timeout poll + `ioctl` |
| ANSI output    | `ENABLE_VIRTUAL_TERMINAL_PROCESSING` + UTF-8 CP  | native                            |
| Terminal size  | `GetConsoleScreenBufferInfo`                     | `TIOCGWINSZ` ioctl (+ env fallback) |

Both paths produce a normalized `Key` event (chars, arrows, Home/End, PageUp/
Down, F1-F12, Ctrl+letter, Escape, Resize, Eof), so nothing outside
Terminal.cpp ever sees platform code. Ctrl+S/Ctrl+Q work on POSIX because XON/XOFF is disabled;
Ctrl+C is handled as a key, so the terminal is always restored on quit.

### FileTree — browsable project tree (pure model)

The tree is a **flat vector in DFS order** (parent before children, sorted:
directories first). Expand/collapse state lives on directory nodes; `visible()`
computes the displayed list. This keeps the model terminal-independent and
fully unit-tested (`tests/test_filetree.cpp`). Ignored entries: dot-files and
common build folders (`build`, `out`, `cmake-build-*`).

### Project — Phase 1 "build"

`Project::lex_all()` finds every `*.nx` file recursively and runs the Lexer
over each, reporting token counts, diagnostics (with `file:line:column`), and
elapsed time. F3 shows the result in the OUTPUT panel:

```
NanoX build (Phase 1: lexer) — 3 file(s)
  main.nx  OK (41 tokens)
  math.nx  OK (12 tokens)
✓ Build succeeded — 53 token(s)
Finished in 0.42s
```

A failed build lists every diagnostic with its exact location. Unsaved editor
changes are explicitly not part of the build (a note says so) — the IDE never
pretends to have built something it did not.

## Key bindings

| Key | Action |
|-----|--------|
| F1 | Help overlay |
| F2 | Focus the project explorer (Up/Down select, Enter open, Right expand, Left collapse) |
| F3 | Build: lexer pass over all `*.nx` files |
| F4 | Run — *not available yet* (interpreter is a later phase); the panel says so |
| F5 | REPL in the OUTPUT panel (Enter lexes the line, Esc leaves) |
| F6 | Focus the OUTPUT panel (PgUp/PgDn scroll) |
| Ctrl+S | Save current file (prompts for a name if untitled) |
| Ctrl+T | Next tab |
| Ctrl+W | Close tab (confirms if modified) |
| Ctrl+Q / Ctrl+C | Quit (confirms if any file is modified) |

## Roadmap (page structure)

| Area | Status |
|------|--------|
| Workspace (explorer, editor, output) | done, Phase 1 |
| Build / Check | done as the Phase-1 lexer pass; grows into parse+check in Phase 2 |
| Run | Phase 3 (interpreter) |
| Diagnostics (errors/warnings/hints) | lexer diagnostics in OUTPUT + red line numbers; hints arrive with later phases |
| REPL | done (lexer-level) |
| AST viewer | Phase 2 (after the Parser) |
| IR viewer | later (after code generation) |
| Settings / `nanox.toml` | not created automatically; a `nanox.toml` (or `CMakeLists.txt`) is used for project-root discovery |

## Known limitations (by design, v1)

- **Comments are not highlighted**: the lexer skips comments and emits no
  token for them; a future lexer option will fix this without changing
  parsing behavior.
- **Undo/redo**: not implemented yet; TextBuffer is the natural place for an
  undo stack.
- **Wide characters**: layout math counts display cells (UTF-8 code points),
  so the TUI's own glyphs (▾/▸, ●, ✓, ✗, …) always align. Double-width CJK
  glyphs are still approximated as 1 cell, which can misalign the cursor in
  CJK source files.
- **Large files**: the buffer is re-lexed per keystroke; fine for educational
  programs, too slow for very large files.

## Testing strategy

- Pure models are fully unit-tested in CI on Linux, Windows, macOS:
  `test_buffer` (TextBuffer), `test_filetree` (FileTree), `test_project`
  (Project build analysis).
- The interactive parts (Terminal, Editor rendering) cannot be meaningfully
  automated; they are verified manually on each platform.
