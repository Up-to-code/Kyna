# Plan 04: Multi-File Packages & Internal Shielding

> **Goal**: Enable packages to span multiple files in a single directory without forward declarations, and enforce compile-time `internal/` package encapsulation.
> **Inspiration**: Go's package compilation model (`go/build`, `cmd/compile`) and the Go `internal/` boundary rule.

---

## 1. Problem Statement & Root Cause

Currently in Kyna:
1. **Single-File Compilation Unit**: Each compilation unit corresponds to a single `.kyna` source file. To split functionality across multiple files, developers must use explicit `import` statements between every file.
2. **No Compiler-Enforced Internal Privacy**: Any module can import any file from any directory. Libraries cannot have private internal utility packages without risking external consumers depending on unstable internals.

---

## 2. Target Architecture

### 2.1 Multi-File Package Loading
When compiling a package directory:
1. The compiler gathers all `*.kyna` files in that directory.
2. Each file is parsed into an individual AST (`FileAST`).
3. Declarations from all files are merged into a single package scope (`PackageScope`).
4. Type resolution and cycle detection occur across the entire package simultaneously without requiring forward declarations.

```cpp
namespace kyna::loading {

struct PackageSource {
  std::string name;
  std::filesystem::path directory;
  std::vector<SourceFile> files;
};

class PackageLoader {
public:
  std::optional<PackageSource> loadPackageDirectory(const std::filesystem::path& dir);
};

} // namespace kyna::loading
```

### 2.2 Compiler-Enforced `internal/` Boundary Rule
A package located at or beneath a directory named `internal` may only be imported by packages that share the parent of that `internal` directory as an ancestor:

```cpp
// compiler/kyna_typecheck/src/checkers/package_checker.cpp
bool isInternalImportAllowed(const std::filesystem::path& importerPath,
                             const std::filesystem::path& importedPath) {
  auto internalPos = importedPath.string().find("/internal/");
  if (internalPos == std::string::npos) return true; // not an internal package

  auto parentOfInternal = importedPath.string().substr(0, internalPos);
  return importerPath.string().starts_with(parentOfInternal);
}
```

---

## 3. Implementation Steps

- [ ] **Step 1: Extend CLI Command Dispatcher**
  - Update `tools/kyna_cli/src/commands/run_command.cpp` and `check_command.cpp` to accept a directory path (package mode) in addition to single `.kyna` files.
- [ ] **Step 2: Implement `PackageLoader`**
  - Create `compiler/kyna_loading/src/loading/package_loader.cpp` to discover all `.kyna` files in a package folder.
- [ ] **Step 3: Multi-AST Merger in Semantic Analysis**
  - Merge declarations from multiple `FileAST` instances into a unified package root scope before statement checking.
- [ ] **Step 4: Implement `internal/` Path Validator**
  - In `import` statement checking, reject imports from `internal/` paths when the importing module is outside the parent tree.

---

## 4. Verification Plan

1. **Test Cases**:
   - Create test packages with 3+ files sharing types and functions without imports.
   - Create tests asserting that external imports of `internal/` packages fail with diagnostic `KSEM1042: use of internal package not allowed`.
2. **Tooling Tests**:
   - Run `ctest --test-dir build-debug`.
