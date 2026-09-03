#pragma once

#include <kyna/types/type.hpp>

#include <vector>

namespace kyna::types {

// A union of mutually compatible member types: a value has one of the member
// types, and the union is assignable to / from any member.
class UnionType final : public Type {
public:
  static const UnionType *make(std::vector<TypePtr> members);

  const std::vector<TypePtr> &members() const { return members_; }

  TypeKind kind() const override { return TypeKind::Union; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  UnionType(std::vector<TypePtr> members);

  std::vector<TypePtr> members_;
};

// A type that additionally admits the null value: the underlying members are
// unchanged, but null is a legal inhabitant.
class NullableType final : public Type {
public:
  static const NullableType *make(TypePtr base);

  TypePtr base() const { return base_; }

  TypeKind kind() const override { return TypeKind::Nullable; }
  const Type *underlying() const override { return base_->underlying(); }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  NullableType(TypePtr base);

  TypePtr base_;
};

} // namespace kyna::types
