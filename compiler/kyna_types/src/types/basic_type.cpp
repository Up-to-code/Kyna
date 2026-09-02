#include "kyna/types/basic_type.hpp"
#include "kyna/types/type.hpp"

#include <array>

namespace kyna::types {
namespace {

// The numeric primitives are mutually assignable (int promotes to float and to
// the generic "num" umbrella). The any/object/func/array classifiers accept any
// value of the corresponding structural family.
bool isNumeric(BasicKind kind) {
  return kind == BasicKind::Int || kind == BasicKind::Float || kind == BasicKind::Num;
}

bool isUnit(BasicKind kind) { return kind == BasicKind::Void || kind == BasicKind::Null; }

} // namespace

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
  // Numeric family is mutually assignable; unit types accept void/null only.
  if (isNumeric(lhs) && isNumeric(rhs))
    return true;
  if (lhs == BasicKind::Null || lhs == BasicKind::Void)
    return isUnit(rhs);
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
