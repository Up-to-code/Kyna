#include "token_scanner.hpp"

#include "kyna/lexing/keyword_catalog.hpp"
#include <cctype>

namespace kyna {

void TokenScanner::scanString() {
  while (!atEnd() && peek() != '"') {
    if (peek() == '\n')
      fail("newline in string literal", "K1002");
    if (peek() == '\\' && peekNext() != '\0')
      advance();
    advance();
  }
  if (atEnd())
    fail("unterminated string literal", "K1001");
  advance();
  add(TokenKind::String);
}

void TokenScanner::scanCharacter() {
  if (atEnd() || peek() == '\n')
    fail("unterminated character literal", "K1003");
  if (peek() == '\\')
    advance();
  if (atEnd())
    fail("unterminated character literal", "K1003");
  advance();
  if (!take('\''))
    fail("character literal must contain exactly one character", "K1004");
  add(TokenKind::Char);
}

void TokenScanner::scanNumber() {
  while (std::isdigit(static_cast<unsigned char>(peek())))
    advance();
  bool floating = false;
  if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
    floating = true;
    advance();
    while (std::isdigit(static_cast<unsigned char>(peek())))
      advance();
  }
  add(floating ? TokenKind::Float : TokenKind::Int);
}

void TokenScanner::scanIdentifier() {
  while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')
    advance();
  add(keywordKind(source.text.substr(tokenStart, current - tokenStart)));
}

void TokenScanner::scanBlockComment() {
  advance();
  while (!(peek() == '*' && peekNext() == '/')) {
    if (atEnd())
      fail("unterminated block comment", "K1005");
    advance();
  }
  advance();
  advance();
}

} // namespace kyna
