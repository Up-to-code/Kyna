// Owns alias resolution, inferred function results, and checked module bindings.
#include "check_helpers.hpp"
#include <kyna/semantics/program_analyzer.hpp>

namespace kyna {

TypeRef Analyzer::resolveType(const TypeRef &input, SourceLocation location, int depth) {
  if (depth > 64) {
    error("type alias cycle involving '" + input.name + "'", location, "KSEM1501",
          "break the cycle between type aliases");
    return analyzerNamedType("void");
  }
  auto result = input;
  if (const auto alias = aliases.find(input.name); alias != aliases.end()) {
    auto target = alias->second;
    const auto *interface = interfaces.find(target.name);
    if (interface && !input.typeArgs.empty())
      target.typeArgs = input.typeArgs;
    result = resolveType(target, location, depth + 1);
    result.nullable = result.nullable || input.nullable;
    result.unionTypes.insert(result.unionTypes.end(), input.unionTypes.begin(), input.unionTypes.end());
    if (!input.typeArgs.empty() && !interface)
      error("type alias '" + input.name + "' does not accept type arguments", location,
            "KSEM1502", "apply type arguments to the underlying generic interface");
  }
  for (auto &argument : result.typeArgs)
    argument = resolveType(argument, location, depth + 1);
  for (auto &arm : result.unionTypes)
    arm = resolveType(arm, location, depth + 1);
  if (const auto *contract = interfaces.find(result.name);
      contract && contract->typeParams.size() != result.typeArgs.size())
    error("interface '" + result.name + "' expects " +
              std::to_string(contract->typeParams.size()) + " type argument(s)", location,
          "KSEM1502", "match the interface's type parameter list");
  return result;
}

TypeRef Analyzer::functionType(const FunctionDecl &function, SourceLocation location) {
  TypeRef result;
  if (function.hasReturnType)
    result = resolveType(function.returnType, location);
  else if (const auto inferred = inferredReturns.find(function.name); inferred != inferredReturns.end())
    result = inferred->second;
  else {
    if (inferringFunctions.contains(function.name)) {
      error("recursive function '" + function.name + "' requires a return type", location,
            "KSEM1503", "annotate the recursive function's result type");
      return analyzerSignature(function, analyzerNamedType("void"));
    }
    checkFunctionDecl(function, location);
    result = inferredReturns.at(function.name);
  }
  auto signature = analyzerSignature(function, result);
  for (auto &parameter : signature.typeArgs)
    parameter = resolveType(parameter, location);
  return signature;
}

std::map<std::string, TypeRef> Analyzer::checkedBindings() {
  auto result = scope->types;
  for (const auto &[name, function] : functions) {
    const auto inferred = inferredReturns.find(name);
    result[name] = analyzerSignature(function, function.hasReturnType ? function.returnType
        : inferred != inferredReturns.end() ? inferred->second : analyzerNamedType("void"));
  }
  for (const auto &[name, target] : aliases) {
    const auto *interface = interfaces.find(target.name);
    auto type = interface && !interface->typeParams.empty() && target.typeArgs.empty()
        ? target : resolveType(target);
    type.name = "type:" + type.name;
    result[name] = std::move(type);
  }
  return result;
}

} // namespace kyna
