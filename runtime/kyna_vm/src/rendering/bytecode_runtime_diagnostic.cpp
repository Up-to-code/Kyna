#include "bytecode_runtime_diagnostic.hpp"

namespace kyna {

Diagnostic makeBytecodeRuntimeDiagnostic(std::string code, std::string message,
                                         SourceSpan span, const BytecodeModule &module,
                                         const std::vector<BytecodeCallFrame> &frames) {
  Diagnostic diagnostic{std::move(message), span, false, std::move(code)};
  diagnostic.category = "runtime";
  for (std::size_t offset = 0; offset < frames.size(); ++offset) {
    const auto index = frames.size() - 1 - offset;
    const auto frameSpan = index + 1 == frames.size() ? span : frames[index + 1].callSite;
    diagnostic.callFrames.push_back({module.functions[frames[index].function].name, frameSpan});
  }
  return diagnostic;
}

} // namespace kyna
