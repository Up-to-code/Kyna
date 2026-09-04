#pragma once

#include <kyna/types/type.hpp>

namespace kyna::types {

struct StructField {
  std::string name;
  TypePtr type;
  std::string tag;
  bool embedded{false};
};

// A structural value type. Field order, names, types, tags, and embedding are
// all part of identity, matching Go's struct identity rule.
class StructType final : public Type {
public:
  static const StructType *make(std::vector<StructField> fields);

  const std::vector<StructField> &fields() const { return fields_; }
  const StructField *field(std::string_view name) const;

  TypeKind kind() const override { return TypeKind::Struct; }
  const Type *underlying() const override { return this; }
  std::string str() const override;
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  explicit StructType(std::vector<StructField> fields) : fields_(std::move(fields)) {}

  std::vector<StructField> fields_;
};

} // namespace kyna::types
