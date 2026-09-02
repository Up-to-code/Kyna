#include "kyna/types/type.hpp"
#include "kyna/types/basic_type.hpp"
#include "kyna/types/signature_type.hpp"
#include "kyna/types/named_type.hpp"
#include "kyna/types/compound_type.hpp"
#include "kyna/types/type_bridge.hpp"

#include <cassert>

namespace {

using namespace kyna::types;

void test_pointer_identity_and_immutability() {
  // The Universe returns stable singleton pointers: repeated access is identical
  // by address, so primitive checks can rely on pointer comparison.
  assert(Universe::Int() == Universe::Int());
  assert(Universe::Bool() == Universe::Bool());
  assert(Universe::String() != Universe::Int());
  assert(Universe::Void() != Universe::Null());
  assert(Universe::Any() == Universe::Any());

  // Basic kinds start void (default-constructed lookup must not influence the
  // singletons).
  assert(Universe::Int()->kind() == TypeKind::Basic);
  assert(Universe::Int()->basicKind() == BasicKind::Int);
  assert(Universe::String()->basicKind() == BasicKind::String);
}

void test_basic_assignability() {
  assert(Universe::Int()->isAssignableTo(Universe::Int()));
  // Numeric family is mutually assignable.
  assert(Universe::Int()->isAssignableTo(Universe::Float()));
  assert(Universe::Float()->isAssignableTo(Universe::Int()));
  assert(Universe::Int()->isAssignableTo(Universe::Num()));
  assert(Universe::Num()->isAssignableTo(Universe::Float()));
  // Any accepts everything.
  assert(Universe::Int()->isAssignableTo(Universe::Any()));
  assert(Universe::String()->isAssignableTo(Universe::Any()));
  // Unit types accept null/void only.
  assert(Universe::Null()->isAssignableTo(Universe::Void()));
  assert(Universe::Void()->isAssignableTo(Universe::Null()));
  assert(!Universe::String()->isAssignableTo(Universe::Bool()));
}

void test_signature_type() {
  auto sig = SignatureType::make({Universe::Int(), Universe::String()}, Universe::Bool());
  assert(sig->kind() == TypeKind::Signature);
  assert(sig->params().size() == 2);
  assert(sig->params()[0] == Universe::Int());
  assert(sig->params()[1] == Universe::String());
  assert(sig->returnType() == Universe::Bool());
  assert(!sig->isVariadic());

  // Identical signatures share a single interned instance.
  auto sigAgain = SignatureType::make({Universe::Int(), Universe::String()}, Universe::Bool());
  assert(sig == sigAgain);
  assert(sig->isIdenticalTo(sigAgain));

  // Different signatures are distinct.
  auto sig2 = SignatureType::make({Universe::Bool()}, Universe::Int());
  assert(sig != sig2);
  assert(!sig->isIdenticalTo(sig2));

  // A function value is assignable to the untyped "func" and "any" classifiers.
  assert(sig->isAssignableTo(Universe::Func()));
  assert(sig->isAssignableTo(Universe::Any()));
  assert(!sig->isAssignableTo(Universe::Int()));
}

void test_named_type() {
  auto circle = NamedType::make("Circle");
  assert(circle->kind() == TypeKind::Named);
  assert(circle->name() == "Circle");
  assert(circle->underlying() == circle); // no underlying -> itself
  assert(circle->isIdenticalTo(circle));

  auto circleAgain = NamedType::make("Circle");
  // Same name interns to the same nominal identity.
  assert(circle == circleAgain);

  auto square = NamedType::make("Square");
  assert(circle != square);
  assert(!circle->isIdenticalTo(square));
}

void test_union_and_nullable() {
  auto u = UnionType::make({Universe::Int(), Universe::String()});
  assert(u->kind() == TypeKind::Union);
  assert(u->members().size() == 2);
  // A union is assignable to any target that every member is assignable to.
  assert(u->isAssignableTo(Universe::Any()));
  assert(!u->isAssignableTo(Universe::Bool()));

  auto n = NullableType::make(Universe::Int());
  assert(n->kind() == TypeKind::Nullable);
  assert(n->base() == Universe::Int());
  assert(n->underlying() == Universe::Int());
  assert(n->isAssignableTo(Universe::Any()));
  assert(n->isAssignableTo(Universe::Object()));
  // Nullable int is not assignable to a plain int.
  assert(!n->isAssignableTo(Universe::Int()));
}

void test_type_bridge_and_analyzer_assignability() {
  kyna::TypeRef integer{"int", false, {}};
  kyna::TypeRef nullableInt{"int", true, {}};
  kyna::TypeRef any{"any", false, {}};
  assert(typeFromRef(integer) == Universe::Int());
  assert(isAssignable(Universe::Int(), Universe::Num()));
  assert(isAssignable(Universe::Int(), typeFromRef(nullableInt)));
  assert(isAssignable(Universe::Null(), typeFromRef(nullableInt)));
  assert(!isAssignable(typeFromRef(nullableInt), Universe::Int()));
  assert(isAssignable(Universe::Int(), typeFromRef(any)));
  assert(isAssignable(typeFromRef(any), Universe::Int()));
}

} // namespace

int main() {
  test_pointer_identity_and_immutability();
  test_basic_assignability();
  test_signature_type();
  test_named_type();
  test_union_and_nullable();
  test_type_bridge_and_analyzer_assignability();
  return 0;
}
