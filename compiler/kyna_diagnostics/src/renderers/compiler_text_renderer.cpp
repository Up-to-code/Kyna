#include "kyna/diagnostics/diagnostic_renderer.hpp"
#include <algorithm>
#include <sstream>

namespace kyna {

std::string renderCompilerDiagnostics(const std::vector<Diagnostic> &diagnostics,
                                      const SourceManager &sources,
                                      DiagnosticRenderOptions options) {
  std::ostringstream output;
  for (const auto &diagnostic : diagnostics) {
    const auto *source = sources.find(diagnostic.location.source);
    const auto name = source ? source->displayName() : std::string("<source>");
    const auto severity = diagnostic.warning ? "warning" : "error";
    const auto prefix = options.color ? (diagnostic.warning ? "\033[33m" : "\033[31m") : "";
    const auto reset = options.color ? "\033[0m" : "";
    output << name << ':' << diagnostic.location.line << ':' << diagnostic.location.column << ": "
           << prefix << severity << '[' << diagnostic.code << "]" << reset << ": "
           << diagnostic.message << '\n';
    const auto sourceLine = sources.line(diagnostic.location.source, diagnostic.location.line);
    if (!sourceLine.empty()) {
      output << "  " << diagnostic.location.line << " | " << sourceLine << '\n' << "    | ";
      output << std::string(static_cast<std::size_t>(std::max(0, diagnostic.location.column - 1)),
                            ' ');
      const auto width =
          diagnostic.location.endLine == diagnostic.location.line
              ? std::max(1, diagnostic.location.endColumn - diagnostic.location.column)
              : 1;
      output << prefix << '^' << std::string(static_cast<std::size_t>(width - 1), '~') << reset
             << '\n';
    }
    for (const auto &note : diagnostic.notes)
      output << "    note: " << note << '\n';
    if (!diagnostic.help.empty())
      output << "    help: " << diagnostic.help << '\n';
    for (const auto &cause : diagnostic.causes)
      output << "    caused by: " << cause.domain << '[' << cause.code << "]: " << cause.message
             << '\n';
    for (const auto &label : diagnostic.labels) {
      const auto *labelSource = sources.find(label.span.source);
      output << "    related: "
             << (labelSource ? labelSource->displayName() : std::string("<source>")) << ':'
             << label.span.line << ':' << label.span.column << ": " << label.message << '\n';
    }
    for (const auto &frame : diagnostic.callFrames) {
      const auto *frameSource = sources.find(frame.span.source);
      output << "    at " << frame.function << " ("
             << (frameSource ? frameSource->displayName() : std::string("<source>")) << ':'
             << frame.span.line << ':' << frame.span.column << ")\n";
    }
  }
  return output.str();
}

} // namespace kyna
