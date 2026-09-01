#include "../support_private.hpp"

#include <fstream>
#include <sstream>

namespace kyna::detail {

bool requiresHostServer(const std::filesystem::path &entry) {
  std::ifstream file(entry, std::ios::binary);
  if (!file) return false;
  std::ostringstream contents; contents << file.rdbuf();
  return contents.str().find("http.server") != std::string::npos;
}

} // namespace kyna::detail
