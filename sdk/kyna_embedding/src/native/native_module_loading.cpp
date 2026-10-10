// Owns shared-library lifetime and versioned native descriptor registration.
#include <kyna/language/native_modules.hpp>
#include "native_module_private.hpp"
#include <memory>
#include <stdexcept>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kyna {
namespace {
struct Library {
#if defined(_WIN32)
  HMODULE handle{};
  explicit Library(const std::filesystem::path &path) : handle(LoadLibraryW(path.c_str())) {}
  ~Library() { if (handle) FreeLibrary(handle); }
  void *symbol(const char *name) { return reinterpret_cast<void *>(GetProcAddress(handle, name)); }
#else
  void *handle{};
  explicit Library(const std::filesystem::path &path) : handle(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL)) {}
  ~Library() { if (handle) dlclose(handle); }
  void *symbol(const char *name) { return dlsym(handle, name); }
#endif
};
struct ResultOwner {
  kyna_native_result &result;
  bool released{false};
  void release() {
    if (released) return;
    released = true;
    if (result.release) result.release(result.context);
  }
  ~ResultOwner() {
    if (!released) {
      try { release(); } catch (...) { /* Preserve the original conversion failure. */ }
    }
  }
};
}
std::vector<NativeFunction> loadNativeModule(const std::filesystem::path &path) {
  auto library = std::make_shared<Library>(std::filesystem::absolute(path));
  if (!library->handle) {
#if defined(_WIN32)
    const auto detail = "Windows loader error " + std::to_string(GetLastError());
#else
    const auto *loaderError = dlerror();
    const auto detail = std::string(loaderError ? loaderError : "unknown loader error");
#endif
    throw std::runtime_error("cannot load native module: " + path.string() + ": " + detail);
  }
  if (auto *entry = library->symbol("kyna_native_module_v2")) return native::loadModuleV2(library, entry);
  auto entry = reinterpret_cast<kyna_native_module_entry>(library->symbol("kyna_native_module_v1"));
  if (!entry) throw std::runtime_error("native module is missing kyna_native_module_v1");
  const auto *module = entry();
  if (!module || module->abi_version != KYNA_NATIVE_ABI_VERSION || module->struct_size != sizeof(kyna_native_module))
    throw std::runtime_error("native module ABI mismatch");
  if (!module->name || !*module->name || !module->version || module->function_count > 10000 ||
      (module->function_count && !module->functions)) throw std::runtime_error("invalid native module descriptor");
  std::vector<NativeFunction> functions;
  for (std::size_t index = 0; index < module->function_count; ++index) {
    const auto *descriptor = &module->functions[index];
    if (!descriptor->name || !*descriptor->name || !descriptor->invoke || descriptor->parameter_count > 1024 ||
        (descriptor->parameter_count && !descriptor->parameters)) throw std::runtime_error("invalid native function descriptor");
    NativeFunction function;
    function.name = descriptor->name;
    function.moduleName = module->name; function.moduleVersion = module->version; function.abiVersion = module->abi_version;
    for (std::size_t parameter = 0; parameter < descriptor->parameter_count; ++parameter)
      function.parameters.push_back(native::decodeType(descriptor->parameters[parameter]));
    function.result = native::decodeType(descriptor->result);
    function.invoke = [library, descriptor](std::span<const Value> arguments, Heap &heap, const NativeCallbacks &) -> NativeCallResult {
      native::InputArena arena;
      std::vector<kyna_native_value> inputs;
      inputs.reserve(arguments.size());
      for (std::size_t index = 0; index < arguments.size(); ++index)
        inputs.push_back(arena.encode(arguments[index], &descriptor->parameters[index]));
      auto result = descriptor->invoke(inputs.data(), inputs.size());
      ResultOwner owner{result};
      NativeCallResult converted;
      if (result.error_code || result.error_message)
        converted.failure = NativeCallFailure{result.error_code ? result.error_code : "KNATIVE1005",
            result.error_message ? result.error_message : "native module failed", {}};
      else converted.value = native::decodeValue(result.value, heap);
      owner.release();
      return converted;
    };
    functions.push_back(std::move(function));
  }
  nativeFunctionBindings(functions);
  return functions;
}
}
