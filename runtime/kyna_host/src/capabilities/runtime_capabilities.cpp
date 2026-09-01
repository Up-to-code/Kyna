#include "kyna/execution/runtime_capabilities.hpp"
#include "../host_private.hpp"

namespace kyna {

RuntimeCapabilities productionRuntimeCapabilities() {
  return {detail::makeLocalFileSystem(), detail::makeLocalProcess(),
          detail::makeCurlNetwork(), detail::makeSystemClock(),
          productionDatabasePort(), detail::makeLocalHostInfo(),
          detail::makeBeastHttpServer()};
}

} // namespace kyna
