#pragma once

#include "kyna/execution/runtime_value.hpp"
#include "kyna/source/source_span.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace kyna {

struct BytecodeCallFrame {
  std::uint32_t function{0};
  std::vector<RuntimeValue> registers;
  std::size_t instructionPointer{0};
  std::optional<std::uint32_t> returnDestination;
  std::optional<RuntimeValue> returnOverride;
  SourceSpan callSite;
  std::vector<VmCaptureCell *> captures;
  std::vector<VmCaptureCell *> registerCells;
};

} // namespace kyna
