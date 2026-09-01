#include "kyna/formats/document_formats.hpp"
#include <toml++/toml.hpp>
#include <sstream>

namespace kyna {
namespace {

FormatValue fromToml(const toml::node &node) {
  if (const auto *table = node.as_table()) {
    FormatValue::Object result;
    for (const auto &[key, value] : *table)
      result.emplace(std::string(key.str()), fromToml(value));
    return result;
  }
  if (const auto *array = node.as_array()) {
    FormatValue::Array result;
    result.reserve(array->size());
    for (const auto &value : *array)
      result.push_back(fromToml(value));
    return result;
  }
  if (node.is_boolean())
    return *node.value<bool>();
  if (node.is_integer())
    return *node.value<std::int64_t>();
  if (node.is_floating_point())
    return *node.value<double>();
  if (node.is_string())
    return *node.value<std::string>();
  std::ostringstream rendered;
  if (const auto value = node.value<toml::date>())
    rendered << *value;
  else if (const auto value = node.value<toml::time>())
    rendered << *value;
  else if (const auto value = node.value<toml::date_time>())
    rendered << *value;
  return rendered.str();
}

} // namespace

FormatResult parseTomlDocument(const std::string &source) {
  try {
    return {fromToml(toml::parse(source)), {}, true};
  } catch (const toml::parse_error &error) {
    return {{}, {"KFORMAT1001", std::string("invalid TOML: ") +
                                       std::string(error.description())}, false};
  }
}

} // namespace kyna
