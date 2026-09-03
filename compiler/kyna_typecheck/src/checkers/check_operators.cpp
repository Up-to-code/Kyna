#include "check_helpers.hpp"
#include <kyna/lexing/token.hpp>
#include <kyna/semantics/program_analyzer.hpp>

namespace kyna {

TypeRef Analyzer::checkUnary(const Unary &n, SourceLocation loc) {
  auto x = expr(n.right);
  if (n.op == TokenKind::Bang)
    return analyzerNamedType("bool");
  if (x.name != "int" && x.name != "float" && x.name != "num" && x.name != "any")
    error("unary '-' requires a numeric operand", loc);
  return x;
}

TypeRef Analyzer::checkBinary(const Binary &n, SourceLocation loc) {
  auto a = expr(n.left), b = expr(n.right);
  if (n.op == TokenKind::EqualEqual || n.op == TokenKind::BangEqual ||
      n.op == TokenKind::AndAnd || n.op == TokenKind::OrOr || n.op == TokenKind::Less ||
      n.op == TokenKind::LessEqual || n.op == TokenKind::Greater ||
      n.op == TokenKind::GreaterEqual)
    return analyzerNamedType("bool");
  if (n.op == TokenKind::Plus && (a.name == "str" || b.name == "str"))
    return analyzerNamedType("str");
  if (a.name == "any" || b.name == "any")
    return analyzerNamedType("any");
  if ((a.name == "int" || a.name == "float" || a.name == "num" || a.name == "any") &&
      (b.name == "int" || b.name == "float" || b.name == "num" || b.name == "any"))
    return (a.name == "float" || b.name == "float")
               ? analyzerNamedType("float")
               : (a.name == "int" && b.name == "int" ? analyzerNamedType("int")
                                                     : analyzerNamedType("num"));
  error("operator requires compatible operands", loc);
  return analyzerNamedType("any");
}

TypeRef Analyzer::checkAssign(const Assign &n, SourceLocation loc) {
  auto a = expr(n.target), b = expr(n.value);
  if (auto v = std::get_if<Variable>(&n.target->node)) {
    if (auto bs = bindingScope(v->name)) {
      if (!bs->mutableBindings[v->name])
        error("cannot assign to immutable binding '" + v->name + "'", loc);
      if (!compatible(bs->types[v->name], b))
        error("cannot assign " + b.str() + " to " + bs->types[v->name].str(), loc);
    }
  } else if (!std::holds_alternative<Member>(n.target->node) &&
             !std::holds_alternative<Index>(n.target->node))
    error("invalid assignment target", loc);
  return a;
}

} // namespace kyna
