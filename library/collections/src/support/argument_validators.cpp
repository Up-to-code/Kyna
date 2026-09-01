#include "../collections_private.hpp"

namespace kyna::detail {

std::pair<ArrayPtr, FunctionPtr> arrayAndFunction(const std::vector<Value> &arguments,
                                                  const std::string &name) {
  if (arguments.size() != 2 || !std::holds_alternative<ArrayPtr>(arguments[0].data) ||
      !std::holds_alternative<FunctionPtr>(arguments[1].data))
    throw KynaError({name + " expects an array and function", {}, false, "KCOL1001"});
  return {std::get<ArrayPtr>(arguments[0].data), std::get<FunctionPtr>(arguments[1].data)};
}

std::vector<Value> elementArguments(const FunctionPtr &function, const Value &value,
                                    std::size_t index) {
  std::vector<Value> arguments{value};
  if (!function->native && function->declaration.params.size() > 1)
    arguments.emplace_back(static_cast<std::int64_t>(index));
  return arguments;
}

} // namespace kyna::detail
