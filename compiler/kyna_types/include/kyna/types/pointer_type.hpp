#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// A reference to storage containing a value of the base type.
class PointerType final : public Type {
public:
  static const PointerType *make(TypePtr base);

  TypePtr base() const { return base_; }

  TypeKind kind() const override { return TypeKind::Pointer; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  explicit PointerType(TypePtr base) : base_(base) {}

  TypePtr base_;
};

} // namespace kyna::types
