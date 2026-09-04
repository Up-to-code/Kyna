#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// A dynamic view over a backing array. Kyna's current mutable `array<T>` values
// use these reference semantics even though their source spelling is `array`.
class SliceType final : public Type {
public:
  static const SliceType *make(TypePtr element);

  TypePtr element() const { return element_; }

  TypeKind kind() const override { return TypeKind::Slice; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  explicit SliceType(TypePtr element) : element_(element) {}

  TypePtr element_;
};

} // namespace kyna::types
