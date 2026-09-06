#include "nanox/Token.h"

#include "test_framework.hpp"

#include <sstream>
#include <string>

using namespace nanox;

NX_TEST_CASE(token_defaults_to_invalid) {
    Token t;
    NX_CHECK(t.is_invalid());
    NX_CHECK(!t.is_eof());
    NX_CHECK(t.lexeme.empty());
    NX_CHECK_EQ(t.location, (SourceLocation{1, 1, 0}));
}

NX_TEST_CASE(token_kind_names_are_stable) {
    NX_CHECK_EQ(std::string(token_kind_name(TokenKind::Fn)), std::string("Fn"));
    NX_CHECK_EQ(std::string(token_kind_name(TokenKind::EndOfFile)), std::string("EndOfFile"));
    NX_CHECK_EQ(std::string(token_kind_name(TokenKind::Identifier)), std::string("Identifier"));
    NX_CHECK_EQ(std::string(token_kind_name(TokenKind::IntegerLiteral)), std::string("IntegerLiteral"));
    NX_CHECK_EQ(std::string(token_kind_name(TokenKind::Arrow)), std::string("Arrow"));
}

NX_TEST_CASE(source_location_equality) {
    SourceLocation a{1, 2, 3};
    SourceLocation b{1, 2, 3};
    SourceLocation c{1, 3, 3};
    SourceLocation d{2, 2, 3};
    NX_CHECK(a == b);
    NX_CHECK(a != c);
    NX_CHECK(a != d);
}

NX_TEST_CASE(source_location_prints_line_column) {
    std::ostringstream os;
    os << (SourceLocation{7, 23, 0});
    NX_CHECK_EQ(os.str(), std::string("7:23"));
}

NX_TEST_CASE(token_stream_operator_is_helpful) {
    Token t;
    t.kind = TokenKind::Identifier;
    t.lexeme = "foo";
    t.location = (SourceLocation{2, 5, 10});

    std::ostringstream os;
    os << t;
    NX_CHECK_EQ(os.str(), std::string("Identifier 'foo' @2:5"));
}
