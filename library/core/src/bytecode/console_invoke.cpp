#include "bytecode_private.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include <chrono>

namespace kyna::detail {

std::optional<NativeCallResult> consoleBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "print" || name == "log") {
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      if (index)
        ctx.output << ' ';
      ctx.output << arguments[index].display();
    }
    ctx.output << '\n';
    return NativeCallResult{};
  }
  if (name == "typeOf") {
    if (arguments.size() != 1)
      return bytecodeFailure("KSTD2001", "typeOf expects exactly one argument");
    return NativeCallResult{RuntimeValue(arguments.front().typeName()), std::nullopt};
  }
  if (name == "toString") {
    if (arguments.size() != 1)
      return bytecodeFailure("KSTD2004", "toString expects exactly one argument");
    return NativeCallResult{RuntimeValue(arguments.front().display()), std::nullopt};
  }
  if (name == "clockMs") {
    if (!arguments.empty())
      return bytecodeFailure("KSTD2005", "clockMs expects no arguments");
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch())
                             .count();
    return NativeCallResult{RuntimeValue(static_cast<std::int64_t>(elapsed)), std::nullopt};
  }
  if (name == "timeNow") {
    if (!arguments.empty())
      return bytecodeFailure("KSTD2005", "timeNow expects no arguments");
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    return NativeCallResult{RuntimeValue(static_cast<std::int64_t>(elapsed)), std::nullopt};
  }
  if (name == "timeSleep") {
    if (arguments.size() != 1 || !std::holds_alternative<std::int64_t>(arguments[0].data))
      return bytecodeFailure("KSTD2007", "timeSleep expects a millisecond duration");
    ctx.capabilities.clock->sleep(
        std::chrono::milliseconds(std::get<std::int64_t>(arguments[0].data)));
    return NativeCallResult{};
  }
  if (name == "profileLog") {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KSTD2006", "profileLog expects a label string and an elapsed value");
    ctx.output << "[profile] " << arguments[0].display() << ": "
               << arguments[1].display() << " ms\n";
    return NativeCallResult{};
  }
  if (name == "error") {
    if (arguments.size() != 1)
      return bytecodeFailure("KSTD2004", "error expects exactly one argument");
    return bytecodeFailure("KRT2300", arguments.front().display(), arguments.front());
  }
  if (name == "slogInfo" || name == "slogWarn" || name == "slogError") {
    if (arguments.empty() || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KLOG1001", "structured log expects a message string");
    const char *level = name == "slogWarn" ? "warn" : name == "slogError" ? "error" : "info";
    ctx.output << "{\"level\":\"" << level
               << "\",\"msg\":" << stringifyJsonValue(arguments[0]);
    if (arguments.size() >= 2)
      ctx.output << ",\"fields\":" << stringifyJsonValue(arguments[1]);
    ctx.output << "}\n";
    return NativeCallResult{};
  }
  return std::nullopt;
}

} // namespace kyna::detail
