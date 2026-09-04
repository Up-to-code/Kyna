#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"
#include "kyna/modules/module_loader.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/semantics/program_analyzer.hpp"

namespace kyna {

LanguageResult LanguageSession::checkSource(std::string name, std::string source) {
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
  ParsedModuleGraph graph;
  graph.entry = name;
  graph.initializationOrder.push_back(name);
  graph.modules.emplace(name, ModuleRecord{std::move(parsed.tree), {}, false, {}});
  auto analysis = analyzeModuleGraph(std::move(graph));
  timer.finish("resolve_check");
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}, std::move(metrics)};
}

LanguageResult LanguageSession::checkSourceAtPath(const std::filesystem::path &entry,
                                                  std::string source) {
  std::vector<PhaseMetric> metrics;
  detail::PhaseTimer timer(options.collectMetrics ? &metrics : nullptr);
  auto loaded = loadModuleGraphWithEntrySource(sources, entry, std::move(source),
                                               ModuleLoadOptions{options.modulePaths});
  timer.finish("load_lex_parse");
  auto diagnostics = std::move(loaded.diagnostics);
  if (!loaded.ok())
    return {std::move(diagnostics), false, {}, std::move(metrics)};
  auto analysis = analyzeModuleGraph(std::move(loaded.graph));
  timer.finish("resolve_check");
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}, std::move(metrics)};
}

} // namespace kyna
