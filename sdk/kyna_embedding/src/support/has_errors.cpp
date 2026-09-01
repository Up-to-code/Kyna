#include "../support_private.hpp"

#include <algorithm>

namespace kyna::detail {

bool hasErrors(const std::vector<Diagnostic> &diagnostics) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const Diagnostic &diagnostic) { return !diagnostic.warning; });
}

} // namespace kyna::detail
