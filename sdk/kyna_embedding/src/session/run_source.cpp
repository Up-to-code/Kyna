#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/semantics/program_analyzer.hpp"

namespace kyna {

LanguageResult LanguageSession::runSource(std::string name, std::string source, bool interactive) {
  std::vector<PhaseMetric> metrics;
  detail::PhaseTimer timer(options.collectMetrics ? &metrics : nullptr);
  const auto sourceId = sources.add(name, std::move(source));
  const auto &file = *sources.find(sourceId);
  auto lexed = tokenize(file);
  timer.finish("lex");
  auto parsed = parseModule(file, std::move(lexed.tokens));
  timer.finish("parse");
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (detail::hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}, std::move(metrics)};
  Analyzer analyzer;
  auto semantic = interactive ? interactiveAnalyzer.analyze(parsed.tree.module.declarations)
                              : analyzer.analyze(parsed.tree.module.declarations);
  timer.finish("check");
  diagnostics.insert(diagnostics.end(), semantic.begin(), semantic.end());
  if (detail::hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}, std::move(metrics)};
  // Interactive submissions must share one runtime environment. The bytecode
  // fast path creates a fresh VM for each source unit, which is correct for a
  // standalone program but would discard declarations and values in a REPL.
  if (!interactive) {
    auto attempt = detail::executeBytecodeSubset(name, parsed.tree, options.capabilities,
                                                 options.collectMetrics);
    metrics.insert(metrics.end(), attempt.metrics.begin(), attempt.metrics.end());
    if (attempt.supported) {
      diagnostics.insert(diagnostics.end(), attempt.diagnostics.begin(), attempt.diagnostics.end());
      const bool executed = !detail::hasErrors(diagnostics);
      return {std::move(diagnostics), executed, attempt.heapStats, std::move(metrics)};
    }
  }
  detail::PhaseTimer executionTimer(options.collectMetrics ? &metrics : nullptr);
  try {
    executor.runtime().execute(parsed.tree.module.declarations);
    executionTimer.finish("tree_execute");
    return {std::move(diagnostics), true, executor.runtime().heap().stats(), std::move(metrics)};
  } catch (const KynaError &error) {
    executionTimer.finish("tree_execute");
    diagnostics.push_back(error.diagnostic);
    return {std::move(diagnostics), false, executor.runtime().heap().stats(), std::move(metrics)};
  }
}

} // namespace kyna
