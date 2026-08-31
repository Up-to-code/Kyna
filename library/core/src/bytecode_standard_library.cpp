#include "kyna/stdlib/bytecode_standard_library.hpp"
#include <ostream>

namespace kyna {
namespace {

NativeCallResult failure(std::string code, std::string message, RuntimeValue cause = {}) {
  return {{}, NativeCallFailure{std::move(code), std::move(message), std::move(cause)}};
}

class StandardLibraryBytecodeAdapter final : public BytecodeNativeAdapter {
public:
  StandardLibraryBytecodeAdapter(RuntimeCapabilities hostCapabilities, std::ostream &output)
      : capabilities(std::move(hostCapabilities)), standardOutput(output) {}

  NativeCallResult invoke(std::string_view name, std::span<const RuntimeValue> arguments,
                          Heap &) override {
    if (name == "print" || name == "log") {
      for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index)
          standardOutput << ' ';
        standardOutput << arguments[index].display();
      }
      standardOutput << '\n';
      return {};
    }
    if (name == "typeOf") {
      if (arguments.size() != 1)
        return failure("KSTD2001", "typeOf expects exactly one argument");
      return {RuntimeValue(arguments.front().typeName()), std::nullopt};
    }
    if (name == "len") {
      if (arguments.size() != 1)
        return failure("KSTD2002", "len expects exactly one argument");
      const auto &value = arguments.front();
      if (const auto text = std::get_if<std::string>(&value.data))
        return {RuntimeValue(static_cast<std::int64_t>(text->size())), std::nullopt};
      if (const auto array = std::get_if<ArrayPtr>(&value.data); array && *array)
        return {RuntimeValue(static_cast<std::int64_t>((*array)->elements.size())), std::nullopt};
      if (const auto object = std::get_if<ObjectPtr>(&value.data); object && *object)
        return {RuntimeValue(static_cast<std::int64_t>((*object)->fields.size())), std::nullopt};
      return failure("KSTD2003", "len requires a string, array, or object", value);
    }
    if (name == "error") {
      if (arguments.size() != 1)
        return failure("KSTD2004", "error expects exactly one argument");
      return failure("KRT2300", arguments.front().display(), arguments.front());
    }
    if (name == "readFile") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2001", "readFile expects exactly one string path");
      std::string message;
      auto contents = capabilities.files->read(std::get<std::string>(arguments[0].data), message);
      return contents ? NativeCallResult{RuntimeValue(std::move(*contents)), std::nullopt}
                      : failure("KFS2001", std::move(message), arguments[0]);
    }
    if (name == "writeFile") {
      if (arguments.size() != 2 || !std::holds_alternative<std::string>(arguments[0].data) ||
          !std::holds_alternative<std::string>(arguments[1].data))
        return failure("KFS2002", "writeFile expects a string path and string content");
      std::string message;
      if (!capabilities.files->write(std::get<std::string>(arguments[0].data),
                                     std::get<std::string>(arguments[1].data), message))
        return failure("KFS2002", std::move(message), arguments[0]);
      return {RuntimeValue(true), std::nullopt};
    }
    if (name == "fileExists") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2003", "fileExists expects exactly one string path");
      std::string message;
      const auto exists =
          capabilities.files->exists(std::get<std::string>(arguments[0].data), message);
      if (!message.empty())
        return failure("KFS2003", std::move(message), arguments[0]);
      return {RuntimeValue(exists), std::nullopt};
    }
    if (name == "createDirectory") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KFS2004", "createDirectory expects exactly one string path");
      std::string message;
      if (!capabilities.files->createDirectories(std::get<std::string>(arguments[0].data),
                                                  message))
        return failure("KFS2004", std::move(message), arguments[0]);
      return {RuntimeValue(true), std::nullopt};
    }
    if (name == "processEnv") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KPROC2001", "processEnv expects exactly one variable name");
      auto value = capabilities.processes->environment(std::get<std::string>(arguments[0].data));
      return {value ? RuntimeValue(std::move(*value)) : RuntimeValue(), std::nullopt};
    }
    if (name == "sleep" || name == "wait") {
      if (arguments.size() != 1 || !std::holds_alternative<std::int64_t>(arguments[0].data))
        return failure("KTIME2001", std::string(name) + " expects integer milliseconds");
      const auto duration = std::get<std::int64_t>(arguments[0].data);
      if (duration < 0)
        return failure("KTIME2002", std::string(name) + " duration cannot be negative",
                       arguments[0]);
      capabilities.clock->sleep(std::chrono::milliseconds(duration));
      return {};
    }
    if (name == "httpGet") {
      if (arguments.size() != 1 || !std::holds_alternative<std::string>(arguments[0].data))
        return failure("KNET2000", "httpGet expects exactly one URL string");
      NetworkRequest request{"GET", std::get<std::string>(arguments[0].data), std::nullopt, {},
                             std::chrono::milliseconds(30000)};
      NetworkFailure networkFailure;
      auto response = capabilities.network->send(request, networkFailure);
      if (!response)
        return failure("KNET2001", "GET request failed during " +
                                       std::string(networkFailurePhaseName(networkFailure.phase)) +
                                       ": " + networkFailure.message,
                       arguments[0]);
      return {RuntimeValue(std::move(response->body)), std::nullopt};
    }
    return failure("KSTD2099", "unknown standard-library native '" + std::string(name) + "'");
  }

private:
  RuntimeCapabilities capabilities;
  std::ostream &standardOutput;
};

} // namespace

const std::vector<std::string> &bytecodeStandardLibraryFunctionNames() {
  static const std::vector<std::string> names{
      "print", "log", "typeOf", "len", "error", "readFile", "writeFile", "fileExists",
      "createDirectory", "processEnv", "sleep", "wait", "httpGet"};
  return names;
}

std::unique_ptr<BytecodeNativeAdapter>
createBytecodeStandardLibrary(RuntimeCapabilities capabilities, std::ostream &standardOutput) {
  return std::make_unique<StandardLibraryBytecodeAdapter>(std::move(capabilities), standardOutput);
}

} // namespace kyna
