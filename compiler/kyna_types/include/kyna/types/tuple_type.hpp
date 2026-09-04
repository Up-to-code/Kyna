#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

// An ordered sequence used for parameter and multi-result lists.
class TupleType final : public Type {
public:
  static const TupleType *make(std::vector<TypePtr> elements);

  const std::vector<TypePtr> &elements() const { return elements_; }

  TypeKind kind() const override { return TypeKind::Tuple; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  explicit TupleType(std::vector<TypePtr> elements) : elements_(std::move(elements)) {}

  std::vector<TypePtr> elements_;
};

} // namespace kyna::types
