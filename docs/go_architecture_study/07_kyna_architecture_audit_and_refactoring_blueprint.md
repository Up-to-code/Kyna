# Kyna Architecture Audit: Critical Weaknesses & Go-Inspired Refactoring Blueprint

> **Scope**: 40+ source files audited across `compiler/`, `runtime/`, `library/`, `tools/`, and `sdk/`.
> **Reference**: Official Go implementation at `/Users/ahmedmansour/Documents/go` (`go/types`, `cmd/compile/internal/types2`, `io`, `context`, `errors`).

---

## Part I: Critical Architectural Defects (Severity: CRITICAL)

### Defect 1: String-Based Type System — The Root of All Evil

**What Kyna Does Wrong**

The entire Kyna type system is a single flat struct with a string name:

```cpp
// compiler/kyna_types/include/kyna/semantics/type_model.hpp (17 lines total)
struct TypeRef {
  std::string name{"void"};   // "int", "str", "func", "class:Circle", "module:math"
  bool nullable{false};
  std::vector<TypeRef> typeArgs;
  std::vector<TypeRef> unionTypes;
  std::string str() const;
};
```

Every type comparison in the compiler is a string comparison:

```cpp
// type_checker.cpp L26-36: Literal type inference
if constexpr (is<Literal>(n)) {
    if (n.kind == LiteralKind::Integer)  return t("int");    // heap-allocates "int"
    if (n.kind == LiteralKind::Float)    return t("float");  // heap-allocates "float"
    if (n.kind == LiteralKind::String)   return t("str");    // heap-allocates "str"
    if (n.kind == LiteralKind::Boolean)  return t("bool");   // heap-allocates "bool"
}

// analyzer.cpp L27-52: Type compatibility via string matching
bool compatible(const TypeRef &e, const TypeRef &a) {
    if (e.name == "any" || a.name == "any") return true;
    if (a.name == "null") return e.nullable;
    if (e.name == "num" && (a.name == "int" || a.name == "float")) return true;
    // ... 25 more string comparisons
}
```

Worse, classes and modules encode metadata as string prefixes:

```cpp
// type_checker.cpp L46:  Classes become "class:Circle"
return t("class:" + n.name);

// type_checker.cpp L122: Modules become "module:math"
c.name.starts_with("module:")

// type_checker.cpp L193: Class lookup strips prefix
className.starts_with("class:")
```

Functions collapse to a typeless `"func"` string, destroying all higher-order type safety:

```cpp
// type_checker.cpp L44: ALL functions become the same type
return t("func");
// A function (int, int) -> int is identical to () -> void
```

**How Go Solves This**

Go's `go/types` package models types as a polymorphic interface with 14 concrete implementations:

```go
// go/types/type.go
type Type interface {
    Underlying() Type
    String() string
}

// Concrete types in dedicated files:
// basic.go:     *Basic     (int, float64, bool, string, untyped constants)
// signature.go: *Signature (params *Tuple, results *Tuple, recv *Var, variadic bool)
// named.go:     *Named     (obj *TypeName, underlying Type, methods []*Func)
// struct.go:    *Struct    (fields []*Var, tags []string)
// interface.go: *Interface (methods []*Func, embeddeds []Type, tset *_TypeSet)
// pointer.go:   *Pointer   (base Type)
// array.go:     *Array     (len int64, elem Type)
// slice.go:     *Slice     (elem Type)
// map.go:       *Map       (key Type, elem Type)
// chan.go:       *Chan      (dir ChanDir, elem Type)
// tuple.go:     *Tuple     (vars []*Var)
// typeparam.go: *TypeParam (obj *TypeName, bound Type)
// union.go:     *Union     (terms []*Term)
// alias.go:     *Alias     (obj *TypeName, orig Type)
```

Predeclared types are singletons in a static array — zero allocation:

```go
// go/types/universe.go
var Typ = [...](*Basic){
    Bool:    {Bool, IsBoolean, "bool"},
    Int:     {Int, IsInteger, "int"},
    Float64: {Float64, IsFloat, "float64"},
    String:  {String, IsString, "string"},
    // ...
}
```

