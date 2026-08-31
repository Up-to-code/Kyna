#include "format_value_codec.hpp"
#include "json_value_codec.hpp"
#include "kyna/formats/document_formats.hpp"
#include "kyna/semantics/modifier_query.hpp"
#include "kyna/execution/runtime_capabilities.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include "kyna/stdlib/collections_library.hpp"
#include "kyna/stdlib/database_library.hpp"
#include "kyna/stdlib/standard_library_catalog.hpp"
#include "kyna/text/unicode_text.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

namespace kyna {
namespace {
std::string safeEndpoint(const std::string &url) {
  const auto query = url.find('?');
  return query == std::string::npos ? url : url.substr(0, query) + "?<redacted>";
}

KynaError networkError(const NetworkRequest &request, const NetworkFailure &failure) {
  Diagnostic diagnostic{request.method + " " + safeEndpoint(request.url) + ": " +
                            networkFailurePhaseName(failure.phase) + " error: " + failure.message,
                        {}, false, "KNET2001"};
  diagnostic.category = "network";
  diagnostic.causes.push_back(
      {"libcurl", std::to_string(failure.nativeCode), failure.message});
  diagnostic.notes.push_back(failure.retryable ? "the transport classified this failure as retryable"
                                               : "the transport classified this failure as non-retryable");
  diagnostic.help =
      "check the endpoint, DNS, proxy, certificate trust, and network access; use --trace for native details";
  return KynaError(diagnostic);
}

struct ServerRoute {
  std::string method;
  std::string pattern;
  FunctionPtr handler;
};

std::optional<std::map<std::string, std::string>> matchRoute(std::string_view pattern,
                                                             std::string_view path) {
  std::map<std::string, std::string> parameters;
  std::istringstream patterns{std::string(pattern)};
  std::istringstream paths{std::string(path)};
  std::string expected, actual;
  while (true) {
    const bool hasExpected = static_cast<bool>(std::getline(patterns, expected, '/'));
    const bool hasActual = static_cast<bool>(std::getline(paths, actual, '/'));
    if (hasExpected != hasActual) return std::nullopt;
    if (!hasExpected) break;
    if (!expected.empty() && expected.front() == ':') parameters.emplace(expected.substr(1), actual);
    else if (expected != actual) return std::nullopt;
  }
  return parameters;
}

ObjectPtr mapObject(Interpreter &interpreter, const std::map<std::string, std::string> &values) {
  auto object = interpreter.heap().allocate();
  for (const auto &[name, value] : values) object->fields[name] = Value(value);
  return object;
}

HttpOutgoingResponse runtimeHttpResponse(const Value &value) {
  if (!std::holds_alternative<ObjectPtr>(value.data))
    return {200, value.display(), {{"content-type", "text/plain; charset=utf-8"}}};
  const auto object = std::get<ObjectPtr>(value.data);
  HttpOutgoingResponse response;
  if (const auto status = object->fields.find("status"); status != object->fields.end() &&
      std::holds_alternative<std::int64_t>(status->second.data))
    response.status = static_cast<int>(std::get<std::int64_t>(status->second.data));
  if (const auto body = object->fields.find("body"); body != object->fields.end())
    response.body = std::holds_alternative<std::string>(body->second.data)
                        ? std::get<std::string>(body->second.data) : body->second.display();
  if (const auto headers = object->fields.find("headers"); headers != object->fields.end() &&
      std::holds_alternative<ObjectPtr>(headers->second.data))
    for (const auto &[name, header] : std::get<ObjectPtr>(headers->second.data)->fields)
      if (std::holds_alternative<std::string>(header.data)) response.headers[name] = std::get<std::string>(header.data);
  return response;
}

} // namespace

void installStandardLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();
  auto capabilities = interpreter.runtimeCapabilities();
  installCollectionsLibrary(interpreter);
  installDatabaseLibrary(interpreter);

