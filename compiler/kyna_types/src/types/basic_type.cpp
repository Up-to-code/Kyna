#include <kyna/types/basic_type.hpp>
#include <kyna/types/type.hpp>

namespace kyna::types {

bool BasicType::isAssignableTo(const Type *target) const {
  if (isIdenticalTo(target))
    return true;
  if (!target || target->kind() != TypeKind::Basic)
    return false;
  const auto *other = static_cast<const BasicType *>(target);
  if (other->basicKind() == BasicKind::Any) // any accepts everything
    return true;
  const auto lhs = basicKind();
  const auto rhs = other->basicKind();
  // Widen int → float and int/float → num. Narrowing (float → int, num → int)
  // stays a type error.
  if (lhs == BasicKind::Int && (rhs == BasicKind::Float || rhs == BasicKind::Num))
    return true;
  if (lhs == BasicKind::Float && rhs == BasicKind::Num)
    return true;
  if (lhs == BasicKind::Null)
    return false;
  return false;
}

bool BasicType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  return other != nullptr && other->kind() == TypeKind::Basic &&
         basicKind() == static_cast<const BasicType *>(other)->basicKind();
}

bool isBasic(const Type *t, BasicKind kind) {
  return t != nullptr && t->kind() == TypeKind::Basic &&
         static_cast<const BasicType *>(t)->basicKind() == kind;
}

} // namespace kyna::types
