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
│ NanoX 0.1.0 [NORMAL]         nanox / main.nx               ● Ready │
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
│ ^P Commands  ^B Build  F4 Run  ^R Redo  : Command  ^Q Quit           │
└──────────────────────────────────────────────────────────────────────┘
```

- **Title bar** — version, the current editing state (`[NORMAL]`, `[INSERT]`,
  `[COMMAND]`, or `[EDITOR]` in nano mode), project name and current file
  (`nanox / main.nx`), status dot: `● Ready` (green) / `● Modified` (yellow) /
  `● Errors` (red).
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
  appear here too. With no prompt active the bar shows the keys that are live in
  the current editing mode and state, generated from the keymap rather than
  written out in the renderer (see "Editing modes" below); a narrow terminal
  truncates it.

### Project root discovery

`nanox` walks up from the start directory looking for `nanox.toml`, the project
marker, and opens that directory as the project. Running `nanox` from inside
`build/` therefore opens the real project instead of showing CMake artifacts;
`build/`, `out/`, `cmake-build-*` and dot-folders are never displayed in the
tree anyway. Without a marker anywhere up the tree, the start directory itself
is the project. (An unrelated `CMakeLists.txt` is deliberately **not** a
marker: attaching to whatever parent happens to contain one — a home
directory, for example — would mean scanning a huge tree as a "project".)

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

## Architecture: six layers

```
Editor      (src/editor/Editor.cpp)   panel/focus state, key loop, rendering
   │
   ├── Keymap (src/editor/Keymap.cpp)   EditingMode + EditorMode -> Command
   │        KeyChord notation, default bindings, multi-key sequences
   │
   ├── Command (src/editor/Command.cpp)  the actions themselves, by name
   │        command_name() / command_from_name(): the plugin-facing identity
   │
   ├── CommandParser (src/editor/CommandParser.cpp)  ":" ex-commands
   │        :w :q :wq :q! :set mode <vim|nano|hybrid>
   │
   ├── Terminal  (src/editor/Terminal.cpp)  platform abstraction
   │        raw mode, key events (F1-F12, Ctrl, Alt), ANSI output, size
   │
   ├── TextBuffer (src/editor/TextBuffer.cpp)  per-file editing logic
   │        lines + cursor, inserts/deletes, undo/redo, search/replace
   │
   └── Project model (src/Project.cpp, src/FileTree.cpp)  project domain
            *.nx discovery, tree browsing, Phase-1 build (lexer pass)
