#include <memory>

#include "kyna/execution/database_port.hpp"
#include "../host_private.hpp"

namespace kyna {

std::shared_ptr<DatabasePort> productionDatabasePort() {
#if defined(KYNA_HAS_POSTGRESQL)
  return detail::makePostgresDatabase();
#else
  return detail::makeUnavailableDatabase();
#endif
}

} // namespace kyna
