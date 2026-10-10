#pragma once

#include <kyna/execution/bytecode_virtual_machine.hpp>
#include <kyna/semantics/type_model.hpp>
#include <functional>
#include <map>
#include <span>

namespace kyna {

// Registered host functions are immutable session contracts. Callbacks execute
// synchronously; NativeCallbacks and argument views must not outlive a call.
class NativeLifetime;
class NativeResources;
struct NativeFunction {
  std::string name;
  std::vector<TypeRef> parameters;
  TypeRef result;
  std::function<NativeCallResult(std::span<const RuntimeValue>, Heap &, const NativeCallbacks &)> invoke;
  std::shared_ptr<NativeLifetime> lifetime{};
  std::shared_ptr<NativeResources> resources{};
  std::function<void()> shutdown{};
  std::string moduleName{}, moduleVersion{};
  unsigned abiVersion{};
};

class Interpreter;
bool nativeValueMatchesType(const TypeRef &, const RuntimeValue &, const NativeCallbacks &);
void installNativeFunctions(Interpreter &, const std::vector<NativeFunction> &);

std::map<std::string, TypeRef> nativeFunctionBindings(const std::vector<NativeFunction> &functions);
NativeCallResult invokeNativeFunction(const NativeFunction &, std::span<const RuntimeValue>,
                                     Heap &, const NativeCallbacks &);

} // namespace kyna
