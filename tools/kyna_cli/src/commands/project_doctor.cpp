#include "project_internals.hpp"
#include <cstdlib>

namespace kyna::cli {

int doctor(const Options &options, std::ostream &output) {
  const auto root = discoverProject();
  const bool pathOk = std::getenv("PATH") != nullptr;
  if (options.jsonOutput)
    output << "{\"cli\":\"ok\",\"version\":\"1.0.0\",\"manifest\":"
           << (root.empty() ? "false" : "true") << ",\"path\":" << (pathOk ? "true" : "false")
           << "}\n";
  else
    output << "Kyna doctor\n  CLI: ok (ky 1.0.0; kyna compatibility enabled)\n  PATH: "
           << (pathOk ? "ok" : "missing") << "\n  Project: "
           << (root.empty() ? "not found" : root.string()) << "\n  Cache: "
           << cacheRoot().string()
           << "\n  VS Code setting: kyna.executable (optional)\n";
  return pathOk ? 0 : 2;
}

} // namespace kyna::cli
