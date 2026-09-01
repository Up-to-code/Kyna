#include "../collections_private.hpp"

namespace kyna::detail {

std::shared_ptr<Function> makeMapOp(Interpreter &interpreter) {
  auto transform = std::make_shared<Function>();
  transform->native = true;
  transform->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    const auto [input, mapper] = arrayAndFunction(arguments, "map");
    auto output = interpreter.heap().allocateArray();
    output->elements.reserve(input->elements.size());
    for (std::size_t index = 0; index < input->elements.size(); ++index)
      output->elements.push_back(
          mapper->call(elementArguments(mapper, input->elements[index], index), interpreter));
    return Value(output);
  };
  return transform;
}

} // namespace kyna::detail
