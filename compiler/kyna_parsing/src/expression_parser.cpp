#include "kyna/parsing/recursive_descent_parser.hpp"

namespace kyna {
namespace {
std::string decodeQuotedFieldName(const std::string &literal) {
  if (literal.size() < 2)
    return literal;
  std::string value;
  value.reserve(literal.size() - 2);
  for (std::size_t index = 1; index + 1 < literal.size(); ++index) {
    if (literal[index] != '\\' || index + 2 >= literal.size()) {
      value.push_back(literal[index]);
      continue;
    }
    switch (literal[++index]) {
    case 'n': value.push_back('\n'); break;
    case 'r': value.push_back('\r'); break;
    case 't': value.push_back('\t'); break;
    case '\\': value.push_back('\\'); break;
    case '\'': value.push_back('\''); break;
    case '"': value.push_back('"'); break;
    default: value.push_back(literal[index]); break;
    }
  }
  return value;
}
} // namespace

ExprPtr Parser::expression() { return assignment(); }
ExprPtr Parser::assignment() {
  auto e = logicOr();
  if (match(TokenKind::Equal)) {
    Token t = previous();
    auto v = assignment();
    return make(Assign{e, v}, t.location);
  }
  return e;
}
ExprPtr Parser::logicOr() {
  auto e = logicAnd();
  while (match(TokenKind::OrOr)) {
    auto op = previous();
    e = make(Binary{e, op.kind, logicAnd()}, op.location);
  }
  return e;
}
ExprPtr Parser::logicAnd() {
  auto e = equality();
  while (match(TokenKind::AndAnd)) {
    auto op = previous();
    e = make(Binary{e, op.kind, equality()}, op.location);
  }
  return e;
}
ExprPtr Parser::equality() {
  auto e = comparison();
  while (match(TokenKind::EqualEqual) || match(TokenKind::BangEqual)) {
    auto op = previous();
    e = make(Binary{e, op.kind, comparison()}, op.location);
  }
  return e;
}
ExprPtr Parser::comparison() {
  auto e = term();
  while (match(TokenKind::Less) || match(TokenKind::LessEqual) || match(TokenKind::Greater) ||
         match(TokenKind::GreaterEqual)) {
    auto op = previous();
    e = make(Binary{e, op.kind, term()}, op.location);
  }
  return e;
}
ExprPtr Parser::term() {
  auto e = factor();
  while (match(TokenKind::Plus) || match(TokenKind::Minus)) {
    auto op = previous();
    e = make(Binary{e, op.kind, factor()}, op.location);
  }
  return e;
}
ExprPtr Parser::factor() {
  auto e = unary();
  while (match(TokenKind::Star) || match(TokenKind::Slash) || match(TokenKind::Percent)) {
    auto op = previous();
    e = make(Binary{e, op.kind, unary()}, op.location);
  }
  return e;
}
ExprPtr Parser::unary() {
  if (match(TokenKind::Bang) || match(TokenKind::Minus)) {
    auto t = previous();
    return make(Unary{t.kind, unary()}, t.location);
  }
  return call();
}
ExprPtr Parser::call() {
  auto e = primary();
  for (;;) {
    if (match(TokenKind::LeftParen)) {
      std::vector<ExprPtr> a;
      if (!check(TokenKind::RightParen)) {
        do {
          a.push_back(expression());
        } while (match(TokenKind::Comma));
      }
      auto t = consume(TokenKind::RightParen, "expected ')' after arguments");
      e = make(Call{e, std::move(a)}, t.location);
    } else if (match(TokenKind::Dot)) {
      Token n = peek();
      if (check(TokenKind::Identifier) || check(TokenKind::AnyType))
        ++current;
      else
        n = consume(TokenKind::Identifier, "expected member name after '.'");
      e = make(Member{e, n.lexeme}, n.location);
    } else if (match(TokenKind::LeftBracket)) {
      auto index = expression();
      auto t = consume(TokenKind::RightBracket, "expected ']' after index");
      e = make(Index{e, index}, t.location);
    } else
      break;
  }
  return e;
}
ExprPtr Parser::primary() {
  Token t = peek();
  if (match(TokenKind::Int) || match(TokenKind::Float) || match(TokenKind::String) ||
      match(TokenKind::Char))
    return make(Literal{t.kind == TokenKind::Int      ? Literal::Kind::Int
                        : t.kind == TokenKind::Float  ? Literal::Kind::Float
                        : t.kind == TokenKind::String ? Literal::Kind::String
                                                      : Literal::Kind::Char,
                        t.lexeme},
                t.location);
  if (match(TokenKind::True) || match(TokenKind::False))
    return make(Literal{Literal::Kind::Bool, t.lexeme}, t.location);
  if (match(TokenKind::Null))
    return make(Literal{Literal::Kind::Null, t.lexeme}, t.location);
  if (match(TokenKind::Identifier) || match(TokenKind::AnyType))
    return make(Variable{t.lexeme}, t.location);
  if (match(TokenKind::Self))
    return make(SelfExpr{}, t.location);
  if (match(TokenKind::Super))
    return make(SuperExpr{}, t.location);
  if (match(TokenKind::New)) {
    auto n = consume(TokenKind::Identifier, "expected class name after new");
    consume(TokenKind::LeftParen, "expected '(' after class name");
    std::vector<ExprPtr> a;
    if (!check(TokenKind::RightParen)) {
      do {
        a.push_back(expression());
      } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RightParen, "expected ')' after new");
    return make(NewExpr{n.lexeme, std::move(a)}, t.location);
  }
  if (match(TokenKind::LeftParen)) {
    auto e = expression();
    consume(TokenKind::RightParen, "expected ')' after expression");
    return e;
  }
  if (match(TokenKind::LeftBracket)) {
    std::vector<ExprPtr> elements;
    if (!check(TokenKind::RightBracket)) {
      do {
        elements.push_back(expression());
      } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RightBracket, "expected ']' after array");
    return make(ArrayExpr{std::move(elements)}, t.location);
  }
  if (match(TokenKind::LeftBrace)) {
    std::vector<ObjectField> fs;
    if (!check(TokenKind::RightBrace)) {
      do {
        Token n = peek();
        if (check(TokenKind::Identifier) || check(TokenKind::String))
          ++current;
        else
          n = consume(TokenKind::Identifier, "expected object field name");
        consume(TokenKind::Colon, "expected ':' after object field");
        fs.push_back(
            {n.kind == TokenKind::String ? decodeQuotedFieldName(n.lexeme) : n.lexeme,
             expression()});
      } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RightBrace, "expected '}' after object");
    return make(ObjectExpr{std::move(fs)}, t.location);
  }
  if (match(TokenKind::If)) {
    consume(TokenKind::LeftParen, "expected '(' after if");
    auto c = expression();
    consume(TokenKind::RightParen, "expected ')' after condition");
    auto y = block();
    StmtPtr n;
    if (match(TokenKind::Else))
      n = block();
    else
      throw KynaError({"if expression requires else", peek().location, false});
    return make(IfExpr{c, y, n}, t.location);
  }
  if (match(TokenKind::Match)) {
    consume(TokenKind::LeftParen, "expected '(' after match");
    auto s = expression();
    consume(TokenKind::RightParen, "expected ')' after match value");
    consume(TokenKind::LeftBrace, "expected '{' after match");
    std::vector<MatchArm> a;
    while (!check(TokenKind::RightBrace)) {
      bool w = match(TokenKind::Identifier) && previous().lexeme == "_";
      ExprPtr p;
      if (!w) {
        p = expression();
      }
      consume(TokenKind::FatArrow, "expected '=>' in match arm");
      auto v = expression();
      consume(TokenKind::Semicolon, "expected ';' after match arm");
      a.push_back({p, v, w});
    }
    consume(TokenKind::RightBrace, "expected '}' after match");
    return make(MatchExpr{s, std::move(a)}, t.location);
  }
  throw KynaError({"expected expression", t.location, false});
}
} // namespace kyna
