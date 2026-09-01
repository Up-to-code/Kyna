#include "../collections_private.hpp"

namespace kyna::detail {

std::shared_ptr<Function> makeAnyOp(Interpreter &interpreter) {
  auto any = std::make_shared<Function>();
  any->native = true;
  any->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    const auto [input, predicate] = arrayAndFunction(arguments, "any");
    for (std::size_t index = 0; index < input->elements.size(); ++index)
      if (predicate->call(elementArguments(predicate, input->elements[index], index), interpreter)
              .isTruthy())
        return Value(true);
    return Value(false);
  };
  return any;
}

} // namespace kyna::detail
