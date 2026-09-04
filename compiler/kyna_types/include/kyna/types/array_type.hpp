#pragma once

#include <kyna/types/type.hpp>

#include <cstddef>

namespace kyna::types {

// A fixed-length value type. Length is part of its identity, as in Go's [N]T.
class ArrayType final : public Type {
public:
  static const ArrayType *make(std::size_t length, TypePtr element);

  std::size_t length() const { return length_; }
  TypePtr element() const { return element_; }

  TypeKind kind() const override { return TypeKind::Array; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  ArrayType(std::size_t length, TypePtr element) : length_(length), element_(element) {}

  std::size_t length_;
  TypePtr element_;
};

} // namespace kyna::types
