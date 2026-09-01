#include "catalog_private.hpp"
#include "../codecs/format/format_value_codec.hpp"
#include "../codecs/json/json_value_codec.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/formats/document_formats.hpp"
#include <string>

namespace kyna::detail {

FormatNatives installFormatsLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();

  auto jsonParse = std::make_shared<Function>();
  jsonParse->native = true;
  jsonParse->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"jsonParse expects one JSON string", {1, 1}, false, "K5100"});
    return parseJsonValue(std::get<std::string>(arguments[0].data), interpreter);
  };
  global->define("jsonParse", Value(jsonParse), false);

  auto jsonStringify = std::make_shared<Function>();
  jsonStringify->native = true;
  jsonStringify->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 1)
      throw KynaError({"jsonStringify expects one value", {1, 1}, false, "K5101"});
    return Value(stringifyJsonValue(arguments[0]));
  };
  global->define("jsonStringify", Value(jsonStringify), false);

  auto json = interpreter.heap().allocate();
  json->fields["parse"] = Value(jsonParse);
  json->fields["stringify"] = Value(jsonStringify);
  global->define("json", Value(json), false);

  auto tomlParse = std::make_shared<Function>();
  tomlParse->native = true;
  tomlParse->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"toml.parse expects one TOML string", {1, 1}, false, "KFORMAT1001"});
    const auto parsed = parseTomlDocument(std::get<std::string>(arguments[0].data));
    if (!parsed.valid)
      throw KynaError({parsed.failure.message, {1, 1}, false, parsed.failure.code});
    return formatValueToRuntime(parsed.value, interpreter.heap());
  };
  global->define("tomlParse", Value(tomlParse), false);

  auto tomlStringify = std::make_shared<Function>();
  tomlStringify->native = true;
  tomlStringify->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 1)
      throw KynaError({"toml.stringify expects one object", {1, 1}, false, "KFORMAT1002"});
    std::string conversionError;
    auto converted = runtimeValueToFormat(arguments[0], conversionError);
    if (!converted)
      throw KynaError({std::move(conversionError), {1, 1}, false, "KFORMAT1002"});
    const auto encoded = stringifyTomlDocument(*converted);
    if (!encoded.valid)
      throw KynaError({encoded.failure.message, {1, 1}, false, encoded.failure.code});
    return Value(std::get<std::string>(encoded.value.data));
  };
  global->define("tomlStringify", Value(tomlStringify), false);
  auto toml = interpreter.heap().allocate();
  toml->fields["parse"] = Value(tomlParse);
  toml->fields["stringify"] = Value(tomlStringify);
  global->define("toml", Value(toml), false);

  auto xmlParse = std::make_shared<Function>();
  xmlParse->native = true;
  xmlParse->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"xml.parse expects one XML string", {1, 1}, false, "KFORMAT1101"});
    const auto parsed = parseXmlDocument(std::get<std::string>(arguments[0].data));
    if (!parsed.valid)
      throw KynaError({parsed.failure.message, {1, 1}, false, parsed.failure.code});
    return formatValueToRuntime(parsed.value, interpreter.heap());
  };
  global->define("xmlParse", Value(xmlParse), false);

  auto xmlStringify = std::make_shared<Function>();
  xmlStringify->native = true;
  xmlStringify->nativeCall = [](const std::vector<Value> &arguments) {
    if (arguments.size() != 1)
      throw KynaError({"xml.stringify expects one XML node", {1, 1}, false, "KFORMAT1102"});
    std::string conversionError;
    auto converted = runtimeValueToFormat(arguments[0], conversionError);
    if (!converted)
      throw KynaError({std::move(conversionError), {1, 1}, false, "KFORMAT1102"});
    const auto encoded = stringifyXmlDocument(*converted);
    if (!encoded.valid)
      throw KynaError({encoded.failure.message, {1, 1}, false, encoded.failure.code});
    return Value(std::get<std::string>(encoded.value.data));
  };
  global->define("xmlStringify", Value(xmlStringify), false);
  auto xml = interpreter.heap().allocate();
  xml->fields["parse"] = Value(xmlParse);
  xml->fields["stringify"] = Value(xmlStringify);
  global->define("xml", Value(xml), false);

  return {jsonParse, jsonStringify};
}

} // namespace kyna::detail
