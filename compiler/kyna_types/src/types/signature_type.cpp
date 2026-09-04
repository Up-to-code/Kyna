#include <kyna/types/signature_type.hpp>
#include <kyna/types/basic_type.hpp>

#include <set>
#include <string>

namespace kyna::types {
namespace {

// Intern pool: signatures are immutable and compared structurally, so equal
// signatures share one instance. The pool intentionally leaks (types live for
// the process lifetime) to keep returned pointers stable.

struct SignatureCompare {
  bool operator()(const SignatureType *lhs, const SignatureType *rhs) const {
    return key(*lhs) < key(*rhs);
  }
  static std::string key(const SignatureType &s) {
    std::string k = s.str();
    k.push_back(s.isVariadic() ? '1' : '0');
    return k;
  }
};

std::set<SignatureType *, SignatureCompare> &sigPool() {
  static std::set<SignatureType *, SignatureCompare> pool;
  return pool;
}

} // namespace

SignatureType::SignatureType(std::vector<TypePtr> params, TypePtr returnType, TypePtr receiver,
                             bool isVariadic)
    : params_(std::move(params)),
      returnType_(returnType),
      receiverType_(receiver),
      isVariadic_(isVariadic) {}

const SignatureType *SignatureType::make(std::vector<TypePtr> params, TypePtr returnType,
                                         TypePtr receiver, bool isVariadic) {
  auto *created = new SignatureType(std::move(params), returnType, receiver, isVariadic);
  auto [it, inserted] = sigPool().insert(created);
  if (!inserted)
    delete created;
  return *it;
}

std::string SignatureType::str() const {
  std::string out = "func(";
  for (std::size_t index = 0; index < params_.size(); ++index) {
    if (index)
      out += ", ";
    out += params_[index]->str();
  }
  out += ") -> ";
  out += returnType_ ? returnType_->str() : "void";
  return out;
}

bool SignatureType::isAssignableTo(const Type *target) const {
  if (!target)
    return false;
  if (target->kind() == TypeKind::Basic) {
    const auto *basic = static_cast<const BasicType *>(target);
    // A function value is assignable to the untyped "func" classifier and to any.
    return basic->basicKind() == BasicKind::Func || basic->basicKind() == BasicKind::Any;
  }
  return isIdenticalTo(target);
}

bool SignatureType::isIdenticalTo(const Type *other) const {
  if (this == other)
    return true;
  if (!other || other->kind() != TypeKind::Signature)
    return false;
  const auto *that = static_cast<const SignatureType *>(other);
  if (isVariadic_ != that->isVariadic_ || params_.size() != that->params_.size())
    return false;
  for (std::size_t index = 0; index < params_.size(); ++index)
    if (!params_[index]->isIdenticalTo(that->params_[index]))
      return false;
  if (receiverType_ != that->receiverType_)
    return false;
  return (returnType_ == that->returnType_) ||
         (returnType_ && that->returnType_ && returnType_->isIdenticalTo(that->returnType_));
}

} // namespace kyna::types
