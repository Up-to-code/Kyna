#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// A generic type parameter and its constraint.
class TypeParam final : public Type {
public:
  static const TypeParam *make(std::string name, TypePtr constraint);

  const std::string &name() const { return name_; }
  TypePtr constraint() const { return constraint_; }

  TypeKind kind() const override { return TypeKind::TypeParam; }
  const Type *underlying() const override;
  std::string str() const override { return name_; }
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  TypeParam(std::string name, TypePtr constraint)
      : name_(std::move(name)), constraint_(constraint) {}

  std::string name_;
  TypePtr constraint_;
};

} // namespace kyna::types
