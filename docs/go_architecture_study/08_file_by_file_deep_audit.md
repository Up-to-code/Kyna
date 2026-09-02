# Deep File-by-File Audit: Every Kyna Source File vs Go Equivalent

> **40+ files audited with exact line numbers, function names, and Go counterpart references.**

---

## Compiler Module: `kyna_typecheck` (Semantic Analysis)

### [`type_checker.cpp`](../../compiler/kyna_typecheck/src/checkers/type_checker.cpp) — 385 lines

#### Problem 1: God-Function `Analyzer::expr` (365 lines, L19–383)
The single largest function in the compiler. Handles literals, identifiers, binary ops, unary ops, calls, member access, index access, assignments, lambda, if-expr, match-expr, array literals, object literals, constructor calls, and module lookups — all in one `std::visit` lambda.

**Go equivalent**: `go/types/expr.go` delegates to `rawExpr`, `exprInternal`, `unary`, `binary`, `call.go/callExpr`, `index.go/indexExpr`, `typexpr.go/typExpr` — each 30–80 lines.

#### Problem 2: String-Based Type Creation (L26–46)
```cpp
if (n.kind == LiteralKind::Integer)  return t("int");    // allocates string
if (n.kind == LiteralKind::Boolean)  return t("bool");   // allocates string
return t("class:" + n.name);   // encodes class identity as string prefix
return t("func");              // ALL functions are the same type
```

**Go equivalent**: Returns `Typ[Int]`, `Typ[Bool]` (zero-alloc singleton pointers), `*Named{obj: className}`, `*Signature{params, results, recv}` (preserves full function signature).

#### Problem 3: Magic Visibility Numbers (L11–17)
```cpp
static int visibility(const std::string &modifier) {
    if (modifier == "private")   return 0;
    if (modifier == "protected") return 1;
    return 2;  // public
}
```
Returns bare `int` instead of `enum class Visibility { Private, Protected, Public }`.

#### Problem 4: Operator Validation via Repetitive String Chains (L66–80)
```cpp
if (a.name == "int" || a.name == "float" || a.name == "num" || a.name == "any") { ... }
```

**Go equivalent**: Uses declarative predicate tables:
```go
unaryOpPredicates = opPredicates{
    token.ADD: allNumeric,
    token.SUB: allNumeric,
    token.NOT: allBoolean,
}
```

#### Problem 5: Manual Scope Stack Without RAII (L314–329)
```cpp
// IfExpr creates scope manually:
auto ifScope = new Scope();        // manual allocation
ifScope->parent = scope;
scope = ifScope;                   // manual push
// ... check body ...
scope = scope->parent;             // manual pop (skipped on exception)
```

**Go equivalent**: `defer` automatically restores environment on any exit path.

---

### [`analyzer.cpp`](../../compiler/kyna_typecheck/src/checkers/analyzer.cpp) — 136 lines

#### Problem 1: Three Copy-Pasted Collision Checks (L90–112)
Identical check for `FunctionDecl` (L90–94), `ClassDecl` (L99–103), and `InterfaceDecl` (L108–112). Should be unified as `declareSymbol(scope, name, kind)`.

#### Problem 2: Deep-Copy Rollback for Interactive Mode (L73–77, L117–124)
```cpp
auto previousTypes = scope->types;           // deep copy entire map
auto previousMutability = scope->mutability; // deep copy entire map
auto previousFunctions = functions;          // deep copy entire map
auto previousClasses = classes;              // deep copy entire map
auto previousInterfaces = interfaces;        // deep copy entire catalog
// ... if error, restore all 5 maps ...
```

**Go equivalent**: Uses persistent scopes — new scopes are pushed, never rolled back.

#### Problem 3: Union Construction via Magic String (L53–60)
```cpp
TypeRef merge(const TypeRef &a, const TypeRef &b) {
    return TypeRef{"union", false, {a, b}};  // "union" is a magic string
}
```

---

### [`statement_checker.cpp`](../../compiler/kyna_typecheck/src/checkers/statement_checker.cpp) — 339 lines

#### Problem 1: God-Function `Analyzer::stmt` (280 lines, L23–302)
Handles `VarDecl`, `FunctionDecl`, `ClassDecl`, `InterfaceDecl`, `IfStmt`, `WhileStmt`, `LoopStmt`, `SwitchStmt`, `TryStmt`, `ThrowStmt`, `BreakStmt`, `ContinueStmt`, `ReturnStmt` in one lambda.

