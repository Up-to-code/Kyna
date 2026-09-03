#include <kyna/modules/package_loader.hpp>
#include "module_path_resolver.hpp"

#include <algorithm>

namespace kyna::package_loading {
namespace {

bool hasKynaExtension(const std::filesystem::path &path) {
  const auto name = path.filename().string();
  return name.ends_with(".kyna") || name.ends_with(".kyna.d") || name.ends_with(".d.ky") ||
         name.ends_with(".ky.d") || name.ends_with(".ky");
}

std::string separators(const std::filesystem::path &path) {
  // Normalize to forward slashes so the internal-bracket search is portable
  // across platforms.
  std::string value = path.generic_string();
  return value;
}

} // namespace

PackageSource discoverPackage(const std::filesystem::path &dir) {
  PackageSource package;
  package.directory = dir;
  std::error_code error;
  if (!std::filesystem::is_directory(dir, error)) {
    error.clear();
    return package;
  }
  for (const auto &entry : std::filesystem::directory_iterator(dir, error)) {
    if (entry.is_regular_file() && hasKynaExtension(entry.path())) {
      const auto path = kyna::module_loading::canonicalize(entry.path());
      package.files.push_back(
          DiscoveredFile{path, kyna::module_loading::isDeclarationFile(entry.path())});
    }
  }
  std::stable_sort(package.files.begin(), package.files.end(),
                   [](const DiscoveredFile &lhs, const DiscoveredFile &rhs) {
                     return lhs.path.string() < rhs.path.string();
                   });
  return package;
}

bool isInternalImportAllowed(const std::filesystem::path &importerPath,
                             const std::filesystem::path &importedPath) {
  const std::string importer = separators(importerPath);
  const std::string imported = separators(importedPath);

  // Find the last "/internal/" segment in the imported path.
  std::size_t position = std::string::npos;
  std::size_t start = 0;
  while ((start = imported.find("/internal/", start)) != std::string::npos) {
    position = start;
    start += std::string("/internal/").size();
  }
  if (position == std::string::npos)
    return true; // the imported package is not an internal package

  const std::string parentOfInternal = imported.substr(0, position);
  // The importer must share the parent of the internal directory as a path
  // ancestor: its path equals the parent or begins with "parent/".
  if (importer == parentOfInternal)
    return true;
  return importer.size() > parentOfInternal.size() &&
         importer.compare(0, parentOfInternal.size(), parentOfInternal) == 0 &&
         importer[parentOfInternal.size()] == '/';
}

} // namespace kyna::package_loading
