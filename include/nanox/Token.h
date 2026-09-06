#pragma once

#include "nanox/SourceLocation.h"
#include "nanox/TokenKind.h"

#include <ostream>
#include <string>
#include <string_view>

namespace nanox {

// A single lexed token.
//
// Design note: the lexeme is stored as a std::string_view that points directly
// into the source buffer. The source buffer must outlive all tokens (which is
// guaranteed in practice: the lexer/parser keep the source alive). This avoids
// a per-token heap allocation for every identifier/keyword/literal.
struct Token {
    TokenKind kind = TokenKind::Invalid;
    SourceLocation location{};
    std::string_view lexeme{};

    bool is(TokenKind k) const { return kind == k; }
    bool is_invalid() const { return kind == TokenKind::Invalid; }
    bool is_eof() const { return kind == TokenKind::EndOfFile; }

    // Returns the lexeme as an owning string (use only when a copy is needed).
    std::string lexeme_str() const { return std::string(lexeme); }
};

inline std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << token.kind;
    if (!token.lexeme.empty()) {
        os << " '" << token.lexeme << "'";
    }
    return os << " @" << token.location;
}

}  // namespace nanox
