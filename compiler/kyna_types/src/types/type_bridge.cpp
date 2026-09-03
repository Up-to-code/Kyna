#include <kyna/types/type_bridge.hpp>
#include <kyna/types/basic_type.hpp>
#include <kyna/types/compound_type.hpp>
#include <kyna/types/named_type.hpp>
#include <kyna/types/signature_type.hpp>

namespace kyna::types {
namespace {

TypePtr primitive(std::string_view name) {
  if (name == "int")
    return Universe::Int();
  if (name == "float")
    return Universe::Float();
  if (name == "bool")
    return Universe::Bool();
  if (name == "str" || name == "string")
    return Universe::String();
  if (name == "char")
    return Universe::Char();
  if (name == "void")
    return Universe::Void();
  if (name == "null")
    return Universe::Null();
  if (name == "any")
    return Universe::Any();
  if (name == "object")
    return Universe::Object();
  if (name == "func")
    return Universe::Func();
  if (name == "array")
    return Universe::Array();
  if (name == "num")
    return Universe::Num();
  if (name == "class")
    return Universe::Class();
  return nullptr;
}

} // namespace

TypePtr typeFromName(std::string_view name) { return primitive(name); }

TypePtr typeFromRef(const TypeRef &ref) {
  TypePtr base = nullptr;
  if (ref.name == "union") {
    std::vector<TypePtr> members;
    members.reserve(ref.typeArgs.size() + ref.unionTypes.size());
    for (const auto &arg : ref.typeArgs)
      members.push_back(typeFromRef(arg));
    for (const auto &arm : ref.unionTypes)
      members.push_back(typeFromRef(arm));
    if (members.empty())
      base = Universe::Any();
    else if (members.size() == 1)
      base = members.front();
    else
      base = UnionType::make(std::move(members));
  } else if (auto found = primitive(ref.name)) {
    base = found;
  } else if (ref.name.starts_with("class:")) {
    base = NamedType::make(ref.name.substr(6), Universe::Object());
  } else if (ref.name.starts_with("module:")) {
    base = NamedType::make(ref.name, Universe::Object());
  } else {
    base = NamedType::make(ref.name);
  }

  if (!ref.unionTypes.empty() && ref.name != "union") {
    std::vector<TypePtr> members{base};
    for (const auto &arm : ref.unionTypes)
      members.push_back(typeFromRef(arm));
    base = UnionType::make(std::move(members));
  }
  if (ref.nullable)
    base = NullableType::make(base);
  return base;
}

TypeRef typeToRef(TypePtr type) {
  if (!type)
    return TypeRef{"void", false, {}};
  if (type->kind() == TypeKind::Nullable) {
    auto inner = typeToRef(static_cast<const NullableType *>(type)->base());
    inner.nullable = true;
    return inner;
  }
  if (type->kind() == TypeKind::Union) {
    TypeRef result{"union", false, {}};
    for (const auto *member : static_cast<const UnionType *>(type)->members())
      result.typeArgs.push_back(typeToRef(member));
    return result;
  }
  if (type->kind() == TypeKind::Signature)
    return TypeRef{"func", false, {}};
  return TypeRef{type->str(), false, {}};
}

bool isAssignable(TypePtr actual, TypePtr expected) {
  if (!actual || !expected)
    return false;
  if (isBasic(expected, BasicKind::Any) || isBasic(actual, BasicKind::Any))
    return true;
  if (actual->isAssignableTo(expected))
    return true;
  if (expected->kind() == TypeKind::Union) {
    for (const auto *member : static_cast<const UnionType *>(expected)->members())
      if (isAssignable(actual, member))
        return true;
    return false;
  }
  if (expected->kind() == TypeKind::Nullable) {
    if (isBasic(actual, BasicKind::Null))
      return true;
    const auto *nullable = static_cast<const NullableType *>(expected);
    if (actual->kind() == TypeKind::Nullable)
      return isAssignable(static_cast<const NullableType *>(actual)->base(), nullable->base());
    return isAssignable(actual, nullable->base());
  }
  if (actual->kind() == TypeKind::Nullable) {
    // A nullable value is not stored in a non-null slot.
    return false;
  }
  return false;
}

} // namespace kyna::types
