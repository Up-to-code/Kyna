// Owns ABI 2 descriptor validation and checked export registration.
#include "native_module_private.hpp"
#include "native_v2_private.hpp"
#include <stdexcept>
namespace kyna::native {
namespace {
struct ResultGuard {
  kyna_native_result_v2 &result;
  bool released{false};
  void release() { if (!released) { released = true; if (result.release) result.release(result.context); } }
  ~ResultGuard() { if (!released) { try { release(); } catch (...) {} } }
};
}
std::vector<NativeFunction> loadModuleV2(std::shared_ptr<void> library, void *entry) {
  auto module = reinterpret_cast<kyna_native_module_v2_entry>(entry)();
  if (!module || module->abi_version != KYNA_NATIVE_ABI_V2 || module->struct_size != sizeof(*module))
    throw std::runtime_error("native module ABI 2 mismatch");
  if (!module->name || !*module->name || !module->version || !module->create || !module->close ||
      module->function_count > 10000 || (module->function_count && !module->functions))
    throw std::runtime_error("invalid ABI 2 module descriptor");
  auto state = std::make_shared<ModuleV2>(); state->library = std::move(library); state->module = module;
  state->host = {KYNA_NATIVE_ABI_V2, sizeof(kyna_native_host_v2), state.get(),
    [](void *p, std::uint64_t id) { return static_cast<ModuleV2 *>(p)->retain(id); },
    [](void *p, std::uint64_t id) { return static_cast<ModuleV2 *>(p)->release(id); }, invokeCallbackV2};
  std::vector<NativeFunction> functions;
  for (std::size_t i = 0; i < module->function_count; ++i) {
    auto descriptor = &module->functions[i];
    if (!descriptor->name || !*descriptor->name || !descriptor->invoke || descriptor->parameter_count > 1024 ||
        (descriptor->parameter_count && !descriptor->parameters)) throw std::runtime_error("invalid ABI 2 function descriptor");
    NativeFunction function; function.name = descriptor->name;
    function.moduleName = module->name; function.moduleVersion = module->version; function.abiVersion = module->abi_version;
    for (std::size_t p = 0; p < descriptor->parameter_count; ++p) function.parameters.push_back(decodeTypeV2(descriptor->parameters[p]));
    function.result = decodeTypeV2(descriptor->result);
    function.lifetime = state->lifetime; function.resources = state->resources;
    function.shutdown = [state] { state->close(); };
    function.invoke = [state, descriptor](std::span<const Value> arguments, Heap &heap, const NativeCallbacks &) -> NativeCallResult {
      if (state->closed) return {{}, NativeCallFailure{"KNATIVE1006", "native module session is closed", {}}};
      CallV2 call(*state, heap); ArenaV2 arena; arena.owner = state;
      std::vector<kyna_native_value_v2> inputs; inputs.reserve(arguments.size());
      for (std::size_t i = 0; i < arguments.size(); ++i) inputs.push_back(arena.encode(arguments[i], &descriptor->parameters[i]));
      auto result = descriptor->invoke(state->context, inputs.data(), inputs.size()); ResultGuard guard{result};
      NativeCallResult converted;
      if (result.error_code || result.error_message) converted.failure = NativeCallFailure{
        result.error_code ? result.error_code : "KNATIVE1005", result.error_message ? result.error_message : "native module failed", {}};
      else converted.value = arena.decode(result.value);
      guard.release(); return converted;
    };
    functions.push_back(std::move(function));
  }
  nativeFunctionBindings(functions);
  state->context = module->create(&state->host);
  if (!state->context) throw std::runtime_error("native module session creation failed");
  return functions;
}
}
