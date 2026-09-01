#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "kyna/text/unicode_text.hpp"
#include "../text_private.hpp"

namespace kyna {

UnicodeTextResult<std::vector<std::string>> unicodeSplit(std::string_view text,
                                                         std::string_view separator) {
  if (auto valid = detail::codePointOffsets(text); !valid)
    return std::unexpected(valid.error());
  if (auto valid = detail::codePointOffsets(separator); !valid)
    return std::unexpected(valid.error());
  if (separator.empty())
    return std::unexpected(UnicodeTextError{"KTEXT2004", "split separator cannot be empty", 0});
  std::vector<std::string> result;
  std::size_t cursor = 0;
  while (true) {
    const auto found = text.find(separator, cursor);
    if (found == std::string_view::npos) {
      result.emplace_back(text.substr(cursor));
      return result;
    }
    result.emplace_back(text.substr(cursor, found - cursor));
    cursor = found + separator.size();
  }
}

} // namespace kyna
