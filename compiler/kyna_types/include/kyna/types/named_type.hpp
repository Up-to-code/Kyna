#pragma once

#include <kyna/types/type.hpp>

#include <string>

namespace kyna::types {

struct NamedMethod {
  std::string name;
  TypePtr signature;
  bool pointerReceiver{false};
};

// A nominal, user-defined type such as a class or a module. Named types carry a
// stable name and an optional underlying type (the structural types they are
// built from, e.g. an object or a primitive alias).
class NamedType final : public Type {
public:
  static const NamedType *make(std::string name, TypePtr underlying = nullptr,
                               std::vector<NamedMethod> methods = {});

  const std::string &name() const { return name_; }
  const std::vector<NamedMethod> &methods() const { return methods_; }

  TypeKind kind() const override { return TypeKind::Named; }
  const Type *underlying() const override { return underlying_ ? underlying_ : this; }
  std::string str() const override { return name_; }
  bool isAssignableTo(const Type *target) const override;
  bool isIdenticalTo(const Type *other) const override;

private:
  NamedType(std::string name, TypePtr underlying, std::vector<NamedMethod> methods);

  std::string name_;
  TypePtr underlying_;
  std::vector<NamedMethod> methods_;
};

} // namespace kyna::types
