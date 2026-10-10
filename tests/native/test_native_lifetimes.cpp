// Verifies resource destruction, retained GC roots, callback activation and expiry.
#include <kyna/language/native_lifetimes.hpp>
#include <kyna/language/language_session.hpp>
#include <cassert>
#include <thread>
int main() {
  using namespace kyna;
  Heap::RetainedRoot expired;
  {
    Heap heap;
    Value value(heap.allocateArray());
    expired = heap.retain(value);
    heap.collect({}); assert(heap.arrayCount() == 1);
    bool refused = false;
    std::thread other([&] { try { expired.value(); } catch (...) { refused = true; } });
    other.join(); assert(refused);
  }
  assert(!expired.valid());
  bool refused = false;
  try { expired.value(); } catch (...) { refused = true; }
  assert(refused);
  Heap heap;
  {
    NativeLifetime contracts;
    NativeCallbacks runtime;
    runtime.arity = [](const Value &) -> std::optional<std::size_t> { return 0; };
    runtime.invoke = [](const Value &, std::span<const Value>) -> NativeCallResult { return {Value("wrong result"), {}}; };
    NativeLifetime::Activation active(contracts, heap, runtime);
    const TypeRef signature{"func", false, {{"int", false, {}, {}}}, {}};
    auto id = contracts.retainCallback(Value(VmFunctionReference{0}), signature);
    auto mismatch = contracts.invokeCallback(id, {});
    assert(mismatch.failure && mismatch.failure->code == "KNATIVE1004");
    Value argument(std::int64_t{1});
    mismatch = contracts.invokeCallback(id, std::span<const Value>(&argument, 1));
    assert(mismatch.failure && mismatch.failure->code == "KNATIVE1003");
    runtime.invoke = [&](const Value &, std::span<const Value>) -> NativeCallResult {
      NativeLifetime::Activation nested(contracts, heap, runtime);
      contracts.releaseCallback(id); return {Value(std::int64_t{42}), {}};
    };
    auto releasedInsideCall = contracts.invokeCallback(id, {});
    assert(!releasedInsideCall.failure && std::get<std::int64_t>(releasedInsideCall.value.data) == 42);
  }
  NativeResources resources, foreign;
  int destroyed = 0;
  Value handle = resources.create(heap, "TestResource", std::shared_ptr<void>(new int(42),
      [&](void *p) { delete static_cast<int *>(p); ++destroyed; }));
  assert(handle.typeName() == "TestResource");
  assert(*static_cast<int *>(resources.get(handle, "TestResource").get()) == 42);
  refused = false;
  try { foreign.get(handle, "TestResource"); } catch (const KynaError &e) { refused = e.diagnostic.code == "KNATIVE1006"; }
  assert(refused);
  resources.destroy(handle); assert(destroyed == 1);
  refused = false;
  try { resources.get(handle, "TestResource"); } catch (const KynaError &) { refused = true; }
  assert(refused);
  auto lifetime = std::make_shared<NativeLifetime>();
  std::uint64_t callback = 0;
  NativeFunction keep{"keepCallback", {{"func", false, {{"int", false, {}, {}}}, {}}},
      {"void", false, {}, {}},
      [lifetime, &callback](auto args, auto &, auto &) -> NativeCallResult {
        callback = lifetime->retainCallback(args[0]); return {};
      }, lifetime};
  NativeFunction fire{"fireCallback", {}, {"int", false, {}, {}},
      [lifetime, &callback](auto, auto &, auto &) -> NativeCallResult {
        return lifetime->invokeCallback(callback, {});
      }, lifetime};
  LanguageSessionOptions options; options.nativeFunctions = {keep, fire};
  {
    LanguageSession session(options);
    assert(session.runSource("callbacks", "fn create(seed: int) { var value = seed; fn next(): int { value = value + 1; return value; } return next; } var next = create(40); keepCallback(next); collectGarbage(); var answer: int = fireCallback();").ok());
    // A different VM execution cannot invoke the previous VM's retained closure.
    auto stale = session.runSource("callbacks", "fireCallback();");
    assert(!stale.ok());
  }
  auto result = lifetime->invokeCallback(callback, {});
  assert(result.failure && result.failure->code == "KNATIVE1006");
}
