#pragma once

#include "kyna/lexing/tokenizer.hpp"
#include <cstddef>
#include <string>

namespace kyna {

// Internal scanner state shared by the lexical scanning domains. The public
// tokenizer facade remains in tokenizer.hpp.
class TokenScanner {
public:
  explicit TokenScanner(const SourceFile &input);
  LexResult scan();

private:
  const SourceFile &source;
  LexResult result;
  std::size_t tokenStart{0};
  std::size_t current{0};
  int line{1};
  int column{1};
  int startLine{1};
  int startColumn{1};

  [[nodiscard]] bool atEnd() const;
  char advance();
  [[nodiscard]] char peek() const;
  [[nodiscard]] char peekNext() const;
  bool take(char expected);
  SourceSpan makeSpan(std::size_t start, std::size_t end, int firstLine, int firstColumn,
                      int lastLine, int lastColumn) const;
  [[nodiscard]] SourceSpan currentSpan() const;
  void add(TokenKind kind);
  [[noreturn]] void fail(std::string message, std::string code);

  void scanString();
  void scanCharacter();
  void scanNumber();
  void scanIdentifier();
  void scanBlockComment();
  void scanToken();
};

} // namespace kyna
