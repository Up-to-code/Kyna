#pragma once

#include "kyna/syntax/declaration_nodes.hpp"
#include "kyna/syntax/expression_nodes.hpp"
#include "kyna/syntax/statement_nodes.hpp"

namespace kyna::syntax {

// A clean, reusable traversal interface over the syntax tree. A concrete
// visitor subclasses `ASTVisitor<Result>` and overrides only the node kinds it
// cares about; the default implementations return `Result{}` so unrelated
// passes remain small. This complements the `std::visit`-based dispatch used by
// individual passes and gives passes a single, type-safe contract to implement.
//
// To keep a pass focused on its own concern, prefer:
//   class MyPass final : public ASTVisitor<TypeRef> { ... };
// and dispatch an expression with `visitExpr(e)` / a statement with
// `visitStmt(s)`.
template <typename Result>
class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  // ---- Expressions -------------------------------------------------------
  virtual Result visitLiteral(const Literal &) { return Result{}; }
  virtual Result visitVariable(const Variable &) { return Result{}; }
  virtual Result visitSelf(const SelfExpr &) { return Result{}; }
  virtual Result visitSuper(const SuperExpr &) { return Result{}; }
  virtual Result visitUnary(const Unary &) { return Result{}; }
  virtual Result visitAwait(const AwaitExpr &) { return Result{}; }
  virtual Result visitBinary(const Binary &) { return Result{}; }
  virtual Result visitAssign(const Assign &) { return Result{}; }
  virtual Result visitCall(const Call &) { return Result{}; }
  virtual Result visitMember(const Member &) { return Result{}; }
  virtual Result visitIndex(const Index &) { return Result{}; }
  virtual Result visitArray(const ArrayExpr &) { return Result{}; }
  virtual Result visitNew(const NewExpr &) { return Result{}; }
  virtual Result visitObject(const ObjectExpr &) { return Result{}; }
  virtual Result visitIfExpression(const IfExpr &) { return Result{}; }
  virtual Result visitMatch(const MatchExpr &) { return Result{}; }

  // ---- Statements --------------------------------------------------------
  virtual Result visitVarDecl(const VarDecl &) { return Result{}; }
  virtual Result visitExprStmt(const ExprStmt &) { return Result{}; }
  virtual Result visitBlock(const BlockStmt &) { return Result{}; }
  virtual Result visitIfStmt(const IfStmt &) { return Result{}; }
  virtual Result visitWhile(const WhileStmt &) { return Result{}; }
  virtual Result visitLoop(const LoopStmt &) { return Result{}; }
  virtual Result visitSwitch(const SwitchStmt &) { return Result{}; }
  virtual Result visitBreak(const BreakStmt &) { return Result{}; }
  virtual Result visitContinue(const ContinueStmt &) { return Result{}; }
  virtual Result visitReturn(const ReturnStmt &) { return Result{}; }
  virtual Result visitThrow(const ThrowStmt &) { return Result{}; }
  virtual Result visitTry(const TryStmt &) { return Result{}; }
  virtual Result visitFunctionDecl(const FunctionDecl &) { return Result{}; }
  virtual Result visitClassDecl(const ClassDecl &) { return Result{}; }
  virtual Result visitInterfaceDecl(const InterfaceDecl &) { return Result{}; }
  virtual Result visitImport(const ImportDecl &) { return Result{}; }
  virtual Result visitExport(const ExportDecl &) { return Result{}; }

  // Convenience dispatchers that walk `Expr` / `Stmt` variant nodes.
  Result visitExpr(const ExprPtr &expr) {
    if (!expr)
      return Result{};
    return std::visit(
        [this](const auto &n) -> Result { return this->visitNode(n); }, expr->node);
  }

  Result visitStmt(const StmtPtr &stmt) {
    if (!stmt)
      return Result{};
    return std::visit(
        [this](const auto &n) -> Result { return this->visitNode(n); }, stmt->node);
  }

private:
  template <typename Node>
  Result visitNode(const Node &n);
};

// Dispatcher table mapping each concrete node type to its virtual hook.
// Defined out-of-line so the template instantiates lazily only for the kinds a
// pass actually traverses.
template <typename Result>
template <typename Node>
Result ASTVisitor<Result>::visitNode(const Node &n) {
  if constexpr (std::is_same_v<Node, Literal>)
    return visitLiteral(n);
  else if constexpr (std::is_same_v<Node, Variable>)
    return visitVariable(n);
  else if constexpr (std::is_same_v<Node, SelfExpr>)
    return visitSelf(n);
  else if constexpr (std::is_same_v<Node, SuperExpr>)
    return visitSuper(n);
  else if constexpr (std::is_same_v<Node, Unary>)
    return visitUnary(n);
  else if constexpr (std::is_same_v<Node, AwaitExpr>)
    return visitAwait(n);
  else if constexpr (std::is_same_v<Node, Binary>)
    return visitBinary(n);
  else if constexpr (std::is_same_v<Node, Assign>)
    return visitAssign(n);
  else if constexpr (std::is_same_v<Node, Call>)
    return visitCall(n);
  else if constexpr (std::is_same_v<Node, Member>)
    return visitMember(n);
  else if constexpr (std::is_same_v<Node, Index>)
    return visitIndex(n);
  else if constexpr (std::is_same_v<Node, ArrayExpr>)
    return visitArray(n);
  else if constexpr (std::is_same_v<Node, NewExpr>)
    return visitNew(n);
  else if constexpr (std::is_same_v<Node, ObjectExpr>)
    return visitObject(n);
  else if constexpr (std::is_same_v<Node, IfExpr>)
    return visitIfExpression(n);
  else if constexpr (std::is_same_v<Node, MatchExpr>)
    return visitMatch(n);
  else if constexpr (std::is_same_v<Node, VarDecl>)
    return visitVarDecl(n);
  else if constexpr (std::is_same_v<Node, ExprStmt>)
    return visitExprStmt(n);
  else if constexpr (std::is_same_v<Node, BlockStmt>)
    return visitBlock(n);
  else if constexpr (std::is_same_v<Node, IfStmt>)
    return visitIfStmt(n);
  else if constexpr (std::is_same_v<Node, WhileStmt>)
    return visitWhile(n);
  else if constexpr (std::is_same_v<Node, LoopStmt>)
    return visitLoop(n);
  else if constexpr (std::is_same_v<Node, SwitchStmt>)
    return visitSwitch(n);
  else if constexpr (std::is_same_v<Node, BreakStmt>)
    return visitBreak(n);
  else if constexpr (std::is_same_v<Node, ContinueStmt>)
    return visitContinue(n);
  else if constexpr (std::is_same_v<Node, ReturnStmt>)
    return visitReturn(n);
  else if constexpr (std::is_same_v<Node, ThrowStmt>)
    return visitThrow(n);
  else if constexpr (std::is_same_v<Node, TryStmt>)
    return visitTry(n);
  else if constexpr (std::is_same_v<Node, FunctionDecl>)
    return visitFunctionDecl(n);
  else if constexpr (std::is_same_v<Node, ClassDecl>)
    return visitClassDecl(n);
  else if constexpr (std::is_same_v<Node, InterfaceDecl>)
    return visitInterfaceDecl(n);
  else if constexpr (std::is_same_v<Node, ImportDecl>)
    return visitImport(n);
  else if constexpr (std::is_same_v<Node, ExportDecl>)
    return visitExport(n);
  else
    return Result{};
}

} // namespace kyna::syntax
