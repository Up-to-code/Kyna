#include "kyna/parsing/recursive_descent_parser.hpp"

namespace kyna {
TypeRef Parser::typeRef() {
  Token t = peek();
  if (!(check(TokenKind::Identifier) || check(TokenKind::IntType) || check(TokenKind::FloatType) ||
        check(TokenKind::NumType) || check(TokenKind::StrType) || check(TokenKind::CharType) ||
        check(TokenKind::BoolType) || check(TokenKind::Null) || check(TokenKind::VoidType) ||
        check(TokenKind::AnyType)))
    throw KynaError({"expected a type", peek().location, false});
  ++current;
  TypeRef r{t.lexeme, false, {}};
  // Generic instantiation: Name<T, U, ...>. Also supports nested types.
  while (match(TokenKind::Less)) {
    TypeRef arg;
    do {
      arg.typeArgs.push_back(typeRef());
    } while (match(TokenKind::Comma));
    consume(TokenKind::Greater, "expected '>' after type arguments");
    TypeRef specialized;
    specialized.name = r.name;
    specialized.typeArgs = std::move(arg.typeArgs);
    specialized.nullable = r.nullable;
    r = std::move(specialized);
  }
  if (match(TokenKind::Question))
    r.nullable = true;
  while (match(TokenKind::Pipe)) {
    TypeRef other = typeRef();
    r.unionTypes.push_back(std::move(other));
  }
  return r;
}

} // namespace kyna
