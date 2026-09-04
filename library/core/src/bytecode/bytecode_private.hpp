#pragma once

#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/execution/runtime_capabilities.hpp"
#include "kyna/text/unicode_text.hpp"
#include <iosfwd>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace kyna::detail {

NativeCallResult bytecodeFailure(std::string code, std::string message,
                                 RuntimeValue cause = {});

NativeCallResult bytecodeTextFailure(const UnicodeTextError &error, const RuntimeValue &cause);

struct BytecodeAdapterContext {
  RuntimeCapabilities &capabilities;
  std::ostream &output;
  Heap &heap;
  const NativeCallbacks *callbacks{nullptr};
};

std::optional<NativeCallResult> consoleBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> textBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> filesystemBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> processHostBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> networkBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> formatsBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> collectionsBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> collectionCallbacksBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);
std::optional<NativeCallResult> cryptoBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx);

} // namespace kyna::detail
