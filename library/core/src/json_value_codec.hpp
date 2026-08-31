#pragma once

#include "kyna/execution/runtime_object_model.hpp"
#include "kyna/memory/tracing_heap.hpp"
#include <string>
#include <string_view>

namespace kyna {

class Interpreter;

Value parseJsonValue(std::string_view source, Interpreter &interpreter);
Value parseJsonValue(std::string_view source, Heap &heap);
std::string stringifyJsonValue(const Value &value);

} // namespace kyna
