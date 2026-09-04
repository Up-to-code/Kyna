#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kyna::types {

// The category of a type object. Mirrors the top-level discriminator used by
// the polymorphic hierarchy so a visitor can dispatch without RTTI.
enum class TypeKind : uint8_t {
  Basic,
  Signature,
  Named,
  Union,
  Nullable,
  Array,
  Slice,
  Map,
  Pointer,
  Struct,
  Interface,
  Tuple,
  Channel,
  TypeParam,
  Alias,
};

// A type value exposed through the public API. Concrete types are immutable and
// interned; callers are handed a stable const pointer that never outlives the
// owning store, so equality and assignability can be answered structurally.
class Type {
public:
  virtual ~Type() = default;

  virtual TypeKind kind() const = 0;
  // The underlying type of a nominal type (itself for primitives/structural
  // types). Used to implement assignability for named aliases.
  virtual const Type *underlying() const = 0;
  // Human-readable rendering, primarily for diagnostics. Not a canonical key.
  virtual std::string str() const = 0;
  // True when a value of this type may be stored where `target` is expected.
  virtual bool isAssignableTo(const Type *target) const = 0;
  // Structural equality (identical shape and identity for nominal types).
  virtual bool isIdenticalTo(const Type *other) const = 0;
};

using TypePtr = const Type *;

// The well-known primitive kinds. Each maps to a process-wide singleton so
// pointer comparison suffices for cheap primitive tests.
enum class BasicKind : uint8_t {
  Int,
  Float,
  Bool,
  String,
  Char,
  Void,
  Null,
  Any,
  Object,
  Func,
  Array,
  Num,
  Class,
};

class BasicType;
class SignatureType;
class NamedType;
class UnionType;
class NullableType;
class ArrayType;
class SliceType;
class MapType;
class PointerType;
class StructType;
class InterfaceType;
class TupleType;
class ChannelType;
class TypeParam;
class AliasType;

// Zero-allocation registry of the primitive singleton instances. All of these
// return stable pointers that are valid for the lifetime of the process.
class Universe {
public:
  static const BasicType *Int();
  static const BasicType *Float();
  static const BasicType *Bool();
  static const BasicType *String();
  static const BasicType *Char();
  static const BasicType *Void();
  static const BasicType *Null();
  static const BasicType *Any();
  static const BasicType *Object();
  static const BasicType *Func();
  static const BasicType *Array();
  static const BasicType *Num();
  static const BasicType *Class();

private:
  friend class BasicType;
  static const BasicType *registry();
};

} // namespace kyna::types
