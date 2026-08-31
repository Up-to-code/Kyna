#pragma once

#include "kyna/source/source_span.hpp"
#include <string>

namespace kyna {

enum class TokenKind {
  End,
  Invalid,
  Identifier,
  Int,
  Float,
  String,
  Char,
  Let,
  Set,
  Func,
  Return,
  If,
  Else,
  While,
  Loop,
  Break,
  Continue,
  Match,
  Try,
  Catch,
  Finally,
  Throw,
  Import,
  Export,
  As,
  Class,
  Extends,
  Implements,
  Init,
  New,
  Self,
  Super,
  Public,
  Private,
  Protected,
  Static,
  Override,
  Final,
  Abstract,
  Intf,
  Trait,
  From,
  Default,
  Type,
  True,
  False,
  Null,
  IntType,
  FloatType,
  NumType,
  StrType,
  CharType,
  BoolType,
  NullType,
  VoidType,
  AnyType,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Bang,
  Equal,
  EqualEqual,
  BangEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  AndAnd,
  OrOr,
  Pipe,
  Question,
  Dot,
  Comma,
  Colon,
  Semicolon,
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  LeftBracket,
  RightBracket,
  Arrow,
  FatArrow
};

struct Token {
  TokenKind kind{TokenKind::Invalid};
  std::string lexeme;
  SourceSpan location;
};

std::string tokenName(TokenKind kind);

} // namespace kyna
