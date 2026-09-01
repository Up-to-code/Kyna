#include "catalog_private.hpp"
#include "kyna/execution/tree_walk_engine.hpp"
#include <chrono>
#include <string>

namespace kyna::detail {

ProcessHostNatives installProcessHostLibrary(Interpreter &interpreter) {
  auto global = interpreter.globals();
  auto capabilities = interpreter.runtimeCapabilities();

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

  return {run, environment};
}

} // namespace kyna::detail
