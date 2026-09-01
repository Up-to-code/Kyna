#include "kyna/formats/document_formats.hpp"
#include <toml++/toml.hpp>
#include <sstream>
#include <type_traits>

namespace kyna {
namespace {

bool insertToml(toml::table &target, const std::string &key, const FormatValue &value,
                std::string &error);

bool appendToml(toml::array &target, const FormatValue &value, std::string &error) {
  return std::visit(
      [&](const auto &item) -> bool {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
          error = "TOML does not support null values";
          return false;
        } else if constexpr (std::is_same_v<T, FormatValue::Object>) {
          toml::table table;
          for (const auto &[name, child] : item)
            if (!insertToml(table, name, child, error))
              return false;
          target.push_back(std::move(table));
        } else if constexpr (std::is_same_v<T, FormatValue::Array>) {
          toml::array array;
          for (const auto &child : item)
            if (!appendToml(array, child, error))
              return false;
          target.push_back(std::move(array));
        } else {
          target.push_back(item);
        }
        return true;
      },
      value.data);
}

bool insertToml(toml::table &target, const std::string &key, const FormatValue &value,
                std::string &error) {
  return std::visit(
      [&](const auto &item) -> bool {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
          error = "TOML does not support null values at key '" + key + "'";
          return false;
        } else if constexpr (std::is_same_v<T, FormatValue::Object>) {
          toml::table table;
          for (const auto &[name, child] : item)
            if (!insertToml(table, name, child, error))
              return false;
          target.insert(key, std::move(table));
        } else if constexpr (std::is_same_v<T, FormatValue::Array>) {
          toml::array array;
          for (const auto &child : item)
            if (!appendToml(array, child, error))
              return false;
          target.insert(key, std::move(array));
        } else {
          target.insert(key, item);
        }
        return true;
      },
      value.data);
}

} // namespace

FormatResult stringifyTomlDocument(const FormatValue &value) {
  const auto *object = std::get_if<FormatValue::Object>(&value.data);
  if (!object)
    return {{}, {"KFORMAT1002", "TOML root must be an object"}, false};
  toml::table table;
  std::string error;
  for (const auto &[name, child] : *object)
    if (!insertToml(table, name, child, error))
      return {{}, {"KFORMAT1002", std::move(error)}, false};
  std::ostringstream output;
  output << table;
  return {FormatValue(output.str()), {}, true};
}

} // namespace kyna
