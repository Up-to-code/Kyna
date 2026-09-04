# Plan 02: Decompose God-Functions & Add AST Visitors

> **Goal**: Break down monolithic `std::visit` functions exceeding 150–365 lines into fine-grained handler methods and provide a clean AST visitor interface.
> **Inspiration**: Go's `go/types/expr.go`, `go/types/stmt.go`, `cmd/compile/internal/walk`, and `go/ast/walk.go`.

---

## 1. Problem Statement & Root Cause

Currently, several files in Kyna inline entire semantic passes inside single functions:

| File | God-Function | Line Count | Problem |
| :--- | :--- | :--- | :--- |
| `type_checker.cpp` | `Analyzer::expr` | **365 lines** (L19–383) | Inlines 14 expression types, type inference, member lookup, constructor calls |
| `statement_checker.cpp` | `Analyzer::stmt` | **280 lines** (L23–302) | Inlines all statements, declarations, exceptions, jumps, and loops |
| `expression_lowering.cpp` (HIR) | `SyntaxLowerer::lowerExpression` | **256 lines** (L77–332) | Inlines lowering for 17 expression variants |
| `expression_lowering.cpp` (MIR) | `HirLowerer::lowerExpression` | **308 lines** (L6–314) | Inlines lowering for 21 HIR expression variants |
| `statement_lowering.cpp` (MIR) | `HirLowerer::lowerStatement` | **212 lines** (L28–239) | Inlines lowering for 13 statement variants |

### Critical Flaws
1. **Unmaintainable Cognitive Load**: Modifying a single expression type requires editing a 365-line function with deeply nested lambdas.
2. **Duplicated AST Traversal Boilerplate**: Every compiler pass writes identical `std::visit` + `std::is_same_v` template boilerplate.
3. **Fragile Scope Balance**: When early-returning on errors inside massive handlers, scope push/pop stacks easily get out of balance.

---

## 2. Target Architecture

### 2.1 Fine-Grained Checker Handlers
Decompose `Analyzer::expr` across dedicated handler files:

```cpp
// compiler/kyna_typecheck/src/checkers/check_expr.cpp
TypePtr Analyzer::checkExpr(const ExprPtr& expr, TypePtr targetHint) {
  if (!expr) return Universe::Void();
  return std::visit([this, targetHint](const auto& n) -> TypePtr {
    using T = std::decay_t<decltype(n)>;
    if constexpr (std::is_same_v<T, Literal>)          return checkLiteral(n, targetHint);
    else if constexpr (std::is_same_v<T, Identifier>)  return checkIdentifier(n);
    else if constexpr (std::is_same_v<T, BinaryExpr>)  return checkBinary(n);
    else if constexpr (std::is_same_v<T, UnaryExpr>)   return checkUnary(n);
    else if constexpr (std::is_same_v<T, CallExpr>)    return checkCall(n);
    else if constexpr (std::is_same_v<T, MemberExpr>)  return checkMember(n);
    else if constexpr (std::is_same_v<T, IndexExpr>)   return checkIndex(n);
    else if constexpr (std::is_same_v<T, AssignExpr>)  return checkAssign(n);
    else if constexpr (std::is_same_v<T, LambdaExpr>)  return checkLambda(n, targetHint);
    else if constexpr (std::is_same_v<T, IfExpr>)      return checkIfExpr(n);
    else if constexpr (std::is_same_v<T, MatchExpr>)   return checkMatchExpr(n);
    else if constexpr (std::is_same_v<T, ArrayLiteral>) return checkArrayLiteral(n, targetHint);
    else if constexpr (std::is_same_v<T, ObjectLiteral>) return checkObjectLiteral(n);
    return Universe::Any();
  }, expr->node);
}
```

### 2.2 Reusable AST Visitor Interface
```cpp
// compiler/kyna_syntax/include/kyna/syntax/ast_visitor.hpp
namespace kyna::syntax {

template <typename Result = void>
class ASTVisitor {
public:
  virtual ~ASTVisitor() = default;

  // Expressions
  virtual Result visit(const Literal& node) = 0;
  virtual Result visit(const Identifier& node) = 0;
  virtual Result visit(const BinaryExpr& node) = 0;
  virtual Result visit(const UnaryExpr& node) = 0;
  virtual Result visit(const CallExpr& node) = 0;
  virtual Result visit(const MemberExpr& node) = 0;
  virtual Result visit(const IndexExpr& node) = 0;

  // Statements
  virtual Result visit(const VarDecl& node) = 0;
  virtual Result visit(const FunctionDecl& node) = 0;
  virtual Result visit(const ClassDecl& node) = 0;
  virtual Result visit(const IfStmt& node) = 0;
  virtual Result visit(const WhileStmt& node) = 0;
  virtual Result visit(const ReturnStmt& node) = 0;
};

} // namespace kyna::syntax
```

---

## 3. Implementation Steps

- [ ] **Step 1: Create `compiler/kyna_syntax/include/kyna/syntax/ast_visitor.hpp`**
  - Define `ASTVisitor<Result>` template base class.
- [ ] **Step 2: Split `type_checker.cpp`**
  - Extract `checkLiteral`, `checkBinary`, `checkUnary` into `compiler/kyna_typecheck/src/checkers/check_operators.cpp`.
  - Extract `checkCall`, `checkMember`, `checkIndex` into `compiler/kyna_typecheck/src/checkers/check_invocations.cpp`.
  - Reduce `type_checker.cpp` to the top-level dispatcher (<80 lines).
- [ ] **Step 3: Split `statement_checker.cpp`**
  - Extract declaration checking (`VarDecl`, `FunctionDecl`, `ClassDecl`) into `compiler/kyna_typecheck/src/checkers/check_declarations.cpp`.
  - Extract control-flow checking (`IfStmt`, `WhileStmt`, `LoopStmt`, `SwitchStmt`) into `compiler/kyna_typecheck/src/checkers/check_control_flow.cpp`.
- [ ] **Step 4: Decompose HIR Lowering**
  - Split `expression_lowering.cpp` and `statement_lowering.cpp` into focused lowerers per category.
- [ ] **Step 5: Decompose MIR Lowering**
  - Break `HirLowerer::lowerExpression` into distinct operator, call, and memory lowering methods.
- [ ] **Step 6: Update `CMakeLists.txt`**
  - Register all newly created files explicitly in `CMakeLists.txt`.

---

## 4. Verification Plan

1. **Automated Unit Tests**:
   - Run semantic tests: `ctest --test-dir build-debug -R semantic`.
   - Run HIR/MIR lowering tests: `ctest --test-dir build-debug -R "hir|mir"`.
2. **Architecture Verifier**:
   - Verify that all newly created `.cpp` files reside in domain subfolders (`src/checkers/`, `src/lowering/`).
   - Run `python3 build_tools/verify_repository_architecture.py`.
