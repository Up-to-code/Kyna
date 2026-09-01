#include <string>

#include "kyna/text/unicode_text.hpp"

namespace kyna::detail {

UnicodeTextError invalidUtf8(std::size_t offset) {
  return {"KTEXT2001", "text contains invalid UTF-8 at byte " + std::to_string(offset), offset};
}

} // namespace kyna::detail
