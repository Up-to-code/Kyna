#pragma once

#include "kyna/diagnostics/diagnostic.hpp"
#include "kyna/hir/hir_program.hpp"
#include "kyna/syntax/syntax_tree.hpp"
#include <optional>
#include <unordered_map>

namespace kyna {

struct HirLoweringResult {
  std::optional<HirProgram> program;
  std::vector<Diagnostic> diagnostics;
  [[nodiscard]] bool ok() const { return program.has_value() && diagnostics.empty(); }
};

struct HirLoweringOptions {
  std::vector<std::string> nativeFunctions;
  std::unordered_map<std::string, std::string> nativeMemberFunctions;
};

[[nodiscard]] HirLoweringResult lowerSyntaxToHir(const std::string &moduleName,
                                                 const SyntaxTree &tree,
                                                 HirLoweringOptions options = {});

} // namespace kyna
