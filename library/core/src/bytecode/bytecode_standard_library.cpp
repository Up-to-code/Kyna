#include "kyna/stdlib/bytecode_standard_library.hpp"
#include "bytecode_private.hpp"
#include <memory>
#include <ostream>

namespace kyna {
namespace {

class StandardLibraryBytecodeAdapter final : public BytecodeNativeAdapter {
public:
  StandardLibraryBytecodeAdapter(RuntimeCapabilities hostCapabilities, std::ostream &output)
      : capabilities(std::move(hostCapabilities)), standardOutput(output) {}

  NativeCallResult invoke(std::string_view name, std::span<const RuntimeValue> arguments,
                          Heap &heap) override {
    return invokeWithCallbacks(name, arguments, heap, {});
  }

  NativeCallResult invokeWithCallbacks(std::string_view name,
      std::span<const RuntimeValue> arguments, Heap &heap, const NativeCallbacks &callbacks) override {
    detail::BytecodeAdapterContext context{capabilities, standardOutput, heap, &callbacks};
    if (auto result = detail::collectionCallbacksBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::consoleBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::textBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::filesystemBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::processHostBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::networkBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::formatsBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::collectionsBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    if (auto result = detail::cryptoBytecodeInvoke(name, arguments, context))
      return std::move(*result);
    return detail::bytecodeFailure("KSTD2099",
                                   "unknown standard-library native '" + std::string(name) + "'");
  }

private:
  RuntimeCapabilities capabilities;
  std::ostream &standardOutput;
};

} // namespace

const std::vector<std::string> &bytecodeStandardLibraryFunctionNames() {
  static const std::vector<std::string> names{
      "print", "log", "typeOf", "toString", "clockMs", "timeNow", "timeSleep", "profileLog", "len", "error", "readFile", "writeFile", "fileExists",
      "createDirectory", "removePath", "listDirectory", "readJsonFile", "writeJsonFile", "copyFile",
      "processEnv", "processRun", "build", "osName", "osArchitecture",
      "osWorkingDirectory", "terminalIsInteractive", "terminalSupportsColor",
      "sleep", "wait", "httpGet", "fetch",
      "fetchResult", "parseIP",
      "responseJson", "responseText", "jsonParse",
      "jsonStringify", "tomlParse", "tomlStringify", "xmlParse", "xmlStringify",
      "push", "pop", "keys", "unique", "sort", "bubbleSort", "map", "filter", "reduce",
      "find", "any", "all", "call", "measure", "collectGarbage", "gcStats", "logColor",
      "createQueue", "enqueue", "dequeue", "peekQueue", "queueIsEmpty",
      "textContains", "textFind", "textSlice", "textReplace", "textSplit", "textTrim",
      "textLower", "textUpper", "cryptoSha256", "slogInfo", "slogWarn", "slogError"};
  return names;
}

std::unique_ptr<BytecodeNativeAdapter>
createBytecodeStandardLibrary(RuntimeCapabilities capabilities, std::ostream &standardOutput) {
  return std::make_unique<StandardLibraryBytecodeAdapter>(std::move(capabilities), standardOutput);
}

} // namespace kyna
