# 05. Standard Library Gap Analysis & Functionality Catalog

## 1. Inventory of Go's Standard Library (`/Users/ahmedmansour/Documents/go/src`)

The Go standard library is widely celebrated as the gold standard of language standard libraries: focused, orthogonal, robust, and batteries-included without bloat. Below is the complete functional catalog of Go's packages:

```text
src/
├── I/O & Streams:          io, bufio, path, path/filepath, archive/tar, archive/zip, compress/gzip, compress/zlib
├── Concurrency:            sync, sync/atomic, context, runtime
├── Networking:             net, net/http, net/url, net/mail, net/rpc
├── Cryptography:           crypto, crypto/aes, crypto/cipher, crypto/sha256, crypto/tls, crypto/rand, crypto/rsa
├── Operating System:       os, os/exec, os/signal, os/user
├── Text & Strings:         fmt, strings, bytes, strconv, unicode, regexp, text/template, html/template
├── Serialization:          encoding/json, encoding/xml, encoding/csv, encoding/binary, encoding/base64, encoding/hex
├── Time & Clocks:          time
├── Math:                   math, math/rand/v2, math/big, math/bits
├── Collections & Iter:     slices, maps, iter, cmp, container/list, container/heap
├── Diagnostics & Tooling:  log, log/slog, testing, flag, reflect
└── Database:               database/sql, database/sql/driver
```

---

## 2. Current Kyna Standard Library Audit

Kyna currently organizes its standard library across 5 CMake modules under `library/`:
- `library/core`: basic I/O, console logging, process execution, simple `httpGet`/`fetch`, JSON parsing, in-memory API store.
- `library/collections`: basic array functional helpers (`filter`, `map`, `reduce`, `sort`).
- `library/database`: PostgreSQL wrapper (`db.query`, `db.execute`) via libpq.
- `library/formats`: TOML (toml++) and XML (pugixml) codecs.
- `library/text`: UTF-8 string functions (`textContains`, `textSlice`, `textReplace`, `textSplit`).

---

## 3. Comprehensive Gap Matrix: Go vs. Kyna

| Domain | Go Standard Library Package | Status in Kyna | Architectural Gap & Impact on Scalability | Priority |
| :--- | :--- | :--- | :--- | :--- |
| **Streaming I/O** | `io`, `bufio` | ❌ Missing | Kyna only supports reading/writing whole files at once into memory (`readFile`/`writeFile`). Cannot stream gigabyte files, parse chunked HTTP streams, or process pipes. | **CRITICAL** |
| **Time & Clocks** | `time` | ⚠️ Rudimentary | Kyna has only `sleep(ms)`. No `Time` struct, no `Duration`, no monotonic clock support, no timers, no tickers, and no timezone parsing. | **CRITICAL** |
| **Concurrency & Locks** | `sync`, `sync/atomic` | ❌ Missing | No mutexes, read-write locks, wait groups, or atomic operations. Multi-threaded workloads cannot safely synchronize state. | **CRITICAL** |
| **Structured Logging** | `log/slog` | ⚠️ Primitive | Kyna only has unstructured `print()`, `log()`, and `logColor()`. Lacks structured key-value logging (`slog.info("user login", "id", 42)`) with JSON handlers. | **HIGH** |
| **Cryptography** | `crypto`, `crypto/sha256`, `crypto/rand` | ❌ Missing | No cryptographic hashing (SHA-256, SHA-512), no AES encryption, no cryptographically secure random bytes generator (`crypto/rand`). | **HIGH** |
| **HTTP Server & Client** | `net/http` | ⚠️ Client Only | Kyna has a basic libcurl HTTP client (`fetch`). It has **no HTTP server** capabilities (`http.ListenAndServe`) to build web APIs, microservices, or webhooks. | **HIGH** |
| **Request Context** | `context` | ❌ Missing | No mechanism to pass deadlines, timeouts, or cancellation signals down call chains across network or database requests. | **HIGH** |
| **Filepath Utilities** | `path/filepath` | ⚠️ Partial | Kyna relies on raw string concatenation for filepaths. Lacks cross-platform path joining, extension extraction, and directory tree walking (`WalkDir`). | **MEDIUM** |
| **Data Encodings** | `encoding/csv`, `encoding/base64`, `encoding/hex` | ❌ Missing | Kyna has JSON, TOML, and XML, but lacks Base64, Hex encoding, and CSV parsing. | **MEDIUM** |
| **Generic Collections** | `slices`, `maps`, `iter` | ⚠️ Partial | Array helpers exist in `collections`, but lack binary search, cloning, generic map keys/values extraction, and lazy iterator pipelines (`iter.Seq`). | **MEDIUM** |
| **Compression & Archives**| `compress/gzip`, `archive/zip` | ❌ Missing | Cannot read or write `.zip` archives or `.tar.gz` compressed streams. | **LOW** |
| **Introspection** | `reflect` | ❌ Missing | Only basic `typeOf(val)` string name. Cannot dynamically inspect struct fields, tags, or dynamically invoke methods. | **LOW** |

