// Owns checked argument/result conversion and native exception translation.
#include <kyna/language/native_functions.hpp>
#include <kyna/language/native_lifetimes.hpp>
#include <kyna/symbols/standard_library_symbols.hpp>
#include <stdexcept>
#include <set>

namespace kyna {
namespace {
bool accepts(const TypeRef &type, const RuntimeValue &value, const NativeCallbacks &callbacks, int depth = 0) {
  if (depth > 64) return false;
  if (type.name == "any") return true;
  if (type.name == "union") {
    for (const auto &arm : type.typeArgs) if (accepts(arm, value, callbacks, depth + 1)) return true;
  }
  for (const auto &arm : type.unionTypes) if (accepts(arm, value, callbacks, depth + 1)) return true;
  if (std::holds_alternative<std::nullptr_t>(value.data))
    return type.nullable || type.name == "null" || type.name == "void";
  if (type.name == "num" || type.name == "float")
    return std::holds_alternative<double>(value.data) || std::holds_alternative<std::int64_t>(value.data);
  if (type.name == "array") {
    const auto *array = std::get_if<ArrayPtr>(&value.data);
    if (!array || !*array) return false;
    if (type.typeArgs.empty()) return true;
    if (type.typeArgs.size() != 1) return false;
    for (const auto &item : (*array)->elements)
      if (!accepts(type.typeArgs.front(), item, callbacks, depth + 1)) return false;
    return true;
  }
  if (type.name == "func") {
    const auto arity = callbacks.arity ? callbacks.arity(value) : std::nullopt;
    return arity && (type.typeArgs.empty() || *arity == type.typeArgs.size() - 1);
  }
  return value.typeName() == type.name;
}
NativeCallResult failure(std::string code, std::string message) {
  return {{}, NativeCallFailure{std::move(code), std::move(message), {}}};
}
}
bool nativeValueMatchesType(const TypeRef &type, const RuntimeValue &value, const NativeCallbacks &callbacks) {
  return accepts(type, value, callbacks);
}
std::map<std::string, TypeRef> nativeFunctionBindings(const std::vector<NativeFunction> &functions) {
  std::map<std::string, TypeRef> bindings;
  for (const auto &function : functions) {
    if (function.name.empty() || !function.invoke || findStandardLibrarySymbol(function.name))
      throw std::invalid_argument("invalid or reserved native function name: " + function.name);
    TypeRef signature{"func", false, function.parameters, {}};
    signature.typeArgs.push_back(function.result);
    if (!bindings.emplace(function.name, std::move(signature)).second)
      throw std::invalid_argument("duplicate native function: " + function.name);
  }
  return bindings;
}
NativeCallResult invokeNativeFunction(const NativeFunction &function,
    std::span<const RuntimeValue> arguments, Heap &heap, const NativeCallbacks &callbacks) {
  if (arguments.size() != function.parameters.size())
    return failure("KNATIVE1002", "native function '" + function.name + "' received the wrong argument count");
  for (std::size_t index = 0; index < arguments.size(); ++index)
    if (!nativeValueMatchesType(function.parameters[index], arguments[index], callbacks))
      return failure("KNATIVE1003", "native argument " + std::to_string(index + 1) + " does not match " + function.parameters[index].str());
  auto roots = heap.rootScope();
  std::vector<RuntimeValue> rooted(arguments.begin(), arguments.end());
  for (auto &value : rooted) roots.protect(value);
  try {
    std::unique_ptr<NativeLifetime::Activation> activation;
    if (function.lifetime) activation = std::make_unique<NativeLifetime::Activation>(*function.lifetime, heap, callbacks);
    auto result = function.invoke(rooted, heap, callbacks);
    if (!result.failure && !nativeValueMatchesType(function.result, result.value, callbacks))
      return failure("KNATIVE1004", "native result does not match " + function.result.str());
    return result;
  } catch (const KynaError &error) {
    return {{}, NativeCallFailure{error.diagnostic.code, error.diagnostic.message, {}, error.diagnostic}};
  } catch (const RuntimeThrownError &error) {
    return {{}, NativeCallFailure{error.value ? error.value->code : "KNATIVE1005", error.what(), {}}};
  } catch (const std::exception &error) {
    return failure("KNATIVE1005", "native exception: " + std::string(error.what()));
  } catch (...) {
    return failure("KNATIVE1005", "unknown native exception");
  }
}
} // namespace kyna
