#include "../collections_private.hpp"

namespace kyna::detail {

std::shared_ptr<Function> makeAllOp(Interpreter &interpreter) {
  auto all = std::make_shared<Function>();
  all->native = true;
  all->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    const auto [input, predicate] = arrayAndFunction(arguments, "all");
    for (std::size_t index = 0; index < input->elements.size(); ++index)
      if (!predicate->call(elementArguments(predicate, input->elements[index], index), interpreter)
               .isTruthy())
        return Value(false);
    return Value(true);
  };
  return all;
}

} // namespace kyna::detail
