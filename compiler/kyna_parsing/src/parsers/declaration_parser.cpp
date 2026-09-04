#include "kyna/parsing/recursive_descent_parser.hpp"
#include <algorithm>
#include <type_traits>

namespace kyna {
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
  if (match(TokenKind::Fn))
    parsed = functionDeclaration(std::move(mods));
  else if (match(TokenKind::Class))
    parsed = classDeclaration(std::move(mods));
  else if (match(TokenKind::Intf))
    parsed = interfaceDeclaration();
  else if (match(TokenKind::Var) || match(TokenKind::Const)) {
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
  if (match(TokenKind::Fn))
    parsed = functionDeclaration(std::move(mods));
  else if (match(TokenKind::Class))
    parsed = classDeclaration(std::move(mods));
  else if (match(TokenKind::Intf))
    parsed = interfaceDeclaration();
  else if (match(TokenKind::Var) || match(TokenKind::Const)) {
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
StmtPtr Parser::varDeclaration() {
  bool mut;
  Token t = peek();
  if (match(TokenKind::Var))
    mut = true;
  else {
    consume(TokenKind::Const, "expected 'var' or 'const'");
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
    if (match(TokenKind::Fn) || match(TokenKind::Init)) {
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
    // Method with explicit 'fn' prefix: fn get(): str;
    if (match(TokenKind::Fn)) {
      Token f = consume(TokenKind::Identifier, "expected method name after 'fn'");
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


} // namespace kyna
