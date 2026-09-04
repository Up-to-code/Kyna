// Implements dynamic slice identity, rendering, assignability, and interning.
#include <kyna/types/slice_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const SliceType *left, const SliceType *right) const {
    return left->str() < right->str();
  }
};
std::set<SliceType *, Less> &pool() {
  static std::set<SliceType *, Less> values;
  return values;
}
} // namespace

const SliceType *SliceType::make(TypePtr element) {
  auto *created = new SliceType(element);
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

std::string SliceType::str() const { return "array<" + element_->str() + ">"; }

bool SliceType::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || isBasic(target, BasicKind::Array) ||
         isBasic(target, BasicKind::Any);
}

bool SliceType::isIdenticalTo(const Type *other) const {
  return this == other ||
         (other && other->kind() == TypeKind::Slice &&
          element_->isIdenticalTo(static_cast<const SliceType *>(other)->element_));
}
} // namespace kyna::types