```

### Key handling: keys are never compared to literals

`Editor::execute(Command)` is the only place an action is carried out. A key
press reaches it through exactly one route:

```
key event -> KeyChord -> Keymap.feed(mode, state) -> Command -> execute()
```

Nothing in the rendering code knows a key exists. The footer text and the help
overlay are both *generated* from the live keymap (`Keymap::footer_hint`,
`Keymap::bindings_for`), so a rebinding cannot leave a stale hint on screen.

`KeyChord` folds Ctrl and Alt onto `Kind::Char` plus a modifier flag, so one
lookup handles plain, control and meta keys. It round-trips through a text
notation — `"Ctrl+S"`, `"Alt+U"`, `"F3"`, `"Escape"` — which is what a plugin
API would speak:

```cpp
keymap.bind("normal", "Ctrl+S", "save");   // names, not enums
```

`Keymap` also owns multi-key sequences, so `dd` and `yy` are a pending prefix
handled inside `feed()`; the Editor never sees a half-typed sequence.

### TextBuffer — pure editing logic

One `TextBuffer` per open file. Owns every editing invariant (cursor clamping,
line joins/splits, dirty tracking, undo history, search/replace) and has zero
I/O dependencies, so it is fully unit-tested (`tests/test_buffer.cpp`).

Undo is snapshot-based and capped at 512 steps. Consecutive character inserts
coalesce into one unit, so undoing a typed word takes one step rather than one
per letter; any other edit starts a new unit. The buffer tracks the undo depth
at which it was last saved, so undoing back to that point clears the `Modified`
marker. If the oldest snapshot is dropped by the cap the save point becomes
unreachable and the buffer stays `Modified` rather than risking a
clean-looking buffer with unsaved changes.

The clipboard/register lives in the Editor, not the buffer: `TextBuffer`
returns the affected text from `delete_current_line()` / `yank_line()` and the
Editor remembers it (plus whether it was linewise).

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
Down, F1-F12, Ctrl+*char*, Alt+*char*, Escape, Resize, Eof), so nothing outside
Terminal.cpp ever sees platform code. Ctrl+S/Ctrl+Q work on POSIX because
XON/XOFF is disabled; Ctrl+C is handled as a key, so the terminal is always
restored on quit.

Ctrl combinations cover `0x1C`–`0x1F` as well as `0x01`–`0x1A`, which is what
makes nano's `^\` (Replace) and `^_` (Go To Line) reachable — a plain
`'a' + c - 1` mapping would drop them.

Alt is the one combination the two platforms report differently, and the
handling is worth knowing about:

- **Windows** sends Alt+*key* as a `0x00` prefix followed by the key's scan
  code, so a scan-code table recovers the character.
- **POSIX** sends Alt+*key* as `ESC` followed by the character — exactly how a
  bare `ESC` key press starts. `read_escape_sequence()` tells them apart by
  polling: a following printable byte is `Alt+<char>`, `[`/`O` continues the
  CSI/SS3 path, and nothing within ~50 ms is a bare `Escape`. That timeout is
  what keeps `ESC` (Vim's INSERT → NORMAL) feeling immediate.

Note that a **redirected stdin** (a pipe or file) carries plain bytes only:
arrows and F-keys cannot be produced at all, and Alt is indistinguishable from
`ESC` followed by a key. That path is used by the verification harness, so the
Alt bindings are exercised on POSIX by the harness and by unit tests, not on
Windows.

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

## Editing modes

`--mode=vim|nano|hybrid` selects how keys are interpreted; **vim is the
default**. The mode can be changed while running with **F8** (cycles) or
`:set mode <vim|nano|hybrid>`.

| Mode | Opens in | Character keys | Leaving the editing state |
|------|----------|----------------|---------------------------|
| `vim` | NORMAL | do nothing until `i`/`a`/`o`/`O` | `i` inserts, `:` opens a command |
| `nano` | EDITOR | always edit (nano has no modes) | — |
| `hybrid` | INSERT | always edit | `ESC` drops into Vim NORMAL |

The status indicator in the title bar shows `NORMAL` / `INSERT` / `COMMAND`, or
`EDITOR` in nano mode (which has no modes to report).

### Global keys (work in every mode)

| Key | Action |
|-----|--------|
| F1 | Help overlay (key list generated from the live keymap) |
| F2 | Focus the project explorer (Up/Down select, Enter open, Right expand, Left collapse) |
| F3 | Build: lexer pass over all `*.nx` files |
| F4 | Run — *not available yet* (interpreter is a later phase); the panel says so |
| F5 | REPL in the OUTPUT panel (Enter lexes the line, Esc leaves) |
| F6 | Focus the OUTPUT panel (PgUp/PgDn scroll) |
| F8 | Cycle the editing mode: vim → nano → hybrid |
| Ctrl+S | Save current file |
| Ctrl+T | Next tab |
| Ctrl+Q / Ctrl+C | Quit (confirms if any file is modified) |

The editing keymap is live only while the editor pane has focus; the tree and
output panels keep the plain navigation keys above.

### Vim mode

| Key | Action |
|-----|--------|
| `i` `a` `I` `A` | Insert here / after the cursor / at line start / at line end |
| `o` `O` | Open a line below / above and insert |
| `h` `j` `k` `l` | Left / down / up / right |
| `0` `$` | Start / end of line |
| `x` | Delete the character under the cursor |
| `D` | Delete to the end of the line |
| `dd` `yy` | Cut / copy the current line |
| `p` `P` | Paste after / before the cursor |
| `u` / `Ctrl+R` | Undo / redo |
| `:` or `Ctrl+P` | Command line: `:w` `:q` `:wq` `:q!` `:x` `:set mode …` |
| `Ctrl+B` | Build |
| `Ctrl+W` | Close tab |
| `ESC` | Back to NORMAL |

### Nano mode

| Key | Action |
|-----|--------|
| `Ctrl+O` | Write out — prompts `File Name to Write:`, prefilled with the current path |
| `Ctrl+X` | Exit (closes the tab; quits when it is the last one) |
| `Ctrl+W` | Search — prompts `Search:` |
| `Ctrl+\` | Replace — prompts `Search:` then `Replace with:` |
| `Ctrl+K` | Cut the current line into the clipboard |
| `Ctrl+U` | Paste the clipboard at the cursor |
| `Ctrl+G` | Help |
| `Ctrl+_` | Go to line — accepts `line` or `line,column` (1-based) |
| `Alt+U` / `Alt+E` | Undo / redo |
| `Alt+6` | Copy the current line |

### Hybrid mode

INSERT uses nano's keys (`Ctrl+O` `Ctrl+W` `Ctrl+K` `Ctrl+U` `Ctrl+G`) plus
`Ctrl+S`/`Ctrl+Q`; `ESC` gives Vim NORMAL, which is bound exactly like Vim's.

### Resolved key conflicts

Where the two editing traditions disagree, the resolution is:

| Conflict | Resolution |
|----------|------------|
| `Ctrl+W` — nano's Search vs. the tab-close shortcut | Search in nano/hybrid; close-tab stays on `Ctrl+W` in Vim only, and `Ctrl+X` closes a tab in every mode |
| `Ctrl+R` — Vim's redo vs. the Run shortcut | Vim keeps `Ctrl+R` = redo (matching Vim); Run stays on `F4`, and the Vim footer says `F4 Run`. Nano and hybrid leave `Ctrl+R` free, so they keep `^R Run` |
| `Ctrl+K` / `Ctrl+U` | Cut/paste in nano and hybrid INSERT; unbound in Vim INSERT, whose footer lists only `ESC/^S/^Q` |
| `Ctrl+G` | Help in nano (unbound elsewhere, where it also means Help) |
| `Ctrl+X` | Close tab everywhere; quits when it was the last tab |

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
| Editing modes | done: Vim / Nano / Hybrid, selectable with `--mode`, F8, or `:set mode` |
| Keymap / plugin API | the `Keymap` layer already speaks names — `keymap.bind("normal", "Ctrl+S", "save")` — and `Command` has a stable name for every action, so `nanox.keymap(...)` / `nanox.command(...)` need a script host, not a redesign. That host arrives with the Lua plugin phase. |
| Settings / `nanox.toml` | `nanox.toml` is the project-root marker today; settings keys are planned |

## Known limitations (by design, v1)

- **Comments are not highlighted**: the lexer skips comments and emits no
  token for them; a future lexer option will fix this without changing
  parsing behavior.
- **Vim is a subset**: the motions the spec lists (`h j k l 0 $`, insert
  variants, `x D dd yy p P u ^R`). There is no word motion (`w`/`b`/`e`), no
  counts (`3dd`), no visual mode, registers, marks, or `.` repeat.
- **Replace is replace-all**, not nano's per-match confirmation: `^\` asks for
  the search text and the replacement, then replaces every match at or after
  the cursor in one undoable step and reports the count in the OUTPUT panel.
- **Search is literal and case-sensitive**, never crosses a line break, and
  wraps around the end of the buffer. The prompt line has no history or
  `Left`/`Right` editing — only backspace.
- **The clipboard is internal to the editor**, not the system clipboard.
- **Wide characters**: layout math uses a compact East Asian Width table
  (`DisplayWidth`): combining/zero-width marks count 0, CJK/Hangul/fullwidth
  glyphs count 2, and everything else -- including the TUI's own ▾/▸, ●, ✓, ✗,
  … -- counts 1. Every framed row therefore fills exactly `cols` cells, so the
  borders stay aligned with CJK paths, file names and source. The *cursor*
  column is still a byte offset (`TextBuffer` has no encoding knowledge), so a
  cursor sitting after CJK text can be drawn a few cells off within the editor
  cell; the frame itself is unaffected.
- **Large files**: the buffer is re-lexed per keystroke; fine for educational
  programs, too slow for very large files.

## Testing strategy

- Pure models are fully unit-tested in CI on Linux, Windows, macOS:
  `test_buffer` (TextBuffer editing, undo/redo, search/replace),
  `test_keymap` (chord notation, per-mode bindings, sequences, rebinding),
  `test_command_parser` (ex-commands), `test_filetree` (FileTree),
  `test_project` (Project build analysis), `test_display_width` (East Asian
  cell widths and truncation).
- The interactive parts (Terminal, Editor rendering) cannot be unit-tested, but
  they are *not* left to manual checking: `Terminal` accepts redirected stdin as
  raw byte reads, so the whole TUI can be driven over a pipe and asserted on —
  mode, footer, status word, ESC transitions, prompts, and the file that
  `:wq` / `^O` actually writes. See "Known limitations" for what a pipe cannot
  express (arrows, F-keys, Alt on Windows).
