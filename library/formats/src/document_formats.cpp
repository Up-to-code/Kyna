#include "kyna/formats/document_formats.hpp"
#include <pugixml.hpp>
#include <toml++/toml.hpp>
#include <sstream>
#include <type_traits>

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

FormatValue fromXml(const pugi::xml_node &node) {
  FormatValue::Object result;
  result["name"] = std::string(node.name());
  FormatValue::Object attributes;
  for (const auto &attribute : node.attributes())
    attributes[attribute.name()] = std::string(attribute.value());
  result["attributes"] = std::move(attributes);
  std::string text;
  FormatValue::Array children;
  for (const auto &child : node.children()) {
    if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
      text += child.value();
    else if (child.type() == pugi::node_element)
      children.push_back(fromXml(child));
  }
  result["text"] = std::move(text);
  result["children"] = std::move(children);
  return result;
}

const FormatValue *field(const FormatValue::Object &object, const std::string &name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

bool appendXml(pugi::xml_node parent, const FormatValue &value, std::string &error) {
  const auto *object = std::get_if<FormatValue::Object>(&value.data);
  if (!object) {
    error = "XML nodes must be objects";
    return false;
  }
  const auto *name = field(*object, "name");
  const auto *nameText = name ? std::get_if<std::string>(&name->data) : nullptr;
  if (!nameText || nameText->empty()) {
    error = "XML node field 'name' must be a non-empty string";
    return false;
  }
  auto node = parent.append_child(nameText->c_str());
  if (const auto *attributes = field(*object, "attributes")) {
    const auto *attributeObject = std::get_if<FormatValue::Object>(&attributes->data);
    if (!attributeObject) {
      error = "XML node field 'attributes' must be an object";
      return false;
    }
    for (const auto &[attributeName, attributeValue] : *attributeObject) {
      const auto *attributeText = std::get_if<std::string>(&attributeValue.data);
      if (!attributeText) {
        error = "XML attribute '" + attributeName + "' must be a string";
        return false;
      }
      node.append_attribute(attributeName.c_str()).set_value(attributeText->c_str());
    }
  }
  if (const auto *text = field(*object, "text")) {
    const auto *textValue = std::get_if<std::string>(&text->data);
    if (!textValue) {
      error = "XML node field 'text' must be a string";
      return false;
    }
    if (!textValue->empty())
      node.append_child(pugi::node_pcdata).set_value(textValue->c_str());
  }
  if (const auto *children = field(*object, "children")) {
    const auto *childArray = std::get_if<FormatValue::Array>(&children->data);
    if (!childArray) {
      error = "XML node field 'children' must be an array";
      return false;
    }
    for (const auto &child : *childArray)
      if (!appendXml(node, child, error))
        return false;
  }
  return true;
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

FormatResult parseXmlDocument(const std::string &source) {
  pugi::xml_document document;
  const auto parsed = document.load_buffer(source.data(), source.size(),
                                           pugi::parse_default | pugi::parse_ws_pcdata);
  if (!parsed)
    return {{}, {"KFORMAT1101", std::string("invalid XML: ") + parsed.description()}, false};
  const auto root = document.document_element();
  if (!root)
    return {{}, {"KFORMAT1101", "invalid XML: document has no root element"}, false};
  return {fromXml(root), {}, true};
}

FormatResult stringifyXmlDocument(const FormatValue &value) {
  pugi::xml_document document;
  std::string error;
  if (!appendXml(document, value, error))
    return {{}, {"KFORMAT1102", std::move(error)}, false};
  std::ostringstream output;
  document.save(output, "  ", pugi::format_default, pugi::encoding_utf8);
  return {FormatValue(output.str()), {}, true};
}

} // namespace kyna
