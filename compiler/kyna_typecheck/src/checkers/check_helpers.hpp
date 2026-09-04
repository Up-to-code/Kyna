#pragma once

#include <kyna/semantics/modifier_query.hpp>
#include <kyna/semantics/type_model.hpp>
#include <kyna/syntax/declaration_nodes.hpp>
#include <kyna/types/type_bridge.hpp>

#include <string>
#include <vector>

namespace kyna {

// Shared spelling helpers for the decomposed expression checkers. Primitive
// names intern through Universe; everything else stays a nominal TypeRef.
inline TypeRef analyzerNamedType(const std::string &name) {
  if (auto primitive = types::typeFromName(name))
    return types::typeToRef(primitive);
  return TypeRef{name, false, {}, {}};
}

inline int analyzerMemberVisibility(const std::vector<std::string> &modifiers) {
  if (hasModifier(modifiers, "public"))
    return 2;
  if (hasModifier(modifiers, "protected"))
    return 1;
  return 0;
}

inline bool analyzerSameParameters(const FunctionDecl &left, const FunctionDecl &right) {
  if (left.params.size() != right.params.size())
    return false;
  for (std::size_t index = 0; index < left.params.size(); ++index)
    if (left.params[index].type.str() != right.params[index].type.str())
      return false;
  return true;
}

} // namespace kyna
