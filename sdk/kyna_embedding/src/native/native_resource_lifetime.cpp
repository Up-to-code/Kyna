// Owns opaque resource leases and deterministic close/destroy behavior.
#include <kyna/language/native_lifetimes.hpp>
namespace kyna {
struct NativeResources::State {
  std::thread::id thread{std::this_thread::get_id()};
  bool closed{false};
  std::vector<std::weak_ptr<Lease>> leases;
};
struct NativeResources::Lease final : OpaqueNativeResource {
  std::weak_ptr<State> owner;
  std::string type;
  std::shared_ptr<void> resource;
  std::string typeName() const override { return type; }
};
namespace {
void stale(std::string message) { throw KynaError({std::move(message), {}, false, "KNATIVE1006"}); }
}
NativeResources::NativeResources() : state(std::make_shared<State>()) {}
NativeResources::~NativeResources() { close(); }
Value NativeResources::create(Heap &heap, std::string type, std::shared_ptr<void> resource) {
  if (state->closed || state->thread != std::this_thread::get_id()) stale("resource session is closed or on the wrong thread");
  if (type.empty() || !resource) stale("invalid native resource");
  auto lease = std::make_shared<Lease>();
  lease->owner = state; lease->type = std::move(type); lease->resource = std::move(resource);
  state->leases.push_back(lease);
  auto *object = heap.allocate(); object->nativeResource = std::move(lease);
  return Value(object);
}
std::shared_ptr<void> NativeResources::get(const Value &value, std::string_view type) const {
  if (state->closed || state->thread != std::this_thread::get_id()) stale("resource session is closed or on the wrong thread");
  const auto *object = std::get_if<ObjectPtr>(&value.data);
  if (!object || !*object) stale("value is not an opaque native resource");
  const auto lease = std::dynamic_pointer_cast<Lease>((*object)->nativeResource);
  if (!lease || lease->owner.lock() != state || !lease->resource || lease->type != type)
    stale("stale, foreign, or incompatible native resource");
  return lease->resource;
}
void NativeResources::destroy(const Value &value) {
  const auto *object = std::get_if<ObjectPtr>(&value.data);
  if (!object || !*object || !(*object)->nativeResource) stale("value is not a native resource");
  get(value, (*object)->nativeResource->typeName());
  std::dynamic_pointer_cast<Lease>((*object)->nativeResource)->resource.reset();
}
void NativeResources::close() {
  if (state->closed) return;
  for (const auto &weak : state->leases) if (auto lease = weak.lock()) lease->resource.reset();
  state->leases.clear(); state->closed = true;
}
}
