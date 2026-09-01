#include <string_view>

#include <utf8proc.h>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna::detail {

bool isUnicodeSpace(std::int32_t codePoint) {
  if (codePoint == '\t' || codePoint == '\n' || codePoint == '\v' || codePoint == '\f' ||
      codePoint == '\r')
    return true;
  const auto category = utf8proc_category(codePoint);
  return category == UTF8PROC_CATEGORY_ZS || category == UTF8PROC_CATEGORY_ZL ||
         category == UTF8PROC_CATEGORY_ZP;
}

std::int32_t codePointAt(std::string_view text, const ByteOffsets &offsets, std::size_t index) {
  std::int32_t codePoint{};
  utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(text.data() + offsets[index]),
                   static_cast<utf8proc_ssize_t>(offsets[index + 1] - offsets[index]), &codePoint);
  return codePoint;
}

} // namespace kyna::detail
