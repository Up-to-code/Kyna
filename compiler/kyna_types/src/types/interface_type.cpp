// Implements structural interface method-set identity and satisfaction.
#include <kyna/types/interface_type.hpp>
#include <kyna/types/basic_type.hpp>
#include <kyna/types/named_type.hpp>
#include <kyna/types/pointer_type.hpp>

#include <set>
#include <stdexcept>

namespace kyna::types {
namespace {
struct Less {
  bool operator()(const InterfaceType *left, const InterfaceType *right) const {
    return left->str() < right->str();
  }
};
std::set<InterfaceType *, Less> &pool() {
  static std::set<InterfaceType *, Less> values;
  return values;
}

bool provides(const Type *candidate, const InterfaceMethod &required, bool throughPointer) {
  if (candidate->kind() == TypeKind::Interface) {
    const auto *contract = static_cast<const InterfaceType *>(candidate);
    for (const auto &method : contract->methods())
      if (method.name == required.name && method.signature->isIdenticalTo(required.signature))
        return true;
    for (const auto *parent : contract->embedded())
      if (parent->kind() == TypeKind::Interface && provides(parent, required, false))
        return true;
    return false;
  }
  if (candidate->kind() == TypeKind::Pointer) {
    return provides(static_cast<const PointerType *>(candidate)->base(), required, true);
  }
  if (candidate->kind() != TypeKind::Named)
    return false;
  for (const auto &method : static_cast<const NamedType *>(candidate)->methods())
    if (method.name == required.name && (!method.pointerReceiver || throughPointer) &&
        method.signature->isIdenticalTo(required.signature))
      return true;
  return false;
}
} // namespace

const InterfaceType *InterfaceType::make(std::vector<InterfaceMethod> methods,
                                         std::vector<TypePtr> embedded) {
  std::set<std::string> names;
  for (const auto &method : methods)
    if (!names.insert(method.name).second)
      throw std::invalid_argument("interface contains duplicate method '" + method.name + "'");
  auto *created = new InterfaceType(std::move(methods), std::move(embedded));
  const auto [found, inserted] = pool().insert(created);
  if (!inserted)
    delete created;
  return *found;
}

bool InterfaceType::isSatisfiedBy(const Type *candidate) const {
  if (!candidate)
    return false;
  for (const auto &parent : embedded_) {
    if (parent->kind() != TypeKind::Interface ||
        !static_cast<const InterfaceType *>(parent)->isSatisfiedBy(candidate))
      return false;
  }
  for (const auto &required : methods_)
    if (!provides(candidate, required, false))
      return false;
  return true;
}

std::string InterfaceType::str() const {
  std::string result = "interface{";
  bool needsSeparator = false;
  for (const auto *parent : embedded_) {
    if (needsSeparator)
      result += "; ";
    result += parent->str();
    needsSeparator = true;
  }
  for (const auto &method : methods_) {
    if (needsSeparator)
      result += "; ";
    result += method.name + method.signature->str();
    needsSeparator = true;
  }
  return result + "}";
}

bool InterfaceType::isAssignableTo(const Type *target) const {
  if (isIdenticalTo(target) || isBasic(target, BasicKind::Any))
    return true;
  return target && target->kind() == TypeKind::Interface &&
         static_cast<const InterfaceType *>(target)->isSatisfiedBy(this);
}

bool InterfaceType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Interface)
    return false;
  const auto *contract = static_cast<const InterfaceType *>(other);
  if (methods_.size() != contract->methods_.size() || embedded_.size() != contract->embedded_.size())
    return false;
  for (std::size_t index = 0; index < methods_.size(); ++index)
    if (methods_[index].name != contract->methods_[index].name ||
        !methods_[index].signature->isIdenticalTo(contract->methods_[index].signature))
      return false;
  for (std::size_t index = 0; index < embedded_.size(); ++index)
    if (!embedded_[index]->isIdenticalTo(contract->embedded_[index]))
      return false;
  return true;
}
} // namespace kyna::types
