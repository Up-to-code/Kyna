#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/semantics/modifier_query.hpp"
#include <algorithm>
#include <set>
#include <type_traits>

namespace kyna {
namespace {
TypeRef t(const std::string &name) { return TypeRef{name, false, {}}; }
int visibility(const std::vector<std::string> &modifiers) {
  if (hasModifier(modifiers, "public")) return 2;
  if (hasModifier(modifiers, "protected")) return 1;
  return 0;
}
bool sameParameters(const FunctionDecl &left, const FunctionDecl &right) {
  if (left.params.size() != right.params.size()) return false;
  for (std::size_t index = 0; index < left.params.size(); ++index)
    if (left.params[index].type.str() != right.params[index].type.str()) return false;
  return true;
}
} // namespace

void Analyzer::stmt(const StmtPtr &s) {
  std::visit(
      [this, &s](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, VarDecl>) {
          if (scope->types.contains(n.name))
            error("binding '" + n.name + "' is already declared in this scope", s->location,
                  "KSEM1102", "use assignment to update a mutable binding");
          if (!scope->parent && (functions.contains(n.name) || classes.contains(n.name)))
            error("binding '" + n.name + "' conflicts with a top-level declaration",
                  s->location, "KSEM1103", "choose a unique top-level name");
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
              error("initializer of '" + n.name + "' has type " + a.str() + ", expected " +
                        n.type.str(),
                    n.initializer->location);
            scope->types[n.name] = n.hasType ? n.type : a;
            scope->mutableBindings[n.name] = n.mutableBinding;
          } else {
            if (n.type.name != "any")
              error("binding '" + n.name + "' needs an initializer (or explicit any)", {1, 1});
            scope->types[n.name] = n.type;
            scope->mutableBindings[n.name] = n.mutableBinding;
          }
        } else if constexpr (std::is_same_v<T, ExprStmt>)
          expr(n.expression);
        else if constexpr (std::is_same_v<T, BlockStmt>) {
          auto old = scope;
          scope = std::make_shared<Scope>();
          scope->parent = old;
          for (const auto &statement : n.statements)
            if (const auto *function = std::get_if<FunctionDecl>(&statement->node)) {
              if (scope->types.contains(function->name))
                error("function '" + function->name +
                          "' is already declared in this block",
                      statement->location, "KSEM1102",
                      "rename or remove one of the nested declarations");
              scope->types[function->name] = t("func");
              scope->mutableBindings[function->name] = false;
            }
          for (auto &x : n.statements)
            stmt(x);
          if (n.tail)
            expr(n.tail);
          scope = old;
        } else if constexpr (std::is_same_v<T, IfStmt>) {
          auto c = expr(n.condition);
          if (c.name != "bool" && c.name != "any")
            error("if condition must be bool", n.condition->location);
          stmt(n.thenBranch);
          if (n.elseBranch)
            stmt(n.elseBranch);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
          auto condition = expr(n.condition);
          if (condition.name != "bool" && condition.name != "any")
            error("while condition must be bool", n.condition->location, "KSEM1304",
                  "compare the value or convert it to bool explicitly");
          activeLoopLabels.push_back(n.label);
          stmt(n.body);
          activeLoopLabels.pop_back();
        } else if constexpr (std::is_same_v<T, LoopStmt>) {
          if (!n.label.empty() &&
              std::find(activeLoopLabels.begin(), activeLoopLabels.end(), n.label) !=
                  activeLoopLabels.end())
            error("loop label '" + n.label + "' is already active", s->location, "KSEM1303",
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
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
          const auto subject = expr(n.subject);
          bool seenDefault = false;
          for (const auto &arm : n.cases) {
            if (arm.isDefault) {
              if (seenDefault)
                error("duplicate default arm in switch", s->location);
              seenDefault = true;
              continue;
            }
            const auto value = expr(arm.value);
            if (!compatible(subject, value))
              error("case value has type " + value.str() + ", but subject has type " +
                        subject.str(),
                    arm.value->location);
          }
          ++switchDepth;
          for (const auto &arm : n.cases)
            stmt(arm.body);
          --switchDepth;
        } else if constexpr (std::is_same_v<T, TryStmt>) {
          stmt(n.tryBranch);
          if (n.catchBranch) {
            auto old = scope;
            scope = std::make_shared<Scope>();
            scope->parent = old;
            scope->types[n.catchName] = t("Error");
            scope->mutableBindings[n.catchName] = false;
            stmt(n.catchBranch);
            scope = old;
          }
          if (n.finallyBranch)
            stmt(n.finallyBranch);
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
          if (activeLoopLabels.empty() && switchDepth == 0)
            error("break must be inside a switch or loop", s->location, "KSEM1301",
                  "remove it or move it into a switch or loop body");
          else if (!n.label.empty() &&
                   std::find(activeLoopLabels.rbegin(), activeLoopLabels.rend(), n.label) ==
                       activeLoopLabels.rend())
            error("break references unknown loop label '" + n.label + "'", s->location,
                  "KSEM1302", "use the label of an enclosing loop");
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
          if (activeLoopLabels.empty())
            error("continue must be inside a loop", s->location, "KSEM1301",
                  "remove it or move it into a loop body");
          else if (!n.label.empty() &&
                   std::find(activeLoopLabels.rbegin(), activeLoopLabels.rend(), n.label) ==
                       activeLoopLabels.rend())
            error("continue references unknown loop label '" + n.label + "'", s->location,
                  "KSEM1302", "use the label of an enclosing loop");
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
          if (!inFunction)
            error("return must be inside a function",
                  n.value ? n.value->location : SourceLocation{});
          TypeRef actual = n.value ? expr(n.value) : t("void");
          if (inFunction && !compatible(currentReturn, actual))
            error("return type " + actual.str() + " does not satisfy " + currentReturn.str(),
                  n.value ? n.value->location : SourceLocation{});
        } else if constexpr (std::is_same_v<T, ThrowStmt>) {
          expr(n.value);
        } else if constexpr (std::is_same_v<T, FunctionDecl>) {
          auto old = scope;
          auto oldReturn = currentReturn;
          bool oldIn = inFunction;
          scope = std::make_shared<Scope>();
          scope->parent = old;
          std::set<std::string> parameterNames;
          for (auto &p : n.params) {
            if (!parameterNames.insert(p.name).second)
              error("parameter '" + p.name + "' is declared more than once in function '" +
                        n.name + "'",
                    s->location, "KSEM1104", "give every parameter a unique name");
            scope->types[p.name] = p.type;
            scope->mutableBindings[p.name] = false;
          }
          currentReturn = n.hasReturnType ? n.returnType : t("any");
          inFunction = true;
          stmt(n.body);
          if (n.hasReturnType && n.returnType.name != "void" && !alwaysReturns(n.body))
            error("function '" + n.name + "' can reach its end without returning " +
                      n.returnType.str(),
                  SourceLocation{});
          scope = old;
          currentReturn = oldReturn;
          inFunction = oldIn;
        } else if constexpr (std::is_same_v<T, ClassDecl>) {
          if (hasModifier(n.modifiers, "abstract") && hasModifier(n.modifiers, "final"))
            error("class '" + n.name + "' cannot be both abstract and final", s->location);
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
                error("override return type for '" + m.name + "' is incompatible", s->location);
              if (inherited != base.methods.end() && !sameParameters(m, *inherited))
                error("override parameters for '" + m.name + "' must match the inherited method",
                      s->location);
              if (inherited != base.methods.end() &&
                  visibility(m.modifiers) < visibility(inherited->modifiers))
                error("override of '" + m.name + "' cannot narrow visibility", s->location);
            }
          }
          for (const auto &contractRef : n.interfaces) {
            const auto *contract = interfaces.find(contractRef.name);
            if (!contract)
              error("unknown interface '" + contractRef.name + "'", s->location);
            else
              classConforms(n, *contract, contractRef, s->location);
          }
          const bool abstractClass = hasModifier(n.modifiers, "abstract");
          for (auto &m : n.methods) {
            const bool abstractMethod = hasModifier(m.modifiers, "abstract");
            if (abstractMethod && !abstractClass)
              error("abstract method '" + m.name + "' requires an abstract class", s->location);
            if (abstractMethod && m.body)
              error("abstract method '" + m.name + "' cannot have a body", s->location);
            if (!abstractMethod && !m.body)
              error("concrete method '" + m.name + "' requires a body", s->location);
            if (!m.body)
              continue;
            auto old = scope;
            auto oldReturn = currentReturn;
            bool oldIn = inFunction;
            auto oldClass = currentClass;
            currentClass = n.name;
            scope = std::make_shared<Scope>();
            scope->parent = old;
            scope->types["self"] = t(n.name);
            scope->mutableBindings["self"] = false;
            for (auto &f : n.fields) {
              scope->types[f.name] = f.type;
              scope->mutableBindings[f.name] = true;
            }
            std::set<std::string> parameterNames;
            for (auto &p : m.params) {
              if (!parameterNames.insert(p.name).second)
                error("parameter '" + p.name + "' is declared more than once in method '" +
                          m.name + "'",
                      s->location, "KSEM1104", "give every parameter a unique name");
              scope->types[p.name] = p.type;
              scope->mutableBindings[p.name] = false;
            }
            currentReturn = m.hasReturnType ? m.returnType : t("any");
            inFunction = true;
            stmt(m.body);
            if (m.hasReturnType && m.returnType.name != "void" && !alwaysReturns(m.body))
              error("method '" + m.name + "' can reach its end without returning " +
                        m.returnType.str(),
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
              if (implementation == n.methods.end() ||
                  hasModifier(implementation->modifiers, "abstract"))
                error("concrete class '" + n.name + "' must implement abstract method '" +
                          inherited.name + "'",
                      s->location);
            }
          }
          if (!n.parent.empty() && !classes.contains(n.parent))
            error("unknown parent class '" + n.parent + "'", {1, 1});
        } else if constexpr (std::is_same_v<T, InterfaceDecl>) {
        }
      },
      s->node);
}