#### Problem 2: Type Comparison via `.str()` Serialization (L15–20)
```cpp
bool sameParameters(const FunctionDecl &left, const FunctionDecl &right) {
    for (size_t index = 0; index < left.params.size(); ++index)
        if (left.params[index].type.str() != right.params[index].type.str())
            return false;   // converts types to strings then compares strings
}
```

**Go equivalent**: `Identical(x, y Type) bool` uses structural type comparison via pointer identity and recursive type traversal.

#### Problem 3: Class Fields Injected Into Method Scope (L257–258)
```cpp
for (const auto &f : classDecl->fields)
    scope->types[f.name] = f.type;  // mixes class fields with local variables
```

**Go equivalent**: Receiver fields are resolved via `Checker.selector` method set lookup, never injected into the function's lexical scope.

#### Problem 4: Incomplete Return Analysis (L304–336)
`alwaysReturns` is an ad-hoc pattern match that cannot model loop breaks, infinite loops, or match expressions. Go uses proper control-flow graph reachability analysis.

---

### [`interface_checker.cpp`](../../compiler/kyna_typecheck/src/checkers/interface_checker.cpp) — 211 lines

#### Problem 1: String-Based Generic Substitution (L19–31)
```cpp
TypeRef substituteContractType(const TypeRef &value, ...) {
    if (depth > 32) return in;  // hardcoded recursion limiter!
    if (value.name == contract.typeParams[index]) { ... }  // string match
}
```

**Go equivalent**: Type parameters use canonical indices (`TypeParam.index`) and proper unification via `Instantiate`.

#### Problem 2: Unmemoized O(N×M) Conformance Checks (L78–164)
```cpp
for (const auto &method : contract.methods) {
    auto it = std::find_if(classMethods.begin(), classMethods.end(),
        [&](const auto &m) { return m.name == method.name; });
    // O(N×M) nested linear scan on every conformance check
}
```

**Go equivalent**: `Interface` constructs a sorted `methodSet` once; conformance uses ordered intersection.

---

## Compiler Module: `kyna_hir` (HIR Lowering)

### [`syntax_lowering.cpp`](../../compiler/kyna_hir/src/lowering/syntax_lowering.cpp) — 295 lines

#### Problem 1: Five Sequential Passes Over the Same AST (L15, L56, L68, L73, L94)
```cpp
// Pass 1: Register classes
for (auto &d : tree.module.declarations) { ... }
// Pass 2: Register class fields
for (auto &d : tree.module.declarations) { ... }
// Pass 3: Register functions
for (auto &d : tree.module.declarations) { ... }
// Pass 4: Lower class methods
for (auto &d : tree.module.declarations) { ... }
// Pass 5: Lower top-level functions
for (auto &d : tree.module.declarations) { ... }
```

**Go equivalent**: Uses on-demand lazy resolution (check declaration when first referenced, not by iterating everything N times).

#### Problem 2: Hardcoded Stdlib Names in Compiler Core (L212–227)
```cpp
bool isFetchCall(const ExprPtr &expr) { /* checks for "fetch" */ }
bool isResponseExpression(const ExprPtr &expr) { /* checks for "responseJson" */ }
```
The compiler should never name specific standard library functions.

#### Problem 3: Parallel Vector Index Coupling (L130–150)
```cpp
program.locals.push_back(...);      // index 0
localOwners.push_back(...);         // must stay in sync with index 0
responseLocals.push_back(false);    // must stay in sync with index 0
```

---

## Compiler Module: `kyna_mir` (MIR Lowering)

### [`hir_lowering.cpp`](../../compiler/kyna_mir/src/lowering/hir_lowering.cpp) — 133 lines

#### Problem 1: Raw Pointer Re-Aliasing After Vector Reallocation (L53–65)
```cpp
void activate(MirFunction &fn) {
    activeTemporaryCount = &fn.temporaryCount;  // raw pointer
    activeBlocks = &fn.blocks;                  // raw pointer — invalidated if mir.functions reallocates!
}
```

#### Problem 2: Silent Label Fallback (L90–97)
```cpp
MirBlockIndex loopTarget(const std::string &label) {
    // If label not found, silently returns loops.back() — wrong target!
    return loops.back();
}
```

### [`expression_lowering.cpp`](../../compiler/kyna_mir/src/lowering/expression_lowering.cpp) — 317 lines

