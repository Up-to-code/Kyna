#include "../collections_private.hpp"

namespace kyna::detail {

std::shared_ptr<Function> makeUniqueOp(Interpreter &interpreter) {
  auto unique = std::make_shared<Function>();
  unique->native = true;
  unique->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      throw KynaError({"unique expects one array", {}, false, "KCOL1003"});
    auto output = interpreter.heap().allocateArray();
    for (const auto &candidate : std::get<ArrayPtr>(arguments[0].data)->elements) {
      bool exists = false;
      for (const auto &accepted : output->elements)
        if (accepted.equals(candidate)) {
          exists = true;
          break;
        }
      if (!exists)
        output->elements.push_back(candidate);
    }
    return Value(output);
  };
  return unique;
}

} // namespace kyna::detail
