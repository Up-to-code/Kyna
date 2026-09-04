#include "kyna/language/language_session.hpp"
#include "../support_private.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"

namespace kyna {

LanguageSession::LanguageSession(LanguageSessionOptions sessionOptions)
    : options(std::move(sessionOptions)), executor(options.capabilities, installStandardLibrary) {
  interactiveAnalyzer.setInteractive(true);
}

AnalysisResult LanguageSession::compile(const std::filesystem::path &entry,
                                        std::vector<Diagnostic> &frontEnd,
                                        std::vector<PhaseMetric> *metrics) {
  detail::PhaseTimer timer(metrics);
  auto loaded = loadModuleGraph(sources, entry, ModuleLoadOptions{options.modulePaths});
  timer.finish("load_lex_parse");
  frontEnd = loaded.diagnostics;
  if (!loaded.ok())
    return {std::nullopt, {}, {}};
  auto result = analyzeModuleGraph(std::move(loaded.graph));
  timer.finish("resolve_check");
  return result;
}

LanguageResult LanguageSession::check(const std::filesystem::path &entry) {
  std::vector<Diagnostic> diagnostics;
  std::vector<PhaseMetric> metrics;
  auto analysis = compile(entry, diagnostics, options.collectMetrics ? &metrics : nullptr);
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  return {std::move(diagnostics), false, {}, std::move(metrics)};
}

LanguageResult LanguageSession::run(const std::filesystem::path &entry) {
  std::vector<Diagnostic> diagnostics;
  std::vector<PhaseMetric> metrics;
  auto analysis = compile(entry, diagnostics, options.collectMetrics ? &metrics : nullptr);
  diagnostics.insert(diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
  if (!analysis.program || detail::hasErrors(diagnostics))
    return {std::move(diagnostics), false, {}, std::move(metrics)};
  if (!detail::requiresHostServer(entry) && analysis.program->modules.modules.size() == 1) {
    const auto module = analysis.program->modules.modules.find(analysis.program->modules.entry);
    if (module != analysis.program->modules.modules.end() && module->second.dependencies.empty()) {
      auto attempt = detail::executeBytecodeSubset(entry.string(), module->second.syntax,
                                                   options.capabilities, options.collectMetrics);
      metrics.insert(metrics.end(), attempt.metrics.begin(), attempt.metrics.end());
      if (attempt.supported) {
        diagnostics.insert(diagnostics.end(), attempt.diagnostics.begin(), attempt.diagnostics.end());
        const bool executed = !detail::hasErrors(diagnostics);
        return {std::move(diagnostics), executed, attempt.heapStats, std::move(metrics)};
      }
    }
  }
  detail::PhaseTimer timer(options.collectMetrics ? &metrics : nullptr);
  auto execution = executor.execute(*analysis.program);
  timer.finish("tree_execute");
  diagnostics.insert(diagnostics.end(), execution.diagnostics.begin(), execution.diagnostics.end());
  return {std::move(diagnostics), execution.ok(), executor.runtime().heap().stats(), std::move(metrics)};
}

} // namespace kyna
