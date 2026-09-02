#include "bytecode_private.hpp"
#include "../catalog/crypto/sha256.hpp"
#include <string>

namespace kyna::detail {

std::optional<NativeCallResult> cryptoBytecodeInvoke(
    std::string_view name, std::span<const RuntimeValue> arguments, BytecodeAdapterContext &ctx) {
  if (name == "cryptoSha256") {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      return bytecodeFailure("KCRYPTO1001", "cryptoSha256 expects one string");
    return NativeCallResult{RuntimeValue(sha256Hex(std::get<std::string>(arguments[0].data))),
                            std::nullopt};
  }
  return std::nullopt;
}

} // namespace kyna::detail
