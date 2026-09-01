#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {

UnicodeTextResult<std::string> unicodeTrim(std::string_view text) {
  auto offsets = detail::codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  std::size_t first = 0;
  std::size_t last = offsets->size() - 1;
  while (first < last && detail::isUnicodeSpace(detail::codePointAt(text, *offsets, first)))
    ++first;
  while (last > first && detail::isUnicodeSpace(detail::codePointAt(text, *offsets, last - 1)))
    --last;
  return std::string(text.substr((*offsets)[first], (*offsets)[last] - (*offsets)[first]));
}

} // namespace kyna
