#include "kyna/lexing/legacy_lexer.hpp"
#include "kyna/lexing/tokenizer.hpp"

namespace kyna {
std::vector<Token> lex(const std::string &source) {
  auto result = tokenize(SourceFile{UnknownSource, {}, source});
  if (!result.diagnostics.empty())
    throw KynaError(result.diagnostics.front());
  return std::move(result.tokens);
}
} // namespace kyna
