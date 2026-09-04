#include "syntax_lowerer.hpp"
#include <algorithm>

namespace kyna {

std::optional<HirExpressionId> SyntaxLowerer::lowerCall(const Call &node, SourceSpan span) {
  const auto *callee = node.callee ? std::get_if<Variable>(&node.callee->node) : nullptr;
  const auto *member = node.callee ? std::get_if<Member>(&node.callee->node) : nullptr;
  std::vector<HirExpressionId> arguments;
  arguments.reserve(node.args.size());
  for (const auto &argument : node.args) {
    const auto lowered = lowerExpression(argument);
    if (!lowered)
      return std::nullopt;
    arguments.push_back(*lowered);
  }
  if (member && (member->name == "json" || member->name == "text") &&
      isResponseExpression(member->object)) {
    const auto receiver = lowerExpression(member->object);
    if (!receiver)
      return std::nullopt;
    arguments.insert(arguments.begin(), *receiver);
    return addExpression(HirNativeCallExpression{member->name == "json" ? "responseJson"
                                                                        : "responseText",
                                                 std::move(arguments)},
                         span);
  }
  if (member && member->object) {
    if (const auto *namespaceName = std::get_if<Variable>(&member->object->node)) {
      const auto qualifiedName = namespaceName->name + "." + member->name;
      if (const auto native = options.nativeMemberFunctions.find(qualifiedName);
          native != options.nativeMemberFunctions.end())
        return addExpression(HirNativeCallExpression{native->second, std::move(arguments)}, span);
    }
  }
  if (callee && !findLocal(callee->name) && !functions.contains(callee->name) &&
      std::find(options.nativeFunctions.begin(), options.nativeFunctions.end(), callee->name) !=
          options.nativeFunctions.end())
    return addExpression(HirNativeCallExpression{callee->name, std::move(arguments)}, span);
  if (callee && !findLocal(callee->name) && functions.contains(callee->name))
    return addExpression(HirCallExpression{functions.at(callee->name), std::move(arguments)}, span);
  const auto loweredCallee = lowerExpression(node.callee);
  if (!loweredCallee)
    return std::nullopt;
  return addExpression(HirIndirectCallExpression{*loweredCallee, std::move(arguments)}, span);
}

std::optional<HirExpressionId> SyntaxLowerer::lowerNew(const NewExpr &node, SourceSpan span) {
  const auto klass = classes.find(node.className);
  if (klass == classes.end()) {
    unsupported("construction of unknown class '" + node.className + "'", span);
    return std::nullopt;
  }
  std::vector<HirExpressionId> arguments;
  arguments.reserve(node.args.size());
  for (const auto &argument : node.args) {
    const auto lowered = lowerExpression(argument);
    if (!lowered)
      return std::nullopt;
    arguments.push_back(*lowered);
  }
  return addExpression(HirNewExpression{klass->second, std::move(arguments)}, span);
}

std::optional<HirExpressionId> SyntaxLowerer::lowerObject(const ObjectExpr &node, SourceSpan span) {
  std::vector<HirObjectField> fields;
  fields.reserve(node.fields.size());
  for (const auto &field : node.fields) {
    const auto lowered = lowerExpression(field.value);
    if (!lowered)
      return std::nullopt;
    fields.push_back({field.name, *lowered});
  }
  return addExpression(HirObjectExpression{std::move(fields)}, span);
}

std::optional<HirExpressionId> SyntaxLowerer::lowerAssign(const Assign &node, SourceSpan span) {
  const auto *target = node.target ? std::get_if<Variable>(&node.target->node) : nullptr;
  const auto local = target ? findLocal(target->name) : std::nullopt;
  const auto value = lowerExpression(node.value);
  if (!value)
    return std::nullopt;
  if (const auto *index = node.target ? std::get_if<Index>(&node.target->node) : nullptr) {
    const auto object = lowerExpression(index->object);
    const auto key = lowerExpression(index->index);
    return object && key
               ? std::optional{addExpression(HirAssignIndexExpression{*object, *key, *value}, span)}
               : std::nullopt;
  }
  if (const auto *member = node.target ? std::get_if<Member>(&node.target->node) : nullptr) {
    const auto object = lowerExpression(member->object);
    return object ? std::optional{addExpression(
                        HirAssignMemberExpression{*object, member->name, *value}, span)}
                  : std::nullopt;
  }
  if (!local) {
    unsupported("non-local assignment", span);
    return std::nullopt;
  }
  captureIfNeeded(*local);
  return addExpression(HirAssignLocalExpression{*local, *value}, span);
}

std::optional<HirExpressionId> SyntaxLowerer::lowerIfExpr(const IfExpr &node, SourceSpan span) {
  const auto condition = lowerExpression(node.condition);
  const auto thenBranch = lowerValueBlock(node.thenBranch);
  const auto elseBranch = lowerValueBlock(node.elseBranch);
  if (!condition || !thenBranch || !elseBranch)
    return std::nullopt;
  return addExpression(HirIfExpression{*condition, thenBranch->prelude, thenBranch->value,
                                       elseBranch->prelude, elseBranch->value},
                       span);
}

std::optional<HirExpressionId> SyntaxLowerer::lowerMatch(const MatchExpr &node, SourceSpan span) {
  const auto subject = lowerExpression(node.subject);
  if (!subject)
    return std::nullopt;
  std::vector<HirMatchArm> arms;
  arms.reserve(node.arms.size());
  for (const auto &arm : node.arms) {
    const auto pattern = arm.wildcard ? std::optional<HirExpressionId>{} : lowerExpression(arm.pattern);
    const auto value = lowerExpression(arm.value);
    if ((!arm.wildcard && !pattern) || !value)
      return std::nullopt;
    arms.push_back({pattern, *value});
  }
  return addExpression(HirMatchExpression{*subject, std::move(arms)}, span);
}

} // namespace kyna
