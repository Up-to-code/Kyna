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
                                      RuntimeCapabilities capabilities, bool collectMetrics,
                                      const std::vector<NativeFunction> &nativeFunctions) {
  std::vector<PhaseMetric> metrics;
  PhaseTimer timer(collectMetrics ? &metrics : nullptr);
  auto loweringOptions = standardLibraryHirOptions();
  for (const auto &function : nativeFunctions) loweringOptions.nativeFunctions.push_back(function.name);
  auto hir = lowerSyntaxToHir(name, tree, std::move(loweringOptions));
  timer.finish("hir");
  if (!hir.program) {
    const bool onlyUnsupported =
        !hir.diagnostics.empty() &&
        std::all_of(hir.diagnostics.begin(), hir.diagnostics.end(),
                    [](const Diagnostic &diagnostic) { return diagnostic.code == "KHIR1201"; });
    return {!onlyUnsupported, onlyUnsupported ? std::vector<Diagnostic>{}
                                             : std::move(hir.diagnostics), {}, std::move(metrics)};
  }
  auto mir = lowerHirToMir(*hir.program);
  timer.finish("mir");
  if (!mir.program)
    return {true, std::move(mir.diagnostics), {}, std::move(metrics)};
  auto bytecode = compileMirToBytecode(*mir.program);
  timer.finish("bytecode");
  if (!bytecode.module)
    return {true, std::move(bytecode.diagnostics), {}, std::move(metrics)};
  auto nativeLibrary = createBytecodeStandardLibrary(std::move(capabilities), std::cout);
  timer.finish("native_setup");
  class Adapter final : public BytecodeNativeAdapter {
  public:
    Adapter(BytecodeNativeAdapter &standard, const std::vector<NativeFunction> &functions)
        : standard(standard), functions(functions) {}
    NativeCallResult invoke(std::string_view name, std::span<const Value> arguments, Heap &heap) override {
      return invokeWithCallbacks(name, arguments, heap, {});
    }
    NativeCallResult invokeWithCallbacks(std::string_view name, std::span<const Value> arguments,
                                         Heap &heap, const NativeCallbacks &callbacks) override {
      for (const auto &function : functions)
        if (function.name == name) return invokeNativeFunction(function, arguments, heap, callbacks);
      return standard.invokeWithCallbacks(name, arguments, heap, callbacks);
    }
  private:
    BytecodeNativeAdapter &standard;
    const std::vector<NativeFunction> &functions;
  } adapter(*nativeLibrary, nativeFunctions);
  auto execution = BytecodeVirtualMachine().execute(*bytecode.module, &adapter);
  timer.finish("vm_execute");
  return {true, std::move(execution.diagnostics), execution.heapStats, std::move(metrics)};
}

} // namespace kyna::detail
