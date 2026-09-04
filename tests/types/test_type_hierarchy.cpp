#include <kyna/types/type.hpp>
#include <kyna/types/basic_type.hpp>
#include <kyna/types/signature_type.hpp>
#include <kyna/types/named_type.hpp>
#include <kyna/types/compound_type.hpp>
#include <kyna/types/type_bridge.hpp>
#include <kyna/types/alias_type.hpp>
#include <kyna/types/array_type.hpp>
#include <kyna/types/channel_type.hpp>
#include <kyna/types/interface_type.hpp>
#include <kyna/types/map_type.hpp>
#include <kyna/types/pointer_type.hpp>
#include <kyna/types/slice_type.hpp>
#include <kyna/types/struct_type.hpp>
#include <kyna/types/tuple_type.hpp>
#include <kyna/types/type_parameter.hpp>

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
  assert(Universe::Int()->isAssignableTo(Universe::Float()));
  assert(!Universe::Float()->isAssignableTo(Universe::Int()));
  assert(Universe::Int()->isAssignableTo(Universe::Num()));
  assert(!Universe::Num()->isAssignableTo(Universe::Float()));
  assert(Universe::Int()->isAssignableTo(Universe::Any()));
  assert(Universe::String()->isAssignableTo(Universe::Any()));
  assert(!Universe::Null()->isAssignableTo(Universe::Void()));
  assert(!Universe::Void()->isAssignableTo(Universe::Null()));
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
  assert(!isAssignable(Universe::Void(), typeFromRef(nullableInt)));
  assert(!isAssignable(typeFromRef(nullableInt), Universe::Int()));
  assert(isAssignable(Universe::Int(), typeFromRef(any)));
  assert(isAssignable(typeFromRef(any), Universe::Int()));
}

void test_go_style_composite_types() {
  const auto *array = ArrayType::make(4, Universe::Int());
  assert(array == ArrayType::make(4, Universe::Int()));
  assert(array->str() == "[4]int");
  assert(!array->isIdenticalTo(ArrayType::make(5, Universe::Int())));

  const auto *slice = SliceType::make(Universe::String());
  assert(slice == SliceType::make(Universe::String()));
  assert(slice->str() == "array<str>");
  assert(slice->isAssignableTo(Universe::Array()));

  const auto *map = MapType::make(Universe::String(), Universe::Int());
  assert(map->key() == Universe::String());
  assert(map->value() == Universe::Int());
  assert(map->str() == "map<str, int>");

  const auto *pointer = PointerType::make(array);
  assert(pointer == PointerType::make(array));
  assert(pointer->base() == array);

  const auto *tuple = TupleType::make({Universe::Int(), Universe::Bool()});
  assert(tuple->str() == "(int, bool)");
  assert(tuple == TupleType::make({Universe::Int(), Universe::Bool()}));

  const auto *channel = ChannelType::make(Universe::Int());
  const auto *send = ChannelType::make(Universe::Int(), ChannelDirection::SendOnly);
  assert(channel->isAssignableTo(send));
  assert(!send->isAssignableTo(channel));
}

void test_struct_interfaces_named_methods_and_aliases() {
  const auto *point = StructType::make({{"x", Universe::Int(), {}, false},
                                        {"y", Universe::Int(), {}, false}});
  assert(point->field("x")->type == Universe::Int());
  assert(point == StructType::make({{"x", Universe::Int(), {}, false},
                                    {"y", Universe::Int(), {}, false}}));

  const auto *read = SignatureType::make({SliceType::make(Universe::Int())}, Universe::Int());
  const auto *reader = InterfaceType::make({{"read", read}});
  const auto *file = NamedType::make("TypeTestFile", Universe::Object(), {{"read", read, false}});
  assert(reader->isSatisfiedBy(file));
  assert(isAssignable(file, reader));

  const auto *close = SignatureType::make({}, Universe::Void());
  const auto *closer = InterfaceType::make({{"close", close}});
  const auto *socket = NamedType::make("TypeTestSocket", Universe::Object(),
                                      {{"close", close, true}});
  assert(!closer->isSatisfiedBy(socket));
  assert(closer->isSatisfiedBy(PointerType::make(socket)));

  const auto *parameter = TypeParam::make("T", reader);
  assert(parameter->constraint() == reader);
  const auto *alias = AliasType::make("Count", Universe::Int());
  assert(alias->isIdenticalTo(Universe::Int()));
  assert(alias->underlying() == Universe::Int());
}

void test_composite_type_bridge() {
  const kyna::TypeRef arrayOfInt{"array", false, {{"int", false, {}, {}}}, {}};
  const kyna::TypeRef stringToBool{
      "map", false, {{"str", false, {}, {}}, {"bool", false, {}, {}}}, {}};
  assert(typeFromRef(arrayOfInt)->kind() == TypeKind::Slice);
  assert(typeFromRef(stringToBool)->kind() == TypeKind::Map);
  assert(typeToRef(typeFromRef(arrayOfInt)).str() == "array<int>");
  assert(typeToRef(typeFromRef(stringToBool)).str() == "map<str, bool>");
}

} // namespace

int main() {
  test_pointer_identity_and_immutability();
  test_basic_assignability();
  test_signature_type();
  test_named_type();
  test_union_and_nullable();
  test_type_bridge_and_analyzer_assignability();
  test_go_style_composite_types();
  test_struct_interfaces_named_methods_and_aliases();
  test_composite_type_bridge();
  return 0;
}
