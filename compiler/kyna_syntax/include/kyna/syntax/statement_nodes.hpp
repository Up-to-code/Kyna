#pragma once

#include "kyna/syntax/expression_nodes.hpp"
#include <string>
#include <variant>
#include <vector>

namespace kyna {

struct ExprStmt {
  ExprPtr expression;
};
struct BlockStmt {
  std::vector<StmtPtr> statements;
  ExprPtr tail;
};
struct IfStmt {
  ExprPtr condition;
  StmtPtr thenBranch;
  StmtPtr elseBranch;
};
struct WhileStmt {
  ExprPtr condition;
  StmtPtr body;
  std::string label;
};
struct LoopStmt {
  StmtPtr initializer;
  ExprPtr condition;
  ExprPtr increment;
  StmtPtr body;
  std::string label;
};
struct SwitchCase {
  ExprPtr value;
  StmtPtr body;
  bool isDefault{false};
};
struct SwitchStmt {
  ExprPtr subject;
  std::vector<SwitchCase> cases;
};
struct BreakStmt {
  std::string label;
};
struct ContinueStmt {
  std::string label;
};
struct ReturnStmt {
  ExprPtr value;
};
struct ThrowStmt {
  ExprPtr value;
};
struct TryStmt {
  StmtPtr tryBranch;
  std::string catchName;
  StmtPtr catchBranch;
  StmtPtr finallyBranch;
};
struct InvalidStmt {};

} // namespace kyna
