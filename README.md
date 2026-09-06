# NanoX

A small, modern, easy-to-learn programming language and its compiler, built
from scratch in C++17.

> **Status:** Phase 1 — Lexer (Token system + lexing) is complete. The Parser,
> AST, and Interpreter are planned but not yet implemented.

## Goal

NanoX is an educational compiler project. The first milestone is a classic
pipeline:

```
Lexer → Parser → AST → Interpreter
```

LLVM-based code generation is explicitly **out of scope** for now and will only
be introduced after the interpreter milestone is solid.

## Project layout

```
nanox/
├── CMakeLists.txt
├── include/nanox/        # core headers (SourceLocation, Token, Lexer, FileTree, Project)
├── include/nanox/editor/ # editor headers (TextBuffer, Terminal, Editor)
├── src/                  # core implementation
├── src/editor/           # editor implementation
├── tests/                # unit tests (dependency-free mini framework)
├── examples/             # sample programs + the nanox CLI + lexer_dump
├── docs/                 # design notes
└── .github/workflows/    # CI on Linux, Windows, macOS
```

## Building

Requirements: CMake 3.20+, a C++17 compiler.

```sh
cmake -S . -B build
cmake --build build
```

Run the tests:

```sh
ctest --test-dir build --output-on-failure
```

Start the IDE-style editor:

```sh
./build/examples/nanox                  # Linux/macOS — current dir as project
build\examples\nanox.exe                # Windows
./build/examples/nanox examples/hello.nx       # open a file
./build/examples/nanox path/to/project-dir     # open a project
```

The TUI has a project explorer, multi-file editor tabs, syntax highlighting
and diagnostics from the Lexer:

| Key      | Action                                    |
|----------|-------------------------------------------|
| F1       | Help overlay                              |
| F2       | Focus the project explorer                |
| F3       | Build: lexer pass over all `*.nx` files   |
| F4       | Run (arrives with the interpreter phase)  |
| F5       | REPL (lexer-level)                        |
| F6       | Focus the OUTPUT panel                    |
| Ctrl+S   | Save current file                         |
| Ctrl+T / Ctrl+W | Next / close tab                  |
| Ctrl+Q / Ctrl+C | Quit (confirms unsaved changes)   |

To just dump the token stream of a file, use `lexer_dump`:

```sh
./build/examples/lexer_dump examples/hello.nx
```

## Sample syntax (v1)

```rust
fn main() {
    println("Hello, NanoX!");
}

let x: int = 10;

fn add(a: int, b: int) -> int {
    return a + b;
}

if x > 5 {
    println(x);
}

while x > 0 {
    x = x - 1;
}
```

## What's implemented

- `SourceLocation` — `line:column` + byte offset, used by every diagnostic.
- `TokenKind` / `Token` — flat enum + zero-copy `std::string_view` lexemes.
- `DiagnosticEngine` — collects and prints `file:line:column: error: ...`.
- `Lexer` — identifiers, keywords, int/float/string literals, operators,
  delimiters, `//` and `/* */` comments, EOF, with full location tracking and
  error recovery.
- `FileTree` / `Project` — project browsing and the Phase 1 "build"
  (lexer pass over every `*.nx` file).
- `nanox` CLI — IDE-style TUI (project explorer, tabs, OUTPUT panel, REPL,
  F1-F6 keys); this will grow into the full driver in later phases.
- Editor toolkit — `TextBuffer` (pure editing logic, fully unit-tested),
  `Terminal` (Windows/POSIX platform abstraction), `Editor` (TUI rendering).

See `docs/lexer.md` and `docs/editor.md` for the design rationale and roadmap.

## Contributing principles

- Modular development: one module at a time, with tests before moving on.
- No hidden errors: the lexer reports problems, it never silently skips them.
- Readability and clear architecture over cleverness.
