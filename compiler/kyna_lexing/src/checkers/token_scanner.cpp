#include "token_scanner.hpp"

#include <algorithm>
#include <utility>

namespace kyna {

TokenScanner::TokenScanner(const SourceFile &input) : source(input) {}

LexResult TokenScanner::scan() {
  while (!atEnd()) {
    tokenStart = current;
    startLine = line;
    startColumn = column;
    try {
      scanToken();
    } catch (const KynaError &error) {
      result.diagnostics.push_back(error.diagnostic);
      result.tokens.push_back({TokenKind::Invalid,
                               source.text.substr(tokenStart, current - tokenStart),
                               currentSpan()});
    }
  }
  result.tokens.push_back(
      {TokenKind::End, "", makeSpan(current, current, line, column, line, column)});
  return std::move(result);
}

bool TokenScanner::atEnd() const { return current >= source.text.size(); }

char TokenScanner::advance() {
  const char character = source.text[current++];
  if (character == '\n') {
    ++line;
    column = 1;
  } else {
    ++column;
  }
  return character;
}

char TokenScanner::peek() const { return atEnd() ? '\0' : source.text[current]; }

char TokenScanner::peekNext() const {
  return current + 1 < source.text.size() ? source.text[current + 1] : '\0';
}

bool TokenScanner::take(char expected) {
  if (peek() != expected)
    return false;
  advance();
  return true;
}

SourceSpan TokenScanner::makeSpan(std::size_t start, std::size_t end, int firstLine,
                                  int firstColumn, int lastLine, int lastColumn) const {
  return {source.id, start, end, firstLine, firstColumn, lastLine, lastColumn};
}

SourceSpan TokenScanner::currentSpan() const {
  return makeSpan(tokenStart, current, startLine, startColumn, line, column);
}

void TokenScanner::add(TokenKind kind) {
  result.tokens.push_back(
      {kind, source.text.substr(tokenStart, current - tokenStart), currentSpan()});
}

[[noreturn]] void TokenScanner::fail(std::string message, std::string code) {
  Diagnostic diagnostic{std::move(message), currentSpan(), false};
  diagnostic.code = std::move(code);
  throw KynaError(diagnostic);
}

bool LexResult::ok() const {
  return std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
}

LexResult tokenize(const SourceFile &source) { return TokenScanner(source).scan(); }

} // namespace kyna
