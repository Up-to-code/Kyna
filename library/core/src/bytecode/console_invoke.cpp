#include "bytecode_private.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include <chrono>

namespace kyna::detail {

std::optional<NativeCallResult> consoleBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "call" || name == "measure") {
    if (arguments.empty() || arguments.size() > (name == "measure" ? 1u : 2u) ||
        !ctx.callbacks || !ctx.callbacks->invoke || !ctx.callbacks->arity ||
        !ctx.callbacks->arity(arguments[0]) ||
        (arguments.size() == 2 && !std::holds_alternative<ArrayPtr>(arguments[1].data)))
      return bytecodeFailure("KSTD2004", std::string(name) + " expects a function and valid arguments");
    std::vector<RuntimeValue> values;
    if (arguments.size() == 2) values = std::get<ArrayPtr>(arguments[1].data)->elements;
    const auto start = name == "measure" ? std::chrono::steady_clock::now()
                                        : std::chrono::steady_clock::time_point{};
    auto result = ctx.callbacks->invoke(arguments[0], values);
    if (name == "measure" && !result.failure)
      result.value = RuntimeValue(std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - start).count());
    return result;
  }
  if (name == "collectGarbage") {
    if (!ctx.callbacks || !ctx.callbacks->collect)
      return bytecodeFailure("KSTD2004", "collection requires an active VM root set");
    ctx.callbacks->collect();
    return NativeCallResult{};
  }
  if (name == "gcStats") {
    const auto s = ctx.heap.stats();
    return NativeCallResult{RuntimeValue("heap: live=" + std::to_string(s.live) +
        " allocated=" + std::to_string(s.allocated) + " reclaimed=" + std::to_string(s.reclaimed) +
        " collections=" + std::to_string(s.collections) + " objects=" + std::to_string(s.objects) +
        " arrays=" + std::to_string(s.arrays) + " captures=" + std::to_string(s.captureCells) +
        " closures=" + std::to_string(s.closures) + " bound-methods=" + std::to_string(s.boundMethods) +
        " errors=" + std::to_string(s.errors)), std::nullopt};
  }
  if (name == "logColor") {
    static const std::map<std::string, std::string> colors{{"black", "30"}, {"red", "31"},
      {"green", "32"}, {"yellow", "33"}, {"blue", "34"}, {"magenta", "35"},
      {"cyan", "36"}, {"white", "37"}, {"reset", "0"}};
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      return bytecodeFailure("KSTD2004", "logColor expects a color and message");
    const auto color = colors.find(std::get<std::string>(arguments[0].data));
    if (color == colors.end()) return bytecodeFailure("KSTD2004", "unknown log color");
    ctx.output << "\033[" << color->second << 'm' << arguments[1].display() << "\033[0m\n";
    return NativeCallResult{};
  }
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