  auto print = std::make_shared<Function>();
  print->native = true;
  print->nativeCall = [](const std::vector<Value> &a) {
    for (size_t i = 0; i < a.size(); ++i) {
      if (i)
        std::cout << ' ';
      std::cout << a[i].display();
    }
    std::cout << '\n';
    return Value();
  };
  global->define("print", Value(print), false);
  global->define("log", Value(print), false);
  auto console = interpreter.heap().allocate();
  console->fields["log"] = Value(print);
  auto consoleValue = Value(console);
  global->define("console", std::move(consoleValue), false);
  auto colorLog = std::make_shared<Function>();
  colorLog->native = true;
  colorLog->nativeCall = [](const std::vector<Value> &a) {
    if (a.size() != 2 || !std::holds_alternative<std::string>(a[0].data) ||
        !std::holds_alternative<std::string>(a[1].data))
      throw KynaError({"logColor expects a color and message", {1, 1}, false});
    static const std::map<std::string, std::string> colors = {
        {"black", "30"},   {"red", "31"},  {"green", "32"}, {"yellow", "33"}, {"blue", "34"},
        {"magenta", "35"}, {"cyan", "36"}, {"white", "37"}, {"reset", "0"}};
    auto found = colors.find(std::get<std::string>(a[0].data));
    if (found == colors.end())
      throw KynaError({"unknown log color", {1, 1}, false});
    std::cout << "\033[" << found->second << "m" << std::get<std::string>(a[1].data) << "\033[0m\n";
    return Value();
  };
  global->define("logColor", Value(colorLog), false);
  auto raise = std::make_shared<Function>();
  raise->native = true;
  raise->nativeCall = [](const std::vector<Value> &a) -> Value {
    if (a.size() != 1)
      throw KynaError({"error expects one message", {1, 1}, false});
    throw KynaError({a[0].display(), {1, 1}, false});
  };
  global->define("error", Value(raise), false);
  auto type = std::make_shared<Function>();
  type->native = true;
  type->nativeCall = [](const std::vector<Value> &a) {
    return a.empty() ? Value(std::string("void")) : Value(a[0].typeName());
  };
  global->define("typeOf", Value(type), false);
  auto collect = std::make_shared<Function>();
  collect->native = true;
  collect->nativeCall = [&interpreter](const std::vector<Value> &) {
    interpreter.heap().collect(interpreter.rootEnvironments());
    return Value();
  };
  global->define("collectGarbage", Value(collect), false);
  auto stats = std::make_shared<Function>();
  stats->native = true;
  stats->nativeCall = [&interpreter, global](const std::vector<Value> &) {
    const auto snapshot = interpreter.heap().stats();
    return Value(std::string("heap: live=") + std::to_string(snapshot.live) +
                 " allocated=" + std::to_string(snapshot.allocated) +
                 " reclaimed=" + std::to_string(snapshot.reclaimed) +
                 " collections=" + std::to_string(snapshot.collections) +
                 " objects=" + std::to_string(snapshot.objects) +
                 " arrays=" + std::to_string(snapshot.arrays) +
                 " captures=" + std::to_string(snapshot.captureCells) +
                 " closures=" + std::to_string(snapshot.closures) +
                 " bound-methods=" + std::to_string(snapshot.boundMethods) +
                 " errors=" + std::to_string(snapshot.errors));
  };
  global->define("gcStats", Value(stats), false);
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
  auto read = std::make_shared<Function>();
  read->native = true;
  read->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<std::string>(a[0].data))
      throw KynaError({"readFile expects a path", {1, 1}, false});
    std::string error;
    auto contents = capabilities.files->read(std::get<std::string>(a[0].data), error);
    if (!contents)
      throw KynaError({std::move(error), {1, 1}, false});
    return Value(std::move(*contents));
  };
  global->define("readFile", Value(read), false);
  auto write = std::make_shared<Function>();
  write->native = true;
  write->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 2 || !std::holds_alternative<std::string>(a[0].data) ||
        !std::holds_alternative<std::string>(a[1].data))
      throw KynaError({"writeFile expects path and string content", {1, 1}, false});
    std::string error;
    if (!capabilities.files->write(std::get<std::string>(a[0].data),
                                   std::get<std::string>(a[1].data), error))
      throw KynaError({std::move(error), {1, 1}, false});
    return Value();
  };
  global->define("writeFile", Value(write), false);

  auto createDirectory = std::make_shared<Function>();
  createDirectory->native = true;
  createDirectory->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"createDirectory expects one path", {1, 1}, false, "K5200"});
    std::string error;
    if (!capabilities.files->createDirectories(std::get<std::string>(arguments[0].data), error))
      throw KynaError({std::move(error), {1, 1}, false, "K5200"});
    return Value(true);
  };
  global->define("createDirectory", Value(createDirectory), false);

  auto fileExists = std::make_shared<Function>();
  fileExists->native = true;
  fileExists->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"fileExists expects one path", {1, 1}, false, "K5201"});
    std::string error;
    const bool found = capabilities.files->exists(std::get<std::string>(arguments[0].data), error);
    if (!error.empty())
      throw KynaError({std::move(error), {1, 1}, false, "K5201"});
    return Value(found);
  };
  global->define("fileExists", Value(fileExists), false);

  auto removePath = std::make_shared<Function>();
  removePath->native = true;
  removePath->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError(
          {"removePath expects one file or empty-directory path", {1, 1}, false, "K5202"});
    std::string error;
    const bool removed =
        capabilities.files->remove(std::get<std::string>(arguments[0].data), error);
    if (!error.empty())
      throw KynaError({std::move(error), {1, 1}, false, "K5202"});
    return Value(removed);
  };
  global->define("removePath", Value(removePath), false);

  auto listDirectory = std::make_shared<Function>();
  listDirectory->native = true;
  listDirectory->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"listDirectory expects one directory path", {1, 1}, false, "K5203"});
    std::string error;
    auto names = capabilities.files->list(std::get<std::string>(arguments[0].data), error);
    if (!names)
      throw KynaError({std::move(error), {1, 1}, false, "K5203"});
    auto result = interpreter.heap().allocateArray();
    for (auto &name : *names)
      result->elements.emplace_back(std::move(name));
    return Value(result);
  };
  global->define("listDirectory", Value(listDirectory), false);

  auto readJsonFile = std::make_shared<Function>();
  readJsonFile->native = true;
  readJsonFile->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"readJsonFile expects one path", {1, 1}, false, "K5204"});
    std::string error;
    auto contents = capabilities.files->read(std::get<std::string>(arguments[0].data), error);
    if (!contents)
      throw KynaError({std::move(error), {1, 1}, false, "K5204"});
    return parseJsonValue(*contents, interpreter);
  };
  global->define("readJsonFile", Value(readJsonFile), false);

  auto writeJsonFile = std::make_shared<Function>();
  writeJsonFile->native = true;
  writeJsonFile->nativeCall = [capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"writeJsonFile expects a path and value", {1, 1}, false, "K5205"});
    std::string error;
    if (!capabilities.files->write(std::get<std::string>(arguments[0].data),
                                   stringifyJsonValue(arguments[1]), error))
      throw KynaError({std::move(error), {1, 1}, false, "K5205"});
    return Value(true);
  };
  global->define("writeJsonFile", Value(writeJsonFile), false);

  auto fileSystem = interpreter.heap().allocate();
  fileSystem->fields["read"] = Value(read);
  fileSystem->fields["write"] = Value(write);
  fileSystem->fields["readJson"] = Value(readJsonFile);
  fileSystem->fields["writeJson"] = Value(writeJsonFile);
  fileSystem->fields["createDirectory"] = Value(createDirectory);
  fileSystem->fields["exists"] = Value(fileExists);
  fileSystem->fields["remove"] = Value(removePath);
  fileSystem->fields["list"] = Value(listDirectory);
  global->define("fs", Value(fileSystem), false);

  auto run = std::make_shared<Function>();
  run->native = true;
  run->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<std::string>(a[0].data))
      throw KynaError({"processRun expects a shell command string", {1, 1}, false});
    return Value(
        static_cast<int64_t>(capabilities.processes->run(std::get<std::string>(a[0].data))));
  };
  global->define("processRun", Value(run), false);
  global->define("build", Value(run), false);
  auto environment = std::make_shared<Function>();
  environment->native = true;
  environment->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<std::string>(a[0].data))
      throw KynaError({"processEnv expects a variable name", {1, 1}, false});
    auto value = capabilities.processes->environment(std::get<std::string>(a[0].data));
    return value ? Value(std::move(*value)) : Value();
  };
  global->define("processEnv", Value(environment), false);

  const auto makeHostFunction = [capabilities](std::string name) {
    auto function = std::make_shared<Function>();
    function->native = true;
    function->nativeCall = [capabilities, name = std::move(name)](
                               const std::vector<Value> &arguments) -> Value {
      if (!arguments.empty())
        throw KynaError({name + " expects no arguments", {1, 1}, false, "KHOST2000"});
      if (!capabilities.host)
        throw KynaError(
            {"host information capability is unavailable", {1, 1}, false, "KHOST2000"});
      if (name == "osName")
        return Value(capabilities.host->operatingSystem());
      if (name == "osArchitecture")
        return Value(capabilities.host->architecture());
      if (name == "terminalIsInteractive")
        return Value(capabilities.host->standardOutputIsTerminal());
      if (name == "terminalSupportsColor")
        return Value(capabilities.host->supportsColor());
      std::string message;
      auto directory = capabilities.host->workingDirectory(message);
      if (!directory)
        throw KynaError({std::move(message), {1, 1}, false, "KHOST2001"});
      return Value(std::move(*directory));
    };
    return function;
  };
  auto osName = makeHostFunction("osName");
  auto osArchitecture = makeHostFunction("osArchitecture");
  auto osWorkingDirectory = makeHostFunction("osWorkingDirectory");
  auto terminalIsInteractive = makeHostFunction("terminalIsInteractive");
  auto terminalSupportsColor = makeHostFunction("terminalSupportsColor");
  global->define("osName", Value(osName), false);
  global->define("osArchitecture", Value(osArchitecture), false);
  global->define("osWorkingDirectory", Value(osWorkingDirectory), false);
  global->define("terminalIsInteractive", Value(terminalIsInteractive), false);
  global->define("terminalSupportsColor", Value(terminalSupportsColor), false);
  auto os = interpreter.heap().allocate();
  os->fields["name"] = Value(osName);
  os->fields["architecture"] = Value(osArchitecture);
  os->fields["cwd"] = Value(osWorkingDirectory);
  global->define("os", Value(os), false);
  auto terminal = interpreter.heap().allocate();
  terminal->fields["interactive"] = Value(terminalIsInteractive);
  terminal->fields["supportsColor"] = Value(terminalSupportsColor);
  global->define("terminal", Value(terminal), false);

  auto sleep = std::make_shared<Function>();
  sleep->native = true;
  sleep->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<int64_t>(a[0].data))
      throw KynaError({"sleep expects milliseconds", {1, 1}, false});
    capabilities.clock->sleep(std::chrono::milliseconds(std::get<int64_t>(a[0].data)));
    return Value();
  };
  global->define("sleep", Value(sleep), false);
  global->define("wait", Value(sleep), false);
  auto get = std::make_shared<Function>();
  get->native = true;
  get->nativeCall = [capabilities](const std::vector<Value> &a) {
    if (a.size() != 1 || !std::holds_alternative<std::string>(a[0].data))
      throw KynaError({"httpGet expects a URL string", {1, 1}, false});
    NetworkRequest request{"GET", std::get<std::string>(a[0].data), std::nullopt, {},
                           std::chrono::milliseconds(30000)};
    NetworkFailure failure;
    auto response = capabilities.network->send(request, failure);
    if (!response)
      throw networkError(request, failure);
    return Value(std::move(response->body));
  };
  global->define("httpGet", Value(get), false);

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

  auto fetch = std::make_shared<Function>();
  fetch->native = true;
  fetch->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2 ||
        !std::holds_alternative<std::string>(arguments[0].data) ||
        (arguments.size() == 2 && !std::holds_alternative<ObjectPtr>(arguments[1].data)))
      throw KynaError({"fetch expects a URL and optional request-options object", {1, 1}, false});
    const auto url = std::get<std::string>(arguments[0].data);
    std::string method = "GET";
    std::optional<std::string> requestBody;
    std::chrono::milliseconds timeout{30000};
    if (arguments.size() == 2) {
      const auto options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto found = options->fields.find("method"); found != options->fields.end()) {
        if (!std::holds_alternative<std::string>(found->second.data))
          throw KynaError({"fetch option 'method' must be a string", {1, 1}, false});
        method = std::get<std::string>(found->second.data);
        std::transform(method.begin(), method.end(), method.begin(),
                       [](unsigned char character) { return std::toupper(character); });
      }
      if (const auto found = options->fields.find("body"); found != options->fields.end()) {
        if (!std::holds_alternative<std::string>(found->second.data))
          throw KynaError({"fetch option 'body' must be a JSON string", {1, 1}, false});
        requestBody = std::get<std::string>(found->second.data);
      }
      if (const auto found = options->fields.find("timeout"); found != options->fields.end()) {
        if (!std::holds_alternative<int64_t>(found->second.data) ||
            std::get<int64_t>(found->second.data) <= 0)
          throw KynaError({"fetch option 'timeout' must be a positive integer in milliseconds",
                           {}, false, "KNET1002"});
        timeout = std::chrono::milliseconds(std::get<int64_t>(found->second.data));
      }
    }
    std::map<std::string, std::string> requestHeaders;
    if (arguments.size() == 2) {
      const auto options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto found = options->fields.find("headers"); found != options->fields.end()) {
        if (!std::holds_alternative<ObjectPtr>(found->second.data))
          throw KynaError({"fetch option 'headers' must be an object", {}, false, "KNET1003"});
        for (const auto &[name, value] : std::get<ObjectPtr>(found->second.data)->fields) {
          if (!std::holds_alternative<std::string>(value.data))
            throw KynaError({"fetch header '" + name + "' must have a string value", {}, false,
                             "KNET1003"});
          requestHeaders.insert_or_assign(name, std::get<std::string>(value.data));
        }
      }
    }
    NetworkRequest request{method, url, requestBody, std::move(requestHeaders), timeout};
    NetworkFailure failure;
    auto networkResponse = capabilities.network->send(request, failure);
    if (!networkResponse)
      throw networkError(request, failure);
    auto response = interpreter.heap().allocate();
    response->fields["ok"] = Value(networkResponse->ok());
    response->fields["status"] = Value(static_cast<std::int64_t>(networkResponse->status));
    response->fields["url"] = Value(networkResponse->effectiveUrl.empty()
                                                ? url
                                                : networkResponse->effectiveUrl);
    response->fields["method"] = Value(method);
    auto responseHeaderObject = interpreter.heap().allocate();
    for (const auto &[name, value] : networkResponse->headers)
      responseHeaderObject->fields.insert_or_assign(name, Value(value));
    response->fields["headers"] = Value(responseHeaderObject);
    auto textMethod = std::make_shared<Function>();
    textMethod->native = true;
    textMethod->nativeCall =
        [contents = networkResponse->body](const std::vector<Value> &methodArguments) {
      if (!methodArguments.empty())
        throw KynaError({"response.text expects no arguments", {1, 1}, false});
      return Value(contents);
    };
    response->fields["text"] = Value(textMethod);
    auto jsonMethod = std::make_shared<Function>();
    jsonMethod->native = true;
    jsonMethod->nativeCall =
        [&interpreter, contents = std::move(networkResponse->body)](
            const std::vector<Value> &methodArguments) {
          if (!methodArguments.empty())
            throw KynaError({"response.json expects no arguments", {1, 1}, false});
          return parseJsonValue(contents, interpreter);
        };
    response->fields["json"] = Value(jsonMethod);
    return Value(response);
  };
  global->define("fetch", Value(fetch), false);

  auto fetchResult = std::make_shared<Function>();
  fetchResult->native = true;
  fetchResult->nativeCall = [&interpreter, fetch](const std::vector<Value> &arguments) {
    try {
      Value response = fetch->nativeCall(arguments);
      auto roots = interpreter.heap().rootScope();
      roots.protect(response);
      auto result = interpreter.heap().allocate();
      bool responseOk = true;
      if (const auto object = std::get_if<ObjectPtr>(&response.data); object && *object) {
        if (const auto found = (*object)->fields.find("ok");
            found != (*object)->fields.end() && std::holds_alternative<bool>(found->second.data))
          responseOk = std::get<bool>(found->second.data);
      }
      result->fields["ok"] = Value(responseOk);
      result->fields["response"] = response;
      result->fields["error"] = Value();
      return Value(result);
    } catch (const KynaError &failure) {
      Value error(interpreter.heap().allocateError(failure.diagnostic.message,
                                                    failure.diagnostic.code, Value()));
      auto roots = interpreter.heap().rootScope();
      roots.protect(error);
      auto result = interpreter.heap().allocate();
      result->fields["ok"] = Value(false);
      result->fields["response"] = Value();
      result->fields["error"] = error;
      return Value(result);
    }
  };
  global->define("fetchResult", Value(fetchResult), false);

  auto http = interpreter.heap().allocate();
  http->fields["fetch"] = Value(fetch);
  http->fields["tryFetch"] = Value(fetchResult);

  auto responseHelper = std::make_shared<Function>();
  responseHelper->native = true;
  responseHelper->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2)
      throw KynaError({"http.response expects a body and optional options object", {}, false, "KHTTP1001"});
    auto response = interpreter.heap().allocate();
    response->fields["body"] = arguments[0]; response->fields["status"] = Value(std::int64_t{200});
    response->fields["headers"] = Value(interpreter.heap().allocate());
    if (arguments.size() == 2) {
      if (!std::holds_alternative<ObjectPtr>(arguments[1].data))
        throw KynaError({"http.response options must be an object", {}, false, "KHTTP1001"});
      const auto options = std::get<ObjectPtr>(arguments[1].data);
      if (const auto status = options->fields.find("status"); status != options->fields.end()) response->fields["status"] = status->second;
      if (const auto headers = options->fields.find("headers"); headers != options->fields.end()) response->fields["headers"] = headers->second;
    }
    return Value(response);
  };
  auto jsonResponse = std::make_shared<Function>();
  jsonResponse->native = true;
  jsonResponse->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2)
      throw KynaError({"http.json expects a value and optional status", {}, false, "KHTTP1002"});
    auto response = interpreter.heap().allocate(); response->fields["body"] = Value(stringifyJsonValue(arguments[0]));
    response->fields["status"] = arguments.size() == 2 ? arguments[1] : Value(std::int64_t{200});
    auto headers = interpreter.heap().allocate(); headers->fields["content-type"] = Value(std::string("application/json; charset=utf-8"));
    response->fields["headers"] = Value(headers); return Value(response);
  };
  auto redirect = std::make_shared<Function>();
  redirect->native = true;
  redirect->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.empty() || arguments.size() > 2 || !std::holds_alternative<std::string>(arguments[0].data))
      throw KynaError({"http.redirect expects a URL and optional status", {}, false, "KHTTP1003"});
    auto response = interpreter.heap().allocate(); response->fields["body"] = Value(std::string{});
    response->fields["status"] = arguments.size() == 2 ? arguments[1] : Value(std::int64_t{302});
    auto headers = interpreter.heap().allocate(); headers->fields["location"] = arguments[0]; response->fields["headers"] = Value(headers);
    return Value(response);
  };
  auto createServer = std::make_shared<Function>();
  createServer->native = true;
  createServer->nativeCall = [&interpreter, capabilities](const std::vector<Value> &arguments) {
    if (arguments.size() > 1 || (!arguments.empty() && !std::holds_alternative<ObjectPtr>(arguments[0].data)))
      throw KynaError({"http.server expects an optional options object", {}, false, "KHTTP1100"});
    HttpServerOptions serverOptions;
    if (!arguments.empty()) {
      const auto options = std::get<ObjectPtr>(arguments[0].data);
      if (const auto host = options->fields.find("host"); host != options->fields.end() && std::holds_alternative<std::string>(host->second.data)) serverOptions.host = std::get<std::string>(host->second.data);
      if (const auto port = options->fields.find("port"); port != options->fields.end() && std::holds_alternative<std::int64_t>(port->second.data)) serverOptions.port = static_cast<std::uint16_t>(std::get<std::int64_t>(port->second.data));
    }
    if (capabilities.processes) {
      if (const auto host = capabilities.processes->environment("KYNA_SERVER_HOST")) serverOptions.host = *host;
      if (const auto port = capabilities.processes->environment("KYNA_SERVER_PORT")) {
        try { const auto parsed = std::stoul(*port); if (parsed > 0 && parsed <= 65535) serverOptions.port = static_cast<std::uint16_t>(parsed); }
        catch (...) { throw KynaError({"KYNA_SERVER_PORT must be an integer from 1 to 65535", {}, false, "KHTTP1104"}); }
      }
    }
    auto routes = std::make_shared<std::vector<ServerRoute>>(); auto server = interpreter.heap().allocate();
    const auto addRoute = [routes](std::string method) {
      auto function = std::make_shared<Function>(); function->native = true;
      function->nativeCall = [routes, method = std::move(method)](const std::vector<Value> &values) {
        if (values.size() != 2 || !std::holds_alternative<std::string>(values[0].data) || !std::holds_alternative<FunctionPtr>(values[1].data))
          throw KynaError({"route registration expects a path and handler", {}, false, "KHTTP1101"});
        routes->push_back({method, std::get<std::string>(values[0].data), std::get<FunctionPtr>(values[1].data)}); return Value();
      }; return function;
    };
    for (const auto &method : {"GET", "POST", "PUT", "PATCH", "DELETE"}) {
      std::string name(method); std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
      server->fields[name] = Value(addRoute(method));
    }
    auto use = std::make_shared<Function>(); use->native = true;
    use->nativeCall = [routes](const std::vector<Value> &values) {
      if (values.size() != 1 || !std::holds_alternative<FunctionPtr>(values[0].data))
        throw KynaError({"app.use expects a middleware function", {}, false, "KHTTP1102"});
      routes->push_back({"*", "*", std::get<FunctionPtr>(values[0].data)}); return Value();
    }; server->fields["use"] = Value(use);
    auto listen = std::make_shared<Function>(); listen->native = true;
    listen->nativeCall = [&interpreter, capabilities, routes, serverOptions](const std::vector<Value> &values) {
      if (!values.empty()) throw KynaError({"app.listen expects no arguments", {}, false, "KHTTP1103"});
      if (!capabilities.server) throw KynaError({"the host has not provided HTTP server capability", {}, false, "KHTTP2000"});
      std::string failure;
      const bool ok = capabilities.server->listen(serverOptions, [&interpreter, routes](const HttpIncomingRequest &incoming) {
        const auto queryStart = incoming.target.find('?'); const auto path = incoming.target.substr(0, queryStart);
        std::map<std::string, std::string> query;
        if (queryStart != std::string::npos) {
          std::istringstream pairs(incoming.target.substr(queryStart + 1)); std::string pair;
          while (std::getline(pairs, pair, '&')) { const auto split = pair.find('='); query[pair.substr(0, split)] = split == std::string::npos ? "" : pair.substr(split + 1); }
        }
        auto request = interpreter.heap().allocate(); request->fields["method"] = Value(incoming.method); request->fields["path"] = Value(path);
        request->fields["query"] = Value(mapObject(interpreter, query)); request->fields["headers"] = Value(mapObject(interpreter, incoming.headers)); request->fields["body"] = Value(incoming.body);
        auto text = std::make_shared<Function>(); text->native = true; text->nativeCall = [body = incoming.body](const std::vector<Value> &args) { if (!args.empty()) throw KynaError({"request.text expects no arguments", {}, false}); return Value(body); };
        auto jsonBody = std::make_shared<Function>(); jsonBody->native = true; jsonBody->nativeCall = [&interpreter, body = incoming.body](const std::vector<Value> &args) { if (!args.empty()) throw KynaError({"request.json expects no arguments", {}, false}); return parseJsonValue(body, interpreter); };
        request->fields["text"] = Value(text); request->fields["json"] = Value(jsonBody);
        for (const auto &route : *routes) {
          if (route.method == "*") { const auto value = interpreter.invoke(route.handler, {Value(request)}); if (!std::holds_alternative<std::nullptr_t>(value.data)) return runtimeHttpResponse(value); continue; }
          if (route.method != incoming.method) continue;
          const auto parameters = matchRoute(route.pattern, path); if (!parameters) continue;
          request->fields["params"] = Value(mapObject(interpreter, *parameters)); return runtimeHttpResponse(interpreter.invoke(route.handler, {Value(request)}));
        }
        return HttpOutgoingResponse{404, "not found", {{"content-type", "text/plain; charset=utf-8"}}};
      }, failure);
      if (!ok) {
        Diagnostic diagnostic{failure, {}, false,
                              failure == "interrupted" ? "KHTTP0130" : "KHTTP2001"};
        diagnostic.category = "http";
        if (failure.find("Address already in use") != std::string::npos ||
            failure.find("address already in use") != std::string::npos) {
          diagnostic.help =
              "stop the existing Kyna Run/Dev task, or change [server].port in kyna.toml";
        }
        throw KynaError(diagnostic);
      }
      return Value();
    }; server->fields["listen"] = Value(listen); return Value(server);
  };
  http->fields["response"] = Value(responseHelper);
  http->fields["json"] = Value(jsonResponse);
  http->fields["redirect"] = Value(redirect);
  http->fields["server"] = Value(createServer);
  global->define("http", Value(http), false);

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

  auto process = interpreter.heap().allocate();
  process->fields["json"] = Value(jsonParse);
  process->fields["stringify"] = Value(jsonStringify);
  process->fields["run"] = Value(run);
  process->fields["env"] = Value(environment);
  global->define("process", Value(process), false);

  auto createApiStore = std::make_shared<Function>();
  createApiStore->native = true;
  createApiStore->nativeCall = [&interpreter](const std::vector<Value> &arguments) {
    if (arguments.size() != 1 || !std::holds_alternative<ArrayPtr>(arguments[0].data))
      throw KynaError({"createApiStore expects an initial record array", {1, 1}, false});
    auto store = interpreter.heap().allocate();
    store->fields["records"] = arguments[0];
    const auto records = std::get<ArrayPtr>(arguments[0].data);

    auto list = std::make_shared<Function>();
    list->native = true;
    list->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (!methodArguments.empty())
        throw KynaError({"store.list expects no arguments", {1, 1}, false});
      return Value(records);
    };
    store->fields["list"] = Value(list);

    auto getRecord = std::make_shared<Function>();
    getRecord->native = true;
    getRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 1)
        throw KynaError({"store.get expects an id", {1, 1}, false});
      for (const auto &record : records->elements)
        if (const auto object = std::get_if<ObjectPtr>(&record.data); object && *object) {
          const auto id = (*object)->fields.find("id");
          if (id != (*object)->fields.end() && id->second.equals(methodArguments[0]))
            return record;
        }
      return Value();
    };
    store->fields["get"] = Value(getRecord);

    auto createRecord = std::make_shared<Function>();
    createRecord->native = true;
    createRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 1 ||
          !std::holds_alternative<ObjectPtr>(methodArguments[0].data))
        throw KynaError({"store.create expects an object record", {1, 1}, false});
      records->elements.push_back(methodArguments[0]);
      return methodArguments[0];
    };
    store->fields["create"] = Value(createRecord);

    auto updateRecord = std::make_shared<Function>();
    updateRecord->native = true;
    updateRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 2 ||
          !std::holds_alternative<ObjectPtr>(methodArguments[1].data))
        throw KynaError({"store.update expects an id and patch object", {1, 1}, false});
      for (auto &record : records->elements)
        if (const auto object = std::get_if<ObjectPtr>(&record.data); object && *object) {
          const auto id = (*object)->fields.find("id");
          if (id == (*object)->fields.end() || !id->second.equals(methodArguments[0]))
            continue;
          for (const auto &[name, value] : std::get<ObjectPtr>(methodArguments[1].data)->fields)
            (*object)->fields.insert_or_assign(name, value);
          return record;
        }
      return Value();
    };
    store->fields["update"] = Value(updateRecord);

    auto removeRecord = std::make_shared<Function>();
    removeRecord->native = true;
    removeRecord->nativeCall = [records](const std::vector<Value> &methodArguments) {
      if (methodArguments.size() != 1)
        throw KynaError({"store.remove expects an id", {1, 1}, false});
      const auto before = records->elements.size();
      std::erase_if(records->elements, [&](const Value &record) {
        const auto object = std::get_if<ObjectPtr>(&record.data);
        if (!object || !*object)
          return false;
        const auto id = (*object)->fields.find("id");
        return id != (*object)->fields.end() && id->second.equals(methodArguments[0]);
      });
      return Value(records->elements.size() != before);
    };
    store->fields["remove"] = Value(removeRecord);
    return Value(store);
  };
  global->define("createApiStore", Value(createApiStore), false);
}
} // namespace kyna
