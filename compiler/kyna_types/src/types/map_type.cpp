// Implements typed map identity, rendering, assignability, and interning.
#include <kyna/types/map_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const MapType *left, const MapType *right) const {
    return left->str() < right->str();
  }
};
std::set<MapType *, Less> &pool() {
  static std::set<MapType *, Less> values;
  return values;
}
} // namespace

const MapType *MapType::make(TypePtr key, TypePtr value) {
  auto *created = new MapType(key, value);
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

std::string MapType::str() const { return "map<" + key_->str() + ", " + value_->str() + ">"; }

bool MapType::isAssignableTo(const Type *target) const {
  return isIdenticalTo(target) || isBasic(target, BasicKind::Object) ||
         isBasic(target, BasicKind::Any);
}

bool MapType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Map)
    return false;
  const auto *map = static_cast<const MapType *>(other);
  return key_->isIdenticalTo(map->key_) && value_->isIdenticalTo(map->value_);
}
} // namespace kyna::types
