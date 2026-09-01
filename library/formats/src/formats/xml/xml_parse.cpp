#include "kyna/formats/document_formats.hpp"
#include <pugixml.hpp>

namespace kyna {
namespace {

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

} // namespace

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

} // namespace kyna
