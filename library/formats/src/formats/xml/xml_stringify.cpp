#include "../formats_private.hpp"
#include <pugixml.hpp>
#include <sstream>

namespace kyna {
namespace {

bool appendXml(pugi::xml_node parent, const FormatValue &value, std::string &error) {
  const auto *object = std::get_if<FormatValue::Object>(&value.data);
  if (!object) {
    error = "XML nodes must be objects";
    return false;
  }
  const auto *name = detail::field(*object, "name");
  const auto *nameText = name ? std::get_if<std::string>(&name->data) : nullptr;
  if (!nameText || nameText->empty()) {
    error = "XML node field 'name' must be a non-empty string";
    return false;
  }
  auto node = parent.append_child(nameText->c_str());
  if (const auto *attributes = detail::field(*object, "attributes")) {
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
  if (const auto *text = detail::field(*object, "text")) {
    const auto *textValue = std::get_if<std::string>(&text->data);
    if (!textValue) {
      error = "XML node field 'text' must be a string";
      return false;
    }
    if (!textValue->empty())
      node.append_child(pugi::node_pcdata).set_value(textValue->c_str());
  }
  if (const auto *children = detail::field(*object, "children")) {
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
