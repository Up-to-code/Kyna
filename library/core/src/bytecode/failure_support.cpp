#include "bytecode_private.hpp"

namespace kyna::detail {

NativeCallResult bytecodeFailure(std::string code, std::string message, RuntimeValue cause) {
  return {{}, NativeCallFailure{std::move(code), std::move(message), std::move(cause)}};
}

NativeCallResult bytecodeTextFailure(const UnicodeTextError &error, const RuntimeValue &cause) {
  return bytecodeFailure(error.code, error.message, cause);
}

} // namespace kyna::detail
