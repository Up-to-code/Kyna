#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace kyna::package_loading {

struct DiscoveredFile {
  std::filesystem::path path;
  bool declarationFile{false};
};

// A package is a single compilation unit spanning one directory: all of its
// `*.kyna` source files (and optional `*.kyna.d` declaration files) resolve
// against a unified scope without explicit cross-file imports.
struct PackageSource {
  std::filesystem::path directory;
  std::vector<DiscoveredFile> files;
  [[nodiscard]] bool empty() const { return files.empty(); }
};

// Discovers every source file belonging to the package rooted at `dir`. Files
// directly inside `dir` (not in `internal`/nested subfolders) form the package.
PackageSource discoverPackage(const std::filesystem::path &dir);

// Implements the Go-style `internal/` boundary rule: a package located at or
// beneath a directory named `internal` may only be imported by packages that
// share the parent of that `internal` directory as a path ancestor.
//
// `importerPath` and `importedPath` should both be canonicalized before the
// comparison so that relative and absolute forms agree.
bool isInternalImportAllowed(const std::filesystem::path &importerPath,
                             const std::filesystem::path &importedPath);

} // namespace kyna::package_loading
