#pragma once

#include <cstdint>
#include <ostream>

namespace nanox {

// The lexical category of a token.
//
// Design note: a single flat enum is used instead of a class hierarchy so that
// TokenKind stays trivially comparable and cheap to copy. Categories are
// grouped logically (literals, keywords, operators, delimiters) for readability.
enum class TokenKind : std::uint8_t {
    // Error / sentinel tokens.
    Invalid,     // A lexically invalid sequence (always paired with a diagnostic).
    EndOfFile,   // The end of the source text.

    // Literals.
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,

    // Keywords.
    Fn,
    Let,
    If,
    Else,
    While,
    Return,
    Int,      // type keyword: int
    Float,    // type keyword: float
    Bool,     // type keyword: bool
    String,   // type keyword: string
    Void,     // type keyword: void
    True,     // boolean literal: true
    False,    // boolean literal: false

    // Operators.
    Plus,         // +
    Minus,        // -
    Star,         // *
    Slash,        // /
    Percent,      // %
    Assign,       // =
    Equal,        // ==
    NotEqual,     // !=
    Less,         // <
    LessEqual,    // <=
    Greater,      // >
    GreaterEqual, // >=
    LogicalAnd,   // &&
    LogicalOr,    // ||
    LogicalNot,   // !
    Arrow,        // ->

    // Delimiters.
    LeftParen,    // (
    RightParen,   // )
    LeftBrace,    // {
    RightBrace,   // }
    Comma,        // ,
    Semicolon,    // ;
    Colon,        // :
};

// Returns a stable, human-readable name for a TokenKind. Used by diagnostics,
// debugging output, and tests. Never returns nullptr.
const char* token_kind_name(TokenKind kind);

inline std::ostream& operator<<(std::ostream& os, TokenKind kind) {
    return os << token_kind_name(kind);
}

}  // namespace nanox
