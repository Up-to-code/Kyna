#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/token.hpp"
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <type_traits>
#include <unordered_map>

namespace kyna {
namespace {

std::string decodeQuotedLiteral(const std::string &literal) {
  if (literal.size() < 2)
    return literal;
  std::string result;
  result.reserve(literal.size() - 2);
  for (std::size_t index = 1; index + 1 < literal.size(); ++index) {
    if (literal[index] != '\\' || index + 2 >= literal.size()) {
      result.push_back(literal[index]);
      continue;
    }
    switch (literal[++index]) {
    case 'n': result.push_back('\n'); break;
    case 'r': result.push_back('\r'); break;
    case 't': result.push_back('\t'); break;
    case '\\': result.push_back('\\'); break;
    case '\'': result.push_back('\''); break;
    case '"': result.push_back('"'); break;
    default: result.push_back(literal[index]); break;
    }
  }
  return result;
}

class SyntaxLowerer {
public:
  explicit SyntaxLowerer(std::string moduleName) { program.name = std::move(moduleName); }

  HirLoweringResult lower(const SyntaxTree &tree) {
    for (const auto &statement : tree.module.declarations)
      if (const auto *function = std::get_if<FunctionDecl>(&statement->node)) {
        if (functions.contains(function->name)) {
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
      }

    if (!diagnostics.empty())
      return {std::nullopt, std::move(diagnostics)};

    for (const auto &statement : tree.module.declarations)
      if (const auto *function = std::get_if<FunctionDecl>(&statement->node))
        if (const auto found = functions.find(function->name); found != functions.end())
          lowerFunction(found->second, *function, statement->location);

    scopes.clear();
    scopes.emplace_back();
    for (const auto &statement : tree.module.declarations) {
      if (std::holds_alternative<ImportDecl>(statement->node) ||
          std::holds_alternative<FunctionDecl>(statement->node))
        continue;
      if (const auto lowered = lowerStatement(statement))
        program.body.push_back(*lowered);
    }
    if (!diagnostics.empty())
      return {std::nullopt, std::move(diagnostics)};
    return {std::move(program), {}};
  }

private:
  HirProgram program;
  std::vector<Diagnostic> diagnostics;
  std::vector<std::unordered_map<std::string, HirLocalId>> scopes;
  std::unordered_map<std::string, HirFunctionId> functions;
  std::vector<std::string> loopLabels;
  std::vector<std::optional<HirFunctionId>> localOwners;
  std::optional<HirFunctionId> currentFunction;
  std::unordered_map<const Stmt *, std::pair<HirFunctionId, HirLocalId>> nestedFunctions;

  void unsupported(std::string construct, SourceSpan span) {
    Diagnostic diagnostic{"HIR lowering does not yet support " + std::move(construct), span,
                          false, "KHIR1201"};
    diagnostic.category = "hir";
    diagnostic.help = "use 'kyna run' while this construct is migrated to the bytecode pipeline";
    diagnostics.push_back(std::move(diagnostic));
  }

  HirExpressionId addExpression(HirExpression::Node node, SourceSpan span) {
    const auto id = HirExpressionId{static_cast<std::uint32_t>(program.expressions.size())};
    program.expressions.push_back({std::move(node), span});
    return id;
  }

  HirStatementId addStatement(HirStatement::Node node, SourceSpan span) {
    const auto id = HirStatementId{static_cast<std::uint32_t>(program.statements.size())};
    program.statements.push_back({std::move(node), span});
    return id;
  }

  HirLocalId addLocal(const VarDecl &declaration, SourceSpan span) {
    const auto id = HirLocalId{static_cast<std::uint32_t>(program.locals.size())};
    program.locals.push_back({declaration.name, declaration.mutableBinding, span});
    localOwners.push_back(currentFunction);
    scopes.back().insert_or_assign(declaration.name, id);
    return id;
  }

  HirLocalId addParameter(const Param &parameter, SourceSpan span) {
    const auto id = HirLocalId{static_cast<std::uint32_t>(program.locals.size())};
    program.locals.push_back({parameter.name, false, span});
    localOwners.push_back(currentFunction);
    scopes.back().insert_or_assign(parameter.name, id);
    return id;
  }

  HirLocalId addNamedLocal(const std::string &name, bool mutableBinding, SourceSpan span) {
    const auto id = HirLocalId{static_cast<std::uint32_t>(program.locals.size())};
    program.locals.push_back({name, mutableBinding, span});
    localOwners.push_back(currentFunction);
    scopes.back().insert_or_assign(name, id);
    return id;
  }

  void captureIfNeeded(HirLocalId local) {
    if (!currentFunction || local.value >= localOwners.size() ||
        localOwners[local.value] == currentFunction)
      return;
    auto &captures = program.functions[currentFunction->value].captures;
    if (std::find(captures.begin(), captures.end(), local) == captures.end())
      captures.push_back(local);
  }

  void lowerFunction(HirFunctionId id, const FunctionDecl &declaration, SourceSpan span,
                     bool preserveOuterScopes = false) {
    const auto previousFunction = currentFunction;
    auto previousLoops = std::move(loopLabels);
    std::vector<std::unordered_map<std::string, HirLocalId>> savedScopes;
    if (!preserveOuterScopes) {
      savedScopes = std::move(scopes);
      scopes.clear();
    }
    currentFunction = id;
    scopes.emplace_back();
    for (const auto &parameter : declaration.params)
      program.functions.at(id.value).parameters.push_back(addParameter(parameter, span));
    const auto body = lowerStatement(declaration.body);
    scopes.pop_back();
    if (body)
      program.functions.at(id.value).body = *body;
    const auto captures = program.functions.at(id.value).captures;
    currentFunction = previousFunction;
    loopLabels = std::move(previousLoops);
    if (!preserveOuterScopes)
      scopes = std::move(savedScopes);
    if (currentFunction)
      for (const auto capture : captures)
        captureIfNeeded(capture);
  }

  std::optional<HirLocalId> findLocal(const std::string &name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope)
      if (const auto found = scope->find(name); found != scope->end())
        return found->second;
    return std::nullopt;
  }

