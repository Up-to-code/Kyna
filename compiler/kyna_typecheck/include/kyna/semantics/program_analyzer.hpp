#pragma once
#include "kyna/syntax/legacy_syntax_handles.hpp"
#include "kyna/diagnostics.hpp"
#include "kyna/semantics/interface_catalog.hpp"
#include "kyna/semantics/scope.hpp"
#include "kyna/semantics/environment.hpp"
#include "kyna/semantics/symbol.hpp"
#include "kyna/types/type_bridge.hpp"
#include <map>
#include <memory>
#include <vector>
namespace kyna {
class Analyzer {
public:
  std::vector<Diagnostic> analyze(const std::vector<StmtPtr> &program);
  void setInteractive(bool enabled) { interactive = enabled; }
  void setExternalBindings(std::map<std::string, TypeRef> bindings) {
    externalBindings = std::move(bindings);
  }
  void setModuleExports(std::map<std::string, std::map<std::string, TypeRef>> exports) {
    moduleExports = std::move(exports);
  }
  void setExternalInterfaces(std::vector<InterfaceDecl> interfaces) {
    externalInterfaces = std::move(interfaces);
  }
  void setExternalClasses(std::vector<ClassDecl> classes) {
    externalClasses = std::move(classes);
  }

private:
  std::vector<Diagnostic> errors;
  struct Scope {
    std::map<std::string, TypeRef> types;
    std::map<std::string, bool> mutableBindings;
    std::shared_ptr<Scope> parent;
  };
  std::shared_ptr<Scope> scope;
  std::map<std::string, FunctionDecl> functions;
  std::map<std::string, ClassDecl> classes;
  std::map<std::string, TypeRef> externalBindings;
  std::map<std::string, std::map<std::string, TypeRef>> moduleExports;
  std::vector<InterfaceDecl> externalInterfaces;
  std::vector<ClassDecl> externalClasses;
  InterfaceCatalog interfaces;
  std::unique_ptr<semantics::Scope> lexicalRoot;
  semantics::Scope *lexical{nullptr};
  semantics::Environment environment;
  std::string currentClass;
  bool interactive{false};
  TypeRef currentReturn{"void", false, {}, {}};
  bool inFunction{false};
  std::vector<std::string> activeLoopLabels;
  int switchDepth{0};
  void stmt(const StmtPtr &);
  void warning(const std::string &, SourceLocation);
  TypeRef expr(const ExprPtr &);
  TypeRef checkUnary(const Unary &, SourceLocation);
  TypeRef checkBinary(const Binary &, SourceLocation);
  TypeRef checkAssign(const Assign &, SourceLocation);
  TypeRef checkCall(const Call &, SourceLocation);
  TypeRef checkMember(const Member &, SourceLocation);
  TypeRef checkIndex(const Index &, SourceLocation);
  TypeRef checkNew(const NewExpr &, SourceLocation);
  TypeRef checkIfExpr(const IfExpr &, SourceLocation);
  TypeRef checkMatch(const MatchExpr &, SourceLocation);
  void checkVarDecl(const VarDecl &, SourceLocation);
  void checkFunctionDecl(const FunctionDecl &, SourceLocation);
  void checkClassDecl(const ClassDecl &, SourceLocation);
  void checkBlock(const BlockStmt &, SourceLocation);
  void checkIf(const IfStmt &);
  void checkWhile(const WhileStmt &);
  void checkLoop(const LoopStmt &, SourceLocation);
  void checkSwitch(const SwitchStmt &, SourceLocation);
  void checkTry(const TryStmt &);
  void checkBreak(const BreakStmt &, SourceLocation);
  void checkContinue(const ContinueStmt &, SourceLocation);
  void checkReturn(const ReturnStmt &);
  TypeRef merge(const TypeRef &, const TypeRef &);
  bool compatible(const TypeRef &, const TypeRef &);
  bool defined(const std::string &) const;
  void error(const std::string &, SourceLocation, std::string code = "K0000",
             std::string help = {});
  Scope *bindingScope(const std::string &) const;
  void bindLexical(const std::string &name, const TypeRef &type, bool mutableBinding,
                   SourceLocation location, bool exported);
  bool alwaysReturns(const StmtPtr &) const;
  const FieldDecl *findField(const ClassDecl &, const std::string &) const;
  const FunctionDecl *findMethod(const ClassDecl &, const std::string &) const;
  bool classConforms(const ClassDecl &, const InterfaceDecl &, const TypeRef &contractRef,
                     SourceLocation);
  bool objectConforms(const ObjectExpr &, const InterfaceDecl &, SourceLocation);
  InterfaceDecl effectiveContract(const InterfaceDecl &, std::vector<std::string> &stack) const;
  TypeRef substitute(const TypeRef &, const InterfaceDecl &, const TypeRef &contractRef) const;
};
} // namespace kyna
