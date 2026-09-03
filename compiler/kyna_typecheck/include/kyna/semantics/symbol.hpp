#pragma once

#include <kyna/types/type.hpp>
#include <kyna/source/source_span.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace kyna::semantics {

// The kind of a declared symbol, used to discriminate the hierarchy without
// RTTI and to drive duplicate-declaration diagnostics.
enum class SymbolKind : uint8_t {
  Variable,
  Constant,
  Function,
  Class,
  Interface,
  Package,
  TypeParam,
};

class Scope;

// A name introduced into a lexical scope. Symbols are polymorphic so that
// variables, functions, classes and interfaces share one lookup surface and a
// scope can record provenance and mutability for each binding.
class Symbol {
public:
  virtual ~Symbol() = default;

  virtual SymbolKind kind() const = 0;
  virtual const std::string &name() const = 0;
  // The static type of the symbol, expressed against the polymorphic type
  // hierarchy.
  virtual types::TypePtr type() const { return nullptr; }
  virtual SourceLocation location() const { return {}; }
  // The scope that owns this symbol (its declarative parent).
  virtual Scope *parentScope() const { return nullptr; }
  virtual bool isExported() const { return false; }
  // Records the owning scope. Invoked by Scope::insert.
  virtual void setParentScope(Scope *parent) { parentScope_ = parent; }

private:
  Scope *parentScope_ = nullptr;
};

using SymbolPtr = std::shared_ptr<Symbol>;

// A mutable or immutable data binding (a `var` or `let`/`const` declaration).
class VarSymbol final : public Symbol {
public:
  VarSymbol(std::string name, types::TypePtr type, bool mutableBinding,
            SourceLocation location = {}, bool exported = false)
      : name_(std::move(name)),
        type_(type),
        mutableBinding_(mutableBinding),
        location_(location),
        exported_(exported) {}

  SymbolKind kind() const override { return SymbolKind::Variable; }
  const std::string &name() const override { return name_; }
  types::TypePtr type() const override { return type_; }
  SourceLocation location() const override { return location_; }
  bool isExported() const override { return exported_; }

  bool isMutable() const { return mutableBinding_; }

private:
  std::string name_;
  types::TypePtr type_;
  bool mutableBinding_;
  SourceLocation location_;
  bool exported_;
};

// A function (or method) declaration carrying its first-class signature.
class FuncSymbol final : public Symbol {
public:
  FuncSymbol(std::string name, types::TypePtr signature, SourceLocation location = {},
             bool exported = false)
      : name_(std::move(name)),
        signature_(signature),
        location_(location),
        exported_(exported) {}

  SymbolKind kind() const override { return SymbolKind::Function; }
  const std::string &name() const override { return name_; }
  types::TypePtr type() const override { return signature_; }
  SourceLocation location() const override { return location_; }
  bool isExported() const override { return exported_; }

private:
  std::string name_;
  types::TypePtr signature_;
  SourceLocation location_;
  bool exported_;
};

// A nominal type (class) declaration.
class ClassSymbol final : public Symbol {
public:
  ClassSymbol(std::string name, SourceLocation location = {}, bool exported = false)
      : name_(std::move(name)),
        location_(location),
        exported_(exported) {}

  SymbolKind kind() const override { return SymbolKind::Class; }
  const std::string &name() const override { return name_; }
  SourceLocation location() const override { return location_; }
  bool isExported() const override { return exported_; }

private:
  std::string name_;
  SourceLocation location_;
  bool exported_;
};

// A callable contract (interface) declaration.
class InterfaceSymbol final : public Symbol {
public:
  InterfaceSymbol(std::string name, SourceLocation location = {}, bool exported = false)
      : name_(std::move(name)),
        location_(location),
        exported_(exported) {}

  SymbolKind kind() const override { return SymbolKind::Interface; }
  const std::string &name() const override { return name_; }
  SourceLocation location() const override { return location_; }
  bool isExported() const override { return exported_; }

private:
  std::string name_;
  SourceLocation location_;
  bool exported_;
};

} // namespace kyna::semantics
