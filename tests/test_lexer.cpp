#include "nanox/Lexer.h"

#include "test_framework.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace nanox;

namespace {

struct LexResult {
    explicit LexResult(std::string filename) : diag(std::move(filename)) {}
    std::vector<Token> tokens;
    DiagnosticEngine diag;
};

// Lexes the entire source and returns every token (including the trailing EOF)
// together with any diagnostics produced along the way.
LexResult lex_all(std::string_view source, std::string filename = "test.nx") {
    LexResult result(filename);
    Lexer lexer(source, filename, result.diag);

    while (true) {
        Token t = lexer.next();
        result.tokens.push_back(t);
        if (t.is_eof()) {
            break;
        }
    }
    return result;
}

void expect_token(const Token& t, TokenKind kind, std::string_view lexeme) {
    NX_CHECK(t.is(kind));
    NX_CHECK_EQ(t.lexeme, lexeme);
}

}  // namespace

// ---------------------------------------------------------------------------
// Basic / EOF
// ---------------------------------------------------------------------------

NX_TEST_CASE(empty_input_produces_single_eof) {
    auto r = lex_all("");
    NX_CHECK_EQ(r.tokens.size(), static_cast<std::size_t>(1));
    NX_CHECK(r.tokens[0].is_eof());
    NX_CHECK_EQ(r.tokens[0].location, (SourceLocation{1, 1, 0}));
}

NX_TEST_CASE(whitespace_only_produces_eof) {
    auto r = lex_all("   \t \n  ");
    NX_CHECK_EQ(r.tokens.size(), static_cast<std::size_t>(1));
    NX_CHECK(r.tokens[0].is_eof());
}

// ---------------------------------------------------------------------------
// Identifiers & keywords
// ---------------------------------------------------------------------------

NX_TEST_CASE(identifiers_are_lexed) {
    auto r = lex_all("foo _bar baz123 _ x1");
    const auto& t = r.tokens;
    expect_token(t[0], TokenKind::Identifier, "foo");
    expect_token(t[1], TokenKind::Identifier, "_bar");
    expect_token(t[2], TokenKind::Identifier, "baz123");
    expect_token(t[3], TokenKind::Identifier, "_");
    expect_token(t[4], TokenKind::Identifier, "x1");
    NX_CHECK(t[5].is_eof());
}

NX_TEST_CASE(keywords_are_recognized) {
    auto r = lex_all("fn let if else while return int float bool string void true false");
    const auto& t = r.tokens;
    expect_token(t[0], TokenKind::Fn, "fn");
    expect_token(t[1], TokenKind::Let, "let");
    expect_token(t[2], TokenKind::If, "if");
    expect_token(t[3], TokenKind::Else, "else");
    expect_token(t[4], TokenKind::While, "while");
    expect_token(t[5], TokenKind::Return, "return");
    expect_token(t[6], TokenKind::Int, "int");
    expect_token(t[7], TokenKind::Float, "float");
    expect_token(t[8], TokenKind::Bool, "bool");
    expect_token(t[9], TokenKind::String, "string");
    expect_token(t[10], TokenKind::Void, "void");
    expect_token(t[11], TokenKind::True, "true");
    expect_token(t[12], TokenKind::False, "false");
    NX_CHECK(t[13].is_eof());
}

NX_TEST_CASE(keyword_is_not_an_identifier) {
    auto r = lex_all("fnfoo");
    // "fnfoo" is not "fn"; it must stay a plain identifier.
    expect_token(r.tokens[0], TokenKind::Identifier, "fnfoo");
}

// ---------------------------------------------------------------------------
// Numbers
// ---------------------------------------------------------------------------

NX_TEST_CASE(integer_literals) {
    auto r = lex_all("0 42 123456");
    expect_token(r.tokens[0], TokenKind::IntegerLiteral, "0");
    expect_token(r.tokens[1], TokenKind::IntegerLiteral, "42");
    expect_token(r.tokens[2], TokenKind::IntegerLiteral, "123456");
}

NX_TEST_CASE(float_literals) {
    auto r = lex_all("3.14 0.5 42.0");
    expect_token(r.tokens[0], TokenKind::FloatLiteral, "3.14");
    expect_token(r.tokens[1], TokenKind::FloatLiteral, "0.5");
    expect_token(r.tokens[2], TokenKind::FloatLiteral, "42.0");
}

NX_TEST_CASE(number_followed_by_dot_without_digits_is_two_tokens) {
    // "10." is lexed as integer "10" followed by an invalid ".". This keeps the
    // lexer rules simple (a float requires digits after the '.') and surfaces a
    // real error instead of silently guessing.
    auto r = lex_all("10.");
    NX_CHECK_EQ(r.tokens.size(), static_cast<std::size_t>(3));
    expect_token(r.tokens[0], TokenKind::IntegerLiteral, "10");
    expect_token(r.tokens[1], TokenKind::Invalid, ".");
    NX_CHECK(r.tokens[2].is_eof());
    NX_CHECK(r.diag.has_errors());
}

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

