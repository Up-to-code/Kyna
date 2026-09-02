# 02. File Anatomy, Coding Rules & Conventions

## 1. Anatomy of an Idiomatic Go Source File

Every file in the Go source tree follows a strict, predictable top-to-bottom layout. When you open any file in `/Users/ahmedmansour/Documents/go/src/`, you see this uniform structure:

```go
// 1. COPYRIGHT & LICENSE HEADER
// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// 2. PACKAGE DOCUMENTATION (if single-file package or primary file)
// Package buffer implements a variable-sized buffer of bytes with Read and Write methods.
package buffer

// 3. IMPORT SECTION (Grouped: stdlib first, empty line, external/internal dependencies)
import (
	"errors"
	"io"
	"unicode/utf8"
)

// 4. CONSTANTS & ENUMS (Grouped with const (...))
const (
	minReadInitialAlloc = 64
	maxInt              = int(^uint(0) >> 1)
)

// 5. SENTINEL ERRORS (Pre-allocated static error instances)
var (
	ErrTooLarge = errors.New("buffer: buffer too large")
	ErrNegativeCount = errors.New("buffer: negative count")
)

// 6. TYPE DEFINITIONS & STRUCTS
// A Buffer is a variable-sized buffer of bytes with Read and Write methods.
// The zero value for Buffer is an empty buffer ready to use.
type Buffer struct {
	buf      []byte // contents are the bytes buf[off : len(buf)]
	off      int    // read at &buf[off], write at &buf[len(buf)]
	lastRead readOp // last read operation, so that Unread* can work correctly
}

// 7. CONSTRUCTOR FUNCTIONS (if zero-value is insufficient or auxiliary options needed)
// NewBuffer creates and initializes a new Buffer using buf as its initial contents.
func NewBuffer(buf []byte) *Buffer {
	return &Buffer{buf: buf}
}

// 8. PUBLIC METHODS (Ordered by conceptual importance)
// Bytes returns a slice of the unread portion of the buffer.
func (b *Buffer) Bytes() []byte {
	return b.buf[b.off:]
}

// Write appends the contents of p to the buffer, growing the buffer as needed.
func (b *Buffer) Write(p []byte) (n int, err error) {
	b.lastRead = opInvalid
	m, ok := b.tryGrowByReslice(len(p))
	if !ok {
		m = b.grow(len(p))
	}
	return copy(b.buf[m:], p), nil
}

// 9. PRIVATE HELPER METHODS & FUNCTIONS
func (b *Buffer) grow(n int) int {
	// internal allocation logic
	return 0
}
```

---

## 2. Visibility by Casing: Eliminating Keyword Clutter

Go rejects access modifier keywords (`public`, `private`, `protected`, `internal`). Instead, visibility is determined solely by the **casing of the first Unicode rune** of an identifier:

| Identifier Casing | Visibility Scope | Examples |
| :--- | :--- | :--- |
| **Uppercase (PascalCase)** | **Exported**: Visible across all packages that import this package | `Buffer`, `Reader`, `Read()`, `ErrTooLarge`, `EOF` |
| **Lowercase (camelCase)** | **Unexported**: Visible *only* within the declaring package | `buf`, `off`, `grow()`, `tryGrowByReslice()`, `readOp` |

### Why This Scales Better Than Keywords
1. **Zero Reading Overhead**: You never need to look up a type definition to know if a member is public or private. `b.Bytes()` is immediately known to be public; `b.off` is immediately known to be package-internal.
2. **Zero Syntax Noise**: Eliminates millions of redundant `public` keywords across large codebases.
3. **No Mismatched Accessors**: Avoids trivial getters/setters like `getOff()` and `setOff()`. If a field is public, export it (`Off`); if it requires invariants, keep it lowercase (`off`) and expose only the validating method.

---

## 3. The Zero-Value Usability Rule

In Go, variables declared without an explicit initializer are automatically initialized to their **zero value**:
- Numeric types: `0`
- Booleans: `false`
- Strings: `""` (empty string, length 0, nil pointer)
- Pointers, functions, interfaces, slices, channels, maps: `nil`
- Structs: all fields set to their respective zero values.

### The Architectural Mandate: Make Zero Values Useful
In Go standard library design, structs are explicitly designed so that their **zero value is immediately functional** without requiring a constructor call:

```go
// Example 1: sync.Mutex — ready to lock immediately, no NewMutex() needed
var mu sync.Mutex
mu.Lock()
// critical section
mu.Unlock()

// Example 2: bytes.Buffer — ready to write immediately, no NewBuffer() needed
var buf bytes.Buffer
buf.WriteString("hello")

// Example 3: sync.WaitGroup — ready to use immediately
var wg sync.WaitGroup
wg.Add(1)
```

### Contrast with Languages Requiring Constructors
In languages where objects must be allocated via `new MyClass()`, null pointer dereferences occur frequently whenever an uninitialized field is accessed. In Go, designing for zero-value usability ensures that uninitialized memory remains safe, deterministic, and valid.

---

