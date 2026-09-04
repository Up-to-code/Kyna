// Implements alias identity forwarding and interning.
#include <kyna/types/alias_type.hpp>

#include <map>

namespace kyna::types {
namespace {
std::map<std::string, AliasType *> &pool() {
  static std::map<std::string, AliasType *> values;
  return values;
}
} // namespace

const AliasType *AliasType::make(std::string name, TypePtr target) {
  const auto key = name + "=" + target->str();
  const auto found = pool().find(key);
  if (found != pool().end())
    return found->second;
  auto *created = new AliasType(std::move(name), target);
  pool().emplace(key, created);
  return created;
}

bool AliasType::isAssignableTo(const Type *target) const {
  return target_->isAssignableTo(target) || target_->isIdenticalTo(target);
}

bool AliasType::isIdenticalTo(const Type *other) const {
  if (!other)
    return false;
  if (other->kind() == TypeKind::Alias)
    return target_->isIdenticalTo(static_cast<const AliasType *>(other)->target_);
  return target_->isIdenticalTo(other);
}
} // namespace kyna::types
