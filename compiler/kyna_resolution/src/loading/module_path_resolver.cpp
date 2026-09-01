#include "module_path_resolver.hpp"

#include <cstring>

namespace kyna::module_loading {

std::filesystem::path canonicalize(const std::filesystem::path &path) {
  std::error_code error;
  auto value = std::filesystem::weakly_canonical(path, error);
  return error ? std::filesystem::absolute(path).lexically_normal() : value;
}

bool isDeclarationFile(const std::filesystem::path &path) {
  const auto name = path.filename().string();
  const auto endsWith = [&](const char *suffix) {
    const auto len = std::char_traits<char>::length(suffix);
    return name.size() >= len && name.compare(name.size() - len, len, suffix) == 0;
  };
  return endsWith(".kyna.d") || endsWith(".d.ky") || endsWith(".ky.d");
}

namespace {
std::vector<std::filesystem::path> candidates(
    const std::filesystem::path &base, const std::filesystem::path &requested) {
  std::vector<std::filesystem::path> result;
  const auto add = [&](const std::filesystem::path &candidate) {
    if (std::filesystem::exists(candidate))
      result.push_back(candidate);
  };
  if (requested.has_extension()) {
    add(base / requested);
  } else {
    const auto stem = base / requested;
    add(std::filesystem::path(stem.string() + ".kyna.d"));
    add(std::filesystem::path(stem.string() + ".kyna"));
    add(std::filesystem::path(stem.string() + ".d.ky"));
    add(std::filesystem::path(stem.string() + ".ky.d"));
    add(std::filesystem::path(stem.string() + ".ky"));
  }
  return result;
}
} // namespace

std::filesystem::path resolveModulePath(
    const std::filesystem::path &importer,
    const std::string &requestedPath,
    const std::vector<std::filesystem::path> &modulePaths) {
  const std::filesystem::path requested(requestedPath);
  for (const auto &relative : candidates(importer.parent_path(), requested))
    if (std::filesystem::exists(relative))
      return canonicalize(relative);
  for (const auto &root : modulePaths)
    for (const auto &candidate : candidates(root, requested))
      if (std::filesystem::exists(candidate))
        return canonicalize(candidate);
  return canonicalize(importer.parent_path() / requested);
}

} // namespace kyna::module_loading
