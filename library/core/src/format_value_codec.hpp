#pragma once

#include "kyna/execution/runtime_value.hpp"
#include "kyna/formats/document_formats.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include <optional>
#include <string>

namespace kyna {

Value formatValueToRuntime(const FormatValue &value, Heap &heap);
std::optional<FormatValue> runtimeValueToFormat(const Value &value, std::string &error);

} // namespace kyna
