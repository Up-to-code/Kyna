#pragma once

#include <string>
#include <vector>

namespace kyna {

struct TypeRef {
  std::string name{"void"};
  bool nullable{false};
  std::vector<TypeRef> typeArgs;
  std::vector<TypeRef> unionTypes;
  std::string str() const;
};

} // namespace kyna
