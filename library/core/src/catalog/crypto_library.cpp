#include "catalog_private.hpp"
#include "crypto/sha256.hpp"
#include <string>

namespace kyna::detail {

void installCryptoLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();

  auto sha256 = std::make_shared<Function>();
  sha256->native = true;
  sha256->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"cryptoSha256 expects one string", {1, 1}, false, "KCRYPTO1001"});
    return Value(sha256Hex(std::get<std::string>(arguments[0].data)));
  };
  global->define("cryptoSha256", Value(sha256), false);
}

} // namespace kyna::detail
