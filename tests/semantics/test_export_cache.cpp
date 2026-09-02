#include "kyna/modules/module_loader.hpp"
#include "kyna/semantics/export_cache.hpp"
#include "kyna/semantics/module_analyzer.hpp"
#include "kyna/source/source_manager.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void writeFile(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream out(path);
  out << contents;
}

std::filesystem::path makeTempDir() {
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  auto dir = std::filesystem::temp_directory_path() / ("kyna_export_cache_" + std::to_string(nonce));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

void test_writes_and_reuses_dependency_stamp() {
  const auto dir = makeTempDir();
  writeFile(dir / "math.kyna", "export fn add(a: int, b: int): int { return a + b; }");
  writeFile(dir / "main.kyna", "import \"./math.kyna\" as math; var n: int = math.add(1, 2);");

  kyna::SourceManager sources;
  auto loaded = kyna::loadModuleGraph(sources, dir / "main.kyna");
  assert(loaded.ok());
  auto first = kyna::analyzeModuleGraph(std::move(loaded.graph));
  assert(first.ok());
  assert(first.cachedModules.empty());
  const auto cache = kyna::export_cache::cachePath(dir / "math.kyna");
  assert(std::filesystem::exists(cache));

  kyna::SourceManager sourcesAgain;
  auto loadedAgain = kyna::loadModuleGraph(sourcesAgain, dir / "main.kyna");
  assert(loadedAgain.ok());
  auto second = kyna::analyzeModuleGraph(std::move(loadedAgain.graph));
  assert(second.ok());
  assert(second.cachedModules.size() == 1);
  assert(std::filesystem::equivalent(second.cachedModules.front(), dir / "math.kyna"));

  writeFile(dir / "math.kyna",
            "export fn add(a: int, b: int): int { return \"nope\"; }");
  kyna::SourceManager sourcesBroken;
  auto loadedBroken = kyna::loadModuleGraph(sourcesBroken, dir / "main.kyna");
  auto broken = kyna::analyzeModuleGraph(std::move(loadedBroken.graph));
  assert(!broken.ok());
  assert(!std::filesystem::exists(cache));

  std::filesystem::remove_all(dir);
}

void test_entry_module_is_never_cached() {
  const auto dir = makeTempDir();
  writeFile(dir / "solo.kyna", "fn main(): int { return 1; }");
  kyna::SourceManager sources;
  auto loaded = kyna::loadModuleGraph(sources, dir / "solo.kyna");
  auto analyzed = kyna::analyzeModuleGraph(std::move(loaded.graph));
  assert(analyzed.ok());
  assert(analyzed.cachedModules.empty());
  assert(!std::filesystem::exists(kyna::export_cache::cachePath(dir / "solo.kyna")));
  std::filesystem::remove_all(dir);
}

} // namespace

int main() {
  test_writes_and_reuses_dependency_stamp();
  test_entry_module_is_never_cached();
}
