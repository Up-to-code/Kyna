#pragma once

#include <memory>

#include "kyna/execution/runtime_capabilities.hpp"

namespace kyna::detail {

std::shared_ptr<FileSystemPort> makeLocalFileSystem();
std::shared_ptr<ProcessPort> makeLocalProcess();
std::shared_ptr<HostInfoPort> makeLocalHostInfo();
std::shared_ptr<ClockPort> makeSystemClock();
std::shared_ptr<HttpServerPort> makeBeastHttpServer();
std::shared_ptr<NetworkPort> makeCurlNetwork();

std::shared_ptr<DatabasePort> makeUnavailableDatabase();
#if defined(KYNA_HAS_POSTGRESQL)
std::shared_ptr<DatabasePort> makePostgresDatabase();
#endif

} // namespace kyna::detail
