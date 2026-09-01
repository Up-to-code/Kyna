#pragma once

#include "kyna/bytecode/bytecode_module.hpp"
#include "kyna/diagnostics/diagnostic.hpp"
#include "../types/bytecode_call_frame.hpp"
#include <string>
#include <vector>

namespace kyna {

Diagnostic makeBytecodeRuntimeDiagnostic(std::string code, std::string message,
                                         SourceSpan span, const BytecodeModule &module,
                                         const std::vector<BytecodeCallFrame> &frames);

} // namespace kyna
