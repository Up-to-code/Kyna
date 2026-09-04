#include "kyna/semantics/scope.hpp"

namespace kyna::semantics {

SymbolPtr Scope::insert(SymbolPtr symbol) {
  if (!symbol)
    return nullptr;
  const std::string &name = symbol->name();
  auto existing = symbols_.find(name);
  if (existing != symbols_.end())
    return existing->second;
  symbol->setParentScope(this);
  symbols_.emplace(name, std::move(symbol));
  return nullptr;
}

SymbolPtr Scope::lookup(const std::string &name) const {
  for (const Scope *cursor = this; cursor; cursor = cursor->parent_) {
    auto it = cursor->symbols_.find(name);
    if (it != cursor->symbols_.end())
      return it->second;
  }
  return nullptr;
}

SymbolPtr Scope::lookupLocal(const std::string &name) const {
  auto it = symbols_.find(name);
  if (it == symbols_.end())
    return nullptr;
  return it->second;
}

} // namespace kyna::semantics
