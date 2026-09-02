#include "syntax_lowerer.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace kyna {

SyntaxLowerer::SyntaxLowerer(std::string moduleName, HirLoweringOptions loweringOptions)
      : options(std::move(loweringOptions)) {
    program.name = std::move(moduleName);
  }

HirLoweringResult SyntaxLowerer::lower(const SyntaxTree &tree) {
    for (const auto &statement : tree.module.declarations) {
      if (const auto *function = std::get_if<FunctionDecl>(&statement->node)) {
        if (functions.contains(function->name) || classes.contains(function->name)) {
          Diagnostic diagnostic{"function '" + function->name + "' is declared more than once",
                                statement->location, false, "KHIR1101"};
          diagnostic.category = "hir";
          diagnostics.push_back(std::move(diagnostic));
          continue;
        }
        const auto id = HirFunctionId{static_cast<std::uint32_t>(program.functions.size())};
        functions.insert_or_assign(function->name, id);
        program.functions.push_back(
            {function->name, {}, {}, statement->location, {}, std::nullopt});
      } else if (const auto *klass = std::get_if<ClassDecl>(&statement->node)) {
        if (classes.contains(klass->name) || functions.contains(klass->name)) {
          Diagnostic diagnostic{"class '" + klass->name + "' is declared more than once",
                                statement->location, false, "KHIR1101"};
          diagnostic.category = "hir";
          diagnostics.push_back(std::move(diagnostic));
          continue;
        }
        const auto classId = HirClassId{static_cast<std::uint32_t>(program.classes.size())};
        classes.insert_or_assign(klass->name, classId);
        HirClass loweredClass{klass->name, std::nullopt, {}, {}, std::nullopt,
                              statement->location};
        for (const auto &field : klass->fields)
          loweredClass.fields.push_back({field.name, statement->location});
        for (const auto &method : klass->methods) {
          const auto functionId =
              HirFunctionId{static_cast<std::uint32_t>(program.functions.size())};
          program.functions.push_back({klass->name + "." + method.name, {}, {},
                                       statement->location, {}, std::nullopt});
          if (method.name == "init")
            loweredClass.constructor = functionId;
          else
            loweredClass.methods.push_back({method.name, functionId});
        }
        program.classes.push_back(std::move(loweredClass));
      }
    }

    for (const auto &statement : tree.module.declarations)
      if (const auto *klass = std::get_if<ClassDecl>(&statement->node);
          klass && !klass->parent.empty()) {
        const auto child = classes.find(klass->name);
        const auto parent = classes.find(klass->parent);
        if (child != classes.end() && parent != classes.end())
          program.classes[child->second.value].parent = parent->second;
      }

    if (!diagnostics.empty())
      return {std::nullopt, std::move(diagnostics)};

    for (const auto &statement : tree.module.declarations)
      if (const auto *function = std::get_if<FunctionDecl>(&statement->node))
        if (const auto found = functions.find(function->name); found != functions.end())
          lowerFunction(found->second, *function, statement->location);

    for (const auto &statement : tree.module.declarations)
      if (const auto *klass = std::get_if<ClassDecl>(&statement->node)) {
        const auto found = classes.find(klass->name);
        if (found == classes.end())
          continue;
        const auto &loweredClass = program.classes[found->second.value];
        for (const auto &method : klass->methods) {
          std::optional<HirFunctionId> function;
          if (method.name == "init")
            function = loweredClass.constructor;
          else
            for (const auto &candidate : loweredClass.methods)
              if (candidate.name == method.name)
                function = candidate.function;
          if (function)
            lowerFunction(*function, method, statement->location, false, found->second);
        }
      }

    scopes.clear();
    scopes.emplace_back();
    for (const auto &statement : tree.module.declarations) {
      if (std::holds_alternative<ImportDecl>(statement->node) ||
          std::holds_alternative<FunctionDecl>(statement->node) ||
          std::holds_alternative<ClassDecl>(statement->node) ||
          std::holds_alternative<InterfaceDecl>(statement->node))
        continue;
      if (const auto lowered = lowerStatement(statement))
        program.body.push_back(*lowered);
    }
    if (!diagnostics.empty())
      return {std::nullopt, std::move(diagnostics)};
    return {std::move(program), {}};
  }

