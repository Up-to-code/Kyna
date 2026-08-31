#include "kyna/parsing/recursive_descent_parser.hpp"
#include <cassert>

int main() {
  auto program =
      kyna::Parser(kyna::lex("class Box { public value: int; } let b = { value: 1 };")).parse();
  assert(program.size() == 2);
  assert(std::holds_alternative<kyna::ClassDecl>(program[0]->node));
  assert(std::holds_alternative<kyna::VarDecl>(program[1]->node));

  auto quotedFields =
      kyna::Parser(kyna::lex("set headers = { \"Content-Type\": \"application/json\", "
                            "\"X-API-Key\": \"test\" };"))
          .parse();
  assert(quotedFields.size() == 1);
  const auto &headersDeclaration = std::get<kyna::VarDecl>(quotedFields.front()->node);
  const auto &headers = std::get<kyna::ObjectExpr>(headersDeclaration.initializer->node);
  assert(headers.fields.size() == 2);
  assert(headers.fields[0].name == "Content-Type");
  assert(headers.fields[1].name == "X-API-Key");

  auto exceptions = kyna::Parser(kyna::lex(
      "try { throw \"failure\"; } catch (failure) { print(failure.message); } "
      "finally { print(\"cleanup\"); } try { print(\"work\"); } finally { print(\"done\"); }"))
                        .parse();
  assert(exceptions.size() == 2);
  const auto &caught = std::get<kyna::TryStmt>(exceptions[0]->node);
  assert(caught.catchBranch);
  assert(caught.finallyBranch);
  const auto &catchBody = std::get<kyna::BlockStmt>(caught.catchBranch->node);
  assert(catchBody.statements.size() == 1);
  const auto &tryBody = std::get<kyna::BlockStmt>(caught.tryBranch->node);
  assert(std::holds_alternative<kyna::ThrowStmt>(tryBody.statements.front()->node));
  const auto &finallyOnly = std::get<kyna::TryStmt>(exceptions[1]->node);
  assert(!finallyOnly.catchBranch);
  assert(finallyOnly.finallyBranch);
}
