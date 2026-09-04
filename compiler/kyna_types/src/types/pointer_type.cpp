// Implements pointer identity, rendering, assignability, and interning.
#include <kyna/types/pointer_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const PointerType *left, const PointerType *right) const {
    return left->str() < right->str();
  }
};
std::set<PointerType *, Less> &pool() {
  static std::set<PointerType *, Less> values;
  return values;
}
} // namespace

const PointerType *PointerType::make(TypePtr base) {
  auto *created = new PointerType(base);
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

std::string PointerType::str() const { return "ptr<" + base_->str() + ">"; }

bool PointerType::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || isBasic(target, BasicKind::Object) ||
         isBasic(target, BasicKind::Any);
}

bool PointerType::isIdenticalTo(const Type *other) const {
  return this == other ||
         (other && other->kind() == TypeKind::Pointer &&
          base_->isIdenticalTo(static_cast<const PointerType *>(other)->base_));
}
} // namespace kyna::types
