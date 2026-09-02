#include "check_helpers.hpp"
#include "kyna/semantics/program_analyzer.hpp"

#include <set>

namespace kyna {

TypeRef Analyzer::checkIfExpr(const IfExpr &n, SourceLocation loc) {
  auto c = expr(n.condition);
  if (c.name != "bool" && c.name != "any")
    error("if condition must be bool", loc);
  const auto branchType = [&](const StmtPtr &branch) {
    const auto &block = std::get<BlockStmt>(branch->node);
    auto parentScope = scope;
    scope = std::make_shared<Scope>();
    scope->parent = parentScope;
    for (const auto &statement : block.statements)
      if (const auto *function = std::get_if<FunctionDecl>(&statement->node)) {
        scope->types[function->name] = analyzerNamedType("func");
        scope->mutableBindings[function->name] = false;
      }
    for (const auto &statement : block.statements)
      stmt(statement);
    const auto result = block.tail ? expr(block.tail) : analyzerNamedType("void");
    scope = parentScope;
    return result;
  };
  auto a = branchType(n.thenBranch);
  auto b = branchType(n.elseBranch);
  return merge(a, b);
}

TypeRef Analyzer::checkMatch(const MatchExpr &n, SourceLocation loc) {
  const auto subject = expr(n.subject);
  TypeRef r = analyzerNamedType("void");
  bool first = true;
  bool wildcardSeen = false;
  bool trueSeen = false;
  bool falseSeen = false;
  std::set<std::string> literalPatterns;
  for (std::size_t index = 0; index < n.arms.size(); ++index) {
    auto &a = n.arms[index];
    if (a.wildcard) {
      if (wildcardSeen || index + 1 != n.arms.size())
        error("match arm is unreachable after a wildcard arm", loc, "KSEM1402",
              "keep exactly one wildcard arm and place it last");
      wildcardSeen = true;
    } else {
      if (wildcardSeen)
        error("match arm is unreachable after a wildcard arm", a.pattern->location, "KSEM1402",
              "move the wildcard arm to the end");
      const auto patternType = expr(a.pattern);
      if (!compatible(subject, patternType))
        error("match pattern has type " + patternType.str() + ", but subject has type " +
                  subject.str(),
              a.pattern->location, "KSEM1404", "use a pattern compatible with the matched value");
      if (const auto *literal = std::get_if<Literal>(&a.pattern->node)) {
        const auto key = std::to_string(static_cast<int>(literal->kind)) + ':' + literal->value;
        if (!literalPatterns.insert(key).second)
          error("duplicate literal pattern in match", a.pattern->location, "KSEM1403",
                "remove the duplicate arm or use a different pattern");
        if (literal->kind == Literal::Kind::Bool)
          (literal->value == "true" ? trueSeen : falseSeen) = true;
      }
    }
    auto arm = expr(a.value);
    if (first) {
      r = arm;
      first = false;
    } else
      r = merge(r, arm);
  }
  if (!wildcardSeen && !(subject.name == "bool" && trueSeen && falseSeen))
    error("match expression is not exhaustive", loc, "KSEM1401", "add a final '_ => value;' arm");
  return r;
}

} // namespace kyna
