#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// A primitive singleton type. Instances are never constructed by user code;
// they are handed out by the `Universe` registry. Pointer identity therefore
// fully determines both equality and kind.
class BasicType final : public Type {
public:
  BasicKind basicKind() const { return basicKind_; }
  std::string_view name() const { return name_; }

  TypeKind kind() const override { return TypeKind::Basic; }
  const Type *underlying() const override { return this; }
  std::string str() const override { return std::string(name_); }
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  friend class Universe;
  BasicType(BasicKind basicKind, std::string name)
      : basicKind_(basicKind), name_(std::move(name)) {}

  BasicKind basicKind_;
  std::string name_;
};

// Convenience predicate used across the semantic passes.
bool isBasic(const Type *t, BasicKind kind);

} // namespace kyna::types