**Impact on Kyna**: Every expression evaluation allocates a new `std::string` for its type. A file with 10,000 expressions creates 10,000+ heap string allocations just for type metadata. Type-safe higher-order functions, generic constraints, and method set calculations are impossible with string names.

---

### Defect 2: God-Functions — Monolithic `std::visit` Lambdas

**What Kyna Does Wrong**

Six critical compiler functions exceed 150 lines each, cramming all logic into a single `std::visit` lambda:

| File | Function | Lines | Responsibility |
|------|----------|-------|----------------|
| `type_checker.cpp` | `Analyzer::expr` | **365** | All expression type synthesis |
| `statement_checker.cpp` | `Analyzer::stmt` | **280** | All statement + declaration checking |
| `expression_lowering.cpp` (HIR) | `SyntaxLowerer::lowerExpression` | **256** | All 17 expression lowering paths |
| `expression_lowering.cpp` (MIR) | `HirLowerer::lowerExpression` | **308** | All 21 HIR expression lowering paths |
| `statement_lowering.cpp` (MIR) | `HirLowerer::lowerStatement` | **212** | All statement MIR lowering |
| `expression_evaluator.cpp` | `Interpreter::eval` | **162** | All runtime expression evaluation |

**How Go Solves This**

Go's type checker delegates to dedicated per-construct functions:

```go
// go/types/expr.go — Top-level dispatches to focused functions:
func (check *Checker) rawExpr(T *target, x *operand, e ast.Expr, ...)
func (check *Checker) unary(x *operand, e *ast.UnaryExpr)
func (check *Checker) binary(x *operand, e ast.Expr, lhs, rhs ast.Expr, op token.Token)
func (check *Checker) exprOrType(x *operand, e ast.Expr, ...)

// go/types/call.go:
func (check *Checker) callExpr(x *operand, call *ast.CallExpr)
func (check *Checker) arguments(call *ast.CallExpr, sig *Signature, ...)

// go/types/index.go:
func (check *Checker) indexExpr(x *operand, e *ast.IndexExpr)

// go/types/typexpr.go:
func (check *Checker) typExpr(e ast.Expr) Type
```

Each function is 30–80 lines and has a single clear responsibility.

---

### Defect 3: No Symbol/Object Hierarchy — Disjoint Ad-Hoc Symbol Tables

**What Kyna Does Wrong**

Symbols are stored in 4+ disconnected maps with no common abstraction:

```cpp
// compiler/kyna_typecheck/include/kyna/semantics/program_analyzer.hpp L28-40
struct Scope {
    std::map<std::string, TypeRef> types;        // variable types
    std::map<std::string, bool>    mutability;   // var vs const
    Scope *parent = nullptr;
};

// Top-level symbols bypass the scope tree entirely:
std::map<std::string, FunctionDecl> functions;   // separate from scope
std::map<std::string, ClassDecl>    classes;     // separate from scope
InterfaceCatalog                    interfaces;  // separate from scope
```

There is no common `Symbol` object representing "a named thing with a type and a source location."

**How Go Solves This**

Go uses a unified `Object` interface stored in a hierarchical `Scope` tree:

```go
// go/types/object.go
type Object interface {
    Parent() *Scope    // enclosing scope
    Pos() token.Pos    // source position
    Pkg() *Package     // owning package
    Name() string      // identifier
    Type() Type        // semantic type
    Exported() bool    // uppercase = exported
    Id() string        // qualified identifier
}
// Concrete: Const, TypeName, Var, Func, PkgName, Label, Builtin, Nil

// go/types/scope.go
type Scope struct {
    parent   *Scope
    children []*Scope
    elems    map[string]Object   // ALL symbols in one map
    pos, end token.Pos           // lexical extent
}
```

All symbols — variables, functions, classes, packages — live in the unified scope tree.

---

### Defect 4: Mutable Global State in the Analyzer (No RAII Environment)

**What Kyna Does Wrong**

The `Analyzer` tracks context through mutable instance variables:

```cpp
// program_analyzer.hpp L41-46
std::string currentClass;           // manually set/cleared
TypeRef currentReturn;              // manually set/cleared
bool inFunction = false;            // manually toggled
std::vector<std::string> activeLoopLabels;  // manually pushed/popped
int switchDepth = 0;                // manually incremented/decremented
```

