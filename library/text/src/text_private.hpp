#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "kyna/text/unicode_text.hpp"

namespace kyna::detail {

using ByteOffsets = std::vector<std::size_t>;

UnicodeTextError invalidUtf8(std::size_t offset);

UnicodeTextResult<ByteOffsets> codePointOffsets(std::string_view text);

bool isUnicodeSpace(std::int32_t codePoint);

std::int32_t codePointAt(std::string_view text, const ByteOffsets &offsets, std::size_t index);

} // namespace kyna::detail
