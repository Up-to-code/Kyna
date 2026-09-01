#include "bytecode_private.hpp"
#include <chrono>
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> processHostBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "processEnv") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KPROC2001", "processEnv expects exactly one variable name");
    auto value = ctx.capabilities.processes->environment(std::get<std::string>(arguments[0].data));
    return NativeCallResult{value ? RuntimeValue(std::move(*value)) : RuntimeValue(), std::nullopt};
  }
  if (name == "osName" || name == "osArchitecture" || name == "osWorkingDirectory" ||
      name == "terminalIsInteractive" || name == "terminalSupportsColor") {
    if (!arguments.empty())
      return bytecodeFailure("KHOST2000", std::string(name) + " expects no arguments");
    if (!ctx.capabilities.host)
      return bytecodeFailure("KHOST2000", "host information capability is unavailable");
    if (name == "osName")
      return NativeCallResult{RuntimeValue(ctx.capabilities.host->operatingSystem()), std::nullopt};
    if (name == "osArchitecture")
      return NativeCallResult{RuntimeValue(ctx.capabilities.host->architecture()), std::nullopt};
    if (name == "terminalIsInteractive")
      return NativeCallResult{RuntimeValue(ctx.capabilities.host->standardOutputIsTerminal()),
                              std::nullopt};
    if (name == "terminalSupportsColor")
      return NativeCallResult{RuntimeValue(ctx.capabilities.host->supportsColor()), std::nullopt};
    std::string message;
    auto directory = ctx.capabilities.host->workingDirectory(message);
    return directory ? NativeCallResult{RuntimeValue(std::move(*directory)), std::nullopt}
                     : bytecodeFailure("KHOST2001", std::move(message));
  }
  if (name == "processRun" || name == "build") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KPROC2002", std::string(name) + " expects one command string");
    return NativeCallResult{RuntimeValue(static_cast<std::int64_t>(
                                ctx.capabilities.processes->run(
                                    std::get<std::string>(arguments[0].data)))),
                            std::nullopt};
  }
  if (name == "sleep" || name == "wait") {
    if (arguments.size() != 1 || !std::holds_alternative<std::int64_t>(arguments[0].data))
      return bytecodeFailure("KTIME2001", std::string(name) + " expects integer milliseconds");
    const auto duration = std::get<std::int64_t>(arguments[0].data);
    if (duration < 0)
      return bytecodeFailure("KTIME2002", std::string(name) + " duration cannot be negative",
                             arguments[0]);
    ctx.capabilities.clock->sleep(std::chrono::milliseconds(duration));
    return NativeCallResult{};
  }
  return std::nullopt;
}

} // namespace kyna::detail
