#include "catalog_private.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace kyna::detail {

void installCollectionLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();

  auto push = std::make_shared<Function>();
  push->native = true;
  push->nativeCall = [](const std::vector<Value> &a) {
    if (a.size() != 2 || !std::holds_alternative<ArrayPtr>(a[0].data))
      throw KynaError({"push expects an array and a value", {1, 1}, false});
    std::get<ArrayPtr>(a[0].data)->elements.push_back(a[1]);
    return Value();
  };
  global->define("push", Value(push), false);
  auto pop = std::make_shared<Function>();
  pop->native = true;
  pop->nativeCall = [](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<ArrayPtr>(a[0].data))
      throw KynaError({"pop expects an array", {1, 1}, false});
    auto array = std::get<ArrayPtr>(a[0].data);
    if (array->elements.empty())
      return Value();
    auto v = array->elements.back();
    array->elements.pop_back();
    return v;
  };
  global->define("pop", Value(pop), false);
  auto keys = std::make_shared<Function>();
  keys->native = true;
  keys->nativeCall = [&interpreter, global](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<ObjectPtr>(a[0].data))
      throw KynaError({"keys expects an object", {1, 1}, false});
    auto array = interpreter.heap().allocateArray();
    for (const auto &[name, value] : std::get<ObjectPtr>(a[0].data)->fields)
      array->elements.emplace_back(name);
    return Value(array);
  };
  global->define("keys", Value(keys), false);

  auto filter = std::make_shared<Function>();
  filter->native = true;
  filter->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 2 || !std::holds_alternative<ArrayPtr>(arguments[0].data) ||
        !std::holds_alternative<FunctionPtr>(arguments[1].data))
      throw KynaError({"filter expects an array and predicate function", {1, 1}, false});
    auto output = interpreter.heap().allocateArray();
    const auto input = std::get<ArrayPtr>(arguments[0].data);
    const auto predicate = std::get<FunctionPtr>(arguments[1].data);
    for (std::size_t index = 0; index < input->elements.size(); ++index) {
      std::vector<Value> predicateArguments{input->elements[index]};
      if (!predicate->native && predicate->declaration.params.size() > 1)
        predicateArguments.emplace_back(static_cast<std::int64_t>(index));
      if (predicate->call(predicateArguments, interpreter).isTruthy())
        output->elements.push_back(input->elements[index]);
    }
    return Value(output);
  };
  global->define("filter", Value(filter), false);

  auto bubbleSort = std::make_shared<Function>();
  bubbleSort->native = true;
  bubbleSort->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2 ||
        !std::holds_alternative<ArrayPtr>(arguments[0].data) ||
        (arguments.size() == 2 && !std::holds_alternative<FunctionPtr>(arguments[1].data)))
      throw KynaError({"bubbleSort expects an array and optional comparator", {1, 1}, false});
    auto output = interpreter.heap().allocateArray();
    output->elements = std::get<ArrayPtr>(arguments[0].data)->elements;
    const auto comparator =
        arguments.size() == 2 ? std::get<FunctionPtr>(arguments[1].data) : FunctionPtr{};
    const auto shouldSwap = [&](const Value &left, const Value &right) {
      if (comparator)
        return comparator->call({left, right}, interpreter).isTruthy();
      if (std::holds_alternative<std::int64_t>(left.data) &&
          std::holds_alternative<std::int64_t>(right.data))
        return std::get<std::int64_t>(left.data) > std::get<std::int64_t>(right.data);
      if ((std::holds_alternative<std::int64_t>(left.data) ||
           std::holds_alternative<double>(left.data)) &&
          (std::holds_alternative<std::int64_t>(right.data) ||
           std::holds_alternative<double>(right.data))) {
        const auto number = [](const Value &value) {
          return std::holds_alternative<std::int64_t>(value.data)
                     ? static_cast<double>(std::get<std::int64_t>(value.data))
                     : std::get<double>(value.data);
        };
        return number(left) > number(right);
      }
      if (std::holds_alternative<std::string>(left.data) &&
          std::holds_alternative<std::string>(right.data))
        return std::get<std::string>(left.data) > std::get<std::string>(right.data);
      throw KynaError({"default bubbleSort supports only numbers or strings", {1, 1}, false});
    };
    for (std::size_t remaining = output->elements.size(); remaining > 1; --remaining) {
      bool changed = false;
      for (std::size_t index = 1; index < remaining; ++index) {
        if (!shouldSwap(output->elements[index - 1], output->elements[index]))
          continue;
        std::swap(output->elements[index - 1], output->elements[index]);
        changed = true;
      }
      if (!changed)
        break;
    }
    return Value(output);
  };
  global->define("bubbleSort", Value(bubbleSort), false);
  global->define("sort", Value(bubbleSort), false);

  auto call = std::make_shared<Function>();
  call->native = true;
  call->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2 ||
        !std::holds_alternative<FunctionPtr>(arguments[0].data) ||
        (arguments.size() == 2 && !std::holds_alternative<ArrayPtr>(arguments[1].data)))
      throw KynaError({"call expects a function and optional argument array", {1, 1}, false});
    std::vector<Value> invocationArguments;
    if (arguments.size() == 2)
      invocationArguments = std::get<ArrayPtr>(arguments[1].data)->elements;
    return std::get<FunctionPtr>(arguments[0].data)->call(invocationArguments, interpreter);
  };
  global->define("call", Value(call), false);
}

} // namespace kyna::detail
