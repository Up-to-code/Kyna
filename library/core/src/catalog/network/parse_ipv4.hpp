#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace kyna::detail {

inline std::optional<std::string> parseIPv4(std::string_view text) {
  unsigned parts[4]{};
  std::size_t offset = 0;
  for (int index = 0; index < 4; ++index) {
    if (offset >= text.size() || text[offset] < '0' || text[offset] > '9')
      return std::nullopt;
    const auto start = offset;
    while (offset < text.size() && text[offset] >= '0' && text[offset] <= '9')
      ++offset;
    if (offset - start > 3)
      return std::nullopt;
    unsigned value = 0;
    const auto converted =
        std::from_chars(text.data() + start, text.data() + offset, value);
    if (converted.ec != std::errc{} || value > 255)
      return std::nullopt;
    parts[index] = value;
    if (index < 3) {
      if (offset >= text.size() || text[offset] != '.')
        return std::nullopt;
      ++offset;
    }
  }
  if (offset != text.size())
    return std::nullopt;
  return std::to_string(parts[0]) + "." + std::to_string(parts[1]) + "." +
         std::to_string(parts[2]) + "." + std::to_string(parts[3]);
}

} // namespace kyna::detail
