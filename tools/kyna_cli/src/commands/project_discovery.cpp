#include "project_internals.hpp"

namespace kyna::cli {

fs::path discoverProject(const fs::path &start) {
  std::error_code ec;
  auto current = fs::absolute(start, ec);
  if (fs::is_regular_file(current, ec))
    current = current.parent_path();
  while (!current.empty()) {
    if (fs::exists(current / "kyna.toml", ec))
      return current;
    const auto parent = current.parent_path();
    if (parent == current)
      break;
    current = parent;
  }
  return {};
}

fs::path projectEntry(const fs::path &root, std::string &error) {
  auto manifest = loadManifest(root, error);
  if (!error.empty())
    return {};
  auto entry = manifest["project"]["entry"].value<std::string>();
  if (!entry) {
    error = "kyna.toml is missing project.entry";
    return {};
  }
  return root / *entry;
}

bool applyProjectServerEnvironment(const fs::path &root, std::string &error) {
  auto manifest = loadManifest(root, error);
  if (!error.empty())
    return false;
  const auto host = manifest["server"]["host"].value_or(std::string("127.0.0.1"));
  const auto port = static_cast<int>(
      manifest["server"]["port"].value_or(std::int64_t{3000}));
  if (port < 1 || port > 65535) {
    error = "kyna.toml server.port must be between 1 and 65535";
    return false;
  }
#if defined(_WIN32)
  _putenv_s("KYNA_SERVER_HOST", host.c_str());
  _putenv_s("KYNA_SERVER_PORT", std::to_string(port).c_str());
#else
  setenv("KYNA_SERVER_HOST", host.c_str(), 1);
  setenv("KYNA_SERVER_PORT", std::to_string(port).c_str(), 1);
#endif
  return true;
}

} // namespace kyna::cli
