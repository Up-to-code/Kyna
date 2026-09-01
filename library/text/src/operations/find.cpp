#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {

UnicodeTextResult<std::optional<std::int64_t>> unicodeFind(std::string_view text,
                                                           std::string_view needle) {
  auto offsets = detail::codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  auto needleOffsets = detail::codePointOffsets(needle);
  if (!needleOffsets)
    return std::unexpected(needleOffsets.error());
  const auto byteOffset = text.find(needle);
  if (byteOffset == std::string_view::npos)
    return std::optional<std::int64_t>{};
  const auto found = std::lower_bound(offsets->begin(), offsets->end(), byteOffset);
  return std::optional<std::int64_t>{static_cast<std::int64_t>(found - offsets->begin())};
}

} // namespace kyna
