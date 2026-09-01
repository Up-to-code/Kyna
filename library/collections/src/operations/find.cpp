#include "../collections_private.hpp"

namespace kyna::detail {

std::shared_ptr<Function> makeFindOp(Interpreter &interpreter) {
  auto find = std::make_shared<Function>();
  find->native = true;
  find->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    const auto [input, predicate] = arrayAndFunction(arguments, "find");
    for (std::size_t index = 0; index < input->elements.size(); ++index)
      if (predicate->call(elementArguments(predicate, input->elements[index], index), interpreter)
              .isTruthy())
        return input->elements[index];
    return Value();
  };
  return find;
}

} // namespace kyna::detail
