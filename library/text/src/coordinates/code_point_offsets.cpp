#include <cstddef>
#include <expected>
#include <string_view>
#include <vector>

#include <utf8proc.h>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna::detail {

UnicodeTextResult<ByteOffsets> codePointOffsets(std::string_view text) {
  ByteOffsets offsets;
  offsets.reserve(text.size() + 1);
  std::size_t offset = 0;
  while (offset < text.size()) {
    offsets.push_back(offset);
    std::int32_t codePoint{};
    const auto width = utf8proc_iterate(
        reinterpret_cast<const utf8proc_uint8_t *>(text.data() + offset),
        static_cast<utf8proc_ssize_t>(text.size() - offset), &codePoint);
    if (width <= 0)
      return std::unexpected(invalidUtf8(offset));
    offset += static_cast<std::size_t>(width);
  }
  offsets.push_back(text.size());
  return offsets;
}

} // namespace kyna::detail
