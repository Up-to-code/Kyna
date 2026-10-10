#include "kyna/language/language_session.hpp"
#include <kyna/language/native_lifetimes.hpp>
#include <set>
#include "../support_private.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"

namespace kyna {

LanguageSession::LanguageSession(LanguageSessionOptions sessionOptions)
    : options(std::move(sessionOptions)), executor(options.capabilities,
        [functions = options.nativeFunctions](Interpreter &interpreter) {
          installStandardLibrary(interpreter);
          installNativeFunctions(interpreter, functions);
        }) {
  interactiveAnalyzer.setExternalBindings(nativeFunctionBindings(options.nativeFunctions));
  interactiveAnalyzer.setInteractive(true);
}

LanguageSession::~LanguageSession() {
  for (const auto &function : options.nativeFunctions) if (function.shutdown) function.shutdown();
  std::set<NativeLifetime *> callbacks;
  std::set<NativeResources *> resources;
  for (const auto &function : options.nativeFunctions) {
    if (function.lifetime && callbacks.insert(function.lifetime.get()).second) function.lifetime->close();
    if (function.resources && resources.insert(function.resources.get()).second) function.resources->close();
  }
}

AnalysisResult LanguageSession::compile(const std::filesystem::path &entry,
                                        std::vector<Diagnostic> &frontEnd,
                                        std::vector<PhaseMetric> *metrics) {
  detail::PhaseTimer timer(metrics);
  auto loaded = loadModuleGraph(sources, entry, ModuleLoadOptions{options.modulePaths, options.sourceOverlays});
  timer.finish("load_lex_parse");
  frontEnd = loaded.diagnostics;
  if (!loaded.ok())
    return {std::nullopt, {}, {}};
  auto result = analyzeModuleGraph(std::move(loaded.graph), nativeFunctionBindings(options.nativeFunctions));
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
                                                   options.capabilities, options.collectMetrics, options.nativeFunctions);
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
