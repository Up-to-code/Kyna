#pragma once

#include "kyna/source/source_span.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kyna {

struct HirExpressionId {
  std::uint32_t value{0};
  auto operator<=>(const HirExpressionId &) const = default;
};

struct HirStatementId {
  std::uint32_t value{0};
  auto operator<=>(const HirStatementId &) const = default;
};

struct HirLocalId {
  std::uint32_t value{0};
  auto operator<=>(const HirLocalId &) const = default;
};

struct HirFunctionId {
  std::uint32_t value{0};
  auto operator<=>(const HirFunctionId &) const = default;
};

using HirConstant = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, char>;

enum class HirUnaryOperator { Negate, Not };
enum class HirBinaryOperator {
  Add,
  Subtract,
  Multiply,
  Divide,
  Remainder,
  Equal,
  NotEqual,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  And,
  Or
};

struct HirConstantExpression {
  HirConstant value;
};
struct HirLocalExpression {
  HirLocalId local;
};
struct HirFunctionReferenceExpression {
  HirFunctionId function;
};
struct HirClosureExpression {
  HirFunctionId function;
};
struct HirUnaryExpression {
  HirUnaryOperator operation;
  HirExpressionId operand;
};
struct HirBinaryExpression {
  HirExpressionId left;
  HirBinaryOperator operation;
  HirExpressionId right;
};
struct HirAssignLocalExpression {
  HirLocalId local;
  HirExpressionId value;
};
struct HirCallExpression {
  HirFunctionId function;
  std::vector<HirExpressionId> arguments;
};
struct HirIndirectCallExpression {
  HirExpressionId callee;
  std::vector<HirExpressionId> arguments;
};
struct HirMemberExpression {
  HirExpressionId object;
  std::string member;
};
struct HirIfExpression {
  HirExpressionId condition;
  HirStatementId thenPrelude;
  HirExpressionId thenValue;
  HirStatementId elsePrelude;
  HirExpressionId elseValue;
};
struct HirMatchArm {
  std::optional<HirExpressionId> pattern;
  HirExpressionId value;
};
struct HirMatchExpression {
  HirExpressionId subject;
  std::vector<HirMatchArm> arms;
};

struct HirExpression {
  using Node = std::variant<HirConstantExpression, HirLocalExpression,
                            HirFunctionReferenceExpression, HirClosureExpression,
                            HirUnaryExpression,
                            HirBinaryExpression, HirAssignLocalExpression, HirCallExpression,
                            HirIndirectCallExpression, HirMemberExpression, HirIfExpression,
                            HirMatchExpression>;
  Node node;
  SourceSpan span;
};

struct HirBindLocalStatement {
  HirLocalId local;
  HirExpressionId initializer;
};
struct HirEvaluateStatement {
  HirExpressionId expression;
};
struct HirReturnStatement {
  HirExpressionId expression;
};
struct HirBlockStatement {
  std::vector<HirStatementId> statements;
};
struct HirIfStatement {
  HirExpressionId condition;
  HirStatementId thenBranch;
  std::optional<HirStatementId> elseBranch;
};
struct HirWhileStatement {
  HirExpressionId condition;
  HirStatementId body;
  std::string label;
};
struct HirLoopStatement {
  std::optional<HirStatementId> initializer;
  HirExpressionId condition;
  std::optional<HirExpressionId> increment;
  HirStatementId body;
  std::string label;
};
struct HirBreakStatement {
  std::string label;
};
struct HirContinueStatement {
  std::string label;
};
struct HirThrowStatement {
  HirExpressionId value;
};
struct HirTryStatement {
  HirStatementId tryBranch;
  std::optional<HirLocalId> catchLocal;
  std::optional<HirStatementId> catchBranch;
  std::optional<HirStatementId> finallyBranch;
};

struct HirStatement {
  using Node = std::variant<HirBindLocalStatement, HirEvaluateStatement, HirReturnStatement,
                            HirBlockStatement, HirIfStatement, HirWhileStatement, HirLoopStatement,
                            HirBreakStatement, HirContinueStatement, HirThrowStatement,
                            HirTryStatement>;
  Node node;
  SourceSpan span;
};

struct HirLocal {
  std::string name;
  bool mutableBinding{false};
  SourceSpan span;
};

struct HirFunction {
  std::string name;
  std::vector<HirLocalId> parameters;
  HirStatementId body;
  SourceSpan span;
  std::vector<HirLocalId> captures;
  std::optional<HirFunctionId> parent;
};

struct HirProgram {
  std::string name;
  std::vector<HirExpression> expressions;
  std::vector<HirStatement> statements;
  std::vector<HirLocal> locals;
  std::vector<HirStatementId> body;
  std::vector<HirFunction> functions;
};

[[nodiscard]] const char *hirBinaryOperatorName(HirBinaryOperator operation);
[[nodiscard]] const char *hirUnaryOperatorName(HirUnaryOperator operation);

} // namespace kyna
