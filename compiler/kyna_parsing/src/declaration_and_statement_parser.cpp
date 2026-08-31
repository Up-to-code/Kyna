#include "kyna/parsing/recursive_descent_parser.hpp"
#include <algorithm>
#include <sstream>
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
    case TokenKind::Let:
    case TokenKind::Set:
    case TokenKind::Func:
    case TokenKind::Class:
    case TokenKind::Intf:
    case TokenKind::If:
    case TokenKind::While:
    case TokenKind::Loop:
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
StmtPtr Parser::declaration() {
  const bool exported = match(TokenKind::Export);
  if (match(TokenKind::Import)) {
    if (exported)
      throw KynaError({"an import cannot be exported", previous().location, false, "K2010"});
    if (seenNonImport)
      throw KynaError(
          {"imports must precede other declarations", previous().location, false, "K2011"});
    --current;
    return importDeclaration();
  }
  seenNonImport = true;
  if (exported) {
    // export { a, b };  re-export list
    if (check(TokenKind::LeftBrace))
      return exportListDeclaration();
    // export default <declaration>;
    const bool defaultExport = match(TokenKind::Default);
    if (defaultExport)
      return defaultExportDeclaration();
  }
  auto mods = modifiers();
  StmtPtr parsed;
  if (match(TokenKind::Func))
    parsed = functionDeclaration(std::move(mods));
  else if (match(TokenKind::Class))
    parsed = classDeclaration(std::move(mods));
  else if (match(TokenKind::Intf))
    parsed = interfaceDeclaration();
  else if (match(TokenKind::Let)) {
    --current;
    parsed = varDeclaration();
  } else if (match(TokenKind::Set)) {
    --current;
    parsed = varDeclaration();
  }
  if (parsed) {
    if (exported)
      markExported(parsed);
    return parsed;
  }
  if (exported)
    throw KynaError(
        {"export must precede a named declaration", previous().location, false, "K2012"});
  if (!mods.empty())
    throw KynaError(
        {"member modifier is only valid on a function or class", previous().location, false});
  return statement();
}
StmtPtr Parser::importDeclaration() {
  const Token start = consume(TokenKind::Import, "expected 'import'");
  // Legacy form: import "path" as alias;
  if (check(TokenKind::String)) {
    const Token path = consume(TokenKind::String, "expected a quoted module path");
    consume(TokenKind::As, "expected 'as' after module path");
    const Token alias = consume(TokenKind::Identifier, "expected module alias");
    consume(TokenKind::Semicolon, "expected ';' after import");
    auto value =
        path.lexeme.size() >= 2 ? path.lexeme.substr(1, path.lexeme.size() - 2) : path.lexeme;
    return make(ImportDecl{std::move(value), alias.lexeme}, start.location);
  }
  // JavaScript-style import clause.
  ImportDecl declaration;
  if (check(TokenKind::LeftBrace)) {
    ++current; // '{'
    while (!check(TokenKind::RightBrace)) {
      const Token imported = consume(TokenKind::Identifier, "expected an import name");
      std::string local = imported.lexeme;
      if (match(TokenKind::As)) {
        const Token l = consume(TokenKind::Identifier, "expected an import alias");
        local = l.lexeme;
      }
      declaration.named.push_back({imported.lexeme, local});
      if (!match(TokenKind::Comma))
        break;
    }
    consume(TokenKind::RightBrace, "expected '}' after named imports");
  } else if (match(TokenKind::Star)) {
    consume(TokenKind::As, "expected 'as' after '*'");
    declaration.namespaceAlias =
        consume(TokenKind::Identifier, "expected a namespace import alias").lexeme;
  } else {
    declaration.defaultName =
        consume(TokenKind::Identifier, "expected a default import name").lexeme;
    // import Name, { a, b } from "...";
    if (match(TokenKind::Comma)) {
      consume(TokenKind::LeftBrace, "expected '{' after ','");
      while (!check(TokenKind::RightBrace)) {
        const Token imported = consume(TokenKind::Identifier, "expected an import name");
        std::string local = imported.lexeme;
        if (match(TokenKind::As)) {
          const Token l = consume(TokenKind::Identifier, "expected an import alias");
          local = l.lexeme;
        }
        declaration.named.push_back({imported.lexeme, local});
        if (!match(TokenKind::Comma))
          break;
      }
      consume(TokenKind::RightBrace, "expected '}' after named imports");
    }
  }
  consume(TokenKind::From, "expected 'from' before module path");
  const Token pathTok = consume(TokenKind::String, "expected a quoted module path");
  consume(TokenKind::Semicolon, "expected ';' after import");
  declaration.path =
      pathTok.lexeme.size() >= 2 ? pathTok.lexeme.substr(1, pathTok.lexeme.size() - 2)
                                 : pathTok.lexeme;
  // If only a named/namespace import is present, the loader aliases the whole
  // module by the first exported symbol to keep module-resolution simple.
  if (!declaration.named.empty())
    declaration.alias = declaration.named.front().local;
  else if (!declaration.namespaceAlias.empty())
    declaration.alias = declaration.namespaceAlias;
  else
    declaration.alias = declaration.defaultName;
  return make(std::move(declaration), start.location);
}
StmtPtr Parser::exportListDeclaration() {
  const Token start = consume(TokenKind::LeftBrace, "expected '{' after 'export'");
  ExportDecl declaration;
  while (!check(TokenKind::RightBrace)) {
    declaration.names.push_back(
        consume(TokenKind::Identifier, "expected an export name").lexeme);
    if (!match(TokenKind::Comma))
      break;
  }
  consume(TokenKind::RightBrace, "expected '}' after export list");
  consume(TokenKind::Semicolon, "expected ';' after export list");
  return make(std::move(declaration), start.location);
}
StmtPtr Parser::defaultExportDeclaration() {
  auto mods = modifiers();
  StmtPtr parsed;
  if (match(TokenKind::Func))
    parsed = functionDeclaration(std::move(mods));
  else if (match(TokenKind::Class))
    parsed = classDeclaration(std::move(mods));
  else if (match(TokenKind::Intf))
    parsed = interfaceDeclaration();
  else if (match(TokenKind::Let)) {
    --current;
    parsed = varDeclaration();
  } else if (match(TokenKind::Set)) {
    --current;
    parsed = varDeclaration();
  }
  if (!parsed)
    throw KynaError(
        {"'export default' must precede a named declaration", previous().location, false,
         "K2013"});
  markExported(parsed);
  std::visit(
      [](auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                      std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
          node.isDefault = true;
      },
      parsed->node);
  return parsed;
}
void Parser::markExported(const StmtPtr &parsed) {
  std::visit(
      [](auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarDecl> || std::is_same_v<T, FunctionDecl> ||
                      std::is_same_v<T, ClassDecl> || std::is_same_v<T, InterfaceDecl>)
          node.exported = true;
      },
      parsed->node);
}
TypeRef Parser::typeRef() {
  Token t = peek();
  if (!(check(TokenKind::Identifier) || check(TokenKind::IntType) || check(TokenKind::FloatType) ||
        check(TokenKind::NumType) || check(TokenKind::StrType) || check(TokenKind::CharType) ||
        check(TokenKind::BoolType) || check(TokenKind::Null) || check(TokenKind::VoidType) ||
        check(TokenKind::AnyType)))
    throw KynaError({"expected a type", peek().location, false});
  ++current;
  TypeRef r{t.lexeme, false, {}};
  // Generic instantiation: Name<T, U, ...>. Also supports nested types.
  while (match(TokenKind::Less)) {
    TypeRef arg;
    do {
      arg.typeArgs.push_back(typeRef());
    } while (match(TokenKind::Comma));
    consume(TokenKind::Greater, "expected '>' after type arguments");
    TypeRef specialized;
    specialized.name = r.name;
    specialized.typeArgs = std::move(arg.typeArgs);
    specialized.nullable = r.nullable;
    r = std::move(specialized);
  }
  if (match(TokenKind::Question))
    r.nullable = true;
  while (match(TokenKind::Pipe)) {
    TypeRef other = typeRef();
    r.unionTypes.push_back(std::move(other));
  }
  return r;
}
StmtPtr Parser::varDeclaration() {
  bool mut;
  Token t = peek();
  if (match(TokenKind::Let))
    mut = true;
  else {
    consume(TokenKind::Set, "expected 'let' or 'set'");
    mut = false;
  }
  Token name = consume(TokenKind::Identifier, "expected binding name");
  VarDecl d{mut, name.lexeme, {"void", false, {}}, false, nullptr};
  if (match(TokenKind::Colon)) {
    d.type = typeRef();
    d.hasType = true;
  }
  if (match(TokenKind::Equal))
    d.initializer = expression();
  else if (!d.hasType || d.type.name != "any")
    throw KynaError({"a non-any binding requires an initializer", name.location, false});
  consume(TokenKind::Semicolon, "expected ';' after declaration");
  return make(std::move(d), t.location);
}
StmtPtr Parser::functionDeclaration(std::vector<std::string> mods) {
  Token name = consume(TokenKind::Identifier, "expected function name");
  consume(TokenKind::LeftParen, "expected '(' after function name");
  std::vector<Param> params;
  if (!check(TokenKind::RightParen)) {
    do {
      Token p = consume(TokenKind::Identifier, "expected parameter name");
      consume(TokenKind::Colon, "function parameters require an explicit type");
      params.push_back({p.lexeme, typeRef()});
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::RightParen, "expected ')' after parameters");
  TypeRef ret{"void", false, {}};
  bool has = false;
  if (match(TokenKind::Colon)) {
    ret = typeRef();
    has = true;
  }
  auto body = block();
  return make(FunctionDecl{name.lexeme, std::move(params), std::move(ret), has, std::move(body),
                           std::move(mods)},
              name.location);
}
StmtPtr Parser::classDeclaration(std::vector<std::string> mods) {
  Token name = consume(TokenKind::Identifier, "expected class name");
  std::string parent;
  if (match(TokenKind::Extends))
    parent = consume(TokenKind::Identifier, "expected parent class name").lexeme;
  std::vector<TypeRef> interfaces;
  if (match(TokenKind::Implements)) {
    do {
      interfaces.push_back(typeRef());
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::LeftBrace, "expected '{' after class header");
  ClassDecl c{name.lexeme, parent, {}, {}, std::move(mods), std::move(interfaces)};
  while (!check(TokenKind::RightBrace) && !check(TokenKind::End)) {
    auto mm = modifiers();
    if (match(TokenKind::Func) || match(TokenKind::Init)) {
      bool isInit = previous().kind == TokenKind::Init;
      Token n = isInit ? Token{TokenKind::Identifier, "init", previous().location}
                       : consume(TokenKind::Identifier, "expected method name");
      consume(TokenKind::LeftParen, "expected '(' after method name");
      std::vector<Param> ps;
      if (!check(TokenKind::RightParen)) {
        do {
          Token p = consume(TokenKind::Identifier, "expected parameter name");
          consume(TokenKind::Colon, "function parameters require an explicit type");
          ps.push_back({p.lexeme, typeRef()});
        } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RightParen, "expected ')' after parameters");
      TypeRef rt{"void", false, {}};
      bool has = false;
      if (match(TokenKind::Colon)) {
        rt = typeRef();
        has = true;
      }
      StmtPtr body;
      if (std::find(mm.begin(), mm.end(), "abstract") != mm.end())
        consume(TokenKind::Semicolon, "expected ';' after abstract method signature");
      else
        body = block();
      c.methods.push_back(
          {n.lexeme, std::move(ps), std::move(rt), has, std::move(body), std::move(mm)});
      continue;
    }
    Token f = consume(TokenKind::Identifier, "expected field name or method");
    consume(TokenKind::Colon, "expected ':' after field name");
    auto ty = typeRef();
    ExprPtr init;
    if (match(TokenKind::Equal))
      init = expression();
    consume(TokenKind::Semicolon, "expected ';' after field");
    c.fields.push_back({f.lexeme, std::move(ty), std::move(init), std::move(mm)});
  }
  consume(TokenKind::RightBrace, "expected '}' after class");
  return make(std::move(c), name.location);
}
StmtPtr Parser::interfaceDeclaration() {
  Token n = consume(TokenKind::Identifier, "expected interface name");
  InterfaceDecl i;
  i.name = n.lexeme;
  // Optional type parameters: intf Box<T, U> ...
  if (match(TokenKind::Less)) {
    do {
      i.typeParams.push_back(
          consume(TokenKind::Identifier, "expected a type parameter name").lexeme);
    } while (match(TokenKind::Comma));
    consume(TokenKind::Greater, "expected '>' after type parameters");
  }
  // Optional parent interfaces: intf B extends A, C<T> { ... }
  if (match(TokenKind::Extends)) {
    do {
      i.parents.push_back(typeRef());
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::LeftBrace, "expected '{' after interface name");
  while (!check(TokenKind::RightBrace) && !check(TokenKind::End)) {
    // Index signature: [keyName: KeyType]: ValueType;
    if (check(TokenKind::LeftBracket)) {
      ++current; // '['
      const Token keyName = consume(TokenKind::Identifier, "expected an index key name");
      consume(TokenKind::Colon, "expected ':' after index key name");
      auto keyType = typeRef();
      consume(TokenKind::RightBracket, "expected ']' after index key type");
      consume(TokenKind::Colon, "expected ':' before index value type");
      auto valueType = typeRef();
      consume(TokenKind::Semicolon, "expected ';' after index signature");
      i.indexSignatures.push_back(
          IndexSignature{keyName.lexeme, std::move(keyType), std::move(valueType)});
      continue;
    }
    // Call signature: (a: A, b: B): R;
    if (check(TokenKind::LeftParen)) {
      ++current; // '('
      CallSignature signature;
      if (!check(TokenKind::RightParen)) {
        do {
          Token p = consume(TokenKind::Identifier, "expected parameter");
          consume(TokenKind::Colon, "parameters require types");
          signature.params.push_back({p.lexeme, typeRef()});
        } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RightParen, "expected ')'");
      consume(TokenKind::Colon, "interface call signatures require return types");
      signature.returnType = typeRef();
      consume(TokenKind::Semicolon, "expected ';'");
      i.callSignatures.push_back(std::move(signature));
      continue;
    }
    // Method with explicit 'func' prefix: func get(): str;
    if (match(TokenKind::Func)) {
      Token f = consume(TokenKind::Identifier, "expected method name after 'func'");
      consume(TokenKind::LeftParen, "expected '(' after method name");
      std::vector<Param> ps;
      if (!check(TokenKind::RightParen)) {
        do {
          Token p = consume(TokenKind::Identifier, "expected parameter");
          consume(TokenKind::Colon, "parameters require types");
          ps.push_back({p.lexeme, typeRef()});
        } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RightParen, "expected ')'");
      consume(TokenKind::Colon, "interface methods require return types");
      auto rt = typeRef();
      consume(TokenKind::Semicolon, "expected ';' after method");
      i.methods.push_back({f.lexeme, std::move(ps), std::move(rt), true, nullptr, {}});
      continue;
    }
    // Field or method.
    Token x = consume(TokenKind::Identifier, "expected interface member");
    if (match(TokenKind::Colon)) {
      auto ty = typeRef();
      i.fields.push_back({x.lexeme, std::move(ty), nullptr, {}});
      consume(TokenKind::Semicolon, "expected ';' after field");
    } else if (match(TokenKind::Question)) {
      // Optional property: name?: Type;
      consume(TokenKind::Colon, "expected ':' after optional property name");
      auto ty = typeRef();
      i.fields.push_back({x.lexeme, std::move(ty), nullptr, {}});
      i.optionalFields.insert(x.lexeme);
      consume(TokenKind::Semicolon, "expected ';' after optional property");
    } else {
      consume(TokenKind::LeftParen, "expected '(' after method name");
      std::vector<Param> ps;
      if (!check(TokenKind::RightParen)) {
        do {
          Token p = consume(TokenKind::Identifier, "expected parameter");
          consume(TokenKind::Colon, "parameters require types");
          ps.push_back({p.lexeme, typeRef()});
        } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RightParen, "expected ')'");
      consume(TokenKind::Colon, "interface methods require return types");
      auto rt = typeRef();
      consume(TokenKind::Semicolon, "expected ';' after method");
      i.methods.push_back({x.lexeme, std::move(ps), std::move(rt), true, nullptr, {}});
    }
  }
  consume(TokenKind::RightBrace, "expected '}' after interface");
  return make(std::move(i), n.location);
}

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
