#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/semantics/modifier_query.hpp"
#include "kyna/symbols/standard_library_symbols.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace kyna {
namespace {
TypeRef t(const std::string &n) { return TypeRef{n, false, {}}; }

// Replace interface type parameters (mapped by name) with concrete arguments
// throughout a merged contract, so `extends Base<int>` binds T -> int.
TypeRef substituteContractType(const TypeRef &in, const std::map<std::string, TypeRef> &mapping,
                               int depth = 0) {
  if (depth > 32)
    return in;
  TypeRef out = in;
  if (const auto found = mapping.find(in.name); found != mapping.end()) {
    out = found->second;
    // A nullable reference (`T?`) keeps its nullability when substituted for
    // the concrete argument (`T?` -> `int?`).
    out.nullable = out.nullable || in.nullable;
    for (auto &u : out.unionTypes)
      u = substituteContractType(u, mapping, depth + 1);
  }
  for (auto &arg : out.typeArgs)
    arg = substituteContractType(arg, mapping, depth + 1);
  for (auto &u : out.unionTypes)
    u = substituteContractType(u, mapping, depth + 1);
  return out;
}

void substituteContractTypes(InterfaceDecl &contract,
                             const std::map<std::string, TypeRef> &mapping) {
  auto remap = [&](TypeRef &in) { in = substituteContractType(in, mapping); };
  for (auto &field : contract.fields)
    remap(field.type);
  for (auto &method : contract.methods) {
    remap(method.returnType);
    for (auto &param : method.params)
      remap(param.type);
  }
  for (auto &sig : contract.callSignatures) {
    remap(sig.returnType);
    for (auto &param : sig.params)
      remap(param.type);
  }
  for (auto &sig : contract.indexSignatures) {
    remap(sig.keyType);
    remap(sig.valueType);
  }
  for (auto &parent : contract.parents)
    remap(parent);
}
int visibility(const std::vector<std::string> &modifiers) {
  if (hasModifier(modifiers, "public"))
    return 2;
  if (hasModifier(modifiers, "protected"))
    return 1;
  return 0;
}
bool sameParameters(const FunctionDecl &left, const FunctionDecl &right) {
  if (left.params.size() != right.params.size())
    return false;
  for (std::size_t index = 0; index < left.params.size(); ++index)
    if (left.params[index].type.str() != right.params[index].type.str())
      return false;
  return true;
}
} // namespace
void Analyzer::error(const std::string &message, SourceLocation location, std::string code,
                     std::string help) {
  Diagnostic diagnostic{message, location, false, std::move(code)};
  diagnostic.category = "semantic";
  diagnostic.help = std::move(help);
  errors.push_back(std::move(diagnostic));
}
void Analyzer::warning(const std::string &m, SourceLocation l) { errors.push_back({m, l, true}); }
Analyzer::Scope *Analyzer::bindingScope(const std::string &n) const {
  for (auto s = scope; s; s = s->parent)
    if (s->types.contains(n))
      return s.get();
  return nullptr;
}
bool Analyzer::defined(const std::string &n) const {
  return bindingScope(n) != nullptr || functions.contains(n) || classes.contains(n) ||
         findStandardLibrarySymbol(n) != nullptr;
}
bool Analyzer::compatible(const TypeRef &e, const TypeRef &a) {
  if (e.name == "any" || a.name == "any")
    return true;
  if (a.name == "null")
    return e.nullable || e.name == "null" ||
           std::any_of(e.unionTypes.begin(), e.unionTypes.end(),
                       [](const auto &x) { return x.name == "null"; });
  if (e.name == "num" && (a.name == "int" || a.name == "float"))
    return true;
  if (const auto *contract = interfaces.find(e.name); contract && classes.contains(a.name)) {
    std::vector<std::string> stack;
    return classConforms(classes[a.name], effectiveContract(*contract, stack), e, {});
  }
  if (e.name == a.name && (!a.nullable || e.nullable || e.name == "null")) {
    if (e.typeArgs.size() != a.typeArgs.size())
      return false;
    for (std::size_t index = 0; index < e.typeArgs.size(); ++index)
      if (!compatible(e.typeArgs[index], a.typeArgs[index]))
        return false;
    return true;
  }
  for (auto &u : e.unionTypes)
    if (compatible(u, a))
      return true;
  return false;
}
TypeRef Analyzer::merge(const TypeRef &a, const TypeRef &b) {
  if (compatible(a, b))
    return a;
  if (compatible(b, a))
    return b;
  TypeRef r{"union", false, {a, b}};
  return r;
}
std::vector<Diagnostic> Analyzer::analyze(const std::vector<StmtPtr> &p) {
  errors.clear();
  if (!interactive || !scope) {
    scope = std::make_shared<Scope>();
    for (const auto &[name, type] : externalBindings) {
      scope->types[name] = type;
      scope->mutableBindings[name] = false;
    }
    functions.clear();
    classes.clear();
    interfaces.clear();
  }
  const auto previousTypes = scope->types;
  const auto previousMutability = scope->mutableBindings;
  const auto previousFunctions = functions;
  const auto previousClasses = classes;
  const auto previousInterfaces = interfaces;
  // Merged-in ambient interfaces from imported type-definition modules
  // (.kyna.d / .d.ky / .ky.d) become globally visible to this module.
  for (const auto &iface : externalInterfaces)
    interfaces.declareInterface(iface);
  // Classes exported by imported modules become visible for `new`, type
  // annotations, and member access across module boundaries.
  for (const auto &klass : externalClasses)
    classes[klass.name] = klass;
  activeLoopLabels.clear();
  std::set<std::string> declarations;
  for (auto &s : p) {
    if (auto f = std::get_if<FunctionDecl>(&s->node)) {
      if (!declarations.insert(f->name).second || functions.contains(f->name) ||
          classes.contains(f->name) || scope->types.contains(f->name) ||
          interfaces.find(f->name))
        error("top-level declaration '" + f->name + "' is defined more than once", s->location,
              "KSEM1101", "rename or remove one of the declarations");
      else
        functions[f->name] = *f;
    }
    if (auto c = std::get_if<ClassDecl>(&s->node)) {
      if (!declarations.insert(c->name).second || functions.contains(c->name) ||
          classes.contains(c->name) || scope->types.contains(c->name) ||
          interfaces.find(c->name))
        error("top-level declaration '" + c->name + "' is defined more than once", s->location,
              "KSEM1101", "rename or remove one of the declarations");
      else
        classes[c->name] = *c;
    }
    if (auto i = std::get_if<InterfaceDecl>(&s->node)) {
      if (!declarations.insert(i->name).second || functions.contains(i->name) ||
          classes.contains(i->name) || scope->types.contains(i->name) ||
          interfaces.find(i->name) || !interfaces.declareInterface(*i))
        error("top-level declaration '" + i->name + "' is defined more than once", s->location,
              "KSEM1101", "rename or remove one of the declarations");
    }
  }
  for (auto &s : p)
    stmt(s);
  if (interactive && std::any_of(errors.begin(), errors.end(),
                                 [](const Diagnostic &diagnostic) { return !diagnostic.warning; })) {
    scope->types = previousTypes;
    scope->mutableBindings = previousMutability;
    functions = previousFunctions;
    classes = previousClasses;
    interfaces = previousInterfaces;
  }
  return errors;
}
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
        } else if constexpr (std::is_same_v<T, BreakStmt> || std::is_same_v<T, ContinueStmt>) {
          const auto operation = std::is_same_v<T, BreakStmt> ? "break" : "continue";
          if (activeLoopLabels.empty())
            error(std::string(operation) + " must be inside a loop", s->location, "KSEM1301",
                  "remove it or move it into a loop body");
          else if (!n.label.empty() &&
                   std::find(activeLoopLabels.rbegin(), activeLoopLabels.rend(), n.label) ==
                       activeLoopLabels.rend())
            error(std::string(operation) + " references unknown loop label '" + n.label + "'",
                  s->location, "KSEM1302", "use the label of an enclosing loop");
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
  return false;
}
const FieldDecl *Analyzer::findField(const ClassDecl &klass, const std::string &name) const {
  const auto found = std::find_if(klass.fields.begin(), klass.fields.end(),
                                  [&](const auto &field) { return field.name == name; });
  if (found != klass.fields.end())
    return &*found;
  if (!klass.parent.empty()) {
    const auto parent = classes.find(klass.parent);
    if (parent != classes.end())
      return findField(parent->second, name);
  }
  return nullptr;
}
const FunctionDecl *Analyzer::findMethod(const ClassDecl &klass, const std::string &name) const {
  const auto found = std::find_if(klass.methods.begin(), klass.methods.end(),
                                  [&](const auto &method) { return method.name == name; });
  if (found != klass.methods.end())
    return &*found;
  if (!klass.parent.empty()) {
    const auto parent = classes.find(klass.parent);
    if (parent != classes.end())
      return findMethod(parent->second, name);
  }
  return nullptr;
}
bool Analyzer::classConforms(const ClassDecl &klass, const InterfaceDecl &contract,
                             const TypeRef &contractRef, SourceLocation location) {
  std::vector<std::string> stack;
  const auto effective = effectiveContract(contract, stack);
  bool conforms = true;
  for (const auto &required : effective.fields) {
    if (effective.optionalFields.contains(required.name))
      continue;
    const auto *field = findField(klass, required.name);
    if (!field || !compatible(substitute(required.type, contract, contractRef), field->type)) {
      conforms = false;
      if (location.known())
        error("class '" + klass.name + "' does not provide compatible field '" + required.name +
                  "' required by interface '" + contract.name + "'",
              location);
    }
  }
  for (const auto &required : effective.methods) {
    const auto *method = findMethod(klass, required.name);
    FunctionDecl substitutedRow = required;
    for (auto &param : substitutedRow.params)
      param.type = substitute(param.type, contract, contractRef);
    substitutedRow.returnType = substitute(required.returnType, contract, contractRef);
    if (!method || !sameParameters(*method, substitutedRow) ||
        !compatible(substitutedRow.returnType, method->returnType) ||
        visibility(method->modifiers) != 2) {
      conforms = false;
      if (location.known())
        error("class '" + klass.name + "' does not provide compatible public method '" +
                  required.name + "' required by interface '" + contract.name + "'",
              location);
    }
  }
  return conforms;
}
TypeRef Analyzer::substitute(const TypeRef &type, const InterfaceDecl &contract,
                             const TypeRef &contractRef) const {
  if (contractRef.typeArgs.empty())
    return type;
  auto replace = [&](const TypeRef &value) -> TypeRef {
    for (std::size_t index = 0; index < contract.typeParams.size() &&
                                index < contractRef.typeArgs.size();
         ++index)
      if (value.name == contract.typeParams[index]) {
        TypeRef result = contractRef.typeArgs[index];
        return result;
      }
    return value;
  };
  TypeRef result = type;
  result.name = replace(type).name;
  result.nullable = type.nullable;
  result.typeArgs.clear();
  for (const auto &arg : type.typeArgs)
    result.typeArgs.push_back(substitute(arg, contract, contractRef));
  result.unionTypes.clear();
  for (const auto &u : type.unionTypes)
    result.unionTypes.push_back(substitute(u, contract, contractRef));
  return result;
}
bool Analyzer::objectConforms(const ObjectExpr &object, const InterfaceDecl &contract,
                              SourceLocation location) {
  std::vector<std::string> stack;
  const auto effective = effectiveContract(contract, stack);
  bool conforms = true;
  for (const auto &required : effective.fields) {
    const auto found = std::find_if(object.fields.begin(), object.fields.end(),
                                    [&](const auto &f) { return f.name == required.name; });
    if (found == object.fields.end() && effective.optionalFields.contains(required.name))
      continue;
    if (found == object.fields.end() || !compatible(required.type, expr(found->value))) {
      conforms = false;
      error("object does not provide compatible field '" + required.name +
                "' required by interface '" + contract.name + "'",
            location);
    }
  }
  if (!effective.methods.empty() || !effective.callSignatures.empty()) {
    conforms = false;
    error("closed object literals cannot provide methods required by interface '" + contract.name +
              "'",
          location);
  }
  return conforms;
}
InterfaceDecl Analyzer::effectiveContract(const InterfaceDecl &declaration,
                                          std::vector<std::string> &stack) const {
  if (std::find(stack.begin(), stack.end(), declaration.name) != stack.end())
    return {}; // cycle guard
  stack.push_back(declaration.name);
  InterfaceDecl merged;
  merged.name = declaration.name;
  merged.typeParams = declaration.typeParams;
  auto mergeIn = [&](const InterfaceDecl &source) {
    for (const auto &field : source.fields)
      if (std::none_of(merged.fields.begin(), merged.fields.end(),
                       [&](const auto &existing) { return existing.name == field.name; }))
        merged.fields.push_back(field);
    for (const auto &method : source.methods)
      if (std::none_of(merged.methods.begin(), merged.methods.end(),
                       [&](const auto &existing) { return existing.name == method.name; }))
        merged.methods.push_back(method);
    for (const auto &sig : source.callSignatures)
      merged.callSignatures.push_back(sig);
    for (const auto &sig : source.indexSignatures)
      merged.indexSignatures.push_back(sig);
    for (const auto &name : source.optionalFields)
      merged.optionalFields.insert(name);
  };
  for (const auto &parentRef : declaration.parents) {
    const auto *parent = interfaces.find(parentRef.name);
    if (!parent)
      continue;
    auto parentEffective = effectiveContract(*parent, stack);
    if (!parentRef.typeArgs.empty()) {
      std::map<std::string, TypeRef> mapping;
      for (std::size_t index = 0;
           index < parentEffective.typeParams.size() && index < parentRef.typeArgs.size();
           ++index)
        mapping[parentEffective.typeParams[index]] = parentRef.typeArgs[index];
      substituteContractTypes(parentEffective, mapping);
    }
    mergeIn(parentEffective);
  }
  mergeIn(declaration);
  stack.pop_back();
  return merged;
}
} // namespace kyna
