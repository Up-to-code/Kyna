#include "../database_private.hpp"

namespace kyna::detail {

DatabaseScalar databaseScalar(const Value &value) {
  return std::visit(
      [](const auto &stored) -> DatabaseScalar {
        using T = std::decay_t<decltype(stored)>;
        if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_same_v<T, bool> ||
                      std::is_same_v<T, std::int64_t> || std::is_same_v<T, double> ||
                      std::is_same_v<T, std::string>)
          return stored;
        else if constexpr (std::is_same_v<T, char>)
          return std::string(1, stored);
        else
          throw KynaError({"database parameters must be null, bool, number, string, or char",
                           {}, false, "KDB1002"});
      },
      value.data);
}

} // namespace kyna::detail