bool Analyzer::alwaysReturns(const StmtPtr &s) const {
  if (!s)
    return false;
  if (std::holds_alternative<ReturnStmt>(s->node) ||
      std::holds_alternative<ThrowStmt>(s->node))
    return true;
  if (auto b = std::get_if<BlockStmt>(&s->node)) {
    for (auto &x : b->statements)
      if (alwaysReturns(x))
        return true;
    return false;
  }
  if (auto i = std::get_if<IfStmt>(&s->node))
    return i->elseBranch && alwaysReturns(i->thenBranch) && alwaysReturns(i->elseBranch);
  if (auto attempt = std::get_if<TryStmt>(&s->node)) {
    if (attempt->finallyBranch && alwaysReturns(attempt->finallyBranch))
      return true;
    if (attempt->catchBranch)
      return alwaysReturns(attempt->tryBranch) && alwaysReturns(attempt->catchBranch);
    return alwaysReturns(attempt->tryBranch);
  }
  if (auto sw = std::get_if<SwitchStmt>(&s->node)) {
    bool defaultSeen = false;
    for (const auto &arm : sw->cases) {
      if (!alwaysReturns(arm.body))
        return false;
      if (arm.isDefault)
        defaultSeen = true;
    }
    return defaultSeen;
  }
  return false;
}

} // namespace kyna
