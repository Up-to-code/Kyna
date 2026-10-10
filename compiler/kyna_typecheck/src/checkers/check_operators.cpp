#include "check_helpers.hpp"
#include <kyna/lexing/token.hpp>
#include <kyna/semantics/program_analyzer.hpp>

namespace kyna {

TypeRef Analyzer::checkUnary(const Unary &n, SourceLocation loc) {
  auto x = expr(n.right);
  if (n.op == TokenKind::Bang) {
    if (x.name != "bool" && x.name != "any")
      error("'!' requires a boolean operand", loc, "KSEM1601");
    return analyzerNamedType("bool");
  }
  if (x.name != "int" && x.name != "float" && x.name != "num" && x.name != "any")
    error("unary '-' requires a numeric operand", loc);
  return x;
}

TypeRef Analyzer::checkBinary(const Binary &n, SourceLocation loc) {
  auto a = expr(n.left), b = expr(n.right);
  const auto numeric = [](const TypeRef &type) {
    return !type.nullable && (type.name == "int" || type.name == "float" || type.name == "num" || type.name == "any");
  };
  if (n.op == TokenKind::EqualEqual || n.op == TokenKind::BangEqual)
    return analyzerNamedType("bool");
  if (n.op == TokenKind::AndAnd || n.op == TokenKind::OrOr) {
    if ((a.name != "bool" && a.name != "any") || (b.name != "bool" && b.name != "any") || a.nullable || b.nullable)
      error("logical operators require boolean operands", loc, "KSEM1601");
    return analyzerNamedType("bool");
  }
  if (n.op == TokenKind::Less || n.op == TokenKind::LessEqual || n.op == TokenKind::Greater || n.op == TokenKind::GreaterEqual) {
    if (!numeric(a) || !numeric(b))
      error("ordered comparisons require numeric operands", loc, "KSEM1602");
    return analyzerNamedType("bool");
  }
  if (n.op == TokenKind::Percent) {
    if ((a.name != "int" && a.name != "any") || (b.name != "int" && b.name != "any") || a.nullable || b.nullable)
      error("'%' requires integer operands", loc, "KSEM1603");
    return analyzerNamedType("int");
  }
  if (n.op == TokenKind::Plus && (a.name == "str" || b.name == "str"))
    return analyzerNamedType("str");
  if (a.name == "any" || b.name == "any")
    return analyzerNamedType("any");
  if ((a.name == "int" || a.name == "float" || a.name == "num" || a.name == "any") &&
      (b.name == "int" || b.name == "float" || b.name == "num" || b.name == "any"))
    return (n.op == TokenKind::Slash || a.name == "float" || b.name == "float")
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
        error("cannot assign " + b.str() + " to " + bs->types[v->name].str(), loc, "KSEM1505");
    }
  } else if (!std::holds_alternative<Member>(n.target->node) &&
             !std::holds_alternative<Index>(n.target->node))
    error("invalid assignment target", loc);
  return a;
}

} // namespace kyna