void SyntaxLowerer::unsupported(std::string construct, SourceSpan span) {
    Diagnostic diagnostic{"HIR lowering does not yet support " + std::move(construct), span,
                          false, "KHIR1201"};
    diagnostic.category = "hir";
    diagnostic.help = "use 'kyna run' while this construct is migrated to the bytecode pipeline";
    diagnostics.push_back(std::move(diagnostic));
  }

HirExpressionId SyntaxLowerer::addExpression(HirExpression::Node node, SourceSpan span) {
    const auto id = HirExpressionId{static_cast<std::uint32_t>(program.expressions.size())};
    program.expressions.push_back({std::move(node), span});
    return id;
  }

HirStatementId SyntaxLowerer::addStatement(HirStatement::Node node, SourceSpan span) {
    const auto id = HirStatementId{static_cast<std::uint32_t>(program.statements.size())};
    program.statements.push_back({std::move(node), span});
    return id;
  }

HirLocalId SyntaxLowerer::addLocal(const VarDecl &declaration, SourceSpan span) {
    const auto id = HirLocalId{static_cast<std::uint32_t>(program.locals.size())};
    program.locals.push_back({declaration.name, declaration.mutableBinding, span});
    localOwners.push_back(currentFunction);
    responseLocals.push_back(false);
    scopes.back().insert_or_assign(declaration.name, id);
    return id;
  }

HirLocalId SyntaxLowerer::addParameter(const Param &parameter, SourceSpan span) {
    const auto id = HirLocalId{static_cast<std::uint32_t>(program.locals.size())};
    program.locals.push_back({parameter.name, false, span});
    localOwners.push_back(currentFunction);
    responseLocals.push_back(false);
    scopes.back().insert_or_assign(parameter.name, id);
    return id;
  }

HirLocalId SyntaxLowerer::addNamedLocal(const std::string &name, bool mutableBinding, SourceSpan span) {
    const auto id = HirLocalId{static_cast<std::uint32_t>(program.locals.size())};
    program.locals.push_back({name, mutableBinding, span});
    localOwners.push_back(currentFunction);
    responseLocals.push_back(false);
    scopes.back().insert_or_assign(name, id);
    return id;
  }

void SyntaxLowerer::captureIfNeeded(HirLocalId local) {
    if (!currentFunction || local.value >= localOwners.size() ||
        localOwners[local.value] == currentFunction)
      return;
    auto &captures = program.functions[currentFunction->value].captures;
    if (std::find(captures.begin(), captures.end(), local) == captures.end())
      captures.push_back(local);
  }

  void SyntaxLowerer::lowerFunction(HirFunctionId id, const FunctionDecl &declaration, SourceSpan span,
                     bool preserveOuterScopes,
                     std::optional<HirClassId> owningClass) {
    const auto previousFunction = currentFunction;
    const auto previousSelf = currentSelf;
    const auto previousClass = currentClass;
    auto previousLoops = std::move(loopLabels);
    std::vector<std::unordered_map<std::string, HirLocalId>> savedScopes;
    if (!preserveOuterScopes) {
      savedScopes = std::move(scopes);
      scopes.clear();
    }
    currentFunction = id;
    scopes.emplace_back();
    if (owningClass) {
      const auto receiver = addNamedLocal("self", false, span);
      currentSelf = receiver;
      currentClass = owningClass;
      program.functions.at(id.value).parameters.push_back(receiver);
    } else if (!preserveOuterScopes) {
      currentSelf.reset();
      currentClass.reset();
    }
    for (const auto &parameter : declaration.params)
      program.functions.at(id.value).parameters.push_back(addParameter(parameter, span));
    const auto body = lowerStatement(declaration.body);
    scopes.pop_back();
    if (body)
      program.functions.at(id.value).body = *body;
    const auto captures = program.functions.at(id.value).captures;
    currentFunction = previousFunction;
    currentSelf = previousSelf;
    currentClass = previousClass;
    loopLabels = std::move(previousLoops);
    if (!preserveOuterScopes)
      scopes = std::move(savedScopes);
    if (currentFunction)
      for (const auto capture : captures)
        captureIfNeeded(capture);
  }

