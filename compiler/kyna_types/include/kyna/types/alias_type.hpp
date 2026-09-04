#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// A spelling alias, not a new nominal identity. It is identical to its target.
class AliasType final : public Type {
public:
  static const AliasType *make(std::string name, TypePtr target);

  const std::string &name() const { return name_; }
  TypePtr target() const { return target_; }

  TypeKind kind() const override { return TypeKind::Alias; }
  const Type *underlying() const override { return target_->underlying(); }
  std::string str() const override { return name_; }
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  AliasType(std::string name, TypePtr target) : name_(std::move(name)), target_(target) {}

  std::string name_;
  TypePtr target_;
};

} // namespace kyna::types
