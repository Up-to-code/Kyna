#include "kyna/semantics/program_analyzer.hpp"

#include <type_traits>
#include <variant>

namespace kyna {

void Analyzer::stmt(const StmtPtr &s) {
  std::visit(
      [this, &s](const auto &n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, VarDecl>)
          checkVarDecl(n, s->location);
        else if constexpr (std::is_same_v<T, ExprStmt>)
          expr(n.expression);
        else if constexpr (std::is_same_v<T, BlockStmt>)
          checkBlock(n, s->location);
        else if constexpr (std::is_same_v<T, IfStmt>)
          checkIf(n);
        else if constexpr (std::is_same_v<T, WhileStmt>)
          checkWhile(n);
        else if constexpr (std::is_same_v<T, LoopStmt>)
          checkLoop(n, s->location);
        else if constexpr (std::is_same_v<T, SwitchStmt>)
          checkSwitch(n, s->location);
        else if constexpr (std::is_same_v<T, TryStmt>)
          checkTry(n);
        else if constexpr (std::is_same_v<T, BreakStmt>)
          checkBreak(n, s->location);
        else if constexpr (std::is_same_v<T, ContinueStmt>)
          checkContinue(n, s->location);
        else if constexpr (std::is_same_v<T, ReturnStmt>)
          checkReturn(n);
        else if constexpr (std::is_same_v<T, ThrowStmt>)
          expr(n.value);
        else if constexpr (std::is_same_v<T, FunctionDecl>)
          checkFunctionDecl(n, s->location);
        else if constexpr (std::is_same_v<T, ClassDecl>)
          checkClassDecl(n, s->location);
        else if constexpr (std::is_same_v<T, InterfaceDecl> || std::is_same_v<T, ImportDecl> ||
                           std::is_same_v<T, ExportDecl> || std::is_same_v<T, InvalidStmt>) {
        }
      },
      s->node);
}

bool Analyzer::alwaysReturns(const StmtPtr &s) const {
  if (!s)
    return false;
  if (std::holds_alternative<ReturnStmt>(s->node) || std::holds_alternative<ThrowStmt>(s->node))
    return true;
  if (auto b = std::get_if<BlockStmt>(&s->node)) {
    for (auto &x : b->statements)
      if (alwaysReturns(x))
        return true;
    return false;
  }
  if (auto i = std::get_if<IfStmt>(&s->node))
    return i->elseBranch && alwaysReturns(i->thenBranch) && alwaysReturns(i->elseBranch);
  if (auto attempt = std::get_if<TryStmt>(&s->node)) {
    if (attempt->finallyBranch && alwaysReturns(attempt->finallyBranch))
      return true;
    if (attempt->catchBranch)
      return alwaysReturns(attempt->tryBranch) && alwaysReturns(attempt->catchBranch);
    return alwaysReturns(attempt->tryBranch);
  }
  if (auto sw = std::get_if<SwitchStmt>(&s->node)) {
    bool defaultSeen = false;
    for (const auto &arm : sw->cases) {
      if (!alwaysReturns(arm.body))
        return false;
      if (arm.isDefault)
        defaultSeen = true;
    }
    return defaultSeen;
  }
  return false;
}

} // namespace kyna
