#include <kyna/types/compound_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>
#include <string>

namespace kyna::types {
namespace {

struct UnionCompare {
  bool operator()(const UnionType *lhs, const UnionType *rhs) const {
    return lhs->str() < rhs->str();
  }
};
std::set<UnionType *, UnionCompare> &unionPool() {
  static std::set<UnionType *, UnionCompare> pool;
  return pool;
}

struct NullableCompare {
  bool operator()(const NullableType *lhs, const NullableType *rhs) const {
    return lhs->base()->str() < rhs->base()->str();
  }
};
std::set<NullableType *, NullableCompare> &nullablePool() {
  static std::set<NullableType *, NullableCompare> pool;
  return pool;
}

} // namespace

UnionType::UnionType(std::vector<TypePtr> members) : members_(std::move(members)) {}

const UnionType *UnionType::make(std::vector<TypePtr> members) {
  auto *created = new UnionType(std::move(members));
  auto [it, inserted] = unionPool().insert(created);
  if (!inserted)
    delete created;
  return *it;
}

std::string UnionType::str() const {
  std::string out;
  for (std::size_t index = 0; index < members_.size(); ++index) {
    if (index)
      out += " | ";
    out += members_[index]->str();
  }
  return out;
}

bool UnionType::isAssignableTo(const Type *target) const {
  if (isIdenticalTo(target))
    return true;
  // A union is assignable when every member is assignable to the target.
  for (const auto *member : members_)
    if (!member->isAssignableTo(target))
      return false;
  return true;
}

bool UnionType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Union)
    return false;
  const auto *that = static_cast<const UnionType *>(other);
  if (members_.size() != that->members_.size())
    return false;
  for (std::size_t index = 0; index < members_.size(); ++index)
    if (!members_[index]->isIdenticalTo(that->members_[index]))
      return false;
  return true;
}

NullableType::NullableType(TypePtr base) : base_(base) {}

const NullableType *NullableType::make(TypePtr base) {
  auto *created = new NullableType(base);
  auto [it, inserted] = nullablePool().insert(created);
  if (!inserted)
    delete created;
  return *it;
}

std::string NullableType::str() const { return base_->str() + "?"; }

bool NullableType::isAssignableTo(const Type *target) const {
  if (isIdenticalTo(target))
    return true;
  // A nullable value cannot be stored where a non-null type is required without
  // a null check, but it is assignable to the untyped any/object classifiers.
  if (!target)
    return false;
  if (target->kind() == TypeKind::Basic) {
    const auto kind = static_cast<const BasicType *>(target)->basicKind();
    if (kind == BasicKind::Any || kind == BasicKind::Object)
      return true;
  }
  return false;
}

bool NullableType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Nullable)
    return false;
  return base_->isIdenticalTo(static_cast<const NullableType *>(other)->base_);
}

} // namespace kyna::types