#### Problem: 1-Based Sentinel Hacking (L33, L64, L122, L221)
```cpp
instruction.function = node.function.value + 1;  // +1 so that 0 means "none"
```
Uses `0` as a null sentinel instead of `std::optional<uint32_t>`.

---

## Lexing Module: `kyna_lexing`

### [`token.hpp`](../../compiler/kyna_lexing/include/kyna/lexing/token.hpp) — 108 lines

#### Problem 1: Flat Enum Without Category Ranges
```cpp
enum class TokenKind {
    LeftParen, RightParen, LeftBrace, RightBrace,  // delimiters
    // ... 93 more in random order ...
    Var, Const, Function, Class, Interface, If, Else, // keywords mixed in
};
```

**Go equivalent**:
```go
const (
    literal_beg Token = iota + 10
    INT; FLOAT; IMAG; CHAR; STRING
    literal_end
    operator_beg
    ADD; SUB; MUL; QUO; REM // ...
    operator_end
    keyword_beg
    BREAK; CASE; CHAN; CONST; // ...
    keyword_end
)
func (tok Token) IsKeyword() bool  { return keyword_beg < tok && tok < keyword_end }
func (tok Token) IsLiteral() bool  { return literal_beg < tok && tok < literal_end }
```

#### Problem 2: Heap String Per Token
```cpp
struct Token {
    TokenKind kind;
    std::string lexeme;  // heap-allocated copy of source text
    int line;
};
```

**Go equivalent**: `token.Pos` is an integer byte offset; the source text is accessed via `token.FileSet.Position(pos)`.

### [`token_scanner.cpp`](../../compiler/kyna_lexing/src/checkers/token_scanner.cpp) — 83 lines

#### Problem: Exception-Driven Error Handling
```cpp
void TokenScanner::fail(const std::string &message) {
    throw KynaError(diagnostic);  // C++ exception for every lexer error
}
```

**Go equivalent**: `scanner.error(pos, msg)` records the error and returns `token.ILLEGAL` — no exception unwinding.

---

## Syntax Module: `kyna_syntax`

### [`expression_nodes.hpp`](../../compiler/kyna_syntax/include/kyna/syntax/expression_nodes.hpp) — 94 lines

#### Problem 1: `shared_ptr` Per Node
```cpp
using ExprPtr = std::shared_ptr<Expr>;  // atomic refcount + heap alloc per subexpr
```

#### Problem 2: String Literal Values
```cpp
struct Literal { LiteralKind kind; std::string value; };  // "42" instead of int64_t 42
```

#### Problem 3: No Visitor Pattern
Every compiler pass writes identical `std::visit` + `if constexpr` boilerplate for 14+ variants.

### [`declaration_nodes.hpp`](../../compiler/kyna_syntax/include/kyna/syntax/declaration_nodes.hpp) — 93 lines

#### Problem: Declarations Inside `Stmt::Node`
```cpp
using Node = std::variant<
    BlockStmt, IfStmt, WhileStmt, ...,       // statements
    FunctionDecl, ClassDecl, InterfaceDecl,   // declarations mixed in
    ImportDecl, ExportDecl                    // module declarations mixed in
>;
```

**Go equivalent**: `ast.Decl` is a separate interface from `ast.Stmt` with distinct marker methods.

---

## Runtime Module: `kyna_vm`

### [`expression_evaluator.cpp`](../../runtime/kyna_vm/src/execution/expression_evaluator.cpp) — 318 lines

#### Problem 1: Monolithic Eval (L7–169, 162 lines)
`Interpreter::eval` handles 14 expression types in one `std::visit`.

#### Problem 2: Hardcoded Error Object Properties (L256–265)
```cpp
if (memberName == "message") return error->message;
if (memberName == "code")    return error->code;
if (memberName == "cause")   return error->cause;
```
Should use a VTable/property descriptor, not hardcoded string comparisons.

#### Problem 3: Placeholder Source Locations (L43, L65, L111, L115, L138)
```cpp
throw KynaError(Diagnostic{..., {1, 1}});  // hardcoded line 1, col 1
```

### [`statement_executor.cpp`](../../runtime/kyna_vm/src/execution/statement_executor.cpp) — 266 lines

#### Problem: In-Band Flow Control via Mutable State
```cpp
struct { Flow::Kind kind; Value value; } flow;  // global mutable control flow
// Checked after every statement: if (flow.kind != Flow::None) return;
```

---

## Standard Library: `library/core`