  void predeclareNestedFunctions(const BlockStmt &block) {
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

  bool hasLoopTarget(const std::string &label) const {
    if (loopLabels.empty())
      return false;
    if (label.empty())
      return true;
    return std::find(loopLabels.rbegin(), loopLabels.rend(), label) != loopLabels.rend();
  }

  std::optional<HirBinaryOperator> lowerOperator(TokenKind operation) {
    switch (operation) {
    case TokenKind::Plus: return HirBinaryOperator::Add;
    case TokenKind::Minus: return HirBinaryOperator::Subtract;
    case TokenKind::Star: return HirBinaryOperator::Multiply;
    case TokenKind::Slash: return HirBinaryOperator::Divide;
    case TokenKind::Percent: return HirBinaryOperator::Remainder;
    case TokenKind::EqualEqual: return HirBinaryOperator::Equal;
    case TokenKind::BangEqual: return HirBinaryOperator::NotEqual;
    case TokenKind::Less: return HirBinaryOperator::Less;
    case TokenKind::LessEqual: return HirBinaryOperator::LessEqual;
    case TokenKind::Greater: return HirBinaryOperator::Greater;
    case TokenKind::GreaterEqual: return HirBinaryOperator::GreaterEqual;
    case TokenKind::AndAnd: return HirBinaryOperator::And;
    case TokenKind::OrOr: return HirBinaryOperator::Or;
    default: return std::nullopt;
    }
  }

  struct ValueBlock {
    HirStatementId prelude;
    HirExpressionId value;
  };

  std::optional<ValueBlock> lowerValueBlock(const StmtPtr &statement) {
    const auto *block = statement ? std::get_if<BlockStmt>(&statement->node) : nullptr;
    if (!block) {
      unsupported("a non-block expression branch", statement ? statement->location : SourceSpan{});
      return std::nullopt;
    }
    scopes.emplace_back();
    predeclareNestedFunctions(*block);
    std::vector<HirStatementId> statements;
    for (const auto &child : block->statements)
      if (const auto lowered = lowerStatement(child))
        statements.push_back(*lowered);
    const auto value = block->tail
                           ? lowerExpression(block->tail)
                           : std::optional{addExpression(HirConstantExpression{nullptr},
                                                         statement->location)};
    scopes.pop_back();
    if (!value)
      return std::nullopt;
    return ValueBlock{addStatement(HirBlockStatement{std::move(statements)},
                                   statement->location),
                      *value};
  }

  std::optional<HirExpressionId> lowerExpression(const ExprPtr &expression) {
    if (!expression)
      return addExpression(HirConstantExpression{nullptr}, {});
    return std::visit(
        [&](const auto &node) -> std::optional<HirExpressionId> {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, Literal>) {
            HirConstant value;
            switch (node.kind) {
            case Literal::Kind::Null: value = nullptr; break;
            case Literal::Kind::Bool: value = node.value == "true"; break;
            case Literal::Kind::Int: value = static_cast<std::int64_t>(std::stoll(node.value)); break;
            case Literal::Kind::Float: value = std::stod(node.value); break;
            case Literal::Kind::String: value = decodeQuotedLiteral(node.value); break;
            case Literal::Kind::Char: {
              const auto decoded = decodeQuotedLiteral(node.value);
              value = decoded.empty() ? '\0' : decoded.front();
              break;
            }
            }
            return addExpression(HirConstantExpression{std::move(value)}, expression->location);
          } else if constexpr (std::is_same_v<T, Variable>) {
            const auto local = findLocal(node.name);
            if (local) {
              captureIfNeeded(*local);
              return addExpression(HirLocalExpression{*local}, expression->location);
            }
            if (const auto function = functions.find(node.name); function != functions.end())
              return addExpression(HirFunctionReferenceExpression{function->second},
                                   expression->location);
            unsupported("an unresolved local '" + node.name + "'", expression->location);
            return std::nullopt;
          } else if constexpr (std::is_same_v<T, Unary>) {
            if (node.op != TokenKind::Minus && node.op != TokenKind::Bang) {
              unsupported("unary operator '" + tokenName(node.op) + "'", expression->location);
              return std::nullopt;
            }
            const auto operand = lowerExpression(node.right);
            if (!operand)
              return std::nullopt;
            return addExpression(
                HirUnaryExpression{node.op == TokenKind::Minus ? HirUnaryOperator::Negate
                                                               : HirUnaryOperator::Not,
                                   *operand},
                expression->location);
          } else if constexpr (std::is_same_v<T, Call>) {
            const auto *callee = node.callee ? std::get_if<Variable>(&node.callee->node) : nullptr;
            std::vector<HirExpressionId> arguments;
            arguments.reserve(node.args.size());
            for (const auto &argument : node.args) {
              const auto lowered = lowerExpression(argument);
              if (!lowered)
                return std::nullopt;
              arguments.push_back(*lowered);
            }
            if (callee && !findLocal(callee->name) && functions.contains(callee->name))
              return addExpression(HirCallExpression{functions.at(callee->name),
                                                     std::move(arguments)},
                                   expression->location);
            const auto loweredCallee = lowerExpression(node.callee);
            if (!loweredCallee)
              return std::nullopt;
            return addExpression(
                HirIndirectCallExpression{*loweredCallee, std::move(arguments)},
                expression->location);
          } else if constexpr (std::is_same_v<T, Binary>) {
            const auto operation = lowerOperator(node.op);
            if (!operation) {
              unsupported("binary operator '" + tokenName(node.op) + "'", expression->location);
              return std::nullopt;
            }
            const auto left = lowerExpression(node.left);
            const auto right = lowerExpression(node.right);
            if (!left || !right)
              return std::nullopt;
            return addExpression(HirBinaryExpression{*left, *operation, *right},
                                 expression->location);
          } else if constexpr (std::is_same_v<T, Member>) {
            const auto object = lowerExpression(node.object);
            return object ? std::optional{addExpression(
                                HirMemberExpression{*object, node.name}, expression->location)}
                          : std::nullopt;
          } else if constexpr (std::is_same_v<T, Assign>) {
            const auto *target = node.target ? std::get_if<Variable>(&node.target->node) : nullptr;
            const auto local = target ? findLocal(target->name) : std::nullopt;
            if (!local) {
              unsupported("non-local assignment", expression->location);
              return std::nullopt;
            }
            const auto value = lowerExpression(node.value);
            if (!value)
              return std::nullopt;
            captureIfNeeded(*local);
            return addExpression(HirAssignLocalExpression{*local, *value}, expression->location);
          } else if constexpr (std::is_same_v<T, IfExpr>) {
            const auto condition = lowerExpression(node.condition);
            const auto thenBranch = lowerValueBlock(node.thenBranch);
            const auto elseBranch = lowerValueBlock(node.elseBranch);
            if (!condition || !thenBranch || !elseBranch)
              return std::nullopt;
            return addExpression(HirIfExpression{*condition, thenBranch->prelude,
                                                 thenBranch->value, elseBranch->prelude,
                                                 elseBranch->value},
                                 expression->location);
          } else if constexpr (std::is_same_v<T, MatchExpr>) {
            const auto subject = lowerExpression(node.subject);
            if (!subject)
              return std::nullopt;
            std::vector<HirMatchArm> arms;
            arms.reserve(node.arms.size());
            for (const auto &arm : node.arms) {
              const auto pattern = arm.wildcard ? std::optional<HirExpressionId>{}
                                                : lowerExpression(arm.pattern);
              const auto value = lowerExpression(arm.value);
              if ((!arm.wildcard && !pattern) || !value)
                return std::nullopt;
              arms.push_back({pattern, *value});
            }
            return addExpression(HirMatchExpression{*subject, std::move(arms)},
                                 expression->location);
          } else {
            unsupported("this expression", expression->location);
            return std::nullopt;
          }
        },
        expression->node);
  }

