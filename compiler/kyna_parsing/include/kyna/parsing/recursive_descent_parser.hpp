#pragma once
#include "kyna/syntax/legacy_syntax_handles.hpp"
#include "kyna/diagnostics.hpp"
#include "kyna/lexing/legacy_lexer.hpp"
#include "kyna/parsing/module_parser.hpp"
#include <vector>
namespace kyna {
class Parser {
public:
  explicit Parser(std::vector<Token> tokens);
  std::vector<StmtPtr> parse();
  ParseResult parseRecovering(const SourceFile &source);

private:
  std::vector<Token> tokens;
  size_t current{0};
  std::vector<Diagnostic> diagnostics;
  bool incomplete{false};
  bool seenNonImport{false};
  const Token &peek() const;
  const Token &previous() const;
  bool check(TokenKind) const;
  bool match(TokenKind);
  const Token &consume(TokenKind, const std::string &);
  StmtPtr declaration();
  StmtPtr importDeclaration();
  StmtPtr exportListDeclaration();
  StmtPtr defaultExportDeclaration();
  StmtPtr statement();
  StmtPtr block();
  StmtPtr varDeclaration();
  StmtPtr functionDeclaration(std::vector<std::string> modifiers);
  StmtPtr classDeclaration(std::vector<std::string> modifiers);
  StmtPtr interfaceDeclaration();
  ExprPtr expression();
  ExprPtr assignment();
  ExprPtr logicOr();
  ExprPtr logicAnd();
  ExprPtr equality();
  ExprPtr comparison();
  ExprPtr term();
  ExprPtr factor();
  ExprPtr unary();
  ExprPtr call();
  ExprPtr primary();
  TypeRef typeRef();
  std::vector<std::string> modifiers();
  std::vector<StmtPtr> parseStatementsUntil(TokenKind end, ExprPtr *tail = nullptr);
  ExprPtr make(Expr::Node node, SourceLocation l);
  StmtPtr make(Stmt::Node node, SourceLocation l);
  void synchronize();
  void markExported(const StmtPtr &declaration);
};
} // namespace kyna
