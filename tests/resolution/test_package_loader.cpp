#include <kyna/modules/package_loader.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using namespace kyna::package_loading;

std::filesystem::path makeTempDir() {
  auto base = std::filesystem::temp_directory_path();
  static int counter = 0;
  auto dir = base / ("kyna_package_test_" + std::to_string(++counter));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

void touch(const std::filesystem::path &path, const std::string &contents = "") {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out << contents;
}

void test_discover_package_files() {
  const auto dir = makeTempDir();
  touch(dir / "a.kyna", "var x = 1;");
  touch(dir / "b.kyna", "var y = 2;");
  touch(dir / "lib.kyna.d", "interface I {}");
  touch(dir / "note.txt", "not a source file");

  const auto package = discoverPackage(dir);
  assert(!package.empty());
  assert(package.directory == dir);
  assert(package.files.size() == 3); // a.kyna, b.kyna, lib.kyna.d

  bool foundDeclaration = false;
  for (const auto &file : package.files)
    if (file.path.filename() == "lib.kyna.d")
      foundDeclaration = file.declarationFile;
  assert(foundDeclaration);

  std::filesystem::remove_all(dir);
}

void test_discover_missing_directory_is_empty() {
  const auto dir = makeTempDir() / "does_not_exist";
  const auto package = discoverPackage(dir);
  assert(package.empty());
}

void test_internal_import_allowed_within_parent_tree() {
  // /repo/foo/internal/secret.kyna may be imported by anything under /repo/foo
  const auto imported = std::filesystem::path("/repo/foo/internal/secret.kyna");

  assert(isInternalImportAllowed("/repo/foo/app/main.kyna", imported));
  assert(isInternalImportAllowed("/repo/foo/app/util/helper.kyna", imported));
  assert(isInternalImportAllowed("/repo/foo/main.kyna", imported));
  assert(isInternalImportAllowed("/repo/foo", imported));
}

void test_internal_import_rejected_outside_parent_tree() {
  const auto imported = std::filesystem::path("/repo/foo/internal/secret.kyna");

  assert(!isInternalImportAllowed("/repo/bar/app/main.kyna", imported));
  assert(!isInternalImportAllowed("/repo/foobar/app/main.kyna", imported));
  assert(!isInternalImportAllowed("/other/main.kyna", imported));
}

void test_non_internal_import_always_allowed() {
  const auto imported = std::filesystem::path("/repo/foo/public/thing.kyna");
  assert(isInternalImportAllowed("/unrelated/main.kyna", imported));
}

} // namespace

int main() {
  test_discover_package_files();
  test_discover_missing_directory_is_empty();
  test_internal_import_allowed_within_parent_tree();
  test_internal_import_rejected_outside_parent_tree();
  test_non_internal_import_always_allowed();
  return 0;
}
