# Plan 03: Lexical Scope Tree & Symbol Object Model

> **Goal**: Unify disjoint symbol tables into a polymorphic `Symbol` hierarchy, implement a first-class lexical `Scope` tree, and manage checking state via an RAII `EnvironmentGuard`.
> **Inspiration**: Go's `go/types/object.go`, `go/types/scope.go`, and `go/types/check.go`.

---

## 1. Problem Statement & Root Cause

Currently in `compiler/kyna_typecheck/include/kyna/semantics/program_analyzer.hpp`:

1. **Symbols Bypassing Scope**:
   Top-level symbols live in disconnected global maps:
   ```cpp
   std::map<std::string, FunctionDecl> functions;
   std::map<std::string, ClassDecl> classes;
   InterfaceCatalog interfaces;
   ```
   Functions and classes declared in nested blocks cannot be properly resolved or tracked.
2. **Primitive Local Scope**:
   `Scope` only maps names to `TypeRef` and `bool`:
   ```cpp
   struct Scope {
     std::map<std::string, TypeRef> types;
     std::map<std::string, bool> mutability;
     Scope* parent = nullptr;
   };
   ```
   It has no child links, source spans, or symbol kind tags.
3. **Fragile Mutable State Machine in Analyzer**:
   ```cpp
   std::string currentClass;
   TypeRef currentReturn;
   bool inFunction = false;
   std::vector<std::string> activeLoopLabels;
   int switchDepth = 0;
   ```
   These fields are manually overwritten when checking nested classes/functions. If analysis encounters an error, state is corrupted.

---

## 2. Target Architecture

### 2.1 The `Symbol` (`Object`) Hierarchy
```cpp
namespace kyna::semantics {

enum class SymbolKind : uint8_t {
  Variable,
  Constant,
  Function,
  Class,
  Interface,
  Package,
  TypeParam
};

class Scope;

class Symbol {
public:
  virtual ~Symbol() = default;
  virtual SymbolKind kind() const = 0;
  virtual const std::string& name() const = 0;
  virtual types::TypePtr type() const = 0;
  virtual SourceLocation location() const = 0;
  virtual Scope* parentScope() const = 0;
  virtual bool isExported() const = 0;
};

using SymbolPtr = std::shared_ptr<Symbol>;

class VarSymbol : public Symbol { /* ... */ };
class FuncSymbol : public Symbol { /* ... */ };
class ClassSymbol : public Symbol { /* ... */ };
class InterfaceSymbol : public Symbol { /* ... */ };

} // namespace kyna::semantics
```

### 2.2 The Lexical `Scope` Tree
```cpp
class Scope {
private:
  Scope* parent_{nullptr};
  std::vector<std::unique_ptr<Scope>> children_;
  std::unordered_map<std::string, SymbolPtr> symbols_;
  SourceSpan extent_;

public:
  explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

  Scope* parent() const { return parent_; }
  Scope* createChild(SourceSpan extent = {});

  // Returns existing conflicting symbol if already defined in THIS scope
  SymbolPtr insert(SymbolPtr symbol);

  // Looks up identifier, searching up the parent chain
  SymbolPtr lookup(const std::string& name) const;

  // Looks up only in the immediate scope
  SymbolPtr lookupLocal(const std::string& name) const;
};
```

### 2.3 RAII `EnvironmentGuard`
```cpp
struct Environment {
  SymbolPtr currentFunc{nullptr};
  SymbolPtr currentClass{nullptr};
  types::TypePtr expectedReturn{nullptr};
  uint32_t loopDepth{0};
  uint32_t switchDepth{0};
  bool allowBreak{false};
  bool allowContinue{false};
};

class EnvironmentGuard {
private:
  Analyzer& analyzer_;
  Environment previous_;

public:
  EnvironmentGuard(Analyzer& analyzer, Environment newEnv)
    : analyzer_(analyzer), previous_(analyzer.env_) {
    analyzer_.env_ = std::move(newEnv);
  }

  ~EnvironmentGuard() {
    analyzer_.env_ = std::move(previous_);
  }
};
```

---

## 3. Implementation Steps

- [ ] **Step 1: Create `compiler/kyna_typecheck/include/kyna/semantics/symbol.hpp`**
  - Implement `Symbol` base interface and concrete `VarSymbol`, `FuncSymbol`, `ClassSymbol`.
- [ ] **Step 2: Upgrade `Scope` in `compiler/kyna_typecheck/include/kyna/semantics/scope.hpp`**
  - Implement `Scope::insert(symbol)` with duplicate detection.
  - Implement parent-walking `Scope::lookup(name)`.
- [ ] **Step 3: Implement `EnvironmentGuard`**
  - Encapsulate mutable analyzer fields in `Environment`.
  - Guard all function, method, and loop checks with RAII guards.
- [ ] **Step 4: Three-Color Cycle Detection for Declarations**
  - Track `Checker.declPath` stack: mark `White` (unresolved), `Grey` (resolving), `Black` (resolved).
  - Detect circular class inheritance or type alias loops cleanly.

---

## 4. Verification Plan

1. **Unit Tests**:
   - Write tests for lexical scope shadowing, duplicate declaration diagnostics, and inheritance cycle detection.
2. **Automated Verification**:
   - Run `ctest --test-dir build-debug -R semantic`.
   - Run `python3 build_tools/verify_repository_architecture.py`.
