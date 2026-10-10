// Installs checked host functions in a tree-walk session environment.
#include <kyna/language/native_functions.hpp>
#include <kyna/execution/tree_walk_engine.hpp>

namespace kyna {
void installNativeFunctions(Interpreter &interpreter, const std::vector<NativeFunction> &functions) {
  nativeFunctionBindings(functions);
  for (const auto &function : functions) {
    auto value = std::make_shared<Function>();
    value->native = true;
    value->nativeCall = [function, &interpreter](const std::vector<Value> &arguments) {
      NativeCallbacks callbacks;
      callbacks.collect = [&interpreter] { interpreter.heap().collect(interpreter.rootEnvironments()); };
      callbacks.arity = [](const Value &value) -> std::optional<std::size_t> {
        const auto *function = std::get_if<FunctionPtr>(&value.data);
        if (!function || !*function || (*function)->native) return std::nullopt;
        return (*function)->declaration.params.size();
      };
      callbacks.invoke = [&interpreter](const Value &value, std::span<const Value> arguments) -> NativeCallResult {
        const auto *function = std::get_if<FunctionPtr>(&value.data);
        if (!function || !*function) return {{}, NativeCallFailure{"KNATIVE1003", "callback is not callable", {}}};
        return {interpreter.invoke(*function, std::vector<Value>(arguments.begin(), arguments.end())), std::nullopt};
      };
      auto result = invokeNativeFunction(function, arguments, interpreter.heap(), callbacks);
      if (result.failure) throw KynaError({result.failure->message, {}, false, result.failure->code});
      return result.value;
    };
    interpreter.globals()->define(function.name, Value(value), false);
  }
}
} // namespace kyna
