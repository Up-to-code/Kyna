#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/semantics/program_analyzer.hpp"

namespace kyna {

LanguageResult LanguageSession::runSource(std::string name, std::string source, bool interactive) {
  const auto sourceId = sources.add(name, std::move(source));
  const auto &file = *sources.find(sourceId);
  auto lexed = tokenize(file);
  auto parsed = parseModule(file, std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (detail::hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  Analyzer analyzer;
  auto semantic = interactive ? interactiveAnalyzer.analyze(parsed.tree.module.declarations)
                              : analyzer.analyze(parsed.tree.module.declarations);
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (detail::hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  // Interactive submissions must share one runtime environment. The bytecode
  // fast path creates a fresh VM for each source unit, which is correct for a
  // standalone program but would discard declarations and values in a REPL.
  if (!interactive) {
    auto attempt = detail::executeBytecodeSubset(name, parsed.tree, options.capabilities);
    if (attempt.supported) {
      diagnostics.insert(diagnostics.end(), attempt.diagnostics.begin(), attempt.diagnostics.end());
      const bool executed = !detail::hasErrors(diagnostics);
      return {std::move(diagnostics), executed, attempt.heapStats};
    }
  }
  try {
    executor.runtime().execute(parsed.tree.module.declarations);
    return {std::move(diagnostics), true, executor.runtime().heap().stats()};
  } catch (const KynaError &error) {
    diagnostics.push_back(error.diagnostic);
    return {std::move(diagnostics), false, executor.runtime().heap().stats()};
  }
}

} // namespace kyna