---

## 4. Detailed Specification of Top 5 Primitives to Implement in Kyna

### Primitive 1: Universal Streaming I/O (`kyna.io`)
Model universal streaming contracts after Go's `io.Reader` and `io.Writer`:

```kyna
intf Reader {
    read(buffer: ByteArray): int; # Returns bytes read, or -1 on EOF
}

intf Writer {
    write(buffer: ByteArray): int; # Returns bytes written
}

intf Closer {
    close(): void;
}

# Universal pipeline operator
fn copy(dst: Writer, src: Reader): int { ... }
```
- Adapts files, network sockets, HTTP response bodies, and in-memory byte buffers to the same streaming seam.
- Allows reading arbitrarily large files in bounded chunks (e.g. 64KB) without running out of memory.

### Primitive 2: High-Precision Time (`kyna.time`)
Model after Go's `time.Time` and `time.Duration`:

```kyna
class Duration {
    public nanoseconds: int;
    public fn milliseconds(): float { return self.nanoseconds / 1000000.0; }
    public fn seconds(): float { return self.nanoseconds / 1000000000.0; }
}

class Time {
    # Stores monotonic nanoseconds + UTC wall clock
    public fn now(): Time { ... }
    public fn add(d: Duration): Time { ... }
    public fn sub(other: Time): Duration { ... }
    public fn before(other: Time): bool { ... }
    public fn after(other: Time): bool { ... }
    public fn format(layout: str): str { ... }
}

fn sleep(d: Duration): void { ... }
```

### Primitive 3: Concurrency & Synchronization (`kyna.sync`)
Introduce thread-safe synchronization primitives wrapping host OS mutexes:

```kyna
class Mutex {
    public fn lock(): void;
    public fn unlock(): void;
}

class WaitGroup {
    public fn add(delta: int): void;
    public fn done(): void;
    public fn wait(): void;
}
```

### Primitive 4: Cryptography & Hashing (`kyna.crypto`)
Provide cryptographic utilities implemented via host-linked OpenSSL or CommonCrypto:

```kyna
fn sha256(data: str | ByteArray): str;
fn sha512(data: str | ByteArray): str;
fn hmacSha256(key: str | ByteArray, data: str | ByteArray): str;
fn randomBytes(count: int): ByteArray;
```

### Primitive 5: Structured Logging (`kyna.log` / `slog`)
Upgrade Kyna's logging to structured JSON output for production environments:

```kyna
slog.info("Processing order", {
    "order_id": 1042,
    "user": "ahmed",
    "amount": 250.75
});
# Outputs: {"time":"2026-09-02T20:45:00Z","level":"INFO","msg":"Processing order","order_id":1042,"user":"ahmed","amount":250.75}
```
