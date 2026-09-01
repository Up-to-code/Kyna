#pragma once

#include "kyna/execution/runtime_capabilities.hpp"
#include "kyna/execution/runtime_object_model.hpp"

namespace kyna {
class Interpreter;
}

namespace kyna::detail {

FunctionPtr createHttpServerFunction(Interpreter &interpreter, RuntimeCapabilities capabilities);

} // namespace kyna::detail
