#include "kyna/formats/document_formats.hpp"
#include <cassert>
#include <iostream>
#include <string>

namespace {

const kyna::FormatValue &field(const kyna::FormatValue &value, const std::string &name) {
  const auto &object = std::get<kyna::FormatValue::Object>(value.data);
  const auto found = object.find(name);
  assert(found != object.end());
  return found->second;
}

} // namespace

int main() {
  const auto toml = kyna::parseTomlDocument(
      "title = \"Kyna\"\n[server]\nport = 8080\nenabled = true\n");
  assert(toml.valid);
  assert(std::get<std::string>(field(toml.value, "title").data) == "Kyna");
  assert(std::get<std::int64_t>(field(field(toml.value, "server"), "port").data) == 8080);
  const auto encodedToml = kyna::stringifyTomlDocument(toml.value);
  assert(encodedToml.valid);
  assert(kyna::parseTomlDocument(std::get<std::string>(encodedToml.value.data)).valid);
  const auto badToml = kyna::parseTomlDocument("broken = [1,");
  assert(!badToml.valid && badToml.failure.code == "KFORMAT1001");

  const auto xml = kyna::parseXmlDocument(
      "<project version=\"1\"><name>Kyna</name><files count=\"2\"/></project>");
  assert(xml.valid);
  assert(std::get<std::string>(field(xml.value, "name").data) == "project");
  assert(std::get<std::string>(field(field(xml.value, "attributes"), "version").data) == "1");
  const auto &children = std::get<kyna::FormatValue::Array>(field(xml.value, "children").data);
  assert(children.size() == 2);
  assert(std::get<std::string>(field(children[0], "text").data) == "Kyna");
  const auto encodedXml = kyna::stringifyXmlDocument(xml.value);
  assert(encodedXml.valid);
  assert(kyna::parseXmlDocument(std::get<std::string>(encodedXml.value.data)).valid);
  const auto badXml = kyna::parseXmlDocument("<project>");
  assert(!badXml.valid && badXml.failure.code == "KFORMAT1101");

  std::cout << "format module tests passed\n";
}