These are manually saved and restored around nested constructs with no RAII guard:

```cpp
// statement_checker.cpp L176-178: Manual save/restore for functions
auto savedReturn = currentReturn;
auto savedInFunction = inFunction;
// ... check function body ...
currentReturn = savedReturn;       // If an exception is thrown, state is corrupted
inFunction = savedInFunction;
```

**How Go Solves This**

Go uses a scoped `environment` struct with `defer`-based RAII restoration:

```go
// go/types/check.go
type environment struct {
    decl    *declInfo
    scope   *Scope
    sig     *Signature
    iota    constant.Value
    // ...
}

// go/types/stmt.go — RAII environment guard:
defer func(env environment, indent int) {
    check.environment = env        // Always restored, even on panic
    check.indent = indent
}(check.environment, check.indent)
```

Control flow validity uses a clean bitset, not mutable counters:

```go
type stmtContext uint
const (
    breakOk    stmtContext = 1 << iota
    continueOk
    fallthroughOk
    finalSwitchCase
    inTypeSwitch
)
```

---

### Defect 5: Dual Execution Engines with Divergent Behavior

**What Kyna Does Wrong**

Kyna has TWO completely separate standard library implementations:

1. **Tree-walk interpreter** (`library/core/src/catalog/*.cpp`) — registers native functions via `global->define(...)` with inline C++ lambdas.
2. **Bytecode VM** (`library/core/src/bytecode/*_invoke.cpp`) — dispatches via `if (name == "...")` string chains with completely different implementations.

Critical divergences:

| Operation | Tree-Walk Engine | Bytecode VM |
|-----------|-----------------|-------------|
| `sort` | $O(n^2)$ bubble sort | $O(n \log n)$ `std::stable_sort` |
| `filter`, `map`, `reduce` | Fully functional | **Throws runtime error** |
| `fetch` response methods | Works via AST evaluation | Requires special HIR lowering hacks |

**How Go Solves This**

Go has ONE execution engine (the compiled binary). Standard library functions are compiled Go code — the same source runs everywhere. There is no "interpreter mode" vs "compiled mode" behavioral split.

---

## Part II: High-Severity Architectural Weaknesses

### Defect 6: Hardcoded Standard Library Names in the Compiler

The HIR lowering pass hardcodes runtime API names:

```cpp
// syntax_lowering.cpp L212-227
bool isFetchCall(const ExprPtr &expr) {
    // Hardcodes "fetch" string in compiler core
}
bool isResponseExpression(const ExprPtr &expr) {
    // Hardcodes "responseJson", "responseText" in compiler core
}
```

Go's compiler never names specific standard library functions in its lowering passes.

### Defect 7: No Cycle Detection for Type Declarations

Kyna performs a single forward pass over declarations. Circular type references cause stack overflow or incomplete validation. Go uses a three-color (White/Grey/Black) graph marking algorithm on `Checker.objPath` to detect cycles immediately.

### Defect 8: Duplicated Helper Functions Across Translation Units

The `visibility()` and `sameParameters()` functions are copy-pasted identically in 3 files:
- `type_checker.cpp` L11-17
- `statement_checker.cpp` L10-20
- `interface_checker.cpp` L8-18

### Defect 9: Exception-Driven Lexer Control Flow

The scanner throws C++ exceptions (`KynaError`) on every lexical error, causing expensive stack unwinding. Go's scanner records errors into an `ErrorList` and returns `token.ILLEGAL`, allowing continuous parsing.

### Defect 10: `std::shared_ptr` on Every AST Node

Every AST subexpression and sub-statement is individually heap-allocated with atomic reference counting:

```cpp
using ExprPtr = std::shared_ptr<Expr>;  // Atomic refcount + heap alloc per node
using StmtPtr = std::shared_ptr<Stmt>;  // Atomic refcount + heap alloc per node
```

Go uses arena-allocated syntax nodes and `ast.Walk`/`ast.Inspect` visitors.

### Defect 11: Modifiers as `std::vector<std::string>`

Every declaration allocates a vector of heap strings for access modifiers:

