#pragma once

#include "kyna/formats/document_formats.hpp"
#include <string>

namespace kyna::detail {

inline const FormatValue *field(const FormatValue::Object &object,
                                const std::string &name) {
  const auto found = object.find(name);
  return found == object.end() ? nullptr : &found->second;
}

} // namespace kyna::detail