  std::optional<HirStatementId> lowerScopedStatement(const StmtPtr &statement) {
    scopes.emplace_back();
    auto lowered = lowerStatement(statement);
    scopes.pop_back();
    return lowered;
  }

  std::optional<HirStatementId> lowerStatement(const StmtPtr &statement) {
    return std::visit(
        [&](const auto &node) -> std::optional<HirStatementId> {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, VarDecl>) {
            const auto initializer = lowerExpression(node.initializer);
            if (!initializer)
              return std::nullopt;
            return addStatement(HirBindLocalStatement{addLocal(node, statement->location),
                                                       *initializer},
                                statement->location);
          } else if constexpr (std::is_same_v<T, ExprStmt>) {
            const auto expression = lowerExpression(node.expression);
            return expression ? std::optional{addStatement(HirEvaluateStatement{*expression},
                                                            statement->location)}
                              : std::nullopt;
          } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            const auto expression = lowerExpression(node.value);
            return expression ? std::optional{addStatement(HirReturnStatement{*expression},
                                                            statement->location)}
                              : std::nullopt;
          } else if constexpr (std::is_same_v<T, BlockStmt>) {
            scopes.emplace_back();
            predeclareNestedFunctions(node);
            std::vector<HirStatementId> statements;
            for (const auto &child : node.statements)
              if (const auto lowered = lowerStatement(child))
                statements.push_back(*lowered);
            if (node.tail)
              if (const auto tail = lowerExpression(node.tail))
                statements.push_back(addStatement(HirEvaluateStatement{*tail},
                                                  node.tail->location));
            scopes.pop_back();
            return addStatement(HirBlockStatement{std::move(statements)}, statement->location);
          } else if constexpr (std::is_same_v<T, IfStmt>) {
            const auto condition = lowerExpression(node.condition);
            const auto thenBranch = lowerScopedStatement(node.thenBranch);
            const auto elseBranch =
                node.elseBranch ? lowerScopedStatement(node.elseBranch) : std::nullopt;
            if (!condition || !thenBranch || (node.elseBranch && !elseBranch))
              return std::nullopt;
            return addStatement(HirIfStatement{*condition, *thenBranch, elseBranch},
                                statement->location);
          } else if constexpr (std::is_same_v<T, WhileStmt>) {
            const auto condition = lowerExpression(node.condition);
            loopLabels.push_back(node.label);
            const auto body = lowerScopedStatement(node.body);
            loopLabels.pop_back();
            if (!condition || !body)
              return std::nullopt;
            return addStatement(HirWhileStatement{*condition, *body, node.label},
                                statement->location);
          } else if constexpr (std::is_same_v<T, LoopStmt>) {
            scopes.emplace_back();
            const auto initializer = node.initializer ? lowerStatement(node.initializer) : std::nullopt;
            const auto condition = node.condition
                                       ? lowerExpression(node.condition)
                                       : std::optional{addExpression(HirConstantExpression{true},
                                                                     statement->location)};
            const auto increment = node.increment ? lowerExpression(node.increment) : std::nullopt;
            loopLabels.push_back(node.label);
            const auto body = lowerStatement(node.body);
            loopLabels.pop_back();
            scopes.pop_back();
            if ((node.initializer && !initializer) || !condition ||
                (node.increment && !increment) || !body)
              return std::nullopt;
            return addStatement(
                HirLoopStatement{initializer, *condition, increment, *body, node.label},
                statement->location);
          } else if constexpr (std::is_same_v<T, BreakStmt>) {
            if (!hasLoopTarget(node.label)) {
              unsupported(node.label.empty() ? "break outside a loop"
                                             : "break to unknown loop label '" + node.label + "'",
                          statement->location);
              return std::nullopt;
            }
            return addStatement(HirBreakStatement{node.label}, statement->location);
          } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            if (!hasLoopTarget(node.label)) {
              unsupported(node.label.empty()
                              ? "continue outside a loop"
                              : "continue to unknown loop label '" + node.label + "'",
                          statement->location);
              return std::nullopt;
            }
            return addStatement(HirContinueStatement{node.label}, statement->location);
          } else if constexpr (std::is_same_v<T, ThrowStmt>) {
            const auto value = lowerExpression(node.value);
            return value ? std::optional{addStatement(HirThrowStatement{*value},
                                                       statement->location)}
                         : std::nullopt;
          } else if constexpr (std::is_same_v<T, TryStmt>) {
            const auto tryBranch = lowerScopedStatement(node.tryBranch);
            std::optional<HirLocalId> catchLocal;
            std::optional<HirStatementId> catchBranch;
            if (node.catchBranch) {
              scopes.emplace_back();
              catchLocal = addNamedLocal(node.catchName, false, statement->location);
              catchBranch = lowerStatement(node.catchBranch);
              scopes.pop_back();
            }
            const auto finallyBranch = node.finallyBranch
                                           ? lowerScopedStatement(node.finallyBranch)
                                           : std::optional<HirStatementId>{};
            if (!tryBranch || (node.catchBranch && !catchBranch) ||
                (node.finallyBranch && !finallyBranch))
              return std::nullopt;
            return addStatement(HirTryStatement{*tryBranch, catchLocal, catchBranch,
                                                 finallyBranch},
                                statement->location);
          } else if constexpr (std::is_same_v<T, FunctionDecl>) {
            const auto nested = nestedFunctions.find(statement.get());
            if (nested == nestedFunctions.end()) {
              unsupported("a function declaration outside its owning block", statement->location);
              return std::nullopt;
            }
            lowerFunction(nested->second.first, node, statement->location, true);
            const auto closure = addExpression(
                HirClosureExpression{nested->second.first}, statement->location);
            return addStatement(
                HirBindLocalStatement{nested->second.second, closure}, statement->location);
          } else if constexpr (std::is_same_v<T, ImportDecl>) {
            return std::nullopt;
          } else {
            unsupported("this statement", statement->location);
            return std::nullopt;
          }
        },
        statement->node);
  }
};

} // namespace

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

HirLoweringResult lowerSyntaxToHir(const std::string &moduleName, const SyntaxTree &tree) {
  return SyntaxLowerer(moduleName).lower(tree);
}

} // namespace kyna
