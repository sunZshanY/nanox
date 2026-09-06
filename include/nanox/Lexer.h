#pragma once

#include "nanox/Diagnostic.h"
#include "nanox/Token.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nanox {

// Turns source text into a stream of Tokens.
//
// The lexer never throws and never stops scanning. When it encounters an
// invalid character or an unterminated literal/comment, it reports a Diagnostic
// (with the exact source location) and emits a Token of kind Invalid, then
// keeps going. This keeps the lexer robust for editor/IDE use and makes sure no
// error is silently dropped.
//
// Usage:
//   DiagnosticEngine diag("main.nx");
//   Lexer lexer(source, "main.nx", diag);
//   Token t = lexer.next();          // consume one token
//   Token look = lexer.peek();       // one-token lookahead, does not consume
class Lexer {
public:
    // `source` must remain alive for the lifetime of the Lexer (tokens store
    // string_views into it). `filename` is used only for diagnostics.
    Lexer(std::string_view source, std::string filename, DiagnosticEngine& diagnostics);

    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;

    // Returns the next token and advances past it.
    Token next();

    // Returns the next token without consuming it. Repeated calls return the
    // same token until next() is called.
    Token peek();

private:
    std::string_view source_;
    std::string filename_;
    DiagnosticEngine& diagnostics_;

    std::size_t pos_ = 0;
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;

    Token peeked_{};
    bool has_peeked_ = false;

    bool at_end() const { return pos_ >= source_.size(); }
    char current() const { return at_end() ? '\0' : source_[pos_]; }
    char lookahead(std::size_t n = 1) const {
        return pos_ + n < source_.size() ? source_[pos_ + n] : '\0';
    }
    // Consumes one character, updating line/column (handles \n, \r\n, \r).
    char advance();

    SourceLocation here() const;

    Token lex_token();
    Token lex_identifier();
    Token lex_number();
    Token lex_string();
    Token lex_operator_or_delimiter();

    void skip_trivia();
    void skip_line_comment();
    void skip_block_comment();

    // Builds a Token whose lexeme is source_[start, start + length).
    Token make(TokenKind kind, std::size_t start, std::size_t length, SourceLocation loc);
};

}  // namespace nanox
