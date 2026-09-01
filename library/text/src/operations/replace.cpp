#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {

UnicodeTextResult<std::string> unicodeReplace(std::string_view text, std::string_view needle,
                                              std::string_view replacement) {
  if (auto valid = detail::codePointOffsets(text); !valid)
    return std::unexpected(valid.error());
  if (auto valid = detail::codePointOffsets(needle); !valid)
    return std::unexpected(valid.error());
  if (auto valid = detail::codePointOffsets(replacement); !valid)
    return std::unexpected(valid.error());
  if (needle.empty())
    return std::unexpected(UnicodeTextError{"KTEXT2003", "replacement needle cannot be empty", 0});
  std::string result;
  std::size_t cursor = 0;
  while (cursor < text.size()) {
    const auto found = text.find(needle, cursor);
    if (found == std::string_view::npos)
      break;
    result.append(text.substr(cursor, found - cursor));
    result.append(replacement);
    cursor = found + needle.size();
  }
  result.append(text.substr(cursor));
  return result;
}

} // namespace kyna
