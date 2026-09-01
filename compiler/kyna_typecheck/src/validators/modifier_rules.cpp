#include "kyna/semantics/modifier_query.hpp"
#include <algorithm>

namespace kyna {
bool hasModifier(const std::vector<std::string> &modifiers, const std::string &modifier) {
  return std::find(modifiers.begin(), modifiers.end(), modifier) != modifiers.end();
}
} // namespace kyna
