#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// An associative reference type with distinct key and value types.
class MapType final : public Type {
public:
  static const MapType *make(TypePtr key, TypePtr value);

  TypePtr key() const { return key_; }
  TypePtr value() const { return value_; }

  TypeKind kind() const override { return TypeKind::Map; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  MapType(TypePtr key, TypePtr value) : key_(key), value_(value) {}

  TypePtr key_;
  TypePtr value_;
};

} // namespace kyna::types
