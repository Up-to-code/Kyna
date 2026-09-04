#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"
#include "kyna/modules/module_loader.hpp"
#include "kyna/parsing/module_parser.hpp"
#include "kyna/lexing/tokenizer.hpp"
#include "kyna/semantics/program_analyzer.hpp"

namespace kyna {

LanguageResult LanguageSession::checkSource(std::string name, std::string source) {
  const auto sourceId = sources.add(name, std::move(source));
  const auto &file = *sources.find(sourceId);
  auto lexed = tokenize(file);
  auto parsed = parseModule(file, std::move(lexed.tokens));
  std::vector<Diagnostic> diagnostics = std::move(lexed.diagnostics);
  diagnostics.insert(diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end());
  if (detail::hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}};
  ParsedModuleGraph graph;
  graph.entry = name;
  graph.initializationOrder.push_back(name);
  graph.modules.emplace(name, ModuleRecord{std::move(parsed.tree), {}, false, {}});
  auto analysis = analyzeModuleGraph(std::move(graph));
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}};
}

LanguageResult LanguageSession::checkSourceAtPath(const std::filesystem::path &entry,
                                                  std::string source) {
  auto loaded = loadModuleGraphWithEntrySource(sources, entry, std::move(source),
                                               ModuleLoadOptions{options.modulePaths});
  auto diagnostics = std::move(loaded.diagnostics);
  if (!loaded.ok())
    return {std::move(diagnostics), false, {}};
  auto analysis = analyzeModuleGraph(std::move(loaded.graph));
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}};
}

} // namespace kyna
