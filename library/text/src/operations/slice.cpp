#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {

UnicodeTextResult<std::string> unicodeSlice(std::string_view text, std::int64_t start,
                                            std::optional<std::int64_t> end) {
  auto offsets = detail::codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  const auto length = static_cast<std::int64_t>(offsets->size() - 1);
  auto normalize = [length](std::int64_t index) { return index < 0 ? length + index : index; };
  const auto first = normalize(start);
  const auto last = normalize(end.value_or(length));
  if (first < 0 || first > length || last < first || last > length)
    return std::unexpected(UnicodeTextError{
        "KTEXT2002", "text slice [" + std::to_string(start) + ", " +
                         std::to_string(end.value_or(length)) + ") is outside length " +
                         std::to_string(length),
        0});
  const auto byteStart = (*offsets)[static_cast<std::size_t>(first)];
  const auto byteEnd = (*offsets)[static_cast<std::size_t>(last)];
  return std::string(text.substr(byteStart, byteEnd - byteStart));
}

} // namespace kyna
