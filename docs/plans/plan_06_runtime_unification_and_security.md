# Plan 06: Runtime Engine Unification & Security Hardening

> **Goal**: Unify standard library native dispatch between tree-walk and bytecode runtimes to eliminate behavioral divergence, and replace insecure shell string execution with argument vector process spawning.
> **Inspiration**: Go's single compiled runtime model and `os/exec.Command`.

---

## 1. Problem Statement & Root Cause

Currently in Kyna:
1. **Behavioral Divergence between Tree-Walk and Bytecode**:
   - `sort` in tree-walk is an $O(n^2)$ bubble sort ([`collections_library.cpp:L67-L114`](../../library/core/src/catalog/collections_library.cpp#L67-L114)); in bytecode it is `std::stable_sort`.
   - Higher-order methods (`filter`, `map`, `reduce`) fail in bytecode mode ([`collections_invoke.cpp:L221-L226`](../../library/core/src/bytecode/collections_invoke.cpp#L221-L226)).
2. **Insecure Process Spawning (Shell Injection)**:
   - `LocalProcess::run` executes `std::system(command.c_str())` with raw string concatenation ([`local_process.cpp:L12`](../../runtime/kyna_host/src/capabilities/local_process.cpp#L12)).
   - `project_dependencies.cpp` runs raw `git clone` commands using `std::system` string concatenation ([`project_dependencies.cpp:L103-L122`](../../tools/kyna_cli/src/commands/project_dependencies.cpp#L103-L122)).
3. **Test Fixtures in Production Code**:
   - `CurlNetwork::send` hardcodes a mock interceptor for `"mock://kyna/users"` ([`curl_network.cpp:L79-L83`](../../runtime/kyna_host/src/capabilities/curl_network.cpp#L79-L83)).

---

## 2. Target Architecture

### 2.1 Unified Native Binding Dispatcher
Instead of duplicating function bodies between `library/core/src/catalog/*.cpp` and `library/core/src/bytecode/*_invoke.cpp`, implement a shared C++ native registry:

```cpp
namespace kyna::runtime {

using NativeFunction = std::function<Value(Span<Value> args, ExecutionContext& ctx)>;

class NativeRegistry {
public:
  static NativeRegistry& instance();
  void registerNative(std::string_view name, NativeFunction fn);
  std::optional<Value> invoke(std::string_view name, Span<Value> args, ExecutionContext& ctx);
};

} // namespace kyna::runtime
```

Both the bytecode VM and the tree-walk interpreter invoke natives through `NativeRegistry`.

### 2.2 Secure Process Spawning (`ProcessPort`)
```cpp
namespace kyna::host {

struct ProcessConfig {
  std::string program;
  std::vector<std::string> args;
  std::filesystem::path workingDir;
  std::unordered_map<std::string, std::string> env;
};

struct ProcessResult {
  int exitCode;
  std::string stdoutText;
  std::string stderrText;
};

class ProcessPort {
public:
  virtual ~ProcessPort() = default;
  virtual ProcessResult spawn(const ProcessConfig& config) = 0;
};

} // namespace kyna::host
```
Implemented via `posix_spawn` / `execv` on POSIX and `CreateProcessW` on Windows — **zero shell interpolation**.

---

## 3. Implementation Steps

- [ ] **Step 1: Create Shared `NativeRegistry`**
  - Implement under `runtime/kyna_vm/src/execution/native_registry.cpp`.
- [ ] **Step 2: Migrate Core Standard Library Builtins**
  - Move implementations of `print`, `len`, `push`, `pop`, `sort` into the unified registry.
- [ ] **Step 3: Remove Mock Interceptor from `CurlNetwork`**
  - Delete mock URL check from `curl_network.cpp`. Use mock network port in tests.
- [ ] **Step 4: Refactor `LocalProcess` to Use Vector Spawning**
  - Replace `std::system` with safe argument vector execution.
- [ ] **Step 5: Refactor CLI Dependency Manager**
  - Update `project_dependencies.cpp` to call `ProcessPort::spawn` with `{"git", "clone", ...}`.

---

## 4. Verification Plan

1. **Test Native Parity**:
   - Run identical scripts through both `--tree-walk` and `--bytecode` and verify identical outputs.
2. **Security & Process Tests**:
   - Test passing arguments containing spaces, quotes, and shell metacharacters (`;&|`) without shell injection.
