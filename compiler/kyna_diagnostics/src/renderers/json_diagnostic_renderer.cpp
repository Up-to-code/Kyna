#include "kyna/diagnostics/diagnostic_renderer.hpp"
#include <sstream>

namespace kyna {
namespace {
std::string escapeJson(const std::string &value) {
  std::ostringstream escaped;
  for (const char character : value) {
    switch (character) {
    case '\\':
      escaped << "\\\\";
      break;
    case '"':
      escaped << "\\\"";
      break;
    case '\n':
      escaped << "\\n";
      break;
    case '\r':
      escaped << "\\r";
      break;
    case '\t':
      escaped << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20)
        escaped << "\\u00" << "0123456789abcdef"[(character >> 4) & 0x0f]
                << "0123456789abcdef"[character & 0x0f];
      else
        escaped << character;
      break;
    }
  }
  return escaped.str();
}
} // namespace

std::string renderJsonDiagnostics(const std::vector<Diagnostic> &diagnostics,
                                  const SourceManager &sources) {
  std::ostringstream output;
  output << "{\"schema\":\"kyna.diagnostic/v1\",\"version\":1,\"diagnostics\":[";
  for (std::size_t index = 0; index < diagnostics.size(); ++index) {
    if (index)
      output << ',';
    const auto &diagnostic = diagnostics[index];
    const auto *source = sources.find(diagnostic.location.source);
    output << "{\"code\":\"" << escapeJson(diagnostic.code) << "\",\"category\":\""
           << escapeJson(diagnostic.category) << "\",\"severity\":\""
           << (diagnostic.warning ? "warning" : "error") << "\",\"message\":\""
           << escapeJson(diagnostic.message) << "\",\"file\":\""
           << escapeJson(source ? source->displayName() : std::string("<source>"))
           << "\",\"range\":{\"start\":{\"line\":" << diagnostic.location.line
           << ",\"column\":" << diagnostic.location.column
           << "},\"end\":{\"line\":" << diagnostic.location.endLine
           << ",\"column\":" << diagnostic.location.endColumn << "},\"bytes\":{\"start\":"
           << diagnostic.location.startByte << ",\"end\":" << diagnostic.location.endByte
           << "}},\"labels\":[";
    for (std::size_t labelIndex = 0; labelIndex < diagnostic.labels.size(); ++labelIndex) {
      if (labelIndex)
        output << ',';
      const auto &label = diagnostic.labels[labelIndex];
      const auto *labelSource = sources.find(label.span.source);
      output << "{\"message\":\"" << escapeJson(label.message) << "\",\"file\":\""
             << escapeJson(labelSource ? labelSource->displayName() : std::string("<source>"))
             << "\",\"range\":{\"start\":{\"line\":" << label.span.line
             << ",\"column\":" << label.span.column << "},\"end\":{\"line\":" << label.span.endLine
             << ",\"column\":" << label.span.endColumn << "}}}";
    }
    output << "],\"notes\":[";
    for (std::size_t noteIndex = 0; noteIndex < diagnostic.notes.size(); ++noteIndex) {
      if (noteIndex)
        output << ',';
      output << '"' << escapeJson(diagnostic.notes[noteIndex]) << '"';
    }
    output << "],\"help\":\"" << escapeJson(diagnostic.help) << "\",\"causes\":[";
    for (std::size_t causeIndex = 0; causeIndex < diagnostic.causes.size(); ++causeIndex) {
      if (causeIndex)
        output << ',';
      const auto &cause = diagnostic.causes[causeIndex];
      output << "{\"domain\":\"" << escapeJson(cause.domain) << "\",\"code\":\""
             << escapeJson(cause.code) << "\",\"message\":\"" << escapeJson(cause.message)
             << "\"}";
    }
    output << "],\"callFrames\":[";
    for (std::size_t frameIndex = 0; frameIndex < diagnostic.callFrames.size(); ++frameIndex) {
      if (frameIndex)
        output << ',';
      const auto &frame = diagnostic.callFrames[frameIndex];
      const auto *frameSource = sources.find(frame.span.source);
      output << "{\"function\":\"" << escapeJson(frame.function) << "\",\"file\":\""
             << escapeJson(frameSource ? frameSource->displayName() : std::string("<source>"))
             << "\",\"line\":" << frame.span.line << ",\"column\":" << frame.span.column << '}';
    }
    output << "]}";
  }
  output << "]}";
  return output.str();
}

} // namespace kyna
