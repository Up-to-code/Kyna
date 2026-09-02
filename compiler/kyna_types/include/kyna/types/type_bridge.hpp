#pragma once

#include "kyna/semantics/type_model.hpp"
#include "kyna/types/type.hpp"

#include <string_view>

namespace kyna::types {

// Maps a parser/AST TypeRef onto the interned type graph. Primitive names
// resolve to Universe singletons; unions, nullability, and nominal names
// become interned compound/named types. Unknown names become NamedType so
// class and interface identities survive the conversion.
TypePtr typeFromRef(const TypeRef &ref);

// Inverse of typeFromRef for values that still flow through TypeRef-shaped
// AST annotations. Rendering matches Type::str() for primitives and named
// types; unions and nullability are reconstructed.
TypeRef typeToRef(TypePtr type);

// Looks up a Universe primitive by source spelling (`int`, `str`, `func`, …).
// Returns nullptr when `name` is not a predeclared basic type.
TypePtr typeFromName(std::string_view name);

// Assignability used by the live analyzer: TypeScript-style `any` is a
// bidirectional escape hatch, null inhabits nullable types, and a value is
// assignable to a union when it is assignable to any member.
bool isAssignable(TypePtr actual, TypePtr expected);

} // namespace kyna::types
