// Owns retained callback roots, runtime activation, and thread-affinity checks.
#include <kyna/language/native_lifetimes.hpp>
#include <limits>
namespace kyna {
namespace {
void fail(std::string message, std::string code = "KNATIVE1006") {
  throw KynaError({std::move(message), {}, false, std::move(code)});
}
}
NativeLifetime::NativeLifetime() : thread(std::this_thread::get_id()) {}
void NativeLifetime::requireThread() const {
  if (thread != std::this_thread::get_id()) fail("native callbacks must run on the runtime/UI thread", "KNATIVE1007");
}
NativeLifetime::Activation::Activation(NativeLifetime &owner, Heap &heap, const NativeCallbacks &callbacks)
    : owner(owner) {
  owner.requireThread();
  if (owner.closed) fail("native callback session is closed");
  owner.active.push_back({&heap, &callbacks});
}
NativeLifetime::Activation::~Activation() { owner.active.pop_back(); }
std::uint64_t NativeLifetime::retainCallback(const Value &value, std::optional<TypeRef> contract) {
  requireThread();
  if (closed || active.empty()) fail("cannot retain a callback outside an active runtime", "KNATIVE1007");
  const auto &runtime = active.back();
  if (!runtime.callbacks->arity || !runtime.callbacks->arity(value)) fail("value is not a language callback", "KNATIVE1003");
  if (contract && !nativeValueMatchesType(*contract, value, *runtime.callbacks))
    fail("callback argument count does not match its native contract", "KNATIVE1003");
  if (next == std::numeric_limits<std::uint64_t>::max()) fail("native callback handle space exhausted");
  const auto handle = next++;
  callbacks.emplace(handle, Callback{runtime.heap, runtime.heap->retain(value), std::move(contract)});
  return handle;
}
void NativeLifetime::releaseCallback(std::uint64_t handle) {
  requireThread();
  if (!callbacks.erase(handle)) fail("stale native callback handle");
}
Value NativeLifetime::callbackValue(std::uint64_t handle) const {
  requireThread();
  const auto found = callbacks.find(handle);
  if (closed || found == callbacks.end() || !found->second.root.valid()) fail("stale native callback handle");
  if (active.empty() || active.back().heap != found->second.heap)
    fail("callback's owning runtime is not active", "KNATIVE1007");
  return found->second.root.value();
}
NativeCallResult NativeLifetime::invokeCallback(std::uint64_t handle, std::span<const Value> args) {
  try {
    requireThread();
    const auto found = callbacks.find(handle);
    if (closed || found == callbacks.end() || !found->second.root.valid()) fail("stale native callback handle");
    if (active.empty() || active.back().heap != found->second.heap)
      fail("callback's owning runtime is not active", "KNATIVE1007");
    // Nested native calls can grow the activation vector while the callback runs.
    const auto runtime = active.back();
    if (!runtime.callbacks->invoke) fail("runtime cannot invoke callbacks", "KNATIVE1007");
    auto roots = runtime.heap->rootScope();
    for (const auto &arg : args) roots.protect(arg);
    Value callback = found->second.root.value();
    roots.protect(callback);
    // Copy the contract before invoking: callbacks may release their own lease.
    const auto contract = found->second.contract;
    if (contract) {
      if (contract->typeArgs.empty() || args.size() != contract->typeArgs.size() - 1)
        fail("native callback argument count does not match its contract", "KNATIVE1003");
      for (std::size_t i = 0; i < args.size(); ++i)
        if (!nativeValueMatchesType(contract->typeArgs[i], args[i], *runtime.callbacks))
          fail("native callback argument does not match its contract", "KNATIVE1003");
    }
    auto result = runtime.callbacks->invoke(callback, args);
    if (!result.failure && contract && !nativeValueMatchesType(contract->typeArgs.back(), result.value, *runtime.callbacks))
      fail("native callback result does not match its contract", "KNATIVE1004");
    return result;
  } catch (const KynaError &error) {
    return {{}, NativeCallFailure{error.diagnostic.code, error.diagnostic.message, {}, error.diagnostic}};
  }
}
void NativeLifetime::close() {
  requireThread();
  if (!active.empty()) fail("cannot close an active callback session", "KNATIVE1007");
  callbacks.clear(); closed = true;
}
}
