#include "kyna/semantics/cycle_detector.hpp"

#include <utility>

namespace kyna::semantics {

std::string CycleDetector::advance(const std::string &name,
                                   const std::vector<std::string> &dependencies) {
  colors_[name] = Color::Grey;
  for (const auto &dep : dependencies) {
    auto it = colors_.find(dep);
    if (it != colors_.end() && it->second == Color::Grey)
      return dep; // dep is already being resolved: this edge closes a cycle
  }
  return {};
}

void CycleDetector::markBlack(const std::string &name) {
  auto it = colors_.find(name);
  if (it == colors_.end())
    return;
  it->second = Color::Black;
}

void CycleDetector::finish(const std::string &name) { markBlack(name); }

bool CycleDetector::isResolving(const std::string &name) const {
  auto it = colors_.find(name);
  return it != colors_.end() && it->second == Color::Grey;
}

} // namespace kyna::semantics
