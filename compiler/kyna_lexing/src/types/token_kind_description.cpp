#include "kyna/lexing/token_kind_description.hpp"

namespace kyna {
std::string tokenName(TokenKind kind) {
  switch (kind) {
  case TokenKind::End:
    return "end of file";
  case TokenKind::Invalid:
    return "invalid token";
  case TokenKind::Identifier:
    return "identifier";
  case TokenKind::Int:
    return "integer";
  case TokenKind::Float:
    return "float";
  case TokenKind::String:
    return "string";
  case TokenKind::Char:
    return "character";
  case TokenKind::Let:
    return "let";
  case TokenKind::Set:
    return "set";
  case TokenKind::Func:
    return "func";
  case TokenKind::Return:
    return "return";
  case TokenKind::If:
    return "if";
  case TokenKind::Else:
    return "else";
  case TokenKind::While:
    return "while";
  case TokenKind::Loop:
    return "loop";
  case TokenKind::Break:
    return "break";
  case TokenKind::Continue:
    return "continue";
  case TokenKind::Match:
    return "match";
  case TokenKind::Try:
    return "try";
  case TokenKind::Catch:
    return "catch";
  case TokenKind::Finally:
    return "finally";
  case TokenKind::Throw:
    return "throw";
  case TokenKind::Import:
    return "import";
  case TokenKind::Export:
    return "export";
  case TokenKind::As:
    return "as";
  case TokenKind::Class:
    return "class";
  case TokenKind::Extends:
    return "extends";
  case TokenKind::Implements:
    return "implements";
  case TokenKind::Init:
    return "init";
  case TokenKind::New:
    return "new";
  case TokenKind::Self:
    return "self";
  case TokenKind::Super:
    return "super";
  case TokenKind::Public:
    return "public";
  case TokenKind::Private:
    return "private";
  case TokenKind::Protected:
    return "protected";
  case TokenKind::Static:
    return "static";
  case TokenKind::Override:
    return "override";
  case TokenKind::Final:
    return "final";
  case TokenKind::Abstract:
    return "abstract";
  case TokenKind::Intf:
    return "intf";
  case TokenKind::Trait:
    return "trait";
  case TokenKind::From:
    return "from";
  case TokenKind::Default:
    return "default";
  case TokenKind::Type:
    return "type";
  case TokenKind::True:
    return "true";
  case TokenKind::False:
    return "false";
  case TokenKind::Null:
    return "null";
  case TokenKind::IntType:
    return "int type";
  case TokenKind::FloatType:
    return "float type";
  case TokenKind::NumType:
    return "num type";
  case TokenKind::StrType:
    return "str type";
  case TokenKind::CharType:
    return "char type";
  case TokenKind::BoolType:
    return "bool type";
  case TokenKind::NullType:
    return "null type";
  case TokenKind::VoidType:
    return "void type";
  case TokenKind::AnyType:
    return "any type";
  case TokenKind::Plus:
    return "'+'";
  case TokenKind::Minus:
    return "'-'";
  case TokenKind::Star:
    return "'*'";
  case TokenKind::Slash:
    return "'/'";
  case TokenKind::Percent:
    return "'%'";
  case TokenKind::Bang:
    return "'!'";
  case TokenKind::Equal:
    return "'='";
  case TokenKind::EqualEqual:
    return "'=='";
  case TokenKind::BangEqual:
    return "'!='";
  case TokenKind::Less:
    return "'<'";
  case TokenKind::LessEqual:
    return "'<='";
  case TokenKind::Greater:
    return "'>'";
  case TokenKind::GreaterEqual:
    return "'>='";
  case TokenKind::AndAnd:
    return "'&&'";
  case TokenKind::OrOr:
    return "'||'";
  case TokenKind::Pipe:
    return "'|'";
  case TokenKind::Question:
    return "'?'";
  case TokenKind::Dot:
    return "'.'";
  case TokenKind::Comma:
    return "','";
  case TokenKind::Colon:
    return "':'";
  case TokenKind::Semicolon:
    return "';'";
  case TokenKind::LeftParen:
    return "'('";
  case TokenKind::RightParen:
    return "')'";
  case TokenKind::LeftBrace:
    return "'{'";
  case TokenKind::RightBrace:
    return "'}'";
  case TokenKind::LeftBracket:
    return "'['";
  case TokenKind::RightBracket:
    return "']'";
  case TokenKind::Arrow:
    return "'->'";
  case TokenKind::FatArrow:
    return "'=>'";
  }
  return "invalid token kind";
}
} // namespace kyna
