// Implements fixed-length array identity, rendering, assignability, and interning.
#include <kyna/types/array_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const ArrayType *left, const ArrayType *right) const {
    return left->str() < right->str();
  }
};
std::set<ArrayType *, Less> &pool() {
  static std::set<ArrayType *, Less> values;
  return values;
}
bool acceptsStructured(const Type *target) {
  return isBasic(target, BasicKind::Any) || isBasic(target, BasicKind::Array);
}
} // namespace

const ArrayType *ArrayType::make(std::size_t length, TypePtr element) {
  auto *created = new ArrayType(length, element);
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

std::string ArrayType::str() const {
  return "[" + std::to_string(length_) + "]" + element_->str();
}

bool ArrayType::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || acceptsStructured(target);
}

bool ArrayType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Array)
    return false;
  const auto *array = static_cast<const ArrayType *>(other);
  return length_ == array->length_ && element_->isIdenticalTo(array->element_);
}
} // namespace kyna::types
