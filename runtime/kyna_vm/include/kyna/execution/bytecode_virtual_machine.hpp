#pragma once

#include "kyna/bytecode/bytecode_module.hpp"
#include "kyna/diagnostics/diagnostic.hpp"
#include "kyna/execution/runtime_value.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include <utility>
#include <vector>
#include <optional>
#include <span>
#include <string_view>
#include <functional>

namespace kyna {

struct BytecodeExecutionResult {
  RuntimeValue value;
  std::vector<Diagnostic> diagnostics;
  HeapStats heapStats;
  BytecodeExecutionResult(RuntimeValue result = {},
                          std::vector<Diagnostic> failures = {},
                          HeapStats statistics = {})
      : value(std::move(result)), diagnostics(std::move(failures)), heapStats(statistics) {}
  [[nodiscard]] bool ok() const { return diagnostics.empty(); }
};

struct NativeCallFailure {
  std::string code;
  std::string message;
  RuntimeValue cause;
  std::optional<Diagnostic> diagnostic{};
};

struct NativeCallResult {
  RuntimeValue value;
  std::optional<NativeCallFailure> failure;
};

// NativeCallbacks invokes language functions on the active VM and heap. The
// interface is valid only during invokeWithCallbacks; adapters must not retain it.
struct NativeCallbacks {
  std::function<void()> collect;
  std::function<std::optional<std::size_t>(const RuntimeValue &)> arity;
  std::function<NativeCallResult(const RuntimeValue &, std::span<const RuntimeValue>)> invoke;
};

class BytecodeNativeAdapter {
public:
  virtual ~BytecodeNativeAdapter() = default;
  [[nodiscard]] virtual NativeCallResult invoke(std::string_view name,
                                                std::span<const RuntimeValue> arguments,
                                                Heap &heap) = 0;
  virtual NativeCallResult invokeWithCallbacks(std::string_view name,
      std::span<const RuntimeValue> arguments, Heap &heap, const NativeCallbacks &) {
    return invoke(name, arguments, heap);
  }
};

class BytecodeVirtualMachine {
public:
  [[nodiscard]] BytecodeExecutionResult
  execute(const BytecodeModule &module, BytecodeNativeAdapter *nativeAdapter = nullptr) const;
};

} // namespace kyna
