#include "bytecode_private.hpp"
#include "../codecs/format/format_value_codec.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include "kyna/formats/document_formats.hpp"
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> formatsBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "jsonParse") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("K5100", "jsonParse expects exactly one JSON string");
    try {
      return NativeCallResult{parseJsonValue(std::get<std::string>(arguments[0].data), ctx.heap),
                              std::nullopt};
    } catch (const KynaError &error) {
      return bytecodeFailure(error.diagnostic.code.empty() ? "K5100" : error.diagnostic.code,
                             error.diagnostic.message, arguments[0]);
    }
  }
  if (name == "jsonStringify") {
    if (arguments.size() != 1)
      return bytecodeFailure("K5101", "jsonStringify expects exactly one value");
    try {
      return NativeCallResult{RuntimeValue(stringifyJsonValue(arguments[0])), std::nullopt};
    } catch (const KynaError &error) {
      return bytecodeFailure(error.diagnostic.code.empty() ? "K5101" : error.diagnostic.code,
                             error.diagnostic.message, arguments[0]);
    }
  }
  if (name == "tomlParse" || name == "xmlParse") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure(name == "tomlParse" ? "KFORMAT1001" : "KFORMAT1101",
                             std::string(name) + " expects exactly one string");
    const auto parsed = name == "tomlParse"
                            ? parseTomlDocument(std::get<std::string>(arguments[0].data))
                            : parseXmlDocument(std::get<std::string>(arguments[0].data));
    if (!parsed.valid)
      return bytecodeFailure(parsed.failure.code, parsed.failure.message, arguments[0]);
    return NativeCallResult{formatValueToRuntime(parsed.value, ctx.heap), std::nullopt};
  }
  if (name == "tomlStringify" || name == "xmlStringify") {
    if (arguments.size() != 1)
      return bytecodeFailure(name == "tomlStringify" ? "KFORMAT1002" : "KFORMAT1102",
                             std::string(name) + " expects exactly one value");
    std::string conversionError;
    auto value = runtimeValueToFormat(arguments[0], conversionError);
    if (!value)
      return bytecodeFailure(name == "tomlStringify" ? "KFORMAT1002" : "KFORMAT1102",
                             std::move(conversionError), arguments[0]);
    const auto encoded = name == "tomlStringify" ? stringifyTomlDocument(*value)
                                                  : stringifyXmlDocument(*value);
    if (!encoded.valid)
      return bytecodeFailure(encoded.failure.code, encoded.failure.message, arguments[0]);
    return NativeCallResult{RuntimeValue(std::get<std::string>(encoded.value.data)), std::nullopt};
  }
  return std::nullopt;
}

} // namespace kyna::detail
