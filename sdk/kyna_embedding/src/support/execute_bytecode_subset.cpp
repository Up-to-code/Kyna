#include "../support_private.hpp"

#include "kyna/bytecode/program_bytecode_compiler.hpp"
#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/mir/hir_lowering.hpp"
#include "kyna/stdlib/bytecode_standard_library.hpp"
#include <algorithm>
#include <iostream>

namespace kyna::detail {

BytecodeAttempt executeBytecodeSubset(const std::string &name, const SyntaxTree &tree,
                                      RuntimeCapabilities capabilities) {
  auto hir = lowerSyntaxToHir(name, tree, standardLibraryHirOptions());
  if (!hir.program) {
    const bool onlyUnsupported =
        !hir.diagnostics.empty() &&
        std::all_of(hir.diagnostics.begin(), hir.diagnostics.end(),
                    [](const Diagnostic &diagnostic) { return diagnostic.code == "KHIR1201"; });
    return {!onlyUnsupported, onlyUnsupported ? std::vector<Diagnostic>{}
                                             : std::move(hir.diagnostics), {}};
  }
  auto mir = lowerHirToMir(*hir.program);
  if (!mir.program)
    return {true, std::move(mir.diagnostics), {}};
  auto bytecode = compileMirToBytecode(*mir.program);
  if (!bytecode.module)
    return {true, std::move(bytecode.diagnostics), {}};
  auto nativeLibrary = createBytecodeStandardLibrary(std::move(capabilities), std::cout);
  auto execution = BytecodeVirtualMachine().execute(*bytecode.module, nativeLibrary.get());
  const bool predicateFallback = std::any_of(
      execution.diagnostics.begin(), execution.diagnostics.end(), [](const Diagnostic &diagnostic) {
        return diagnostic.code == "KCOL1014";
      });
  if (predicateFallback)
    return {false, {}, execution.heapStats};
  return {true, std::move(execution.diagnostics), execution.heapStats};
}

} // namespace kyna::detail
