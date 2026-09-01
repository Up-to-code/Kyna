#include "catalog_private.hpp"
#include <chrono>
#include <iostream>
#include <map>
#include <string>

namespace kyna::detail {

void installConsoleLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();

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
  auto strFn = std::make_shared<Function>();
  strFn->native = true;
  strFn->nativeCall = [](const std::vector<Value> &a) {
    if (a.size() != 1)
      throw KynaError({"str expects exactly one argument", {1, 1}, false});
    return Value(a[0].display());
  };
  global->define("toString", Value(strFn), false);
  auto clock = std::make_shared<Function>();
  clock->native = true;
  clock->nativeCall = [](const std::vector<Value> &a) {
    if (!a.empty())
      throw KynaError({"clockMs expects no arguments", {1, 1}, false});
    const auto now = std::chrono::steady_clock::now();
    return Value(static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                now.time_since_epoch())
                                                .count()));
  };
  global->define("clockMs", Value(clock), false);
  auto profileLog = std::make_shared<Function>();
  profileLog->native = true;
  profileLog->nativeCall = [](const std::vector<Value> &a) {
    if (a.size() != 2)
      throw KynaError({"profileLog expects a label and an elapsed value", {1, 1}, false});
    std::cout << "[profile] " << a[0].display() << ": " << a[1].display() << " ms\n";
    return Value();
  };
  global->define("profileLog", Value(profileLog), false);
  auto measure = std::make_shared<Function>();
  measure->native = true;
  measure->nativeCall = [&interpreter](const std::vector<Value> &a) {
    if (a.size() != 1)
      throw KynaError({"measure expects a zero-argument function", {1, 1}, false});
    const auto function = std::get_if<FunctionPtr>(&a[0].data);
    if (!function || !*function)
      throw KynaError({"measure expects a zero-argument function", {1, 1}, false});
    const auto start = std::chrono::steady_clock::now();
    interpreter.invoke(*function, {});
    const auto stop = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration<double, std::milli>(stop - start).count();
    return Value(ms);
  };
  global->define("measure", Value(measure), false);
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
}

} // namespace kyna::detail