### [`collections_library.cpp`](../../library/core/src/catalog/collections_library.cpp) — 132 lines

#### Problem: O(n²) Bubble Sort as Standard Library `sort`
```cpp
// L67-114: bubbleSort — the standard library sort algorithm is O(n²)
for (int i = 0; i < n - 1; i++)
    for (int j = 0; j < n - i - 1; j++)
        if (compare(arr[j], arr[j+1])) std::swap(arr[j], arr[j+1]);
```

**Go equivalent**: `slices.Sort` uses pattern-defeating quicksort — $O(n \log n)$ guaranteed.

### [`bytecode_standard_library.cpp`](../../library/core/src/bytecode/bytecode_standard_library.cpp) — 64 lines

#### Problem: 7-Level Cascading String Dispatch
```cpp
Value invoke(string_view name, vector<Value> &args) {
    if (auto r = invokeConsole(name, args)) return *r;    // string chain
    if (auto r = invokeText(name, args)) return *r;       // string chain
    if (auto r = invokeCollections(name, args)) return *r; // string chain
    if (auto r = invokeFilesystem(name, args)) return *r;  // string chain
    // ... 3 more cascades
}
```

**Go equivalent**: Uses indexed function tables or hash maps for $O(1)$ dispatch.

---

## Runtime Host: `kyna_host`

### [`curl_network.cpp`](../../runtime/kyna_host/src/capabilities/curl_network.cpp) — 157 lines

#### Problem: Hardcoded Test Mock in Production (L79–83)
```cpp
if (request.url == "mock://kyna/users") {
    return NetworkResponse{200, R"([{"id":1,"name":"alice"}])", {}};
}
```

### [`local_process.cpp`](../../runtime/kyna_host/src/capabilities/local_process.cpp) — 24 lines

#### Problem: Shell Injection (L12)
```cpp
int run(const std::string &command) {
    return std::system(command.c_str());  // shell injection vulnerability
}
```

### [`beast_http_server.cpp`](../../runtime/kyna_host/src/capabilities/beast_http_server.cpp) — 84 lines

#### Problem 1: Global Signal Hijacking (L44–45)
```cpp
std::signal(SIGINT, interruptServer);   // overwrites process-global signal handlers
std::signal(SIGTERM, interruptServer);
```

#### Problem 2: Busy-Polling Accept Loop (L48–50)
```cpp
if (ec == boost::asio::error::would_block) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));  // busy poll
    continue;
}
```

---

## Tools: `kyna_cli`

### [`project_dev.cpp`](../../tools/kyna_cli/src/commands/project_dev.cpp) — 253 lines

#### Problem 1: Bypasses ProcessPort (L67–85)
Implements raw POSIX `fork`/`execv`/`waitpid`/`kill` directly instead of using the injected `ProcessPort` capability abstraction.

#### Problem 2: CPU-Bound Filesystem Polling (L48–65)
```cpp
// Every 100ms: recursively walk entire directory tree, hash every file
std::this_thread::sleep_for(std::chrono::milliseconds(100));
auto newHash = hashDirectory(projectPath);
```

**Go equivalent**: Uses `fsnotify` (wrapping `kqueue`/`inotify`/`ReadDirectoryChangesW`).

### [`project_dependencies.cpp`](../../tools/kyna_cli/src/commands/project_dependencies.cpp) — 150 lines

#### Problem: Shell Command String Formatting (L103–122)
```cpp
std::system(("git clone " + url + " " + targetPath).c_str());
// No argument escaping, no error handling, bypasses ProcessPort
```

### Five Copy-Pasted Dump Commands
`bytecode_dump_command.cpp`, `hir_dump_command.cpp`, `mir_dump_command.cpp`, `syntax_dump_command.cpp`, `token_dump_command.cpp` — each ~27 lines of nearly identical boilerplate. Should be one parameterized handler.

---

## SDK: `kyna_embedding`

### [`statement_helpers.cpp`](../../sdk/kyna_embedding/src/support/statement_helpers.cpp) — 71 lines

#### Problem: 3 Redundant `std::visit` Traversals
```cpp
// L7-42:  statementKind — visits all 14 variants to return a string
// L44-56: statementName — visits all 14 variants to return a name
// L58-68: statementExported — visits all 14 variants to return a bool
```

If AST nodes had a base `Node` with virtual `kind()`, `name()`, `exported()` — or an `ASTVisitor` — these would be zero boilerplate.
