// Owns per-module ABI 2 lifetime state and C callback services.
#include "native_v2_private.hpp"
#include <limits>
namespace kyna::native {
CallV2::CallV2(ModuleV2 &owner, Heap &heap) : owner(owner), heap(heap) { owner.active.push_back(this); }
CallV2::~CallV2() { for (auto handle : borrowed) owner.release(handle); owner.active.pop_back(); }
ResourceV2::~ResourceV2() {
  if (!owner->closed && owner->context) {
    try { owner->module->resource_destroy(owner->context, handle); } catch (...) {}
  }
}
ModuleV2::~ModuleV2() { close(); }
void ModuleV2::close() {
  if (closed) return;
  closed = true;
  resources->close();
  if (context) { try { module->close(context); } catch (...) {} context = nullptr; }
  for (const auto &[handle, _] : references) { try { lifetime->releaseCallback(handle); } catch (...) {} }
  references.clear(); lifetime->close();
}
int ModuleV2::retain(std::uint64_t handle) {
  if (std::this_thread::get_id() != thread || closed) return 0;
  const auto found = references.find(handle);
  if (found == references.end() || found->second == std::numeric_limits<std::size_t>::max()) return 0;
  try { lifetime->callbackValue(handle); } catch (...) { return 0; }
  ++found->second; return 1;
}
int ModuleV2::release(std::uint64_t handle) {
  if (std::this_thread::get_id() != thread) return 0;
  const auto found = references.find(handle);
  if (found == references.end()) return 0;
  if (--found->second == 0) {
    references.erase(found); try { lifetime->releaseCallback(handle); } catch (...) {}
  }
  return 1;
}
std::uint64_t ModuleV2::borrow(const Value &value, const kyna_native_type_v2 *type) {
  if (active.empty() || closed) throw KynaError({"native runtime is inactive", {}, false, "KNATIVE1007"});
  const auto handle = lifetime->retainCallback(value, type ? std::optional<TypeRef>(decodeTypeV2(*type)) : std::nullopt);
  references.emplace(handle, 1); active.back()->borrowed.push_back(handle); return handle;
}
static kyna_native_result_v2 invokeCallbackV2Impl(void *context, std::uint64_t handle,
    const kyna_native_value_v2 *arguments, std::size_t count) {
  auto &owner = *static_cast<ModuleV2 *>(context);
  if (std::this_thread::get_id() != owner.thread) {
    kyna_native_result_v2 failure{}; failure.error_code = "KNATIVE1007";
    failure.error_message = "callback must run on the runtime thread"; return failure;
  }
  kyna_native_result_v2 output{};
  auto storage = std::make_unique<ArenaV2>(); storage->owner = owner.shared_from_this();
  try {
    if (owner.closed || owner.active.empty() || count > 1024 || (count && !arguments))
      throw KynaError({"native callback runtime is inactive or arguments are invalid", {}, false, "KNATIVE1007"});
    // callbackValue also verifies runtime and thread ownership before decoding.
    owner.lifetime->callbackValue(handle);
    auto roots = owner.active.back()->heap.rootScope();
    std::vector<Value> values; values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) { values.push_back(storage->decode(arguments[i])); roots.protect(values.back()); }
    auto result = owner.lifetime->invokeCallback(handle, values);
    if (result.failure) { storage->code = result.failure->code; storage->message = result.failure->message; }
    else {
      storage->resultRoot = owner.active.back()->heap.retain(result.value);
      output.value = storage->encode(result.value);
    }
  } catch (const KynaError &error) { storage->code = error.diagnostic.code; storage->message = error.what(); }
  catch (const std::exception &error) { storage->code = "KNATIVE1005"; storage->message = error.what(); }
  catch (...) { storage->code = "KNATIVE1005"; storage->message = "unknown callback exception"; }
  if (!storage->code.empty()) { output.error_code = storage->code.c_str(); output.error_message = storage->message.c_str(); }
  output.context = storage.release(); output.release = [](void *p) { delete static_cast<ArenaV2 *>(p); };
  return output;
}
kyna_native_result_v2 invokeCallbackV2(void *context, std::uint64_t handle,
    const kyna_native_value_v2 *arguments, std::size_t count) {
  try { return invokeCallbackV2Impl(context, handle, arguments, count); }
  catch (...) { kyna_native_result_v2 result{}; result.error_code = "KNATIVE1005";
    result.error_message = "native callback allocation or conversion failed"; return result; }
}
}
