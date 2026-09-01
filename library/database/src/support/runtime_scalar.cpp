#include "../database_private.hpp"

namespace kyna::detail {

Value runtimeScalar(const DatabaseScalar &value) {
  return std::visit([](const auto &stored) { return Value(stored); }, value);
}

} // namespace kyna::detail
