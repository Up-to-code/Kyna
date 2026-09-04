#include "check_helpers.hpp"
#include <kyna/semantics/program_analyzer.hpp>

#include <algorithm>
#include <set>

namespace kyna {

void Analyzer::checkBlock(const BlockStmt &n, SourceLocation) {
  auto old = scope;
  auto *oldLexical = lexical;
  scope = std::make_shared<Scope>();
  scope->parent = old;
  if (lexical)
    lexical = lexical->createChild();
  for (const auto &statement : n.statements)
    if (const auto *function = std::get_if<FunctionDecl>(&statement->node)) {
      if (scope->types.contains(function->name))
        error("function '" + function->name + "' is already declared in this block",
              statement->location, "KSEM1102", "rename or remove one of the nested declarations");
      scope->types[function->name] = analyzerNamedType("func");
      scope->mutableBindings[function->name] = false;
    }
  for (auto &x : n.statements)
    stmt(x);
  if (n.tail)
    expr(n.tail);
  scope = old;
  lexical = oldLexical;
}

void Analyzer::checkIf(const IfStmt &n) {
  auto c = expr(n.condition);
  if (c.name != "bool" && c.name != "any")
    error("if condition must be bool", n.condition->location);
  stmt(n.thenBranch);
  if (n.elseBranch)
    stmt(n.elseBranch);
}

void Analyzer::checkWhile(const WhileStmt &n) {
  auto condition = expr(n.condition);
  if (condition.name != "bool" && condition.name != "any")
    error("while condition must be bool", n.condition->location, "KSEM1304",
          "compare the value or convert it to bool explicitly");
  activeLoopLabels.push_back(n.label);
  stmt(n.body);
  activeLoopLabels.pop_back();
}

void Analyzer::checkLoop(const LoopStmt &n, SourceLocation loc) {
  if (!n.label.empty() &&
      std::find(activeLoopLabels.begin(), activeLoopLabels.end(), n.label) !=
          activeLoopLabels.end())
    error("loop label '" + n.label + "' is already active", loc, "KSEM1303",
          "use a unique label for each nested loop");
  if (n.initializer)
    stmt(n.initializer);
  if (n.condition) {
    auto condition = expr(n.condition);
    if (condition.name != "bool" && condition.name != "any")
      error("loop condition must be bool", n.condition->location, "KSEM1304",
            "compare the value or convert it to bool explicitly");
  }
  if (n.increment)
    expr(n.increment);
  activeLoopLabels.push_back(n.label);
  stmt(n.body);
  activeLoopLabels.pop_back();
}

void Analyzer::checkSwitch(const SwitchStmt &n, SourceLocation loc) {
  const auto subject = expr(n.subject);
  bool seenDefault = false;
  for (const auto &arm : n.cases) {
    if (arm.isDefault) {
      if (seenDefault)
        error("duplicate default arm in switch", loc);
      seenDefault = true;
      continue;
    }
    const auto value = expr(arm.value);
    if (!compatible(subject, value))
      error("case value has type " + value.str() + ", but subject has type " + subject.str(),
            arm.value->location);
  }
  ++switchDepth;
  for (const auto &arm : n.cases)
    stmt(arm.body);
  --switchDepth;
}

void Analyzer::checkTry(const TryStmt &n) {
  stmt(n.tryBranch);
  if (n.catchBranch) {
    auto old = scope;
    scope = std::make_shared<Scope>();
    scope->parent = old;
    scope->types[n.catchName] = analyzerNamedType("Error");
    scope->mutableBindings[n.catchName] = false;
    stmt(n.catchBranch);
    scope = old;
  }
  if (n.finallyBranch)
    stmt(n.finallyBranch);
}

void Analyzer::checkBreak(const BreakStmt &n, SourceLocation loc) {
  if (activeLoopLabels.empty() && switchDepth == 0)
    error("break must be inside a switch or loop", loc, "KSEM1301",
          "remove it or move it into a switch or loop body");
  else if (!n.label.empty() &&
           std::find(activeLoopLabels.rbegin(), activeLoopLabels.rend(), n.label) ==
               activeLoopLabels.rend())
    error("break references unknown loop label '" + n.label + "'", loc, "KSEM1302",
          "use the label of an enclosing loop");
}

void Analyzer::checkContinue(const ContinueStmt &n, SourceLocation loc) {
  if (activeLoopLabels.empty())
    error("continue must be inside a loop", loc, "KSEM1301",
          "remove it or move it into a loop body");
  else if (!n.label.empty() &&
           std::find(activeLoopLabels.rbegin(), activeLoopLabels.rend(), n.label) ==
               activeLoopLabels.rend())
    error("continue references unknown loop label '" + n.label + "'", loc, "KSEM1302",
          "use the label of an enclosing loop");
}

void Analyzer::checkReturn(const ReturnStmt &n) {
  if (!inFunction)
    error("return must be inside a function", n.value ? n.value->location : SourceLocation{});
  TypeRef actual = n.value ? expr(n.value) : analyzerNamedType("void");
  if (inFunction && !compatible(currentReturn, actual))
    error("return type " + actual.str() + " does not satisfy " + currentReturn.str(),
          n.value ? n.value->location : SourceLocation{});
}

} // namespace kyna
