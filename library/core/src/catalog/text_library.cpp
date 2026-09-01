#include "catalog_private.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/text/unicode_text.hpp"
#include <optional>
#include <string>

namespace kyna::detail {

void installTextLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();

  auto length = std::make_shared<Function>();
  length->native = true;
  length->nativeCall = [](const std::vector<Value> &a) {
    if (a.size() != 1)
      throw KynaError({"len expects one argument", {1, 1}, false});
    if (auto s = std::get_if<std::string>(&a[0].data)) {
      auto result = unicodeLength(*s);
      if (!result)
        throw KynaError({result.error().message, {1, 1}, false, result.error().code});
      return Value(*result);
    }
    if (auto x = std::get_if<ArrayPtr>(&a[0].data))
      return Value(static_cast<int64_t>((*x)->elements.size()));
    if (auto o = std::get_if<ObjectPtr>(&a[0].data))
      return Value(static_cast<int64_t>((*o)->fields.size()));
    throw KynaError({"len requires a string, array, or object", {1, 1}, false});
  };
  global->define("len", Value(length), false);
  auto textContains = std::make_shared<Function>();
  textContains->native = true;
  textContains->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      throw KynaError({"textContains expects text and a string needle", {1, 1}, false,
                       "KTEXT2010"});
    auto result = unicodeFind(std::get<std::string>(arguments[0].data),
                              std::get<std::string>(arguments[1].data));
    if (!result)
      throw KynaError({result.error().message, {1, 1}, false, result.error().code});
    return Value(result->has_value());
  };
  global->define("textContains", Value(textContains), false);
  auto textFind = std::make_shared<Function>();
  textFind->native = true;
  textFind->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      throw KynaError({"textFind expects text and a string needle", {1, 1}, false, "KTEXT2011"});
    auto result = unicodeFind(std::get<std::string>(arguments[0].data),
                              std::get<std::string>(arguments[1].data));
    if (!result)
      throw KynaError({result.error().message, {1, 1}, false, result.error().code});
    return result->has_value() ? Value(**result) : Value();
  };
  global->define("textFind", Value(textFind), false);
  auto textSlice = std::make_shared<Function>();
  textSlice->native = true;
  textSlice->nativeCall = [](const std::vector<Value> &arguments) {
    if ((arguments.size() != 2 && arguments.size() != 3) ||
        !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::int64_t>(arguments[1].data) ||
        (arguments.size() == 3 && !std::holds_alternative<std::int64_t>(arguments[2].data)))
      throw KynaError({"textSlice expects text, start, and optional end integers", {1, 1}, false,
                       "KTEXT2012"});
    const auto end = arguments.size() == 3
                         ? std::optional{std::get<std::int64_t>(arguments[2].data)}
                         : std::nullopt;
    auto result = unicodeSlice(std::get<std::string>(arguments[0].data),
                               std::get<std::int64_t>(arguments[1].data), end);
    if (!result)
      throw KynaError({result.error().message, {1, 1}, false, result.error().code});
    return Value(std::move(*result));
  };
  global->define("textSlice", Value(textSlice), false);
  auto textReplace = std::make_shared<Function>();
  textReplace->native = true;
  textReplace->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 3 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data) ||
        !std::holds_alternative<std::string>(arguments[2].data))
      throw KynaError({"textReplace expects text, needle, and replacement strings", {1, 1},
                       false, "KTEXT2013"});
    auto result = unicodeReplace(std::get<std::string>(arguments[0].data),
                                 std::get<std::string>(arguments[1].data),
                                 std::get<std::string>(arguments[2].data));
    if (!result)
      throw KynaError({result.error().message, {1, 1}, false, result.error().code});
    return Value(std::move(*result));
  };
  global->define("textReplace", Value(textReplace), false);
  auto textSplit = std::make_shared<Function>();
  textSplit->native = true;
  textSplit->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
        !std::holds_alternative<std::string>(arguments[1].data))
      throw KynaError({"textSplit expects text and separator strings", {1, 1}, false,
                       "KTEXT2014"});
    auto pieces = unicodeSplit(std::get<std::string>(arguments[0].data),
                               std::get<std::string>(arguments[1].data));
    if (!pieces)
      throw KynaError({pieces.error().message, {1, 1}, false, pieces.error().code});
    auto *array = interpreter.heap().allocateArray();
    for (auto &piece : *pieces)
      array->elements.emplace_back(std::move(piece));
    return Value(array);
  };
  global->define("textSplit", Value(textSplit), false);
  const auto defineUnaryText = [&global](const std::string &name,
                                        auto operation) {
    auto function = std::make_shared<Function>();
    function->native = true;
    function->nativeCall = [name, operation](const std::vector<Value> &arguments) {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        throw KynaError({name + " expects exactly one string", {1, 1}, false, "KTEXT2015"});
      auto result = operation(std::get<std::string>(arguments[0].data));
      if (!result)
        throw KynaError({result.error().message, {1, 1}, false, result.error().code});
      return Value(std::move(*result));
    };
    global->define(name, Value(function), false);
  };
  defineUnaryText("textTrim", unicodeTrim);
  defineUnaryText("textLower", unicodeLower);
  defineUnaryText("textUpper", unicodeUpper);
}

} // namespace kyna::detail
