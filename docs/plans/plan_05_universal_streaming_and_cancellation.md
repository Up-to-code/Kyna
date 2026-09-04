# Plan 05: Universal Streaming & Context Cancellation

> **Goal**: Introduce streaming byte I/O (`io.Reader`/`io.Writer` equivalent) and a cancellation/deadline context into the host runtime capabilities.
> **Inspiration**: Go's `io.Reader`, `io.Writer`, and `context.Context`.

---

## 1. Problem Statement & Root Cause

Currently in `runtime/kyna_host/include/kyna/execution/runtime_capabilities.hpp`:
1. **Whole-Buffer Slurping**:
   `FileSystemPort::read` returns `std::optional<std::string>` and `FileSystemPort::write` accepts `std::string`. Reading a 2GB file requires allocating a 2GB contiguous heap string.
2. **Synchronous Network & HTTP**:
   Network and server requests block OS threads completely. There is no cancellation token or timeout capability passed through compiler or runtime pipelines.

---

## 2. Target Architecture

### 2.1 The Streaming Ports (`IReader` & `IWriter`)
```cpp
namespace kyna::host {

class IReader {
public:
  virtual ~IReader() = default;
  // Reads up to `size` bytes into `buffer`. Returns bytes read (0 = EOF).
  virtual size_t read(uint8_t* buffer, size_t size) = 0;
};

class IWriter {
public:
  virtual ~IWriter() = default;
  // Writes `size` bytes from `buffer`. Returns bytes written.
  virtual size_t write(const uint8_t* buffer, size_t size) = 0;
};

class ICloser {
public:
  virtual ~ICloser() = default;
  virtual void close() = 0;
};

using ReadCloser = std::shared_ptr<IReader>;
using WriteCloser = std::shared_ptr<IWriter>;

} // namespace kyna::host
```

### 2.2 Context Propagation & Cancellation
```cpp
namespace kyna::host {

class Context {
public:
  virtual ~Context() = default;
  virtual bool isCancelled() const = 0;
  virtual std::optional<std::chrono::system_clock::time_point> deadline() const = 0;
  virtual void cancel() = 0;

  static std::shared_ptr<Context> Background();
  static std::pair<std::shared_ptr<Context>, std::function<void()>>
  WithTimeout(const std::shared_ptr<Context>& parent, std::chrono::milliseconds timeout);
};

} // namespace kyna::host
```

---

## 3. Implementation Steps

- [ ] **Step 1: Add `IReader` and `IWriter` to `runtime_capabilities.hpp`**
- [ ] **Step 2: Implement File Stream Adapters**
  - Implement `FileStreamReader` and `FileStreamWriter` under `runtime/kyna_host/src/capabilities/`.
- [ ] **Step 3: Introduce `Context` to Compilation & Evaluation Pipelines**
  - Pass `const Context&` to `Analyzer::analyze` and `Interpreter::eval` so long-running operations can be cancelled safely.
- [ ] **Step 4: Expose Stream APIs to the Standard Library**
  - Expose `openFileStream(path, mode)` returning stream handles to Kyna scripts.

---

## 4. Verification Plan

1. **Unit Tests**:
   - Write stream chunking tests verifying that a 10MB file can be processed in 64KB chunks with constant memory usage.
   - Write cancellation tests verifying that an infinite loop or heavy compilation aborts when the timeout fires.
