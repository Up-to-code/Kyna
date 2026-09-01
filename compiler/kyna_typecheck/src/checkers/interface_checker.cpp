#include "kyna/semantics/program_analyzer.hpp"
#include "kyna/semantics/modifier_query.hpp"
#include <algorithm>
#include <map>

namespace kyna {
namespace {
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
TypeRef substituteContractType(const TypeRef &in, const std::map<std::string, TypeRef> &mapping,
                               int depth = 0) {
  if (depth > 32) return in;
  TypeRef out = in;
  if (const auto found = mapping.find(in.name); found != mapping.end()) {
    out = found->second;
    out.nullable = out.nullable || in.nullable;
    for (auto &u : out.unionTypes) u = substituteContractType(u, mapping, depth + 1);
  }
  for (auto &arg : out.typeArgs) arg = substituteContractType(arg, mapping, depth + 1);
  for (auto &u : out.unionTypes) u = substituteContractType(u, mapping, depth + 1);
  return out;
}
void substituteContractTypes(InterfaceDecl &contract,
                             const std::map<std::string, TypeRef> &mapping) {
  auto remap = [&](TypeRef &in) { in = substituteContractType(in, mapping); };
  for (auto &field : contract.fields) remap(field.type);
  for (auto &method : contract.methods) {
    remap(method.returnType);
    for (auto &param : method.params) remap(param.type);
  }
  for (auto &sig : contract.callSignatures) {
    remap(sig.returnType);
    for (auto &param : sig.params) remap(param.type);
  }
  for (auto &sig : contract.indexSignatures) {
    remap(sig.keyType);
    remap(sig.valueType);
  }
  for (auto &parent : contract.parents) remap(parent);
}
} // namespace

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
