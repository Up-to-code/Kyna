#include "token_scanner.hpp"

#include <cctype>
#include <string>

namespace kyna {

void TokenScanner::scanToken() {
  const char character = advance();
  if (std::isspace(static_cast<unsigned char>(character)))
    return;
  if (character == '#') {
    while (!atEnd() && peek() != '\n')
      advance();
    return;
  }
  if (character == '/' && peek() == '/') {
    while (!atEnd() && peek() != '\n')
      advance();
    return;
  }
  if (character == '/' && peek() == '*') {
    scanBlockComment();
    return;
  }
  switch (character) {
  case '(':
    add(TokenKind::LeftParen);
    return;
  case ')':
    add(TokenKind::RightParen);
    return;
  case '{':
    add(TokenKind::LeftBrace);
    return;
  case '}':
    add(TokenKind::RightBrace);
    return;
  case '[':
    add(TokenKind::LeftBracket);
    return;
  case ']':
    add(TokenKind::RightBracket);
    return;
  case ',':
    add(TokenKind::Comma);
    return;
  case ':':
    add(TokenKind::Colon);
    return;
  case ';':
    add(TokenKind::Semicolon);
    return;
  case '.':
    add(TokenKind::Dot);
    return;
  case '+':
    add(TokenKind::Plus);
    return;
  case '*':
    add(TokenKind::Star);
    return;
  case '%':
    add(TokenKind::Percent);
    return;
  case '-':
    add(take('>') ? TokenKind::Arrow : TokenKind::Minus);
    return;
  case '=':
    add(take('=') ? TokenKind::EqualEqual : (take('>') ? TokenKind::FatArrow : TokenKind::Equal));
    return;
  case '!':
    add(take('=') ? TokenKind::BangEqual : TokenKind::Bang);
    return;
  case '<':
    add(take('=') ? TokenKind::LessEqual : TokenKind::Less);
    return;
  case '>':
    add(take('=') ? TokenKind::GreaterEqual : TokenKind::Greater);
    return;
  case '&':
    if (!take('&'))
      fail("expected '&' after '&'", "K1006");
    add(TokenKind::AndAnd);
    return;
  case '|':
    add(take('|') ? TokenKind::OrOr : TokenKind::Pipe);
    return;
  case '?':
    add(TokenKind::Question);
    return;
  case '/':
    add(TokenKind::Slash);
    return;
  case '"':
    scanString();
    return;
  case '\'':
    scanCharacter();
    return;
  default:
    if (std::isdigit(static_cast<unsigned char>(character)))
      scanNumber();
    else if (std::isalpha(static_cast<unsigned char>(character)) || character == '_')
      scanIdentifier();
    else
      fail("unexpected character: " + std::string(1, character), "K1000");
  }
}

} // namespace kyna
