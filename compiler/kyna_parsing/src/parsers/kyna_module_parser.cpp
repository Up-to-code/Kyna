#include "kyna/parsing/recursive_descent_parser.hpp"
#include "kyna/parsing/module_parser.hpp"
#include <algorithm>

namespace kyna {

bool ParseResult::ok() const {
  return std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
}

ParseResult parseModule(const SourceFile &source, std::vector<Token> tokens) {
  return Parser(std::move(tokens)).parseRecovering(source);
}

} // namespace kyna
