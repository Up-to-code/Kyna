#include <kyna/types/named_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>
#include <string>

namespace kyna::types {
namespace {

struct NamedCompare {
  bool operator()(const NamedType *lhs, const NamedType *rhs) const {
    return lhs->name() < rhs->name();
  }
};

std::set<NamedType *, NamedCompare> &namedPool() {
  static std::set<NamedType *, NamedCompare> pool;
  return pool;
}

} // namespace

NamedType::NamedType(std::string name, TypePtr underlying)
    : name_(std::move(name)), underlying_(underlying) {}

const NamedType *NamedType::make(std::string name, TypePtr underlying) {
  auto *created = new NamedType(std::move(name), underlying);
  auto [it, inserted] = namedPool().insert(created);
  if (!inserted)
    delete created;
  return *it;
}

bool NamedType::isAssignableTo(const Type *target) const {
  if (isIdenticalTo(target))
    return true;
  if (!target)
    return false;
  // A named type is assignable to its own underlying structural type.
  if (underlying_ && underlying_->isAssignableTo(target))
    return true;
  // Nominal types are assignable to the untyped object/any classifiers.
  if (target->kind() == TypeKind::Basic) {
    const auto kind = static_cast<const BasicType *>(target)->basicKind();
    if (kind == BasicKind::Any || kind == BasicKind::Object)
      return true;
  }
  return false;
}

bool NamedType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Named)
    return false;
  return name_ == static_cast<const NamedType *>(other)->name_;
}

} // namespace kyna::types