NX_TEST_CASE(string_literals) {
    auto r = lex_all("\"hello\" \"a\\\"b\" \"tab\\t\"");
    expect_token(r.tokens[0], TokenKind::StringLiteral, "\"hello\"");
    expect_token(r.tokens[1], TokenKind::StringLiteral, "\"a\\\"b\"");
    expect_token(r.tokens[2], TokenKind::StringLiteral, "\"tab\\t\"");
    NX_CHECK(!r.diag.has_errors());
}

NX_TEST_CASE(unterminated_string_reports_error_at_start) {
    auto r = lex_all("\"abc");
    NX_CHECK_EQ(r.tokens.size(), static_cast<std::size_t>(2));
    expect_token(r.tokens[0], TokenKind::Invalid, "\"abc");
    NX_CHECK(r.diag.has_errors());
    NX_CHECK_EQ(r.diag.diagnostics().size(), static_cast<std::size_t>(1));
    NX_CHECK_EQ(r.diag.diagnostics()[0].location, (SourceLocation{1, 1, 0}));
    NX_CHECK_EQ(r.diag.diagnostics()[0].message, std::string("unterminated string literal"));
}

// ---------------------------------------------------------------------------
// Operators & delimiters
// ---------------------------------------------------------------------------

NX_TEST_CASE(single_and_double_char_operators) {
    auto r = lex_all("+ - * / % = == != < <= > >= && || ! ->");
    const auto& t = r.tokens;
    expect_token(t[0], TokenKind::Plus, "+");
    expect_token(t[1], TokenKind::Minus, "-");
    expect_token(t[2], TokenKind::Star, "*");
    expect_token(t[3], TokenKind::Slash, "/");
    expect_token(t[4], TokenKind::Percent, "%");
    expect_token(t[5], TokenKind::Assign, "=");
    expect_token(t[6], TokenKind::Equal, "==");
    expect_token(t[7], TokenKind::NotEqual, "!=");
    expect_token(t[8], TokenKind::Less, "<");
    expect_token(t[9], TokenKind::LessEqual, "<=");
    expect_token(t[10], TokenKind::Greater, ">");
    expect_token(t[11], TokenKind::GreaterEqual, ">=");
    expect_token(t[12], TokenKind::LogicalAnd, "&&");
    expect_token(t[13], TokenKind::LogicalOr, "||");
    expect_token(t[14], TokenKind::LogicalNot, "!");
    expect_token(t[15], TokenKind::Arrow, "->");
    NX_CHECK(t[16].is_eof());
}

NX_TEST_CASE(delimiters) {
    auto r = lex_all("( ) { } , ; :");
    const auto& t = r.tokens;
    expect_token(t[0], TokenKind::LeftParen, "(");
    expect_token(t[1], TokenKind::RightParen, ")");
    expect_token(t[2], TokenKind::LeftBrace, "{");
    expect_token(t[3], TokenKind::RightBrace, "}");
    expect_token(t[4], TokenKind::Comma, ",");
    expect_token(t[5], TokenKind::Semicolon, ";");
    expect_token(t[6], TokenKind::Colon, ":");
    NX_CHECK(t[7].is_eof());
}

NX_TEST_CASE(unexpected_character_is_an_error) {
    auto r = lex_all("@");
    NX_CHECK_EQ(r.tokens.size(), static_cast<std::size_t>(2));
    expect_token(r.tokens[0], TokenKind::Invalid, "@");
    NX_CHECK(r.diag.has_errors());
    NX_CHECK_EQ(r.diag.diagnostics()[0].message, std::string("unexpected character '@'"));
}

NX_TEST_CASE(single_ampersand_is_an_error) {
    // Only "&&" is a token; a lone "&" must be rejected rather than ignored.
    auto r = lex_all("a & b");
    const auto& t = r.tokens;
    expect_token(t[0], TokenKind::Identifier, "a");
    expect_token(t[1], TokenKind::Invalid, "&");
    expect_token(t[2], TokenKind::Identifier, "b");
    NX_CHECK(r.diag.has_errors());
}

// ---------------------------------------------------------------------------
// Comments
// ---------------------------------------------------------------------------

NX_TEST_CASE(line_and_block_comments_are_skipped) {
    auto r = lex_all("let a = 1; // trailing\nlet b = 2; /* inline */ let c = 3;");
    const auto& t = r.tokens;
    // let a = 1 ; let b = 2 ; let c = 3 ;
    expect_token(t[0], TokenKind::Let, "let");
    expect_token(t[1], TokenKind::Identifier, "a");
    expect_token(t[2], TokenKind::Assign, "=");
    expect_token(t[3], TokenKind::IntegerLiteral, "1");
    expect_token(t[4], TokenKind::Semicolon, ";");
    expect_token(t[5], TokenKind::Let, "let");
    expect_token(t[6], TokenKind::Identifier, "b");
    expect_token(t[7], TokenKind::Assign, "=");
    expect_token(t[8], TokenKind::IntegerLiteral, "2");
    expect_token(t[9], TokenKind::Semicolon, ";");
    expect_token(t[10], TokenKind::Let, "let");
    expect_token(t[11], TokenKind::Identifier, "c");
    expect_token(t[12], TokenKind::Assign, "=");
    expect_token(t[13], TokenKind::IntegerLiteral, "3");
    expect_token(t[14], TokenKind::Semicolon, ";");
    NX_CHECK(t[15].is_eof());
    NX_CHECK(!r.diag.has_errors());
}

