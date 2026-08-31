#pragma once

#include "kyna/execution/bytecode_virtual_machine.hpp"
#include "kyna/execution/runtime_capabilities.hpp"
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace kyna {

[[nodiscard]] const std::vector<std::string> &bytecodeStandardLibraryFunctionNames();

[[nodiscard]] std::unique_ptr<BytecodeNativeAdapter>
createBytecodeStandardLibrary(RuntimeCapabilities capabilities, std::ostream &standardOutput);

} // namespace kyna
