#pragma once

#include "kyna/execution/runtime_capabilities.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include "kyna/source/source_manager.hpp"
#include "kyna/syntax/syntax_tree.hpp"
#include "kyna/language/language_session.hpp"
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace kyna::detail {

bool hasErrors(const std::vector<Diagnostic> &diagnostics);
bool requiresHostServer(const std::filesystem::path &entry);

struct BytecodeAttempt {
  bool supported{false};
  std::vector<Diagnostic> diagnostics;
  HeapStats heapStats;
  std::vector<PhaseMetric> metrics;
};

// PhaseTimer reads the clock only when metrics were requested.
class PhaseTimer {
public:
  explicit PhaseTimer(std::vector<PhaseMetric> *target) : target(target) {
    if (target) start = std::chrono::steady_clock::now();
  }
  void finish(const char *phase) {
    if (!target) return;
    const auto end = std::chrono::steady_clock::now();
    target->push_back({phase, static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count())});
    start = end;
  }
private:
  std::vector<PhaseMetric> *target;
  std::chrono::steady_clock::time_point start;
};

HirLoweringOptions standardLibraryHirOptions();
BytecodeAttempt executeBytecodeSubset(const std::string &name, const SyntaxTree &tree,
                                      RuntimeCapabilities capabilities, bool collectMetrics = false);
std::string escapeJson(std::string_view value);
std::string statementKind(const Stmt &statement);
std::string statementName(const Stmt &statement);
bool statementExported(const Stmt &statement);

} // namespace kyna::detail
