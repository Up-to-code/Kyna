#include "check_helpers.hpp"
#include "kyna/semantics/program_analyzer.hpp"

namespace kyna {

TypeRef Analyzer::expr(const ExprPtr &e) {
  return std::visit(
      [this, &e](const auto &n) -> TypeRef {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, Literal>) {
          switch (n.kind) {
          case Literal::Kind::Null:
            return analyzerNamedType("null");
          case Literal::Kind::Bool:
            return analyzerNamedType("bool");
          case Literal::Kind::Int:
            return analyzerNamedType("int");
          case Literal::Kind::Float:
            return analyzerNamedType("float");
          case Literal::Kind::String:
            return analyzerNamedType("str");
          case Literal::Kind::Char:
            return analyzerNamedType("char");
          }
        } else if constexpr (std::is_same_v<T, Variable>) {
          if (!defined(n.name) && !interactive)
            error("undefined name '" + n.name + "'", e->location);
          if (auto bs = bindingScope(n.name))
            return bs->types[n.name];
          if (functions.contains(n.name))
            return analyzerNamedType("func");
          if (classes.contains(n.name))
            return analyzerNamedType("class:" + n.name);
          return analyzerNamedType("any");
        } else if constexpr (std::is_same_v<T, SelfExpr>)
          return currentClass.empty() ? analyzerNamedType("object")
                                      : analyzerNamedType(currentClass);
        else if constexpr (std::is_same_v<T, SuperExpr>) {
          if (!currentClass.empty() && classes.contains(currentClass) &&
              !classes[currentClass].parent.empty())
            return analyzerNamedType(classes[currentClass].parent);
          return analyzerNamedType("object");
        } else if constexpr (std::is_same_v<T, Unary>)
          return checkUnary(n, e->location);
        else if constexpr (std::is_same_v<T, AwaitExpr>)
          return expr(n.operand);
        else if constexpr (std::is_same_v<T, Binary>)
          return checkBinary(n, e->location);
        else if constexpr (std::is_same_v<T, Assign>)
          return checkAssign(n, e->location);
        else if constexpr (std::is_same_v<T, Call>)
          return checkCall(n, e->location);
        else if constexpr (std::is_same_v<T, Member>)
          return checkMember(n, e->location);
        else if constexpr (std::is_same_v<T, Index>)
          return checkIndex(n, e->location);
        else if constexpr (std::is_same_v<T, ArrayExpr>) {
          for (auto &element : n.elements)
            expr(element);
          return analyzerNamedType("array");
        } else if constexpr (std::is_same_v<T, NewExpr>)
          return checkNew(n, e->location);
        else if constexpr (std::is_same_v<T, ObjectExpr>) {
          for (auto &f : n.fields)
            expr(f.value);
          return analyzerNamedType("object");
        } else if constexpr (std::is_same_v<T, IfExpr>)
          return checkIfExpr(n, e->location);
        else if constexpr (std::is_same_v<T, MatchExpr>)
          return checkMatch(n, e->location);
        else
          return analyzerNamedType("any");
      },
      e->node);
}

} // namespace kyna
