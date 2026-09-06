#include "nanox/TokenKind.h"

namespace nanox {

const char* token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::Invalid:      return "Invalid";
        case TokenKind::EndOfFile:    return "EndOfFile";

        case TokenKind::Identifier:    return "Identifier";
        case TokenKind::IntegerLiteral:return "IntegerLiteral";
        case TokenKind::FloatLiteral:  return "FloatLiteral";
        case TokenKind::StringLiteral: return "StringLiteral";

        case TokenKind::Fn:      return "Fn";
        case TokenKind::Let:     return "Let";
        case TokenKind::If:      return "If";
        case TokenKind::Else:    return "Else";
        case TokenKind::While:   return "While";
        case TokenKind::Return:  return "Return";
        case TokenKind::Int:     return "Int";
        case TokenKind::Float:   return "Float";
        case TokenKind::Bool:    return "Bool";
        case TokenKind::String:  return "String";
        case TokenKind::Void:    return "Void";
        case TokenKind::True:    return "True";
        case TokenKind::False:   return "False";

        case TokenKind::Plus:         return "Plus";
        case TokenKind::Minus:        return "Minus";
        case TokenKind::Star:         return "Star";
        case TokenKind::Slash:        return "Slash";
        case TokenKind::Percent:      return "Percent";
        case TokenKind::Assign:       return "Assign";
        case TokenKind::Equal:        return "Equal";
        case TokenKind::NotEqual:     return "NotEqual";
        case TokenKind::Less:         return "Less";
        case TokenKind::LessEqual:    return "LessEqual";
        case TokenKind::Greater:      return "Greater";
        case TokenKind::GreaterEqual: return "GreaterEqual";
        case TokenKind::LogicalAnd:   return "LogicalAnd";
        case TokenKind::LogicalOr:    return "LogicalOr";
        case TokenKind::LogicalNot:   return "LogicalNot";
        case TokenKind::Arrow:        return "Arrow";

        case TokenKind::LeftParen:  return "LeftParen";
        case TokenKind::RightParen: return "RightParen";
        case TokenKind::LeftBrace:  return "LeftBrace";
        case TokenKind::RightBrace: return "RightBrace";
        case TokenKind::Comma:      return "Comma";
        case TokenKind::Semicolon:  return "Semicolon";
        case TokenKind::Colon:      return "Colon";
    }
    return "<unknown>";
}

}  // namespace nanox
