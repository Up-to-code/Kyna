#include "check_helpers.hpp"
#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/symbols/standard_library_symbols.hpp"

#include <algorithm>
#include <limits>

namespace kyna {

TypeRef Analyzer::checkCall(const Call &n, SourceLocation loc) {
  auto c = expr(n.callee);
  std::vector<TypeRef> argumentTypes;
  argumentTypes.reserve(n.args.size());
  for (auto &a : n.args)
    argumentTypes.push_back(expr(a));
  if (auto v = std::get_if<Variable>(&n.callee->node); v && functions.contains(v->name)) {
    auto &f = functions[v->name];
    if (f.params.size() != n.args.size())
      error("function '" + v->name + "' expects " + std::to_string(f.params.size()) +
                " argument(s), but " + std::to_string(n.args.size()) + " were provided",
            loc, "KSEM1201", "match the declared parameter list");
    for (size_t i = 0; i < std::min(f.params.size(), n.args.size()); ++i) {
      const auto &at = argumentTypes[i];
      if (!compatible(f.params[i].type, at))
        error("argument " + std::to_string(i + 1) + " to '" + v->name + "' has type " +
                  at.str() + ", expected " + f.params[i].type.str(),
              n.args[i]->location, "KSEM1202",
              "pass a value compatible with parameter '" + f.params[i].name + "'");
    }
    return f.hasReturnType ? f.returnType : analyzerNamedType("any");
  }
  if (auto imported = std::get_if<Variable>(&n.callee->node);
      imported && c.name.starts_with("module:")) {
    const auto alias = c.name.substr(std::string_view("module:").size());
    const auto module = moduleExports.find(alias);
    if (module != moduleExports.end() && module->second.contains(imported->name))
      return module->second.at(imported->name);
  }
  if (auto variable = std::get_if<Variable>(&n.callee->node)) {
    if (const auto *builtin = findStandardLibrarySymbol(variable->name);
        builtin && builtin->callable) {
      if (n.args.size() < builtin->minimumArguments ||
          n.args.size() > builtin->maximumArguments) {
        const auto expected = builtin->minimumArguments == builtin->maximumArguments
                                  ? std::to_string(builtin->minimumArguments)
                                  : std::to_string(builtin->minimumArguments) + " to " +
                                        (builtin->maximumArguments ==
                                                 std::numeric_limits<std::size_t>::max()
                                             ? std::string("any number of")
                                             : std::to_string(builtin->maximumArguments));
        error("standard-library function '" + variable->name + "' expects " + expected +
                  " argument(s), but " + std::to_string(n.args.size()) + " were provided",
              loc, "KSEM1204", "use the documented function signature");
      }
      for (std::size_t index = 0;
           index < std::min(argumentTypes.size(), builtin->argumentKinds.size()); ++index) {
        if (!acceptsBuiltinArgument(builtin->argumentKinds[index], argumentTypes[index]))
          error("argument " + std::to_string(index + 1) + " to '" + variable->name +
                    "' has type " + argumentTypes[index].str() + ", expected " +
                    std::string(builtinArgumentKindName(builtin->argumentKinds[index])),
                n.args[index]->location, "KSEM1205",
                "pass a value matching the standard-library function signature");
      }
      return builtin->returnType;
    }
  }
  if (std::holds_alternative<Member>(n.callee->node))
    return c;
  if (c.name != "func" && c.name != "any")
    error("value of type '" + c.str() + "' is not callable", n.callee->location, "KSEM1203",
          "call a function value or check the binding's type");
  return analyzerNamedType("any");
}

TypeRef Analyzer::checkMember(const Member &n, SourceLocation loc) {
  auto objectType = expr(n.object);
  if (objectType.name == "null") {
    error("cannot read member '" + n.name + "' from null", n.object->location, "KSEM2401",
          "check the value against null before accessing the member");
    return analyzerNamedType("any");
  }
  constexpr std::string_view modulePrefix = "module:";
  if (objectType.name.starts_with(modulePrefix)) {
    const auto alias = objectType.name.substr(modulePrefix.size());
    const auto module = moduleExports.find(alias);
    if (module == moduleExports.end() || !module->second.contains(n.name)) {
      error("module '" + alias + "' has no exported member '" + n.name + "'", loc);
      return analyzerNamedType("any");
    }
    return module->second.at(n.name);
  }
  if (objectType.name == "any" || objectType.name == "object")
    return analyzerNamedType("any");
  if (objectType.name == "Error") {
    if (n.name == "message" || n.name == "code")
      return analyzerNamedType("str");
    if (n.name == "cause")
      return analyzerNamedType("any");
    error("Error has no member '" + n.name + "'", loc, "KSEM2404",
          "use message, code, or cause");
    return analyzerNamedType("any");
  }
  bool staticAccess = false;
  std::string className = objectType.name;
  constexpr std::string_view classPrefix = "class:";
  if (className.starts_with(classPrefix)) {
    staticAccess = true;
    className.erase(0, classPrefix.size());
  }
  if (classes.contains(className)) {
    const ClassDecl *owner = &classes[className];
    const FieldDecl *field = nullptr;
    const FunctionDecl *method = nullptr;
    while (owner) {
      auto fieldIt =
          std::find_if(owner->fields.begin(), owner->fields.end(),
                       [&](const auto &candidate) { return candidate.name == n.name; });
      auto methodIt =
          std::find_if(owner->methods.begin(), owner->methods.end(),
                       [&](const auto &candidate) { return candidate.name == n.name; });
      if (fieldIt != owner->fields.end())
        field = &*fieldIt;
      if (methodIt != owner->methods.end())
        method = &*methodIt;
      if (field || method)
        break;
      if (owner->parent.empty() || !classes.contains(owner->parent))
        owner = nullptr;
      else
        owner = &classes[owner->parent];
    }
    const auto *modifiers =
        field ? &field->modifiers : method ? &method->modifiers : nullptr;
    if (!modifiers) {
      error("unknown member '" + n.name + "' on '" + className + "'", loc);
      return analyzerNamedType("any");
    }
    if (hasModifier(*modifiers, "static") != staticAccess)
      error("member '" + n.name + "' must be accessed through " +
                std::string(staticAccess ? "an instance" : "the class"),
            loc);
    const int access = analyzerMemberVisibility(*modifiers);
    bool subclassAccess = false;
    for (auto cursor = currentClass; !cursor.empty() && classes.contains(cursor);
         cursor = classes[cursor].parent)
      if (owner && cursor == owner->name)
        subclassAccess = true;
    if (access == 0 && (!owner || currentClass != owner->name))
      error("private member '" + n.name + "' is not accessible here", loc);
    if (access == 1 && (!owner || (!subclassAccess && currentClass != owner->name)))
      error("protected member '" + n.name + "' is not accessible here", loc);
    return field ? field->type
                 : (method->hasReturnType ? method->returnType : analyzerNamedType("any"));
  }
  if (const auto *contract = interfaces.find(className)) {
    std::vector<std::string> stack;
    const auto effective = effectiveContract(*contract, stack);
    const TypeRef contractRef{className, objectType.nullable, objectType.typeArgs, {}};
    const auto field =
        std::find_if(effective.fields.begin(), effective.fields.end(),
                     [&](const auto &candidate) { return candidate.name == n.name; });
    if (field != effective.fields.end())
      return substitute(field->type, *contract, contractRef);
    const auto method =
        std::find_if(effective.methods.begin(), effective.methods.end(),
                     [&](const auto &candidate) { return candidate.name == n.name; });
    if (method != effective.methods.end())
      return substitute(method->returnType, *contract, contractRef);
  }
  error("member access requires a class, interface, object, or module", loc);
  return analyzerNamedType("any");
}

TypeRef Analyzer::checkIndex(const Index &n, SourceLocation) {
  auto object = expr(n.object);
  auto index = expr(n.index);
  if (object.name == "array" && index.name != "int" && index.name != "any")
    error("array index must be int, got '" + index.str() + "'", n.index->location, "KSEM2403",
          "use an integer index between zero and the array length minus one");
  if (object.name == "object" && index.name != "str" && index.name != "any")
    error("object key must be str, got '" + index.str() + "'", n.index->location, "KSEM2403",
          "use a string key such as object[\"field\"]");
  if (object.name != "array" && object.name != "object" && object.name != "any")
    error(object.name == "null" ? "cannot index null"
                                : "indexing requires an array or object, got '" + object.str() +
                                      "'",
          n.object->location, "KSEM2402", "check the value and its type before indexing it");
  return analyzerNamedType("any");
}

TypeRef Analyzer::checkNew(const NewExpr &n, SourceLocation loc) {
  std::vector<TypeRef> argumentTypes;
  for (auto &a : n.args)
    argumentTypes.push_back(expr(a));
  if (!classes.contains(n.className)) {
    error("unknown class '" + n.className + "'", loc);
  } else {
    const auto *initializer = findMethod(classes[n.className], "init");
    if (!initializer && !n.args.empty())
      error("constructor for '" + n.className + "' takes no arguments", loc);
    if (initializer) {
      if (initializer->params.size() != n.args.size())
        error("constructor for '" + n.className + "' expects " +
                  std::to_string(initializer->params.size()) + " argument(s)",
              loc);
      for (std::size_t index = 0;
           index < std::min(initializer->params.size(), argumentTypes.size()); ++index)
        if (!compatible(initializer->params[index].type, argumentTypes[index]))
          error("constructor argument " + std::to_string(index + 1) + " for '" + n.className +
                    "' has incompatible type",
                n.args[index]->location);
    }
  }
  return analyzerNamedType(n.className);
}

} // namespace kyna
