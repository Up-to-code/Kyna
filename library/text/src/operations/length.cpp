#include <cstdint>
#include <expected>
#include <string_view>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {

UnicodeTextResult<std::int64_t> unicodeLength(std::string_view text) {
  auto offsets = detail::codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  return static_cast<std::int64_t>(offsets->size() - 1);
}

} // namespace kyna
