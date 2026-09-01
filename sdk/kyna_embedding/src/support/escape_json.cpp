#include "../support_private.hpp"

#include <sstream>

namespace kyna::detail {

std::string escapeJson(std::string_view value) {
  std::ostringstream output;
  for (const char character : value) {
    switch (character) {
    case '\\':
      output << "\\\\";
      break;
    case '"':
      output << "\\\"";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << character;
      break;
    }
  }
  return output.str();
}

} // namespace kyna::detail
