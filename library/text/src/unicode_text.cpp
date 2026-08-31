#include "kyna/text/unicode_text.hpp"
#include <utf8proc.h>
#include <algorithm>
#include <limits>

namespace kyna {
namespace {

using ByteOffsets = std::vector<std::size_t>;

UnicodeTextError invalidUtf8(std::size_t offset) {
  return {"KTEXT2001", "text contains invalid UTF-8 at byte " + std::to_string(offset), offset};
}

UnicodeTextResult<ByteOffsets> codePointOffsets(std::string_view text) {
  ByteOffsets offsets;
  offsets.reserve(text.size() + 1);
  std::size_t offset = 0;
  while (offset < text.size()) {
    offsets.push_back(offset);
    utf8proc_int32_t codePoint{};
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

UnicodeTextResult<std::string> mapCase(std::string_view text, bool upper) {
  auto offsets = codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  std::string result;
  result.reserve(text.size());
  for (std::size_t index = 0; index + 1 < offsets->size(); ++index) {
    utf8proc_int32_t codePoint{};
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

bool isUnicodeSpace(utf8proc_int32_t codePoint) {
  if (codePoint == '\t' || codePoint == '\n' || codePoint == '\v' || codePoint == '\f' ||
      codePoint == '\r')
    return true;
  const auto category = utf8proc_category(codePoint);
  return category == UTF8PROC_CATEGORY_ZS || category == UTF8PROC_CATEGORY_ZL ||
         category == UTF8PROC_CATEGORY_ZP;
}

} // namespace

UnicodeTextResult<std::int64_t> unicodeLength(std::string_view text) {
  auto offsets = codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  return static_cast<std::int64_t>(offsets->size() - 1);
}

UnicodeTextResult<std::string> unicodeSlice(std::string_view text, std::int64_t start,
                                            std::optional<std::int64_t> end) {
  auto offsets = codePointOffsets(text);
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

UnicodeTextResult<std::optional<std::int64_t>> unicodeFind(std::string_view text,
                                                           std::string_view needle) {
  auto offsets = codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  auto needleOffsets = codePointOffsets(needle);
  if (!needleOffsets)
    return std::unexpected(needleOffsets.error());
  const auto byteOffset = text.find(needle);
  if (byteOffset == std::string_view::npos)
    return std::optional<std::int64_t>{};
  const auto found = std::lower_bound(offsets->begin(), offsets->end(), byteOffset);
  return std::optional<std::int64_t>{static_cast<std::int64_t>(found - offsets->begin())};
}

UnicodeTextResult<std::string> unicodeReplace(std::string_view text, std::string_view needle,
                                              std::string_view replacement) {
  if (auto valid = codePointOffsets(text); !valid)
    return std::unexpected(valid.error());
  if (auto valid = codePointOffsets(needle); !valid)
    return std::unexpected(valid.error());
  if (auto valid = codePointOffsets(replacement); !valid)
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

UnicodeTextResult<std::vector<std::string>> unicodeSplit(std::string_view text,
                                                         std::string_view separator) {
  if (auto valid = codePointOffsets(text); !valid)
    return std::unexpected(valid.error());
  if (auto valid = codePointOffsets(separator); !valid)
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

UnicodeTextResult<std::string> unicodeTrim(std::string_view text) {
  auto offsets = codePointOffsets(text);
  if (!offsets)
    return std::unexpected(offsets.error());
  std::size_t first = 0;
  std::size_t last = offsets->size() - 1;
  auto codePointAt = [&](std::size_t index) {
    utf8proc_int32_t codePoint{};
    utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t *>(text.data() + (*offsets)[index]),
                     static_cast<utf8proc_ssize_t>((*offsets)[index + 1] - (*offsets)[index]),
                     &codePoint);
    return codePoint;
  };
  while (first < last && isUnicodeSpace(codePointAt(first)))
    ++first;
  while (last > first && isUnicodeSpace(codePointAt(last - 1)))
    --last;
  return std::string(text.substr((*offsets)[first], (*offsets)[last] - (*offsets)[first]));
}

UnicodeTextResult<std::string> unicodeLower(std::string_view text) {
  return mapCase(text, false);
}

UnicodeTextResult<std::string> unicodeUpper(std::string_view text) {
  return mapCase(text, true);
}

} // namespace kyna
