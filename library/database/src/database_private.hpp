#pragma once

#include <cstdint>
#include <string>

#include "kyna/diagnostics/diagnostic.hpp"
#include "kyna/execution/database_port.hpp"
#include "kyna/execution/runtime_object_model.hpp"

namespace kyna::detail {

KynaError databaseError(const DatabaseFailure &failure);

DatabaseScalar databaseScalar(const Value &value);

Value runtimeScalar(const DatabaseScalar &value);

} // namespace kyna::detail
