#include "../collections_private.hpp"

namespace kyna::detail {

std::shared_ptr<Function> makeReduceOp(Interpreter &interpreter) {
  auto reduce = std::make_shared<Function>();
  reduce->native = true;
  reduce->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 3 || !std::holds_alternative<ArrayPtr>(arguments[0].data) ||
        !std::holds_alternative<FunctionPtr>(arguments[1].data))
      throw KynaError(
          {"reduce expects an array, reducer function, and initial value", {}, false, "KCOL1002"});
    const auto input = std::get<ArrayPtr>(arguments[0].data);
    const auto reducer = std::get<FunctionPtr>(arguments[1].data);
    Value accumulated = arguments[2];
    for (std::size_t index = 0; index < input->elements.size(); ++index) {
      std::vector<Value> reducerArguments{accumulated, input->elements[index]};
      if (!reducer->native && reducer->declaration.params.size() > 2)
        reducerArguments.emplace_back(static_cast<std::int64_t>(index));
      accumulated = reducer->call(reducerArguments, interpreter);
    }
    return accumulated;
  };
  return reduce;
}

} // namespace kyna::detail
