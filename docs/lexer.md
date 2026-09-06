# Lexer design (Phase 1)

This document records the design decisions behind NanoX's token system and
lexer, and explains *why* each choice was made.

## Goals

1. Produce a flat, cheap, easy-to-inspect token stream.
2. Never crash and never hide errors — an invalid character must surface a
   diagnostic with a `line:column` location, not be silently dropped.
3. Be reusable by later phases (Parser, diagnostics, tooling).

## Components

### `SourceLocation`

- `line` and `column` are **1-based** (first character is `1:1`), matching what
  editors and humans expect.
- `offset` is a **0-based byte offset** into the source. It is stored now so
  future phases (e.g. rendering a caret under an error) can jump straight to
  the offending text without re-scanning.
- Prints as `line:column` (e.g. `7:23`).

### `TokenKind`

A single `enum class` instead of a class hierarchy. Rationale: a token kind is
a cheap, frequently-compared value; an enum keeps it trivially copyable and
`switch`-able, which the parser will rely on heavily.

Kinds are grouped for readability: literals, keywords, operators, delimiters.
There is a dedicated `Invalid` kind for lexically-bad sequences and a
`EndOfFile` sentinel so consumers never have to handle "no token".

### `Token`

`lexeme` is a `std::string_view` into the source buffer. This means **zero
allocation per token** for identifiers/keywords/literals. The trade-off is a
lifetime constraint: the source must outlive its tokens. In practice the
source is a single string owned by the driver, so this is safe and fast.

### `DiagnosticEngine`

- Owns the source filename and a vector of diagnostics.
- `error()` / `warning()` record a message with a `SourceLocation`.
- `print_all()` emits the canonical form `file:line:column: error: message`.

It is deliberately decoupled from the lexer (the lexer only *references* it) so
that the Parser and later phases can reuse it without coupling.

## Lexing rules

| Input | Result |
|-------|--------|
| `[A-Za-z_][A-Za-z0-9_]*` | `Identifier`, unless it is a keyword |
| `[0-9]+` | `IntegerLiteral` |
| `[0-9]+\.[0-9]+` | `FloatLiteral` |
| `"..."` with `\\` escapes | `StringLiteral` |
| `+ - * / %` | arithmetic operators |
| `== != < <= > >=` | comparison operators |
| `&& \|\| !` | logical operators |
| `=` | `Assign` |
| `->` | `Arrow` (function return type) |
| `( ) { } , ; :` | delimiters |
| `// ...` | line comment (skipped) |
| `/* ... */` | block comment (skipped) |
| anything else | `Invalid` token **plus** a diagnostic |

### Keyword table

Keywords are looked up in a `std::unordered_map<std::string_view, TokenKind>`
rather than an `if`/`else` chain. Adding a keyword is a one-line change. The
string-view keys point into the source buffer, which is valid only during the
lookup (we never store the map keys).

Keywords: `fn let if else while return int float bool string void true false`.

Note that `true`/`false` are lexed as keywords (they are boolean literals);
the interpreter will interpret them as `bool` values in a later phase.

### Number rules

A float **requires** digits after the `.`. So:

- `3.14` → `FloatLiteral`
- `10.`  → `IntegerLiteral(10)` then `Invalid(.)` with a diagnostic

This keeps the rule simple and refuses to guess about a trailing dot (there is
no member-access syntax in NanoX v1, so `.` is always unexpected). Scientific
notation (`1e5`) and digit separators (`1_000`) are intentionally deferred.

### String rules

- Must not contain an unescaped newline.
- `\\` escapes are consumed at the lexical level but **not decoded** yet
  (decoding is a later phase).
- An unterminated string reports `unterminated string literal` at the opening
  quote and emits an `Invalid` token.

### Comments

- `//` runs to end of line.
- `/* */` block comments are supported but **not nested** (kept simple; nesting
  is rare and can be added later without breaking anything).
- An unterminated block comment reports `unterminated block comment`.

### Newline handling

`\n`, `\r\n`, and a lone `\r` are all treated as a single newline, so the
lexer behaves identically on Linux, Windows, and old-Mac source files.

## Error recovery

The lexer never throws. On error it:

1. records a `Diagnostic` at the exact location, and
2. emits an `Invalid` token (or, for comments, skips to the end).

The lexer then continues scanning, so a single bad character does not hide
subsequent good tokens.

## Why `std::string_view` over `std::string`

`std::string` would copy every identifier/keyword/literal, which is wasted work
for a compiler that mostly needs to *read* lexemes. `std::string_view` is
non-owning and C++17-native, so it keeps the hot path allocation-free.

## Testing strategy

The lexer is covered by `tests/test_lexer.cpp` using the dependency-free
`tests/test_framework.hpp`. Highlights:

- exact token kinds *and* lexemes,
- exact `line:column` locations (including CRLF input),
- error cases (unterminated string/comment, unknown character, lone `&`),
- `peek()` lookahead semantics,
- a full `hello.nx` program lexed to its expected token sequence.
