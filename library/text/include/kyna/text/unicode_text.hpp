#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kyna {

struct UnicodeTextError {
  std::string code;
  std::string message;
  std::size_t byteOffset{};
};

template <typename Value> using UnicodeTextResult = std::expected<Value, UnicodeTextError>;

[[nodiscard]] UnicodeTextResult<std::int64_t> unicodeLength(std::string_view text);
[[nodiscard]] UnicodeTextResult<std::string>
unicodeSlice(std::string_view text, std::int64_t start, std::optional<std::int64_t> end = {});
[[nodiscard]] UnicodeTextResult<std::optional<std::int64_t>>
unicodeFind(std::string_view text, std::string_view needle);
[[nodiscard]] UnicodeTextResult<std::string>
unicodeReplace(std::string_view text, std::string_view needle, std::string_view replacement);
[[nodiscard]] UnicodeTextResult<std::vector<std::string>>
unicodeSplit(std::string_view text, std::string_view separator);
[[nodiscard]] UnicodeTextResult<std::string> unicodeTrim(std::string_view text);
[[nodiscard]] UnicodeTextResult<std::string> unicodeLower(std::string_view text);
[[nodiscard]] UnicodeTextResult<std::string> unicodeUpper(std::string_view text);

} // namespace kyna
