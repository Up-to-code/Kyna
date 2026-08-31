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

  // Generic interface with extends, optional fields, call and index signatures.
  auto intf = kyna::Parser(kyna::lex(
                            "intf Named<T> extends Base { "
                            "name?: str; value: T; (a: int): str; [key: str]: int; "
                            "}"))
                  .parse();
  assert(intf.size() == 1);
  const auto &typeDef = std::get<kyna::InterfaceDecl>(intf.front()->node);
  assert(typeDef.typeParams.size() == 1 && typeDef.typeParams[0] == "T");
  assert(typeDef.parents.size() == 1 && typeDef.parents[0].name == "Base");
  assert(typeDef.optionalFields.count("name") == 1);
  assert(typeDef.callSignatures.size() == 1);
  assert(typeDef.callSignatures[0].params.size() == 1);
  assert(typeDef.callSignatures[0].params[0].name == "a");
  assert(typeDef.indexSignatures.size() == 1);
  assert(typeDef.indexSignatures[0].keyName == "key");
  assert(typeDef.indexSignatures[0].valueType.name == "int");

  // JavaScript-style imports bind names, default, and namespace aliases.
  auto jsImports = kyna::Parser(kyna::lex(
                            "import { a as x, b } from \"./m.kyna\"; "
                            "import d from \"./m.kyna\"; "
                            "import * as ns from \"./m.kyna\";"))
                       .parse();
  assert(jsImports.size() == 3);
  const auto &namedImport = std::get<kyna::ImportDecl>(jsImports[0]->node);
  assert(namedImport.named.size() == 2);
  assert(namedImport.named[0].imported == "a" && namedImport.named[0].local == "x");
  assert(namedImport.named[1].imported == "b" && namedImport.named[1].local == "b");
  assert(namedImport.defaultName.empty() && namedImport.namespaceAlias.empty());
  const auto &defaultImport = std::get<kyna::ImportDecl>(jsImports[1]->node);
  assert(defaultImport.defaultName == "d");
  const auto &nsImport = std::get<kyna::ImportDecl>(jsImports[2]->node);
  assert(nsImport.namespaceAlias == "ns");
}
