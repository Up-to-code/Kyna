#pragma once

#include "kyna/execution/runtime_capabilities.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include "kyna/source/source_manager.hpp"
#include "kyna/syntax/syntax_tree.hpp"
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
};

HirLoweringOptions standardLibraryHirOptions();
BytecodeAttempt executeBytecodeSubset(const std::string &name, const SyntaxTree &tree,
                                      RuntimeCapabilities capabilities);
std::string escapeJson(std::string_view value);
std::string statementKind(const Stmt &statement);
std::string statementName(const Stmt &statement);
bool statementExported(const Stmt &statement);

} // namespace kyna::detail