## 4. Method Receivers: Pointer vs. Value Semantics

Go methods associate a function with a type using a **receiver argument** before the function name:

```go
func (recv ReceiverType) MethodName(param ParamType) ReturnType
```

### The Rules of Receiver Choice

| Receiver Type | When to Use | Architectural Meaning | Example from Go Stdlib |
| :--- | :--- | :--- | :--- |
| **Value Receiver** `(t T)` | • Small immutable values<br>• Basic types or small structs (<= 24 bytes)<br>• When copies should not share state | **Value Semantics**: The method operates on a local copy. The caller's instance cannot be modified. | `time.Time`<br>`func (t Time) Add(...) Time`<br>`func (t Time) After(...) bool` |
| **Pointer Receiver** `(t *T)` | • The method must mutate the receiver's state<br>• Struct contains synchronization primitives (`sync.Mutex`)<br>• Large structs where copying is expensive<br>• Structs representing unique resources | **Reference/Identity Semantics**: The method operates directly on the caller's memory. | `bytes.Buffer`<br>`net.TCPConn`<br>`sync.Mutex`<br>`http.Client` |

### Receiver Consistency Rule
As stated in Go's official style guide:
> If any method of a type requires a pointer receiver, **all methods of that type should take a pointer receiver**, even those that do not modify the receiver. This ensures that the type's method set is uniform and that interface satisfaction remains consistent.

---

## 5. Error Handling: Errors as Values, Not Exceptions

Go explicitly rejects try/catch exception hierarchies for normal control flow. Instead, errors are ordinary values that implement the built-in `error` interface:

```go
type error interface {
	Error() string
}
```

### Key Error Handling Idioms in Go

#### 1. Explicit Inspection (`if err != nil`)
Functions that can fail return an error as their final return value:
```go
data, err := os.ReadFile("config.json")
if err != nil {
	return fmt.Errorf("reading config file: %w", err)
}
```

#### 2. Sentinel Errors vs. Custom Error Types
- **Sentinel Errors** (`var ErrNotFound = errors.New("not found")`): Used when callers only need to test for a specific condition using `errors.Is(err, ErrNotFound)`.
- **Error Structs** (`type PathError struct { Op, Path string, Err error }`): Used when callers need structured metadata about the failure. Callers extract the struct using `errors.As(err, &pathErr)`.

#### 3. Error Wrapping with `%w`
Go 1.13 introduced error wrapping:
```go
return fmt.Errorf("failed to connect to %s: %w", host, err)
```
This preserves the full causal error chain, allowing `errors.Is` and `errors.As` to inspect root causes through arbitrary layers of abstraction.

#### 4. The `defer` Pattern for Resource Cleanup
Resource acquisition is immediately followed by a `defer` cleanup call:
```go
f, err := os.Open(filename)
if err != nil {
	return err
}
defer f.Close() // Guaranteed to execute on any function return path
```

---

## 6. Documentation Conventions: The Godoc Standard

Go documentation is generated directly from source code comments without any special markup language (no `@param`, `@return`, `@throws`).

### The 4 Iron Rules of Godoc

1. **The First Sentence Rule**:
   Every doc comment for an exported symbol **must start with the name of the symbol itself**:
   ```go
   // Good:
   // Reader is the interface that wraps the basic Read method.
   type Reader interface { ... }

   // Bad:
   // This interface provides a read method.
   type Reader interface { ... }
   ```
   This ensures that summary listings (such as CLI help or IDE hover cards) read as grammatically complete, natural English sentences.

2. **Package Comments in `doc.go`**:
   For complex packages, all package-level narrative documentation is placed in a dedicated file named `doc.go` that contains only the package comment and the `package <name>` statement. See `/Users/ahmedmansour/Documents/go/src/net/http/doc.go` or `src/cmd/compile/doc.go`.

3. **Symbol Linking with Brackets**:
   Doc comments reference other types or functions using bracketed links:
   ```go
   // [Reader] wraps the underlying stream. See also [io.Copy].
   ```

4. **Code Examples**:
   Any comment line indented with a tab or four spaces is rendered as a formatted, monospace code block in godoc and IDEs.

---

## 7. Lessons for Kyna Syntax and Style

Kyna currently uses Python-style comments (`#`), standard statement syntax, and traditional class/interface constructs. To scale Kyna codebases, we can adopt these Go idioms:

1. **Adopt Casing as Export Modifier**:
   In Kyna, identifiers starting with an uppercase letter (`Title`, `FetchUser`) could automatically be exported, while lowercase identifiers (`count`, `calculateHash`) remain module-private, eliminating redundant `export` keywords.
2. **First-Class Error Returning Tuple**:
   Introduce ergonomic tuple unpacking for error handling:
   ```kyna
   var (result, err) = readFile("data.txt");
   if (err != null) {
       return error("Failed: " + err.message);
   }
   ```
3. **Adopt `defer` in Kyna**:
   Implement a `defer <expression>;` statement in Kyna that pushes a thunk onto the active execution frame stack and executes in LIFO order upon function exit.
