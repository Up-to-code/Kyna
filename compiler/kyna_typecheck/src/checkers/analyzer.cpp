#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/semantics/modifier_query.hpp"
#include "kyna/symbols/standard_library_symbols.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace kyna {
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








} // namespace kyna
