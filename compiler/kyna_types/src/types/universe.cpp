#include <kyna/types/type.hpp>
#include <kyna/types/basic_type.hpp>

namespace kyna::types {

// All 13 primitive singletons, statically initialized so every access is
// allocation-free and pointer-identity-stable for the process lifetime.
const BasicType *Universe::registry() {
  static const BasicType instants[] = {
      BasicType(BasicKind::Int, "int"),   BasicType(BasicKind::Float, "float"),
      BasicType(BasicKind::Bool, "bool"), BasicType(BasicKind::String, "str"),
      BasicType(BasicKind::Char, "char"), BasicType(BasicKind::Void, "void"),
      BasicType(BasicKind::Null, "null"), BasicType(BasicKind::Any, "any"),
      BasicType(BasicKind::Object, "object"), BasicType(BasicKind::Func, "func"),
      BasicType(BasicKind::Array, "array"), BasicType(BasicKind::Num, "num"),
      BasicType(BasicKind::Class, "class"),
  };
  return instants;
}

const BasicType *Universe::Int() { return &registry()[0]; }
const BasicType *Universe::Float() { return &registry()[1]; }
const BasicType *Universe::Bool() { return &registry()[2]; }
const BasicType *Universe::String() { return &registry()[3]; }
const BasicType *Universe::Char() { return &registry()[4]; }
const BasicType *Universe::Void() { return &registry()[5]; }
const BasicType *Universe::Null() { return &registry()[6]; }
const BasicType *Universe::Any() { return &registry()[7]; }
const BasicType *Universe::Object() { return &registry()[8]; }
const BasicType *Universe::Func() { return &registry()[9]; }
const BasicType *Universe::Array() { return &registry()[10]; }
const BasicType *Universe::Num() { return &registry()[11]; }
const BasicType *Universe::Class() { return &registry()[12]; }

} // namespace kyna::types
