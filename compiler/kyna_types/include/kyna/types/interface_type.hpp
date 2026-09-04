#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

struct InterfaceMethod {
  std::string name;
  TypePtr signature;
};

// A structural method contract. Implementers opt in by shape, not declaration.
class InterfaceType final : public Type {
public:
  static const InterfaceType *make(std::vector<InterfaceMethod> methods,
                                   std::vector<TypePtr> embedded = {});

  const std::vector<InterfaceMethod> &methods() const { return methods_; }
  const std::vector<TypePtr> &embedded() const { return embedded_; }
  bool isSatisfiedBy(const Type *candidate) const;

  TypeKind kind() const override { return TypeKind::Interface; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  InterfaceType(std::vector<InterfaceMethod> methods, std::vector<TypePtr> embedded)
      : methods_(std::move(methods)), embedded_(std::move(embedded)) {}

  std::vector<InterfaceMethod> methods_;
  std::vector<TypePtr> embedded_;
};

} // namespace kyna::types
