// Implements constrained generic type-parameter identity and interning.
#include <kyna/types/type_parameter.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const TypeParam *left, const TypeParam *right) const {
    if (left->name() != right->name())
      return left->name() < right->name();
    return left->constraint()->str() < right->constraint()->str();
  }
};
std::set<TypeParam *, Less> &pool() {
  static std::set<TypeParam *, Less> values;
  return values;
}
} // namespace

const TypeParam *TypeParam::make(std::string name, TypePtr constraint) {
  auto *created = new TypeParam(std::move(name), constraint ? constraint : Universe::Any());
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

const Type *TypeParam::underlying() const { return constraint_->underlying(); }

bool TypeParam::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || constraint_->isAssignableTo(target);
}

bool TypeParam::isIdenticalTo(const Type *other) const {
  return this == other ||
         (other && other->kind() == TypeKind::TypeParam &&
          name_ == static_cast<const TypeParam *>(other)->name_ &&
          constraint_->isIdenticalTo(static_cast<const TypeParam *>(other)->constraint_));
}
} // namespace kyna::types
