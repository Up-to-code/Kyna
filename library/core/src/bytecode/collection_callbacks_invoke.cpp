// Executes collection callbacks through the active VM callback interface.
#include "bytecode_private.hpp"

#include <array>
#include <vector>

namespace kyna::detail {

std::optional<NativeCallResult> collectionCallbacksBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  const bool reduce = name == "reduce";
  if (!reduce && name != "map" && name != "filter" && name != "find" &&
      name != "any" && name != "all") return std::nullopt;
  if (arguments.size() != (reduce ? 3u : 2u) ||
      !std::holds_alternative<ArrayPtr>(arguments[0].data) ||
      !ctx.callbacks || !ctx.callbacks->arity || !ctx.callbacks->invoke ||
      !ctx.callbacks->arity(arguments[1]))
    return bytecodeFailure(reduce ? "KCOL1002" : "KCOL1001",
                           std::string(name) + " expects an array and function" +
                               (reduce ? " and initial value" : ""));
  auto *input = std::get<ArrayPtr>(arguments[0].data);
  const auto arity = *ctx.callbacks->arity(arguments[1]);
  RuntimeValue output;
  RuntimeValue accumulated = reduce ? arguments[2] : RuntimeValue();
  auto roots = ctx.heap.rootScope();
  roots.protect(output);
  roots.protect(accumulated);
  if (name == "map" || name == "filter") output = RuntimeValue(ctx.heap.allocateArray());
  for (std::size_t index = 0; index < input->elements.size(); ++index) {
    std::vector<RuntimeValue> values;
    if (reduce) values.push_back(accumulated);
    values.push_back(input->elements[index]);
    if (arity > values.size()) values.emplace_back(static_cast<std::int64_t>(index));
    auto elementRoots = ctx.heap.rootScope();
    for (const auto &value : values) elementRoots.protect(value);
    auto result = ctx.callbacks->invoke(arguments[1], values);
    if (result.failure) return result;
    if (reduce) accumulated = std::move(result.value);
    else if (name == "map")
      std::get<ArrayPtr>(output.data)->elements.push_back(std::move(result.value));
    else if (name == "any" && result.value.isTruthy())
      return NativeCallResult{RuntimeValue(true), std::nullopt};
    else if (name == "all" && !result.value.isTruthy())
      return NativeCallResult{RuntimeValue(false), std::nullopt};
    else if ((name == "filter" || name == "find") && result.value.isTruthy()) {
      if (index >= input->elements.size())
        return bytecodeFailure("KRT2104", "callback removed the current collection element");
      if (name == "find") return NativeCallResult{input->elements[index], std::nullopt};
      std::get<ArrayPtr>(output.data)->elements.push_back(input->elements[index]);
    }
  }
  if (reduce) return NativeCallResult{accumulated, std::nullopt};
  if (name == "all" || name == "any")
    return NativeCallResult{RuntimeValue(name == "all"), std::nullopt};
  return NativeCallResult{output, std::nullopt};
}

} // namespace kyna::detail