std::optional<HirLocalId> SyntaxLowerer::findLocal(const std::string &name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
      if (const auto found = scope->find(name); found != scope->end())
        return found->second;
    return std::nullopt;
  }

bool SyntaxLowerer::isFetchCall(const ExprPtr &expression) const {
    const ExprPtr *node = &expression;
    if (const auto *await = (*node) ? std::get_if<AwaitExpr>(&(*node)->node) : nullptr)
      node = &await->operand;
    const auto *call = (*node) ? std::get_if<Call>(&(*node)->node) : nullptr;
    const auto *callee = call && call->callee ? std::get_if<Variable>(&call->callee->node) : nullptr;
    return callee && callee->name == "fetch";
  }

bool SyntaxLowerer::isResponseExpression(const ExprPtr &expression) const {
    if (isFetchCall(expression))
      return true;
    const auto *variable = expression ? std::get_if<Variable>(&expression->node) : nullptr;
    const auto local = variable ? findLocal(variable->name) : std::nullopt;
    return local && local->value < responseLocals.size() && responseLocals[local->value];
  }

void SyntaxLowerer::predeclareNestedFunctions(const BlockStmt &block) {
    for (const auto &statement : block.statements) {
      const auto *declaration = std::get_if<FunctionDecl>(&statement->node);
      if (!declaration)
        continue;
      if (scopes.back().contains(declaration->name)) {
        Diagnostic diagnostic{"function '" + declaration->name +
                                  "' conflicts with another binding in this block",
                              statement->location, false, "KHIR1102"};
        diagnostic.category = "hir";
        diagnostics.push_back(std::move(diagnostic));
        continue;
      }
      const auto function =
          HirFunctionId{static_cast<std::uint32_t>(program.functions.size())};
      program.functions.push_back(
          {declaration->name, {}, {}, statement->location, {}, currentFunction});
      const auto local = addNamedLocal(declaration->name, false, statement->location);
      nestedFunctions.insert_or_assign(statement.get(), std::pair{function, local});
    }
  }

bool SyntaxLowerer::hasLoopTarget(const std::string &label) const {
    if (loopLabels.empty())
      return false;
    if (label.empty())
      return true;
    return std::find(loopLabels.rbegin(), loopLabels.rend(), label) != loopLabels.rend();
  }

bool SyntaxLowerer::hasBreakTarget(const std::string &label) const {
    if (!label.empty())
      return hasLoopTarget(label);
    return !loopLabels.empty() || switchDepth > 0;
  }


const char *hirBinaryOperatorName(HirBinaryOperator operation) {
  switch (operation) {
  case HirBinaryOperator::Add: return "add";
  case HirBinaryOperator::Subtract: return "subtract";
  case HirBinaryOperator::Multiply: return "multiply";
  case HirBinaryOperator::Divide: return "divide";
  case HirBinaryOperator::Remainder: return "remainder";
  case HirBinaryOperator::Equal: return "equal";
  case HirBinaryOperator::NotEqual: return "not_equal";
  case HirBinaryOperator::Less: return "less";
  case HirBinaryOperator::LessEqual: return "less_equal";
  case HirBinaryOperator::Greater: return "greater";
  case HirBinaryOperator::GreaterEqual: return "greater_equal";
  case HirBinaryOperator::And: return "and";
  case HirBinaryOperator::Or: return "or";
  }
  return "unknown";
}

const char *hirUnaryOperatorName(HirUnaryOperator operation) {
  return operation == HirUnaryOperator::Negate ? "negate" : "not";
}

HirLoweringResult lowerSyntaxToHir(const std::string &moduleName, const SyntaxTree &tree,
                                   HirLoweringOptions options) {
  return SyntaxLowerer(moduleName, std::move(options)).lower(tree);
}

} // namespace kyna
