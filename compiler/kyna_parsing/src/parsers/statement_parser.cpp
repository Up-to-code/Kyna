#include "kyna/parsing/recursive_descent_parser.hpp"

namespace kyna {
StmtPtr Parser::block() {
  Token t = consume(TokenKind::LeftBrace, "expected '{'");
  BlockStmt b;
  while (!check(TokenKind::RightBrace) && !check(TokenKind::End)) {
    if (check(TokenKind::Let) || check(TokenKind::Set))
      b.statements.push_back(varDeclaration());
    else if (check(TokenKind::Func)) {
      ++current;
      b.statements.push_back(functionDeclaration({}));
    } else if (check(TokenKind::Class)) {
      ++current;
      b.statements.push_back(classDeclaration({}));
    } else if (check(TokenKind::If) || check(TokenKind::While) || check(TokenKind::Loop) ||
               check(TokenKind::Break) || check(TokenKind::Continue) || check(TokenKind::Return) ||
               check(TokenKind::Throw) || check(TokenKind::Try) || check(TokenKind::LeftBrace))
      b.statements.push_back(statement());
    else {
      auto e = expression();
      if (match(TokenKind::Semicolon))
        b.statements.push_back(make(ExprStmt{e}, e->location));
      else {
        if (!check(TokenKind::RightBrace))
          throw KynaError({"expected ';' after expression", peek().location, false});
        b.tail = e;
      }
    }
  }
  consume(TokenKind::RightBrace, "expected '}' after block");
  return make(std::move(b), t.location);
}
StmtPtr Parser::statement() {
  if (match(TokenKind::LeftBrace)) {
    --current;
    return block();
  }
  if (match(TokenKind::If)) {
    Token t = previous();
    consume(TokenKind::LeftParen, "expected '(' after if");
    auto c = expression();
    consume(TokenKind::RightParen, "expected ')' after condition");
    auto yes = block();
    StmtPtr no;
    if (match(TokenKind::Else))
      no = check(TokenKind::If) ? statement() : block();
    return make(IfStmt{c, yes, no}, t.location);
  }
  if (match(TokenKind::While)) {
    Token t = previous();
    consume(TokenKind::LeftParen, "expected '(' after while");
    auto c = expression();
    consume(TokenKind::RightParen, "expected ')' after condition");
    return make(WhileStmt{c, block(), ""}, t.location);
  }
  if (check(TokenKind::Identifier) && current + 1 < tokens.size() &&
      tokens[current + 1].kind == TokenKind::Colon) {
    std::string label = peek().lexeme;
    ++current;
    ++current;
    if (check(TokenKind::Loop)) {
      Token t = peek();
      ++current;
      StmtPtr init;
      ExprPtr cond, inc;
      if (match(TokenKind::LeftParen)) {
        if (!check(TokenKind::Semicolon)) {
          if (check(TokenKind::Let) || check(TokenKind::Set))
            init = varDeclaration();
          else {
            auto e = expression();
            consume(TokenKind::Semicolon, "expected ';' in loop");
            init = make(ExprStmt{e}, e->location);
          }
        } else
          ++current;
        if (!check(TokenKind::Semicolon))
          cond = expression();
        consume(TokenKind::Semicolon, "expected ';' in loop");
        if (!check(TokenKind::RightParen))
          inc = expression();
        consume(TokenKind::RightParen, "expected ')' after loop clauses");
      }
      return make(LoopStmt{init, cond, inc, block(), label}, t.location);
    }
    throw KynaError({"label must precede a loop", peek().location, false});
  }
  if (match(TokenKind::Loop)) {
    Token t = previous();
    StmtPtr init;
    ExprPtr cond, inc;
    if (match(TokenKind::LeftParen)) {
      if (!check(TokenKind::Semicolon)) {
        if (check(TokenKind::Let) || check(TokenKind::Set))
          init = varDeclaration();
        else {
          auto e = expression();
          consume(TokenKind::Semicolon, "expected ';' in loop");
          init = make(ExprStmt{e}, e->location);
        }
      } else
        ++current;
      if (!check(TokenKind::Semicolon))
        cond = expression();
      consume(TokenKind::Semicolon, "expected ';' in loop");
      if (!check(TokenKind::RightParen))
        inc = expression();
      consume(TokenKind::RightParen, "expected ')' after loop clauses");
    }
    return make(LoopStmt{init, cond, inc, block(), ""}, t.location);
  }
  if (match(TokenKind::Break)) {
    Token t = previous();
    std::string l;
    if (check(TokenKind::Identifier))
      l = peek().lexeme, ++current;
    consume(TokenKind::Semicolon, "expected ';' after break");
    return make(BreakStmt{l}, t.location);
  }
  if (match(TokenKind::Continue)) {
    Token t = previous();
    std::string l;
    if (check(TokenKind::Identifier))
      l = peek().lexeme, ++current;
    consume(TokenKind::Semicolon, "expected ';' after continue");
    return make(ContinueStmt{l}, t.location);
  }
  if (match(TokenKind::Try)) {
    Token t = previous();
    auto tryBranch = block();
    std::string catchName;
    StmtPtr catchBranch;
    StmtPtr finallyBranch;
    if (match(TokenKind::Catch)) {
      consume(TokenKind::LeftParen, "expected '(' after catch");
      Token name = consume(TokenKind::Identifier, "expected catch binding name");
      catchName = name.lexeme;
      consume(TokenKind::RightParen, "expected ')' after catch binding");
      catchBranch = block();
    }
    if (match(TokenKind::Finally))
      finallyBranch = block();
    if (!catchBranch && !finallyBranch)
      throw KynaError({"try requires a catch or finally block", t.location, false,
                       "KPAR2401"});
    return make(TryStmt{tryBranch, std::move(catchName), std::move(catchBranch),
                        std::move(finallyBranch)},
                t.location);
  }
  if (match(TokenKind::Throw)) {
    Token t = previous();
    auto value = expression();
    consume(TokenKind::Semicolon, "expected ';' after throw value");
    return make(ThrowStmt{std::move(value)}, t.location);
  }
  if (match(TokenKind::Return)) {
    Token t = previous();
    ExprPtr v;
    if (!check(TokenKind::Semicolon))
      v = expression();
    consume(TokenKind::Semicolon, "expected ';' after return");
    return make(ReturnStmt{v}, t.location);
  }
  auto e = expression();
  consume(TokenKind::Semicolon, "expected ';' after expression");
  return make(ExprStmt{e}, e->location);
}

} // namespace kyna
