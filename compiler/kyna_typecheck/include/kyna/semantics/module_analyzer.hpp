#pragma once

#include "kyna/diagnostics/diagnostic.hpp"
#include "kyna/modules/module_graph.hpp"
#include "kyna/semantics/checked_program.hpp"
#include <optional>

namespace kyna {

struct AnalysisResult {
  std::optional<CheckedProgram> program;
  std::vector<Diagnostic> diagnostics;
  // Dependency modules whose bodies were skipped because a matching `.kyc`
  // stamp was present. The entry module is never listed.
  std::vector<std::filesystem::path> cachedModules;
  [[nodiscard]] bool ok() const;
};

AnalysisResult analyzeModuleGraph(ParsedModuleGraph graph);

} // namespace kyna
