# Master LLM Prompt: Implementing Kyna's Architectural Overhaul

> **Instructions for the User**: Copy and paste the prompt below directly into an LLM or AI coding assistant (e.g. Antigravity, Claude, GPT-4) to execute the architectural refactoring of Kyna systematically.

---

```markdown
You are a Principal Compiler and Systems Engineer tasked with executing the architectural refactoring of the Kyna programming language repository.

Your mission is to execute the engineering plans documented in `docs/plans/` step-by-step, transforming Kyna's compiler, type system, runtime, and standard library to match the modularity, safety, and high performance of Go (`golang/go`).

---

## 1. CRITICAL INVARIANTS & REPOSITORY RULES (MUST NEVER BE VIOLATED)

1. **Naming Invariant (Zero Tolerance)**:
   The language is named **Kyna**. Never use legacy project names anywhere in source code, headers, comments, or documentation. The verifier (`build_tools/verify_repository_architecture.py`) rejects any file containing legacy naming patterns.

2. **Clean Folder Architecture (`AGENTS.md`)**:
   - **One thing per file, one domain per folder**: Private implementations under `src/` must live in domain subfolders (`types/`, `validators/`, `checkers/`, `lowering/`, `rendering/`, `catalog/`, `codecs/`, etc.).
   - No `.cpp`, `.cc`, or `.cxx` files directly under a module's `src/` root (except the documented facades in `library/core/src/format_value_codec.cpp` and `json_value_codec.cpp`).
   - Do NOT create empty `register_*.cpp` templates.

3. **Explicit CMake Source Discovery (No Globs)**:
   - CMake uses explicit source lists. Never use `file(GLOB ...)` or `file(GLOB_RECURSE ...)`.
   - Every newly created `.cpp` file must be added explicitly to its owning module's `CMakeLists.txt`.

4. **4-Point Registration Invariant for Builtin Natives**:
   Every callable standard-library function MUST be registered in all 4 places:
   1. `library/core/src/bytecode/bytecode_standard_library.cpp` (bytecode invocation)
   2. `library/core/src/catalog/standard_library_catalog.cpp` (tree-walk catalog)
   3. `compiler/kyna_symbols/src/catalog/standard_library_symbols.cpp` (semantic symbol table)
   4. `tests/tooling/verify_language_examples.py` (`BUILTIN_COVERAGE` entry and example call)

5. **Verification Pipeline**:
   After every modification, run the automated verification suite:
   ```bash
   python3 build_tools/verify_repository_architecture.py
   python3 build_tools/verify_vscode_extension.py
   ctest --test-dir build-debug --output-on-failure
   ```

---

## 2. REPOSITORY & REFERENCE PATHS

- **Kyna Root**: Current repository root directory
- **Go Reference Source**: `/Users/ahmedmansour/Documents/go`
- **Architecture Research**: `docs/go_architecture_study/`
- **Executable Plans**: `docs/plans/`
  - `plan_01_type_system_polymorphic_hierarchy.md`
  - `plan_02_decompose_god_functions_and_visitors.md`
  - `plan_03_scope_tree_and_symbol_object_model.md`
  - `plan_04_multi_file_packages_and_internal_shielding.md`
  - `plan_05_universal_streaming_and_cancellation.md`
  - `plan_06_runtime_unification_and_security.md`
  - `plan_07_stdlib_expansion_and_high_performance_sort.md`

---

## 3. EXECUTION PHASES & STEP-BY-STEP TASKS

Execute the plans in strict dependency order:

### Phase 1: Type System Polymorphic Hierarchy (Plan 01)
1. In `compiler/kyna_types/include/kyna/types/type.hpp`, define the abstract `Type` interface (`kind()`, `underlying()`, `str()`, `isAssignableTo()`, `isIdenticalTo()`).
2. Implement concrete type classes in `compiler/kyna_types/src/types/`:
   - `BasicType`: `Int`, `Float`, `Bool`, `String`, `Void`, `Null`, `Any`.
   - `SignatureType`: first-class function signatures with `params`, `returnType`, `receiverType`.
   - `NamedType`: nominal classes and user types with underlying types and method sets.
   - `UnionType` and `NullableType`.
3. Implement `Universe` singleton registry (`Universe::Int()`, `Universe::Bool()`) for zero-allocation primitive type checks.
4. Update `compiler/kyna_types/CMakeLists.txt` with all new source files.
5. Provide a backward-compatible adapter so existing code using `TypeRef` compiles incrementally.
6. Verify with `ctest -R type` and architecture verifier.

### Phase 2: Decompose God-Functions & AST Visitors (Plan 02)
1. Create `compiler/kyna_syntax/include/kyna/syntax/ast_visitor.hpp` with template `ASTVisitor<Result>`.
2. Decompose `Analyzer::expr` (365 lines in `compiler/kyna_typecheck/src/checkers/type_checker.cpp`):
   - Extract operator checking to `src/checkers/check_operators.cpp`.
   - Extract call/index/member checking to `src/checkers/check_invocations.cpp`.
   - Keep `type_checker.cpp` as a clean, <80 line dispatcher.
3. Decompose `Analyzer::stmt` (280 lines in `compiler/kyna_typecheck/src/checkers/statement_checker.cpp`):
   - Extract declarations to `src/checkers/check_declarations.cpp`.
   - Extract control flow to `src/checkers/check_control_flow.cpp`.
4. Update `compiler/kyna_typecheck/CMakeLists.txt`.
5. Verify with `ctest -R semantic` and architecture verifier.

### Phase 3: Lexical Scope Tree & Symbol Object Model (Plan 03)
1. Create `compiler/kyna_typecheck/include/kyna/semantics/symbol.hpp` defining the `Symbol` interface and concrete `VarSymbol`, `FuncSymbol`, `ClassSymbol`.
2. Refactor `Scope` in `compiler/kyna_typecheck/include/kyna/semantics/scope.hpp` into a full lexical tree with `insert(symbol)` duplicate detection and parent-walking `lookup(name)`.
3. Implement `EnvironmentGuard` RAII helper to replace mutable fields (`currentClass`, `currentReturn`, `activeLoopLabels`) in `Analyzer`.
4. Implement three-color (White/Grey/Black) cycle detection for recursive type declarations.
5. Verify with `ctest -R semantic` and architecture verifier.

### Phase 4: Multi-File Packages & Internal Shielding (Plan 04)
1. Update `tools/kyna_cli/src/commands/run_command.cpp` and `check_command.cpp` to accept package directories.
2. Implement `PackageLoader` under `compiler/kyna_loading/src/loading/package_loader.cpp` to parse all `*.kyna` files in a folder into a single package unit.
3. Implement `internal/` package import shielding in semantic analysis.
4. Verify with multi-file package test cases.

### Phase 5: Universal Streaming & Cancellation (Plan 05)
1. Define `IReader` and `IWriter` stream interfaces in `runtime/kyna_host/include/kyna/host/runtime_capabilities.hpp`.
2. Implement `FileStreamReader` and `FileStreamWriter` under `runtime/kyna_host/src/capabilities/`.
3. Introduce `Context` cancellation tokens across `Analyzer::analyze` and runtime evaluation.
4. Expose stream APIs to standard library.

### Phase 6: Runtime Engine Unification & Security Hardening (Plan 06)
1. Implement `NativeRegistry` in `runtime/kyna_vm/src/execution/native_registry.cpp` to unify native function dispatch between tree-walk and bytecode engines.
2. Remove the mock interceptor from `runtime/kyna_host/src/capabilities/curl_network.cpp`.
3. Replace insecure `std::system()` in `runtime/kyna_host/src/capabilities/local_process.cpp` and `tools/kyna_cli/src/commands/project_dependencies.cpp` with safe argument vector spawning via `posix_spawn` / `execv`.
4. Verify that tree-walk and bytecode produce identical execution output.

### Phase 7: Stdlib Expansion & High-Performance Sort (Plan 07)
1. Replace $O(n^2)$ `bubbleSort` in `library/core/src/catalog/collections_library.cpp` with $O(n \log n)$ introsort.
2. Add high-precision monotonic time (`timeNow`, `timeSleep`) strictly adhering to the 4-point registration rule.
3. Add cryptographic hashing (`cryptoSha256`) following the 4-point registration rule.
4. Run `python3 tests/tooling/verify_language_examples.py ./build-debug/bin/ky .` to verify 100% builtin coverage.

---

## 4. CODE STYLE & PATTERNS TO ENFORCE

- **Modern C++**: Use C++20 features where appropriate (`std::string_view`, `std::optional`, `std::variant`, `std::span`).
- **Zero Raw Pointers without Ownership**: Use `std::unique_ptr` for exclusive ownership, interned raw pointers (`const Type*`) for immutable singletons.
- **RAII Everything**: Use scope guards instead of manual push/pop operations.
- **Error Values Over Exceptions**: Avoid throwing C++ exceptions on normal semantic or lexical errors; collect `Diagnostic` values into a diagnostic sink.
- **Explicit Include Paths**: Public headers use `#include <kyna/...>`; internal private headers use relative paths.

Begin by confirming which Phase you are ready to execute.
```
