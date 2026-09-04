#include "kyna/parsing/recursive_descent_parser.hpp"
#include <type_traits>

namespace kyna {
Parser::Parser(std::vector<Token> t) : tokens(std::move(t)) {}
const Token &Parser::peek() const { return tokens[current]; }
const Token &Parser::previous() const { return tokens[current - 1]; }
bool Parser::check(TokenKind k) const { return peek().kind == k; }
bool Parser::match(TokenKind k) {
  if (!check(k))
    return false;
  ++current;
  return true;
}
const Token &Parser::consume(TokenKind k, const std::string &msg) {
  if (check(k))
    return tokens[current++];
  Diagnostic diagnostic{msg + ", got " + tokenName(peek().kind), peek().location, false};
  diagnostic.code = "K2000";
  if (peek().kind == TokenKind::End)
    incomplete = true;
  throw KynaError(diagnostic);
}
ExprPtr Parser::make(Expr::Node n, SourceLocation l) {
  return std::make_shared<Expr>(Expr{std::move(n), l});
}
StmtPtr Parser::make(Stmt::Node n, SourceLocation l) {
  return std::make_shared<Stmt>(Stmt{std::move(n), l});
}

std::vector<StmtPtr> Parser::parse() {
  auto result = parseRecovering(SourceFile{});
  if (!result.diagnostics.empty())
    throw KynaError(result.diagnostics.front());
  return std::move(result.tree.module.declarations);
}
ParseResult Parser::parseRecovering(const SourceFile &source) {
  ParsedModule module{source.id, source.path, {}, {}};
  diagnostics.clear();
  incomplete = false;
  seenNonImport = false;
  while (!check(TokenKind::End)) {
    const auto before = current;
    try {
      auto parsed = declaration();
      if (parsed) {
        std::visit(
            [&](const auto &node) {
              using T = std::decay_t<decltype(node)>;
              if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                            std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>) {
                if (node.exported)
                  module.exports.insert(node.name);
              }
            },
            parsed->node);
        module.declarations.push_back(std::move(parsed));
      }
    } catch (const KynaError &error) {
      diagnostics.push_back(error.diagnostic);
      synchronize();
    }
    if (current == before && !check(TokenKind::End))
      ++current;
  }
  return {SyntaxTree{std::move(module)}, std::move(diagnostics), incomplete};
}
void Parser::synchronize() {
  while (!check(TokenKind::End)) {
    if (current > 0 && previous().kind == TokenKind::Semicolon)
      return;
    switch (peek().kind) {
    case TokenKind::Import:
    case TokenKind::Export:
    case TokenKind::Var:
    case TokenKind::Const:
    case TokenKind::Fn:
    case TokenKind::Class:
    case TokenKind::Intf:
    case TokenKind::If:
    case TokenKind::While:
    case TokenKind::Loop:
    case TokenKind::Switch:
    case TokenKind::Return:
    case TokenKind::Throw:
    case TokenKind::Try:
      return;
    case TokenKind::RightBrace:
      ++current;
      return;
    default:
      ++current;
      break;
    }
  }
}
std::vector<std::string> Parser::modifiers() {
  std::vector<std::string> r;
  while (check(TokenKind::Public) || check(TokenKind::Private) || check(TokenKind::Protected) ||
         check(TokenKind::Static) || check(TokenKind::Override) || check(TokenKind::Final) ||
         check(TokenKind::Abstract)) {
    r.push_back(peek().lexeme);
    ++current;
  }
  return r;
}

} // namespace kyna
