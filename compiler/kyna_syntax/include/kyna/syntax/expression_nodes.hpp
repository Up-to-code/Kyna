#pragma once

#include "kyna/lexing/token.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace kyna {

struct Expr;
struct Stmt;
using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

struct Literal {
  enum class Kind { Null, Bool, Int, Float, String, Char };
  Kind kind;
  std::string value;
};
struct Variable {
  std::string name;
};
struct SelfExpr {};
struct SuperExpr {};
struct Unary {
  TokenKind op;
  ExprPtr right;
};
struct AwaitExpr {
  ExprPtr operand;
};
struct Binary {
  ExprPtr left;
  TokenKind op;
  ExprPtr right;
};
struct Assign {
  ExprPtr target;
  ExprPtr value;
};
struct Call {
  ExprPtr callee;
  std::vector<ExprPtr> args;
};
struct Member {
  ExprPtr object;
  std::string name;
};
struct Index {
  ExprPtr object;
  ExprPtr index;
};
struct ArrayExpr {
  std::vector<ExprPtr> elements;
};
struct NewExpr {
  std::string className;
  std::vector<ExprPtr> args;
};
struct ObjectField {
  std::string name;
  ExprPtr value;
};
struct ObjectExpr {
  std::vector<ObjectField> fields;
};
struct IfExpr {
  ExprPtr condition;
  StmtPtr thenBranch;
  StmtPtr elseBranch;
};
struct MatchArm {
  ExprPtr pattern;
  ExprPtr value;
  bool wildcard{false};
};
struct MatchExpr {
  ExprPtr subject;
  std::vector<MatchArm> arms;
};
struct InvalidExpr {};

struct Expr {
  using Node =
      std::variant<Literal, Variable, SelfExpr, SuperExpr, Unary, AwaitExpr, Binary, Assign,
                   Call, Member, Index, ArrayExpr, NewExpr, ObjectExpr, IfExpr, MatchExpr,
                   InvalidExpr>;
  Node node;
  SourceSpan location;
};

} // namespace kyna
