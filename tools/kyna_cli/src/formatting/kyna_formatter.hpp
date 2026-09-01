#pragma once
#include <string>
#include <string_view>

namespace kyna::cli {
struct FormatResult {
  std::string text;
  std::string error;
  [[nodiscard]] bool ok() const { return error.empty(); }
};
FormatResult formatKyna(std::string_view source);
} // namespace kyna::cli