```cpp
std::vector<std::string> modifiers;  // {"public", "static", "final"}
// Checked via: std::find(modifiers.begin(), modifiers.end(), "public")
```

Go uses boolean fields or bitmask flags ($O(1)$ bitwise AND).

### Defect 12: Linear Standard Library Symbol Lookup

`findStandardLibrarySymbol` scans 74 entries linearly on every identifier resolution:

```cpp
// standard_library_symbols.cpp L106-111
for (const auto &symbol : symbols) {
    if (symbol.name == name) return symbol;  // O(n) on every lookup
}
```

Go's `Universe` scope uses a hash map for $O(1)$ lookup.

### Defect 13: Hardcoded Test Mock in Production Network Adapter

```cpp
// curl_network.cpp L79-83 — IN PRODUCTION CODE:
if (request.url == "mock://kyna/users") {
    return NetworkResponse{200, R"([{"id":1,"name":"alice"}])", ...};
}
```

### Defect 14: Shell Injection in Process Execution

```cpp
// local_process.cpp L12 — Raw shell interpolation:
int run(const std::string &command) { return std::system(command.c_str()); }
```

Go's `os/exec.Command` uses argument vectors, never shell interpolation.

### Defect 15: 5 Copy-Pasted CLI Dump Commands

`bytecode_dump_command.cpp`, `hir_dump_command.cpp`, `mir_dump_command.cpp`, `syntax_dump_command.cpp`, and `token_dump_command.cpp` are nearly identical 27-line files.

---

## Part III: Missing Abstractions (vs Go Standard Patterns)

### Missing 1: Universal `Reader`/`Writer` Streaming Interface

Kyna's `FileSystemPort::read` returns entire files as `std::string`. Go's `io.Reader` streams chunks:

```go
type Reader interface { Read(p []byte) (n int, err error) }
type Writer interface { Write(p []byte) (n int, err error) }
```

### Missing 2: Request `Context` with Cancellation

No mechanism to cancel long-running compilation, network requests, or database queries. Go threads `context.Context` through every pipeline boundary.

### Missing 3: AST Visitor Pattern

Every pass writes identical `std::visit` + `if constexpr` boilerplate. Go provides `ast.Walk(visitor, node)` and `ast.Inspect(node, func)`.

### Missing 4: Bidirectional Type Inference (Target Types)

Kyna's expression checker is purely bottom-up. Go passes a `*target` type downward for contextual inference of untyped constants and generic instantiation.

### Missing 5: `Scope.Insert()` with Conflict Reporting

Kyna directly writes `scope->types[name] = type` with no collision detection. Go's `Scope.Insert(obj)` returns the existing conflicting `Object` for precise duplicate-declaration diagnostics.

---

## Part IV: Prioritized Refactoring Roadmap

### Phase 1: Type System Foundation (Highest Impact)

| Task | Files Affected | Go Reference |
|------|---------------|--------------|
| Replace `TypeRef` with polymorphic `Type` hierarchy (`BasicType`, `FunctionType`, `ClassType`, `InterfaceType`, `UnionType`) | `kyna_types/`, `kyna_typecheck/` | `go/types/*.go` |
| Create singleton `Universe` scope with static `Typ[Int]`, `Typ[Bool]`, etc. | `kyna_typecheck/`, `kyna_symbols/` | `go/types/universe.go` |
| Create unified `Symbol` hierarchy (`VarSymbol`, `FuncSymbol`, `ClassSymbol`) | `kyna_typecheck/` | `go/types/object.go` |
| Upgrade `Scope` to store `map<string, SymbolPtr>` with parent/children tree | `kyna_typecheck/` | `go/types/scope.go` |
| Replace `findStandardLibrarySymbol` linear scan with hash map | `kyna_symbols/` | `go/types/universe.go` |

### Phase 2: Checker Architecture (High Impact)

