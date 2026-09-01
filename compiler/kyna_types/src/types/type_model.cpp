#include "kyna/semantics/type_formatting.hpp"

namespace kyna {
std::string TypeRef::str() const {
  std::string result = name;
  if (!typeArgs.empty()) {
    result += '<';
    for (std::size_t index = 0; index < typeArgs.size(); ++index) {
      if (index)
        result += ", ";
      result += typeArgs[index].str();
    }
    result += '>';
  }
  if (nullable)
    result += "?";
  for (const auto &type : unionTypes)
    result += " | " + type.str();
  return result;
}
} // namespace kyna
