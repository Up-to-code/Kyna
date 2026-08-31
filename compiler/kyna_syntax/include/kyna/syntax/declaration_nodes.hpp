#pragma once

#include "kyna/semantics/type_model.hpp"
#include "kyna/syntax/statement_nodes.hpp"
#include <set>

namespace kyna {

struct ImportSpecifier {
  std::string imported;
  std::string local;
};
struct Param {
  std::string name;
  TypeRef type;
};
struct CallSignature {
  std::vector<Param> params;
  TypeRef returnType;
};
struct IndexSignature {
  std::string keyName;
  TypeRef keyType;
  TypeRef valueType;
};
struct VarDecl {
  bool mutableBinding;
  std::string name;
  TypeRef type;
  bool hasType{false};
  ExprPtr initializer;
  bool exported{false};
  bool isDefault{false};
};
struct FunctionDecl {
  std::string name;
  std::vector<Param> params;
  TypeRef returnType;
  bool hasReturnType{false};
  StmtPtr body;
  std::vector<std::string> modifiers;
  bool exported{false};
  bool isDefault{false};
};
struct FieldDecl {
  std::string name;
  TypeRef type;
  ExprPtr initializer;
  std::vector<std::string> modifiers;
};
struct ClassDecl {
  std::string name;
  std::string parent;
  std::vector<FieldDecl> fields;
  std::vector<FunctionDecl> methods;
  std::vector<std::string> modifiers;
  std::vector<TypeRef> interfaces;
  bool exported{false};
  bool isDefault{false};
};
struct InterfaceDecl {
  std::string name;
  std::vector<std::string> typeParams;
  std::vector<TypeRef> parents;
  std::vector<FieldDecl> fields;
  std::vector<FunctionDecl> methods;
  std::vector<CallSignature> callSignatures;
  std::vector<IndexSignature> indexSignatures;
  std::set<std::string> optionalFields;
  bool exported{false};
  bool isDefault{false};
};
struct ImportDecl {
  std::string path;
  std::string alias;
  std::string defaultName;
  std::vector<ImportSpecifier> named;
  std::string namespaceAlias;
};
struct ExportDecl {
  std::vector<std::string> names;
};

struct Stmt {
  using Node = std::variant<VarDecl, ExprStmt, BlockStmt, IfStmt, WhileStmt, LoopStmt, BreakStmt,
                            ContinueStmt, ReturnStmt, ThrowStmt, TryStmt, FunctionDecl, ClassDecl,
                            InterfaceDecl, ImportDecl, ExportDecl, InvalidStmt>;
  Node node;
  SourceSpan location;
};

} // namespace kyna
