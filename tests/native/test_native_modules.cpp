// Verifies dynamic module ownership, ABI rejection, and checked invocation.
#include <kyna/language/native_modules.hpp>
#include <kyna/language/language_session.hpp>
#include <cassert>
#include <stdexcept>
#include <fstream>
#include <chrono>
int main(int argc, char **argv) {
  assert(argc == 3);
  auto functions = kyna::loadNativeModule(argv[1]);
  assert(functions.size() == 5);
  kyna::Heap heap;
  kyna::Value argument(std::int64_t{21});
  auto result = kyna::invokeNativeFunction(functions[0], std::span<const kyna::Value>(&argument, 1), heap, {});
  assert(!result.failure && std::get<std::int64_t>(result.value.data) == 42);
  result = kyna::invokeNativeFunction(functions[1], {}, heap, {});
  assert(!result.failure && std::get<kyna::ArrayPtr>(result.value.data)->elements.size() == 2);
  result = kyna::invokeNativeFunction(functions[3], {}, heap, {});
  assert(!result.failure && std::get<std::int64_t>(result.value.data) == 1);
  result = kyna::invokeNativeFunction(functions[2], {}, heap, {});
  assert(result.failure && result.failure->code == "KNATIVE1005");
  result = kyna::invokeNativeFunction(functions[3], {}, heap, {});
  assert(!result.failure && std::get<std::int64_t>(result.value.data) == 2);
  result = kyna::invokeNativeFunction(functions[4], std::span<const kyna::Value>(&argument, 1), heap, {});
  assert(!result.failure && std::get<double>(result.value.data) == 10.5);
  kyna::LanguageSessionOptions options;
  options.nativeFunctions = functions;
  functions.clear(); // The session keeps the library mapped.
  kyna::LanguageSession session(std::move(options));
  assert(session.runSource("native", "nativeTwice(21);").ok());
  assert(!session.checkSource("native", "nativeTwice(false);").ok());
  const auto directory = std::filesystem::temp_directory_path() /
      ("kyna-native-import-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  std::ofstream(directory / "package.ky") << "export fn twice(value: int): int { return nativeTwice(value); }";
  std::ofstream(directory / "app.ky") << "import { twice } from \"./package.ky\"; twice(21);";
  assert(session.check(directory / "app.ky").ok());
  assert(session.run(directory / "app.ky").ok());
  std::filesystem::remove_all(directory);
  bool rejected = false;
  try { kyna::loadNativeModule(argv[2]); }
  catch (const std::runtime_error &) { rejected = true; }
  assert(rejected);
  rejected = false;
  try { kyna::loadNativeModule("missing-library"); }
  catch (const std::runtime_error &) { rejected = true; }
  assert(rejected);
}
