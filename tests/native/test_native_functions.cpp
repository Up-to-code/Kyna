// Verifies one checked contract across host execution, VM calls, and conversion failures.
#include <kyna/language/language_session.hpp>
#include <cassert>
#include <stdexcept>

int main() {
  using namespace kyna;
  int calls = 0;
  NativeFunction twice{"hostTwice", {{"int", false, {}, {}}}, {"int", false, {}, {}},
      [&calls](std::span<const Value> args, Heap &, const NativeCallbacks &) -> NativeCallResult {
        ++calls;
        return {Value(std::get<std::int64_t>(args[0].data) * 2), {}};
      }};
  LanguageSessionOptions options;
  options.nativeFunctions = {twice};
  LanguageSession session(options);
  assert(session.checkSource("native", "var x: int = hostTwice(21);").ok());
  assert(!session.checkSource("native", "hostTwice(\"wrong\");").ok());
  assert(session.inspectBytecode("native", "hostTwice(21);").ok());
  assert(session.runSource("native", "hostTwice(21);").ok());
  assert(session.runSource("native", "hostTwice(21);", true).ok());
  assert(calls == 2);
  // Calls through explicit any still pass through runtime conversion validation.
  auto bad = session.runSource("native", "var f: any = hostTwice; f(\"wrong\");");
  assert(!bad.ok());
  assert(calls == 2);
  Heap heap;
  Value wrong("wrong");
  auto result = invokeNativeFunction(twice, std::span<const Value>(&wrong, 1), heap, {});
  assert(result.failure && result.failure->code == "KNATIVE1003");
  result = invokeNativeFunction(twice, {}, heap, {});
  assert(result.failure && result.failure->code == "KNATIVE1002");
  NativeFunction invalidResult{"invalid", {}, {"int", false, {}, {}},
      [](auto, auto &, auto &) -> NativeCallResult { return {Value("wrong"), {}}; }};
  result = invokeNativeFunction(invalidResult, {}, heap, {});
  assert(result.failure && result.failure->code == "KNATIVE1004");
  NativeFunction throwing{"throwing", {}, {"int", false, {}, {}},
      [](auto, auto &, auto &) -> NativeCallResult { throw std::runtime_error("adapter failed"); }};
  result = invokeNativeFunction(throwing, {}, heap, {});
  assert(result.failure && result.failure->code == "KNATIVE1005");
  NativeFunction nullable{"nullable", {{"union", false, {{"int", false, {}, {}}, {"null", false, {}, {}}}, {}}}, {"void", false, {}, {}},
      [](auto, auto &, auto &) -> NativeCallResult { return {Value(), {}}; }};
  Value null;
  assert(!invokeNativeFunction(nullable, std::span<const Value>(&null, 1), heap, {}).failure);
  Value array(heap.allocateArray());
  std::get<ArrayPtr>(array.data)->elements.push_back(Value(std::int64_t{42}));
  heap.allocateArray(); // An unrooted allocation should be reclaimed during the call.
  NativeFunction collect{"collect", {{"array", false, {{"int", false, {}, {}}}, {}}},
      {"array", false, {{"int", false, {}, {}}}, {}},
      [](std::span<const Value> args, Heap &heap, const NativeCallbacks &) -> NativeCallResult {
        heap.collect({});
        return {args[0], {}};
      }};
  result = invokeNativeFunction(collect, std::span<const Value>(&array, 1), heap, {});
  assert(!result.failure);
  assert(std::get<ArrayPtr>(result.value.data)->elements.front().equals(Value(std::int64_t{42})));
  assert(heap.stats().reclaimed >= 1);
  bool rejected = false;
  try { nativeFunctionBindings({twice, twice}); }
  catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);
}
