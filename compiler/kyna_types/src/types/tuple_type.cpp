// Implements internal tuple identity, rendering, assignability, and interning.
#include <kyna/types/tuple_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const TupleType *left, const TupleType *right) const {
    return left->str() < right->str();
  }
};
std::set<TupleType *, Less> &pool() {
  static std::set<TupleType *, Less> values;
  return values;
}
} // namespace

const TupleType *TupleType::make(std::vector<TypePtr> elements) {
  auto *created = new TupleType(std::move(elements));
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

std::string TupleType::str() const {
  std::string result = "(";
  for (std::size_t index = 0; index < elements_.size(); ++index) {
    if (index)
      result += ", ";
    result += elements_[index]->str();
  }
  return result + ")";
}

bool TupleType::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || isBasic(target, BasicKind::Any);
}

bool TupleType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Tuple)
    return false;
  const auto &elements = static_cast<const TupleType *>(other)->elements_;
  if (elements_.size() != elements.size())
    return false;
  for (std::size_t index = 0; index < elements_.size(); ++index)
    if (!elements_[index]->isIdenticalTo(elements[index]))
      return false;
  return true;
}
} // namespace kyna::types