NX_TEST_CASE(unterminated_block_comment_reports_error) {
    auto r = lex_all("let x = 1; /* oops");
    NX_CHECK(r.tokens.back().is_eof());
    NX_CHECK(r.diag.has_errors());
    NX_CHECK_EQ(r.diag.diagnostics()[0].message, std::string("unterminated block comment"));
}

// ---------------------------------------------------------------------------
// Location tracking
// ---------------------------------------------------------------------------

NX_TEST_CASE(locations_track_lines_and_columns) {
    auto r = lex_all("let a = 1;\nlet b = 2;");
    const auto& t = r.tokens;

    NX_CHECK_EQ(t[0].location, (SourceLocation{1, 1, 0}));   // let
    NX_CHECK_EQ(t[1].location, (SourceLocation{1, 5, 4}));   // a
    NX_CHECK_EQ(t[2].location, (SourceLocation{1, 7, 6}));   // =
    NX_CHECK_EQ(t[3].location, (SourceLocation{1, 9, 8}));   // 1
    NX_CHECK_EQ(t[4].location, (SourceLocation{1, 10, 9}));  // ;
    NX_CHECK_EQ(t[5].location, (SourceLocation{2, 1, 11}));  // let
    NX_CHECK_EQ(t[6].location, (SourceLocation{2, 5, 15}));  // b
    NX_CHECK_EQ(t[7].location, (SourceLocation{2, 7, 17}));  // =
    NX_CHECK_EQ(t[8].location, (SourceLocation{2, 9, 19}));  // 2
    NX_CHECK_EQ(t[9].location, (SourceLocation{2, 10, 20})); // ;
}

NX_TEST_CASE(crlf_line_endings_are_single_newlines) {
    auto r = lex_all("a\r\nb");
    const auto& t = r.tokens;
    expect_token(t[0], TokenKind::Identifier, "a");
    NX_CHECK_EQ(t[0].location, (SourceLocation{1, 1, 0}));
    expect_token(t[1], TokenKind::Identifier, "b");
    NX_CHECK_EQ(t[1].location, (SourceLocation{2, 1, 3}));
}

// ---------------------------------------------------------------------------
// Lookahead
// ---------------------------------------------------------------------------

NX_TEST_CASE(peek_does_not_consume) {
    DiagnosticEngine diag("test.nx");
    Lexer lexer("let x = 1;", "test.nx", diag);

    Token a = lexer.peek();
    Token b = lexer.peek();
    NX_CHECK(a.is(TokenKind::Let));
    NX_CHECK(b.is(TokenKind::Let));
    NX_CHECK_EQ(a.location, b.location);

    Token c = lexer.next();
    NX_CHECK(c.is(TokenKind::Let));
    NX_CHECK_EQ(c.location, a.location);

    Token d = lexer.next();
    NX_CHECK(d.is(TokenKind::Identifier));
}

// ---------------------------------------------------------------------------
// Whole program
// ---------------------------------------------------------------------------

NX_TEST_CASE(hello_world_program_lexes_to_expected_sequence) {
    const std::string program = R"(
fn main() {
    println("Hello, NanoX!");
}

let x: int = 10;

if x > 5 {
    println(x);
}

while x > 0 {
    x = x - 1;
}
)";
    auto r = lex_all(program);

    const std::vector<TokenKind> expected = {
        TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen,
        TokenKind::LeftBrace, TokenKind::Identifier, TokenKind::LeftParen,
        TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace, TokenKind::Let, TokenKind::Identifier, TokenKind::Colon,
        TokenKind::Int, TokenKind::Assign, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::If, TokenKind::Identifier, TokenKind::Greater, TokenKind::IntegerLiteral,
        TokenKind::LeftBrace, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier,
        TokenKind::RightParen, TokenKind::Semicolon, TokenKind::RightBrace,
        TokenKind::While, TokenKind::Identifier, TokenKind::Greater, TokenKind::IntegerLiteral,
        TokenKind::LeftBrace, TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier,
        TokenKind::Minus, TokenKind::IntegerLiteral, TokenKind::Semicolon, TokenKind::RightBrace,
        TokenKind::EndOfFile,
    };

    NX_CHECK_EQ(r.tokens.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (r.tokens[i].kind != expected[i]) {
            std::ostringstream msg;
            msg << "token " << i << ": expected " << expected[i]
                << " but got " << r.tokens[i].kind;
            throw ::nanox::testing::TestFailure{__FILE__, __LINE__, msg.str()};
        }
    }
    NX_CHECK(!r.diag.has_errors());
}
