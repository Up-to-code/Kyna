#pragma once
#include "kyna/syntax/legacy_syntax_handles.hpp"
#include "kyna/diagnostics.hpp"
#include "kyna/semantics/interface_catalog.hpp"
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
  std::string currentClass;
  bool interactive{false};
  TypeRef currentReturn{"void", false, {}};
  bool inFunction{false};
  std::vector<std::string> activeLoopLabels;
  int switchDepth{0};
  void stmt(const StmtPtr &);
  void warning(const std::string &, SourceLocation);
  TypeRef expr(const ExprPtr &);
  TypeRef merge(const TypeRef &, const TypeRef &);
  bool compatible(const TypeRef &, const TypeRef &);
  bool defined(const std::string &) const;
  void error(const std::string &, SourceLocation, std::string code = "K0000",
             std::string help = {});
  Scope *bindingScope(const std::string &) const;
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
