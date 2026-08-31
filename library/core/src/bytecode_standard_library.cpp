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
    return failure("KSTD2099", "unknown standard-library native '" + std::string(name) + "'");
  }

private:
  [[maybe_unused]] RuntimeCapabilities capabilities;
  std::ostream &standardOutput;
};

} // namespace

const std::vector<std::string> &bytecodeStandardLibraryFunctionNames() {
  static const std::vector<std::string> names{"print", "log", "typeOf", "len", "error"};
  return names;
}

std::unique_ptr<BytecodeNativeAdapter>
createBytecodeStandardLibrary(RuntimeCapabilities capabilities, std::ostream &standardOutput) {
  return std::make_unique<StandardLibraryBytecodeAdapter>(std::move(capabilities), standardOutput);
}

} // namespace kyna
