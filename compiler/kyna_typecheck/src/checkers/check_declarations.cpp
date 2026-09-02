#include "check_helpers.hpp"
#include "kyna/semantics/program_analyzer.hpp"

#include <algorithm>
#include <set>

namespace kyna {

void Analyzer::checkVarDecl(const VarDecl &n, SourceLocation loc) {
  if (scope->types.contains(n.name))
    error("binding '" + n.name + "' is already declared in this scope", loc, "KSEM1102",
          "use assignment to update a mutable binding");
  if (!scope->parent && (functions.contains(n.name) || classes.contains(n.name)))
    error("binding '" + n.name + "' conflicts with a top-level declaration", loc, "KSEM1103",
          "choose a unique top-level name");
  for (auto p = scope->parent; p; p = p->parent)
    if (p->types.contains(n.name)) {
      warning("binding '" + n.name + "' shadows an outer binding", SourceLocation{});
      break;
    }
  if (n.initializer) {
    auto a = expr(n.initializer);
    if (n.hasType) {
      if (const auto *contract = interfaces.find(n.type.name))
        if (const auto *object = std::get_if<ObjectExpr>(&n.initializer->node))
          objectConforms(*object, *contract, n.initializer->location);
    }
    if (n.hasType && !compatible(n.type, a))
      error("initializer of '" + n.name + "' has type " + a.str() + ", expected " + n.type.str(),
            n.initializer->location);
    scope->types[n.name] = n.hasType ? n.type : a;
    scope->mutableBindings[n.name] = n.mutableBinding;
    bindLexical(n.name, scope->types[n.name], n.mutableBinding, loc, n.exported);
  } else {
    if (n.type.name != "any")
      error("binding '" + n.name + "' needs an initializer (or explicit any)", {1, 1});
    scope->types[n.name] = n.type;
    scope->mutableBindings[n.name] = n.mutableBinding;
    bindLexical(n.name, n.type, n.mutableBinding, loc, n.exported);
  }
}

void Analyzer::checkFunctionDecl(const FunctionDecl &n, SourceLocation loc) {
  auto old = scope;
  auto oldReturn = currentReturn;
  bool oldIn = inFunction;
  scope = std::make_shared<Scope>();
  scope->parent = old;
  std::set<std::string> parameterNames;
  for (auto &p : n.params) {
    if (!parameterNames.insert(p.name).second)
      error("parameter '" + p.name + "' is declared more than once in function '" + n.name + "'",
            loc, "KSEM1104", "give every parameter a unique name");
    scope->types[p.name] = p.type;
    scope->mutableBindings[p.name] = false;
  }
  currentReturn = n.hasReturnType ? n.returnType : analyzerNamedType("any");
  inFunction = true;
  stmt(n.body);
  if (n.hasReturnType && n.returnType.name != "void" && !alwaysReturns(n.body))
    error("function '" + n.name + "' can reach its end without returning " + n.returnType.str(),
          SourceLocation{});
  scope = old;
  currentReturn = oldReturn;
  inFunction = oldIn;
}

void Analyzer::checkClassDecl(const ClassDecl &n, SourceLocation loc) {
  if (hasModifier(n.modifiers, "abstract") && hasModifier(n.modifiers, "final"))
    error("class '" + n.name + "' cannot be both abstract and final", loc);
  if (!n.parent.empty() && classes.contains(n.parent)) {
    auto &base = classes[n.parent];
    if (hasModifier(base.modifiers, "final"))
      error("cannot extend final class '" + n.parent + "'", SourceLocation{});
    for (auto &m : n.methods) {
      auto inherited = std::find_if(base.methods.begin(), base.methods.end(),
                                    [&](const auto &x) { return x.name == m.name; });
      if (hasModifier(m.modifiers, "override") && inherited == base.methods.end())
        error("method '" + m.name + "' is marked override but no inherited method exists",
              SourceLocation{});
      if (inherited != base.methods.end() && !hasModifier(m.modifiers, "override") &&
          m.name != "init")
        error("overriding method '" + m.name + "' requires the override modifier",
              SourceLocation{});
      if (inherited != base.methods.end() && hasModifier(inherited->modifiers, "final"))
        error("cannot override final method '" + m.name + "'", SourceLocation{});
      if (inherited != base.methods.end() && m.hasReturnType && inherited->hasReturnType &&
          !compatible(inherited->returnType, m.returnType))
        error("override return type for '" + m.name + "' is incompatible", loc);
      if (inherited != base.methods.end() && !analyzerSameParameters(m, *inherited))
        error("override parameters for '" + m.name + "' must match the inherited method", loc);
      if (inherited != base.methods.end() &&
          analyzerMemberVisibility(m.modifiers) < analyzerMemberVisibility(inherited->modifiers))
        error("override of '" + m.name + "' cannot narrow visibility", loc);
    }
  }
  for (const auto &contractRef : n.interfaces) {
    const auto *contract = interfaces.find(contractRef.name);
    if (!contract)
      error("unknown interface '" + contractRef.name + "'", loc);
    else
      classConforms(n, *contract, contractRef, loc);
  }
  const bool abstractClass = hasModifier(n.modifiers, "abstract");
  for (auto &m : n.methods) {
    const bool abstractMethod = hasModifier(m.modifiers, "abstract");
    if (abstractMethod && !abstractClass)
      error("abstract method '" + m.name + "' requires an abstract class", loc);
    if (abstractMethod && m.body)
      error("abstract method '" + m.name + "' cannot have a body", loc);
    if (!abstractMethod && !m.body)
      error("concrete method '" + m.name + "' requires a body", loc);
    if (!m.body)
      continue;
    auto old = scope;
    auto oldReturn = currentReturn;
    bool oldIn = inFunction;
    auto oldClass = currentClass;
    currentClass = n.name;
    scope = std::make_shared<Scope>();
    scope->parent = old;
    scope->types["self"] = analyzerNamedType(n.name);
    scope->mutableBindings["self"] = false;
    for (auto &f : n.fields) {
      scope->types[f.name] = f.type;
      scope->mutableBindings[f.name] = true;
    }
    std::set<std::string> parameterNames;
    for (auto &p : m.params) {
      if (!parameterNames.insert(p.name).second)
        error("parameter '" + p.name + "' is declared more than once in method '" + m.name + "'",
              loc, "KSEM1104", "give every parameter a unique name");
      scope->types[p.name] = p.type;
      scope->mutableBindings[p.name] = false;
    }
    currentReturn = m.hasReturnType ? m.returnType : analyzerNamedType("any");
    inFunction = true;
    stmt(m.body);
    if (m.hasReturnType && m.returnType.name != "void" && !alwaysReturns(m.body))
      error("method '" + m.name + "' can reach its end without returning " + m.returnType.str(),
            SourceLocation{});
    scope = old;
    currentReturn = oldReturn;
    inFunction = oldIn;
    currentClass = std::move(oldClass);
  }
  if (!abstractClass && !n.parent.empty() && classes.contains(n.parent)) {
    for (const auto &inherited : classes[n.parent].methods) {
      if (!hasModifier(inherited.modifiers, "abstract"))
        continue;
      const auto implementation =
          std::find_if(n.methods.begin(), n.methods.end(), [&](const FunctionDecl &method) {
            return method.name == inherited.name;
          });
      if (implementation == n.methods.end() || hasModifier(implementation->modifiers, "abstract"))
        error("concrete class '" + n.name + "' must implement abstract method '" + inherited.name +
                  "'",
              loc);
    }
  }
  if (!n.parent.empty() && !classes.contains(n.parent))
    error("unknown parent class '" + n.parent + "'", {1, 1});
}

} // namespace kyna
