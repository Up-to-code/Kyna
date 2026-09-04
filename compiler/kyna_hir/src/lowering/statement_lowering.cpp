#include "syntax_lowerer.hpp"
#include <type_traits>

namespace kyna {

std::optional<HirStatementId> SyntaxLowerer::lowerScopedStatement(const StmtPtr &statement) {
    scopes.emplace_back();
    auto lowered = lowerStatement(statement);
    scopes.pop_back();
    return lowered;
  }

std::optional<HirStatementId> SyntaxLowerer::lowerStatement(const StmtPtr &statement) {
    return std::visit(
        [&](const auto &node) -> std::optional<HirStatementId> {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, VarDecl>) {
            const auto initializer = lowerExpression(node.initializer);
            if (!initializer)
              return std::nullopt;
            const auto local = addLocal(node, statement->location);
            responseLocals[local.value] = isFetchCall(node.initializer);
            return addStatement(HirBindLocalStatement{local, *initializer},
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
          } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            const auto subject = lowerExpression(node.subject);
            bool lowered = subject.has_value();
            ++switchDepth;
            HirSwitchStatement sw;
            for (const auto &arm : node.cases) {
              HirSwitchCase loweredArm;
              if (arm.value) {
                const auto value = lowerExpression(arm.value);
                if (!value) {
                  lowered = false;
                  break;
                }
                loweredArm.value = *value;
              }
              const auto body = lowerScopedStatement(arm.body);
              if (!body) {
                lowered = false;
                break;
              }
              loweredArm.body = *body;
              sw.cases.push_back(std::move(loweredArm));
            }
            --switchDepth;
            if (!lowered)
              return std::nullopt;
            return addStatement(HirSwitchStatement{*subject, std::move(sw.cases)},
                                statement->location);
          } else if constexpr (std::is_same_v<T, BreakStmt>) {
            if (!hasBreakTarget(node.label)) {
              unsupported(node.label.empty() ? "break outside a switch or loop"
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

} // namespace kyna
