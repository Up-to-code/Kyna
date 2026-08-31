#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace kyna {

struct FormatValue {
  using Array = std::vector<FormatValue>;
  using Object = std::map<std::string, FormatValue>;
  using Data = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object>;

  Data data{nullptr};
  FormatValue() = default;
  template <class T> FormatValue(T value) : data(std::move(value)) {}
};

struct FormatFailure {
  std::string code;
  std::string message;
};

struct FormatResult {
  FormatValue value;
  FormatFailure failure;
  bool valid{false};
};

[[nodiscard]] FormatResult parseTomlDocument(const std::string &source);
[[nodiscard]] FormatResult stringifyTomlDocument(const FormatValue &value);
[[nodiscard]] FormatResult parseXmlDocument(const std::string &source);
[[nodiscard]] FormatResult stringifyXmlDocument(const FormatValue &value);

} // namespace kyna
