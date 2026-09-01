#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include <utf8proc.h>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {
namespace {

UnicodeTextResult<std::string> mapCase(std::string_view text, bool upper) {
  auto offsets = detail::codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  std::string result;
  result.reserve(text.size());
  for (std::size_t index = 0; index + 1 < offsets->size(); ++index) {
    std::int32_t codePoint{};
    utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(text.data() + (*offsets)[index]),
                     static_cast<utf8proc_ssize_t>((*offsets)[index + 1] - (*offsets)[index]),
                     &codePoint);
    codePoint = upper ? utf8proc_toupper(codePoint) : utf8proc_tolower(codePoint);
    utf8proc_uint8_t encoded[4]{};
    const auto width = utf8proc_encode_char(codePoint, encoded);
    result.append(reinterpret_cast<const char *>(encoded), static_cast<std::size_t>(width));
  }
  return result;
}

} // namespace

UnicodeTextResult<std::string> unicodeLower(std::string_view text) {
  return mapCase(text, false);
}

UnicodeTextResult<std::string> unicodeUpper(std::string_view text) {
  return mapCase(text, true);
}

} // namespace kyna
