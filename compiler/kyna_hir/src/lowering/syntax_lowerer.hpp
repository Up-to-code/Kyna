#pragma once

#include "kyna/hir/syntax_lowering.hpp"
#include "kyna/lexing/token.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kyna {

class SyntaxLowerer {
public:
  explicit SyntaxLowerer(std::string moduleName, HirLoweringOptions loweringOptions);
  HirLoweringResult lower(const SyntaxTree &tree);

private:
  HirProgram program;
  HirLoweringOptions options;
  std::vector<Diagnostic> diagnostics;
  std::vector<std::unordered_map<std::string, HirLocalId>> scopes;
  std::unordered_map<std::string, HirFunctionId> functions;
  std::unordered_map<std::string, HirClassId> classes;
  std::vector<std::string> loopLabels;
  int switchDepth{0};
  std::vector<std::optional<HirFunctionId>> localOwners;
  std::vector<bool> responseLocals;
  std::optional<HirFunctionId> currentFunction;
  std::optional<HirLocalId> currentSelf;
  std::optional<HirClassId> currentClass;
  std::unordered_map<const Stmt *, std::pair<HirFunctionId, HirLocalId>> nestedFunctions;

  void unsupported(std::string construct, SourceSpan span);
  HirExpressionId addExpression(HirExpression::Node node, SourceSpan span);
  HirStatementId addStatement(HirStatement::Node node, SourceSpan span);
  HirLocalId addLocal(const VarDecl &declaration, SourceSpan span);
  HirLocalId addParameter(const Param &parameter, SourceSpan span);
  HirLocalId addNamedLocal(const std::string &name, bool mutableBinding, SourceSpan span);
  void captureIfNeeded(HirLocalId local);
  void lowerFunction(HirFunctionId id, const FunctionDecl &declaration, SourceSpan span,
                     bool preserveOuterScopes = false,
                     std::optional<HirClassId> owningClass = std::nullopt);
  std::optional<HirLocalId> findLocal(const std::string &name) const;
  bool isFetchCall(const ExprPtr &expression) const;
  bool isResponseExpression(const ExprPtr &expression) const;
  void predeclareNestedFunctions(const BlockStmt &block);
  bool hasLoopTarget(const std::string &label) const;
  bool hasBreakTarget(const std::string &label) const;
  std::optional<HirBinaryOperator> lowerOperator(TokenKind operation);
  struct ValueBlock {
    HirStatementId prelude;
    HirExpressionId value;
  };
  std::optional<ValueBlock> lowerValueBlock(const StmtPtr &statement);
  std::optional<HirExpressionId> lowerExpression(const ExprPtr &expression);
  std::optional<HirStatementId> lowerScopedStatement(const StmtPtr &statement);
  std::optional<HirStatementId> lowerStatement(const StmtPtr &statement);
};

} // namespace kyna
