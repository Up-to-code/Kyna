#include "bytecode_private.hpp"
#include <cstdint>
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> textBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "len") {
    if (arguments.size() != 1)
      return bytecodeFailure("KSTD2002", "len expects exactly one argument");
    const auto &value = arguments.front();
    if (const auto text = std::get_if<std::string>(&value.data)) {
      auto length = unicodeLength(*text);
      return length ? NativeCallResult{RuntimeValue(*length), std::nullopt}
                    : bytecodeTextFailure(length.error(), value);
    }
    if (const auto array = std::get_if<ArrayPtr>(&value.data); array && *array)
      return NativeCallResult{RuntimeValue(static_cast<std::int64_t>((*array)->elements.size())),
                              std::nullopt};
    if (const auto object = std::get_if<ObjectPtr>(&value.data); object && *object)
      return NativeCallResult{RuntimeValue(static_cast<std::int64_t>((*object)->fields.size())),
                              std::nullopt};
    return bytecodeFailure("KSTD2003", "len requires a string, array, or object", value);
  }
  if (name == "textContains") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      return bytecodeFailure("KTEXT2010", "textContains expects a string and a string needle");
    auto found = unicodeFind(std::get<std::string>(arguments[0].data),
                             std::get<std::string>(arguments[1].data));
    return found ? NativeCallResult{RuntimeValue(found->has_value()), std::nullopt}
                 : bytecodeTextFailure(found.error(), arguments[0]);
  }
  if (name == "textFind") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      return bytecodeFailure("KTEXT2011", "textFind expects a string and a string needle");
    auto found = unicodeFind(std::get<std::string>(arguments[0].data),
                             std::get<std::string>(arguments[1].data));
    if (!found)
      return bytecodeTextFailure(found.error(), arguments[0]);
    return NativeCallResult{found->has_value() ? RuntimeValue(**found) : RuntimeValue(),
                            std::nullopt};
  }
  if (name == "textSlice") {
    if ((arguments.size() != 2 && arguments.size() != 3) ||
        !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::int64_t>(arguments[1].data) ||
        (arguments.size() == 3 && !std::holds_alternative<std::int64_t>(arguments[2].data)))
      return bytecodeFailure("KTEXT2012", "textSlice expects text, start, and optional end integers");
    const auto end = arguments.size() == 3
                         ? std::optional{std::get<std::int64_t>(arguments[2].data)}
                         : std::nullopt;
    auto sliced = unicodeSlice(std::get<std::string>(arguments[0].data),
                               std::get<std::int64_t>(arguments[1].data), end);
    return sliced ? NativeCallResult{RuntimeValue(std::move(*sliced)), std::nullopt}
                  : bytecodeTextFailure(sliced.error(), arguments[0]);
  }
  if (name == "textReplace") {
    if (arguments.size() != 3 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data) ||
        !std::holds_alternative<std::string>(arguments[2].data))
      return bytecodeFailure("KTEXT2013",
                             "textReplace expects text, needle, and replacement strings");
    auto replaced = unicodeReplace(std::get<std::string>(arguments[0].data),
                                   std::get<std::string>(arguments[1].data),
                                   std::get<std::string>(arguments[2].data));
    return replaced ? NativeCallResult{RuntimeValue(std::move(*replaced)), std::nullopt}
                    : bytecodeTextFailure(replaced.error(), arguments[0]);
  }
  if (name == "textSplit") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      return bytecodeFailure("KTEXT2014", "textSplit expects text and separator strings");
    auto pieces = unicodeSplit(std::get<std::string>(arguments[0].data),
                               std::get<std::string>(arguments[1].data));
    if (!pieces)
      return bytecodeTextFailure(pieces.error(), arguments[0]);
    auto *result = ctx.heap.allocateArray();
    for (auto &piece : *pieces)
      result->elements.emplace_back(std::move(piece));
    return NativeCallResult{RuntimeValue(result), std::nullopt};
  }
  if (name == "textTrim" || name == "textLower" || name == "textUpper") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KTEXT2015", std::string(name) + " expects exactly one string");
    UnicodeTextResult<std::string> transformed =
        name == "textTrim"    ? unicodeTrim(std::get<std::string>(arguments[0].data))
        : name == "textLower" ? unicodeLower(std::get<std::string>(arguments[0].data))
                              : unicodeUpper(std::get<std::string>(arguments[0].data));
    return transformed ? NativeCallResult{RuntimeValue(std::move(*transformed)), std::nullopt}
                       : bytecodeTextFailure(transformed.error(), arguments[0]);
  }
  return std::nullopt;
}

} // namespace kyna::detail
