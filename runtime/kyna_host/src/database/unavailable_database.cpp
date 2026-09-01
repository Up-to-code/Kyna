#include <optional>

#include "kyna/execution/database_port.hpp"
#include "../host_private.hpp"

namespace kyna::detail {

class UnavailableDatabase final : public DatabasePort {
public:
  std::optional<DatabaseResult> execute(const DatabaseRequest &, DatabaseFailure &failure) override {
    failure = {DatabaseFailurePhase::Configuration, "KDB-NO-ADAPTER",
               "this Kyna build does not include the PostgreSQL adapter", false};
    return std::nullopt;
  }
};

std::shared_ptr<DatabasePort> makeUnavailableDatabase() {
  return std::make_shared<UnavailableDatabase>();
}

} // namespace kyna::detail
