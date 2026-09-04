#include "bytecode_private.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> collectionsBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "push") {
    if (arguments.size() != 2 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1004", "push expects an array and a value");
    std::get<ArrayPtr>(arguments[0].data)->elements.push_back(arguments[1]);
    return NativeCallResult{};
  }
  if (name == "pop") {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1005", "pop expects exactly one array");
    auto *array = std::get<ArrayPtr>(arguments[0].data);
    if (array->elements.empty())
      return NativeCallResult{};
    auto value = array->elements.back();
    array->elements.pop_back();
    return NativeCallResult{std::move(value), std::nullopt};
  }
  if (name == "keys") {
    if (arguments.size() != 1 || !std::holds_alternative<ObjectPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1006", "keys expects exactly one object");
    auto *result = ctx.heap.allocateArray();
    for (const auto &[key, value] : std::get<ObjectPtr>(arguments[0].data)->fields)
      result->elements.emplace_back(key);
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "unique") {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1003", "unique expects exactly one array");
    auto *result = ctx.heap.allocateArray();
    for (const auto &candidate : std::get<ArrayPtr>(arguments[0].data)->elements) {
      const auto duplicate =
          std::any_of(result->elements.begin(), result->elements.end(),
                      [&](const RuntimeValue &accepted) { return accepted.equals(candidate); });
      if (!duplicate)
        result->elements.push_back(candidate);
    }
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "sort" || name == "bubbleSort") {
    if (arguments.empty() || arguments.size() > 2 || !std::holds_alternative<ArrayPtr>(arguments[0].data) ||
        (arguments.size() == 2 && (!ctx.callbacks || !ctx.callbacks->arity ||
                                   !ctx.callbacks->arity(arguments[1]))))
      return bytecodeFailure("KCOL1007", std::string(name) +
                                             " currently expects one array in bytecode execution");
    auto *result = ctx.heap.allocateArray();
    result->elements = std::get<ArrayPtr>(arguments[0].data)->elements;
    RuntimeValue protectedResult(result);
    auto roots = ctx.heap.rootScope();
    roots.protect(protectedResult);
    // Sorting moves values through native temporary storage, outside VM roots.
    const auto originalElements = result->elements;
    for (const auto &element : originalElements) roots.protect(element);
    const auto less = [&](const RuntimeValue &left, const RuntimeValue &right) {
      if (arguments.size() == 2) {
        // Preserve the established should-swap comparator convention.
        const std::vector<RuntimeValue> values{right, left};
        auto outcome = ctx.callbacks->invoke(arguments[1], values);
        if (outcome.failure) throw *outcome.failure;
        return outcome.value.isTruthy();
      }
      if (const auto leftInteger = std::get_if<std::int64_t>(&left.data)) {
        if (const auto rightInteger = std::get_if<std::int64_t>(&right.data))
          return *leftInteger < *rightInteger;
        if (const auto rightFloat = std::get_if<double>(&right.data))
          return static_cast<double>(*leftInteger) < *rightFloat;
      }
      if (const auto leftFloat = std::get_if<double>(&left.data)) {
        if (const auto rightInteger = std::get_if<std::int64_t>(&right.data))
          return *leftFloat < static_cast<double>(*rightInteger);
        if (const auto rightFloat = std::get_if<double>(&right.data))
          return *leftFloat < *rightFloat;
      }
      if (const auto leftString = std::get_if<std::string>(&left.data))
        if (const auto rightString = std::get_if<std::string>(&right.data))
          return *leftString < *rightString;
      throw NativeCallFailure{"KCOL1007", "sort supports only numbers or strings", {}};
    };
    try {
      std::stable_sort(result->elements.begin(), result->elements.end(), less);
    } catch (const NativeCallFailure &failure) {
      return NativeCallResult{{}, failure};
    }
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "createQueue") {
    if (!arguments.empty())
      return bytecodeFailure("KCOL1020", "createQueue expects no arguments");
    return NativeCallResult{RuntimeValue(ctx.heap.allocateArray()), std::nullopt};
  }
  if (name == "enqueue") {
    if (arguments.size() != 2 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1021", "enqueue expects a queue and a value");
    std::get<ArrayPtr>(arguments[0].data)->elements.push_back(arguments[1]);
    return NativeCallResult{};
  }
  if (name == "dequeue") {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1022", "dequeue expects a queue");
    auto *queue = std::get<ArrayPtr>(arguments[0].data);
    if (queue->elements.empty())
      return NativeCallResult{};
    auto value = queue->elements.front();
    queue->elements.erase(queue->elements.begin());
    return NativeCallResult{std::move(value), std::nullopt};
  }
  if (name == "peekQueue") {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1023", "peekQueue expects a queue");
    auto *queue = std::get<ArrayPtr>(arguments[0].data);
    if (queue->elements.empty())
      return NativeCallResult{};
    return NativeCallResult{queue->elements.front(), std::nullopt};
  }
  if (name == "queueIsEmpty") {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1024", "queueIsEmpty expects a queue");
    return NativeCallResult{
        RuntimeValue(std::get<ArrayPtr>(arguments[0].data)->elements.empty()), std::nullopt};
  }

  if (name == "push" || name == "pop" || name == "len" || name == "size" ||
      name == "length" || name == "first" || name == "last" || name == "get" ||
      name == "setAt" || name == "contains" || name == "includes" || name == "indexOf" ||
      name == "clear" || name == "reverse" || name == "join" || name == "slice" ||
      name == "isEmpty" || name == "concat") {
    if (arguments.empty() || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      return bytecodeFailure("KCOL1008", "array method '" + std::string(name) +
                                             "' expects an array receiver");
    auto *array = std::get<ArrayPtr>(arguments[0].data);
    auto count = [&](std::size_t expected) {
      if (arguments.size() - 1 != expected)
        return bytecodeFailure("KCOL1009",
                               "array method '" + std::string(name) + "' expects " +
                                   std::to_string(expected) + " argument(s), but " +
                                   std::to_string(arguments.size() - 1) + " were provided");
      return NativeCallResult{};
    };

    if (name == "push") {
      if (auto e = count(1); e.failure)
        return e;
      array->elements.push_back(arguments[1]);
      return NativeCallResult{RuntimeValue(array), std::nullopt};
    }
    if (name == "pop") {
      if (auto e = count(0); e.failure)
        return e;
      if (array->elements.empty())
        return NativeCallResult{};
      auto value = array->elements.back();
      array->elements.pop_back();
      return NativeCallResult{std::move(value), std::nullopt};
    }
    if (name == "len" || name == "size" || name == "length" || name == "isEmpty") {
      if (auto e = count(0); e.failure)
        return e;
      if (name == "isEmpty")
        return NativeCallResult{RuntimeValue(array->elements.empty()), std::nullopt};
      return NativeCallResult{RuntimeValue(static_cast<std::int64_t>(array->elements.size())),
                              std::nullopt};
    }
    if (name == "first" || name == "last") {
      if (auto e = count(0); e.failure)
        return e;
      if (array->elements.empty())
        return NativeCallResult{};
      return name == "first" ? NativeCallResult{RuntimeValue(array->elements.front()),
                                                std::nullopt}
                             : NativeCallResult{RuntimeValue(array->elements.back()),
                                                std::nullopt};
    }
    if (name == "get") {
      if (auto e = count(1); e.failure)
        return e;
      if (!std::holds_alternative<std::int64_t>(arguments[1].data))
        return bytecodeFailure("KCOL1010", "get expects an integer index", arguments[1]);
      auto position = std::get<std::int64_t>(arguments[1].data);
      if (position < 0 || static_cast<std::size_t>(position) >= array->elements.size())
        return NativeCallResult{};
      return NativeCallResult{array->elements[static_cast<std::size_t>(position)], std::nullopt};
    }
    if (name == "setAt") {
      if (auto e = count(2); e.failure)
        return e;
      if (!std::holds_alternative<std::int64_t>(arguments[1].data))
        return bytecodeFailure("KCOL1011", "setAt expects an integer index", arguments[1]);
      auto position = std::get<std::int64_t>(arguments[1].data);
      if (position < 0 || static_cast<std::size_t>(position) >= array->elements.size())
        return bytecodeFailure("KRT2104",
                               "array index " + std::to_string(position) + " out of bounds");
      array->elements[static_cast<std::size_t>(position)] = arguments[2];
      return NativeCallResult{RuntimeValue(array), std::nullopt};
    }
    if (name == "contains" || name == "includes" || name == "indexOf") {
      if (auto e = count(1); e.failure)
        return e;
      for (std::size_t i = 0; i < array->elements.size(); ++i)
        if (array->elements[i].equals(arguments[1]))
          return name == "indexOf"
                     ? NativeCallResult{RuntimeValue(static_cast<std::int64_t>(i)), std::nullopt}
                     : NativeCallResult{RuntimeValue(true), std::nullopt};
      return name == "indexOf"
                 ? NativeCallResult{RuntimeValue(std::int64_t{-1}), std::nullopt}
                 : NativeCallResult{RuntimeValue(false), std::nullopt};
    }
    if (name == "clear") {
      if (auto e = count(0); e.failure)
        return e;
      array->elements.clear();
      return NativeCallResult{RuntimeValue(array), std::nullopt};
    }
    if (name == "reverse") {
      if (auto e = count(0); e.failure)
        return e;
      std::reverse(array->elements.begin(), array->elements.end());
      return NativeCallResult{RuntimeValue(array), std::nullopt};
    }
    if (name == "join") {
      if (arguments.size() > 2)
        return bytecodeFailure("KCOL1009", "join expects at most one separator");
      if (arguments.size() == 2 && !std::holds_alternative<std::string>(arguments[1].data))
        return bytecodeFailure("KCOL1012", "join expects a string separator", arguments[1]);
      const std::string separator =
          arguments.size() == 2 ? std::get<std::string>(arguments[1].data) : "";
      std::string result;
      for (std::size_t i = 0; i < array->elements.size(); ++i) {
        if (i)
          result += separator;
        result += array->elements[i].display();
      }
      return NativeCallResult{RuntimeValue(result), std::nullopt};
    }
    if (name == "slice") {
      std::size_t start = 0;
      std::size_t end = array->elements.size();
      if (arguments.size() >= 2) {
        if (!std::holds_alternative<std::int64_t>(arguments[1].data))
          return bytecodeFailure("KCOL1013", "slice expects integer bounds", arguments[1]);
        auto s = std::get<std::int64_t>(arguments[1].data);
        const auto size = static_cast<std::int64_t>(array->elements.size());
        start = s < 0 ? (s + size < 0 ? 0 : static_cast<std::size_t>(s + size))
                      : static_cast<std::size_t>(s);
      }
      if (arguments.size() >= 3) {
        if (!std::holds_alternative<std::int64_t>(arguments[2].data))
          return bytecodeFailure("KCOL1013", "slice expects integer bounds", arguments[2]);
        auto e = std::get<std::int64_t>(arguments[2].data);
        const auto size = static_cast<std::int64_t>(array->elements.size());
        end = e < 0 ? (e + size < 0 ? 0 : static_cast<std::size_t>(e + size))
                    : static_cast<std::size_t>(e);
      }
      auto *result = ctx.heap.allocateArray();
      for (std::size_t i = start; i < end && i < array->elements.size(); ++i)
        result->elements.push_back(array->elements[i]);
      return NativeCallResult{RuntimeValue(result), std::nullopt};
    }
    if (name == "concat") {
      auto *result = ctx.heap.allocateArray();
      result->elements = array->elements;
      for (std::size_t i = 1; i < arguments.size(); ++i)
        if (const auto other = std::get_if<ArrayPtr>(&arguments[i].data))
          for (const auto &element : (*other)->elements)
            result->elements.push_back(element);
      return NativeCallResult{RuntimeValue(result), std::nullopt};
    }
  }
  if (name == "filter" || name == "map" || name == "reduce" || name == "find" ||
      name == "some" || name == "every") {
    return bytecodeFailure("KCOL1014",
                           "array method '" + std::string(name) +
                               "' with a callback is not yet supported by the bytecode runtime");
  }
  return std::nullopt;
}

} // namespace kyna::detail
