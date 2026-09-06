#include "nanox/Lexer.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace nanox {

namespace {

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_continue(char c) {
    return is_ident_start(c) || is_digit(c);
}

// Keyword table. Kept as a local translation unit table rather than a giant
// if/else chain so that adding a keyword is a one-line change and lookup is
// O(1). The string_views point into the program's source buffer, which is why
// they are safe to use as map keys only for the duration of the lookup.
const std::unordered_map<std::string_view, TokenKind> kKeywords = {
    {"fn", TokenKind::Fn},
    {"let", TokenKind::Let},
    {"if", TokenKind::If},
    {"else", TokenKind::Else},
    {"while", TokenKind::While},
    {"return", TokenKind::Return},
    {"int", TokenKind::Int},
    {"float", TokenKind::Float},
    {"bool", TokenKind::Bool},
    {"string", TokenKind::String},
    {"void", TokenKind::Void},
    {"true", TokenKind::True},
    {"false", TokenKind::False},
};

TokenKind keyword_or_identifier(std::string_view text) {
    auto it = kKeywords.find(text);
    if (it != kKeywords.end()) {
        return it->second;
    }
    return TokenKind::Identifier;
}

}  // namespace

Lexer::Lexer(std::string_view source, std::string filename, DiagnosticEngine& diagnostics)
    : source_(source), filename_(std::move(filename)), diagnostics_(diagnostics) {}

char Lexer::advance() {
    char c = source_[pos_];
    ++pos_;

    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else if (c == '\r') {
        // Treat "\r\n" as a single newline: the following '\n' will reset the
        // line/column, so here we only move the column forward.
        if (pos_ < source_.size() && source_[pos_] == '\n') {
            ++column_;
        } else {
            // A lone '\r' (old Mac line ending) is still a newline.
            ++line_;
            column_ = 1;
        }
    } else {
        ++column_;
    }
    return c;
}

SourceLocation Lexer::here() const {
    return SourceLocation{line_, column_, static_cast<std::uint32_t>(pos_)};
}

Token Lexer::make(TokenKind kind, std::size_t start, std::size_t length, SourceLocation loc) {
    Token token;
    token.kind = kind;
    token.location = loc;
    token.lexeme = source_.substr(start, length);
    return token;
}

Token Lexer::next() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }
    return lex_token();
}

Token Lexer::peek() {
    if (!has_peeked_) {
        peeked_ = lex_token();
        has_peeked_ = true;
    }
    return peeked_;
}

Token Lexer::lex_token() {
    skip_trivia();

    SourceLocation loc = here();

    if (at_end()) {
        return make(TokenKind::EndOfFile, pos_, 0, loc);
    }

    char c = current();
    if (is_ident_start(c)) {
        return lex_identifier();
    }
    if (is_digit(c)) {
        return lex_number();
    }
    if (c == '"') {
        return lex_string();
    }
    return lex_operator_or_delimiter();
}

Token Lexer::lex_identifier() {
    const std::size_t start = pos_;
    const SourceLocation loc = here();

    while (is_ident_continue(current())) {
        advance();
    }

    const std::size_t length = pos_ - start;
    return make(keyword_or_identifier(source_.substr(start, length)), start, length, loc);
}

Token Lexer::lex_number() {
    const std::size_t start = pos_;
    const SourceLocation loc = here();

    while (is_digit(current())) {
        advance();
    }

    TokenKind kind = TokenKind::IntegerLiteral;
    if (current() == '.' && is_digit(lookahead())) {
        advance();  // consume '.'
        while (is_digit(current())) {
            advance();
        }
        kind = TokenKind::FloatLiteral;
    }

    return make(kind, start, pos_ - start, loc);
}

Token Lexer::lex_string() {
    const std::size_t start = pos_;
    const SourceLocation loc = here();
    advance();  // opening quote

    while (true) {
        // A string may not contain an unescaped newline and must be closed.
        if (at_end() || current() == '\n' || current() == '\r') {
            diagnostics_.error(loc, "unterminated string literal");
            return make(TokenKind::Invalid, start, pos_ - start, loc);
        }

        char c = current();
        if (c == '"') {
            advance();  // closing quote
            break;
        }

        if (c == '\\') {
            advance();  // backslash
            if (at_end()) {
                diagnostics_.error(loc, "unterminated string literal");
                return make(TokenKind::Invalid, start, pos_ - start, loc);
            }
            advance();  // escaped character (escape decoding is a later phase)
            continue;
        }

        advance();
    }

    return make(TokenKind::StringLiteral, start, pos_ - start, loc);
}

Token Lexer::lex_operator_or_delimiter() {
    const std::size_t start = pos_;
    const SourceLocation loc = here();
    char c = advance();

    auto single = [&](TokenKind kind) {
        return make(kind, start, pos_ - start, loc);
    };

    switch (c) {
        case '+': return single(TokenKind::Plus);
        case '-':
            if (current() == '>') { advance(); return single(TokenKind::Arrow); }
            return single(TokenKind::Minus);
        case '*': return single(TokenKind::Star);
        case '/': return single(TokenKind::Slash);
        case '%': return single(TokenKind::Percent);

        case '=':
            if (current() == '=') { advance(); return single(TokenKind::Equal); }
            return single(TokenKind::Assign);

        case '!':
            if (current() == '=') { advance(); return single(TokenKind::NotEqual); }
            return single(TokenKind::LogicalNot);

        case '<':
            if (current() == '=') { advance(); return single(TokenKind::LessEqual); }
            return single(TokenKind::Less);

        case '>':
            if (current() == '=') { advance(); return single(TokenKind::GreaterEqual); }
            return single(TokenKind::Greater);

        case '&':
            if (current() == '&') { advance(); return single(TokenKind::LogicalAnd); }
            break;  // single '&' is not part of the language yet -> error below

        case '|':
            if (current() == '|') { advance(); return single(TokenKind::LogicalOr); }
            break;  // single '|' is not part of the language yet -> error below

        case '(': return single(TokenKind::LeftParen);
        case ')': return single(TokenKind::RightParen);
        case '{': return single(TokenKind::LeftBrace);
        case '}': return single(TokenKind::RightBrace);
        case ',': return single(TokenKind::Comma);
        case ';': return single(TokenKind::Semicolon);
        case ':': return single(TokenKind::Colon);

        default: break;
    }

    diagnostics_.error(loc, "unexpected character '" + std::string(1, c) + "'");
    return make(TokenKind::Invalid, start, pos_ - start, loc);
}

void Lexer::skip_trivia() {
    while (!at_end()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
            continue;
        }
        if (c == '/' && lookahead() == '/') {
            skip_line_comment();
            continue;
        }
        if (c == '/' && lookahead() == '*') {
            skip_block_comment();
            continue;
        }
        break;
    }
}

void Lexer::skip_line_comment() {
    // Consume until (but not including) the newline; skip_trivia will handle it.
    while (!at_end() && current() != '\n' && current() != '\r') {
        advance();
    }
}

void Lexer::skip_block_comment() {
    const SourceLocation start = here();
    advance();  // '/'
    advance();  // '*'

    while (true) {
        if (at_end()) {
            diagnostics_.error(start, "unterminated block comment");
            return;
        }
        if (current() == '*' && lookahead() == '/') {
            advance();  // '*'
            advance();  // '/'
            return;
        }
        advance();
    }
}

}  // namespace nanox
