#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// A first-class function signature: parameter list, return type, an optional
// receiver (for methods) and a variadic flag. Signatures participate in
// callable-type checking and allow higher-order functions to retain their
// parameter/return shape instead of collapsing to a bare "func" string.
class SignatureType final : public Type {
public:
  // Factory that interns signatures when a matching one already exists.
  static const SignatureType *make(std::vector<TypePtr> params, TypePtr returnType,
                                   TypePtr receiver = nullptr, bool isVariadic = false);

  const std::vector<TypePtr> &params() const { return params_; }
  TypePtr returnType() const { return returnType_; }
  TypePtr receiverType() const { return receiverType_; }
  bool isVariadic() const { return isVariadic_; }

  TypeKind kind() const override { return TypeKind::Signature; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  SignatureType(std::vector<TypePtr> params, TypePtr returnType, TypePtr receiver,
                bool isVariadic);

  std::vector<TypePtr> params_;
  TypePtr returnType_;
  TypePtr receiverType_;
  bool isVariadic_;
};

} // namespace kyna::types