| Task | Files Affected | Go Reference |
|------|---------------|--------------|
| Break `Analyzer::expr` (365 lines) into per-node handler methods | `type_checker.cpp` | `go/types/expr.go`, `call.go`, `index.go` |
| Break `Analyzer::stmt` (280 lines) into per-statement methods | `statement_checker.cpp` | `go/types/stmt.go` |
| Replace mutable `currentClass`/`currentReturn` with RAII `EnvironmentGuard` | `program_analyzer.hpp`, `statement_checker.cpp` | `go/types/check.go` |
| Implement tri-color cycle detection for type declarations | `analyzer.cpp` | `go/types/decl.go` |
| Extract duplicated `visibility()` and `sameParameters()` into shared header | `type_checker.cpp`, `statement_checker.cpp`, `interface_checker.cpp` | — |
| Convert modifiers from `vector<string>` to `enum class ModifierFlags : uint16_t` | `declaration_nodes.hpp` | Go uses boolean fields |
| Add bidirectional target-type inference to `expr()` | `type_checker.cpp` | `go/types/expr.go` |

### Phase 3: Lexer & AST Performance (Medium Impact)

| Task | Files Affected | Go Reference |
|------|---------------|--------------|
| Group `TokenKind` into contiguous ranges with sentinel bounds | `token.hpp` | `go/token/token.go` |
| Replace `std::string lexeme` with `std::string_view` or byte offsets | `token.hpp`, `token_scanner.cpp` | `go/scanner/scanner.go` |
| Replace exception-driven scanner errors with diagnostic recording | `token_scanner.cpp` | `go/scanner/scanner.go` |
| Introduce `ASTVisitor<T>` base class for tree traversal | `expression_nodes.hpp`, `statement_nodes.hpp` | `go/ast/walk.go` |
| Separate `Decl` nodes from `Stmt::Node` variant | `declaration_nodes.hpp`, `statement_nodes.hpp` | `go/ast/ast.go` |

### Phase 4: Runtime & Standard Library Unification (Medium Impact)

| Task | Files Affected | Go Reference |
|------|---------------|--------------|
| Unify tree-walk and bytecode stdlib into single implementation | `library/core/src/catalog/`, `library/core/src/bytecode/` | Go has ONE compiled stdlib |
| Replace $O(n^2)$ `bubbleSort` with $O(n \log n)$ sort | `collections_library.cpp` | `slices.Sort` |
| Replace linear `if (name == "...")` dispatch with hash map | `bytecode_standard_library.cpp`, `*_invoke.cpp` | Go uses indexed function tables |
| Remove hardcoded mock from `CurlNetwork` production code | `curl_network.cpp` | Injected test transports |
| Replace `std::system()` with safe process spawning | `local_process.cpp`, `project_dependencies.cpp` | `os/exec.Command` |
| Introduce `StreamPort` (`Reader`/`Writer`) into `RuntimeCapabilities` | `runtime_capabilities.hpp` | `io.Reader`, `io.Writer` |
| Remove hardcoded `"fetch"`/`"responseJson"` from HIR lowerer | `syntax_lowering.cpp` | Go never names stdlib in compiler |
| Merge duplicate `*_dump_command.cpp` into single parameterized handler | `tools/kyna_cli/src/commands/` | — |

---

## Summary: Defect Severity Heat Map

```text
                     CRITICAL    HIGH       MEDIUM     LOW
Type System          ████████    ░░░░░░░░   ░░░░░░░░   ░░░░░░░░
  String TypeRef     ████████
  No FunctionType    ████████
  No Universe        ░░░░░░░░    ████████

Checker Architecture ░░░░░░░░    ████████   ░░░░░░░░   ░░░░░░░░
  God-Functions      ░░░░░░░░    ████████
  No RAII Env        ░░░░░░░░    ████████
  No Cycle Detect    ░░░░░░░░    ████████
  Duplicated Helpers ░░░░░░░░    ░░░░░░░░   ████████

Runtime              ████████    ████████   ░░░░░░░░   ░░░░░░░░
  Dual Engines       ████████
  Mock in Prod       ░░░░░░░░    ████████
  Shell Injection    ░░░░░░░░    ████████

Performance          ░░░░░░░░    ░░░░░░░░   ████████   ░░░░░░░░
  shared_ptr AST     ░░░░░░░░    ░░░░░░░░   ████████
  String Modifiers   ░░░░░░░░    ░░░░░░░░   ████████
  Linear Symbol Scan ░░░░░░░░    ░░░░░░░░   ████████
  Exception Lexer    ░░░░░░░░    ░░░░░░░░   ████████
```
