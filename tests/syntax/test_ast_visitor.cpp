#include "kyna/syntax/ast_visitor.hpp"

#include <cassert>
#include <memory>
#include <string>

namespace {

using namespace kyna;
using namespace kyna::syntax;

// A visitor that counts every expression / statement kind it encounters. It is
// used to prove the template dispatches correctly to the virtual hooks.
class CountingVisitor final : public ASTVisitor<int> {
public:
  int expressions = 0;
  int statements = 0;

  int visitLiteral(const Literal &) override {
    ++expressions;
    return 1;
  }
  int visitBinary(const Binary &) override {
    ++expressions;
    return 1;
  }
  int visitCall(const Call &) override {
    ++expressions;
    return 1;
  }
  int visitReturn(const ReturnStmt &) override {
    ++statements;
    return 1;
  }
  int visitFunctionDecl(const FunctionDecl &) override {
    ++statements;
    return 1;
  }
};

ExprPtr makeInt(int value) {
  auto e = std::make_shared<Expr>();
  e->node = Literal{Literal::Kind::Int, std::to_string(value)};
  return e;
}

void test_expression_dispatch() {
  auto binary = std::make_shared<Expr>();
  binary->node = Binary{makeInt(1), TokenKind::Plus, makeInt(2)};

  CountingVisitor visitor;
  int result = visitor.visitExpr(binary);
  assert(result == 1);
  assert(visitor.expressions == 1);
}

void test_statement_dispatch() {
  auto fn = std::make_shared<Stmt>();
  FunctionDecl decl;
  decl.name = "add";
  fn->node = decl;

  CountingVisitor visitor;
  int result = visitor.visitStmt(fn);
  assert(result == 1);
  assert(visitor.statements == 1);
}

void test_default_hooks_return_zero_and_do_not_count() {
  auto member = std::make_shared<Expr>();
  member->node = Member{makeInt(1), "field"};

  CountingVisitor visitor;
  int result = visitor.visitExpr(member); // no override -> default 0
  assert(result == 0);
  assert(visitor.expressions == 0);
}

} // namespace

int main() {
  test_expression_dispatch();
  test_statement_dispatch();
  test_default_hooks_return_zero_and_do_not_count();
  return 0;
}
