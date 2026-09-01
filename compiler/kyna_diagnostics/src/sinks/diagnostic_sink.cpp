#include "kyna/diagnostics/diagnostic_sink.hpp"
#include <algorithm>

namespace kyna {
bool DiagnosticSink::hasErrors() const {
  return std::any_of(entries.begin(), entries.end(),
                     [](const Diagnostic &entry) { return !entry.warning; });
}
} // namespace kyna
