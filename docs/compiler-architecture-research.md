# Compiler architecture research

This record converts mechanisms from production compilers into Kyna decisions. It is intentionally selective: a useful mechanism must solve a measured Kyna problem and fit Kyna's ownership model. Popularity alone is not a reason to copy code or add a dependency.

## Evidence and decisions

| System | Observed mechanism | Kyna decision |
| --- | --- | --- |
| Go | The compiler is organized as explicit front-end, middle-end, and back-end phases; packages and commands have clear ownership. | Adopt predictable source anatomy, domain-owned folders, small interfaces, and end-to-end capability registration. Keep Kyna's CMake modules instead of imitating Go's physical tree. |
| Rust | `rustc` uses explicit representations and demand-driven, memoized queries; code generation is separated behind backend interfaces. | Design an incremental query layer above source identity and below the embedding session. Make query keys, inputs, results, invalidation, diagnostics, and persistence explicit before implementation. Keep the current batch pipeline until profiling shows a useful first query boundary. |
| LLVM | The new pass manager makes analysis dependencies, preservation, invalidation, and pass nesting explicit. | Introduce a pass interface only when Kyna has multiple MIR transforms. A pass must declare what it consumes, changes, preserves, and verifies. Do not build a pass framework around a single transform. |
| MLIR | Dialect conversion separates legality, rewrite patterns, and type conversion; interfaces expose behavior without hard-coding every operation type. | Apply the concepts to HIR→MIR evolution: define legal output invariants and verification at the conversion seam. Do not embed MLIR until multi-level or heterogeneous lowering creates enough leverage to justify its size. |
| Cranelift | The reusable code generator is separated from object/JIT adapters and prioritizes compilation speed. | Keep a future native backend behind `kyna_codegen`. Evaluate Cranelift as a fast-development backend only through a C-compatible or process boundary with a pinned version. Kyna's C++ implementation makes direct Rust integration and API stability real costs. |
| Clang | A thin driver builds explicit compilation actions while reusable compiler behavior lives in libraries; argument parsing avoids repeated allocation and parsing. | Keep `kyna_cli` as a command adapter over `kyna_embedding`. Add an invocation/action model before native compile/link commands multiply. The embedding interface remains usable without terminal or process behavior. |
| GCC | Optimization and code generation are sequenced as named passes over progressively lower representations. | Preserve HIR, MIR, and executable IR as distinct contracts. Prefer inspectable named passes over hidden optimizer work in visitors. |
| Swift | Compiler performance work distinguishes front-end work, IR passes, and back-end work and relies on measurement. | Add phase timings and representative compilation workloads before choosing an optimizer or allocator. Track compile latency, execution latency, peak memory, output size, and diagnostic stability separately. |
| Kani | The driver, Rust compiler integration, and model-checking backend are separate stages; StableMIR is being explored as a less compiler-private seam; major designs use RFCs. | Keep verification tooling outside normal compilation. Use a versioned Kyna MIR export if a verifier is added. Require a small decision record for backend, object format, ABI, package-registry, or persistent-cache choices. See [the Kani study](githubs/kani.md). |
| CPack and rustup | CPack produces platform packages; rustup manages selected toolchains, profiles, updates, and proxies. | Continue release archives and verified per-user installers now. Add package-manager manifests and a channel/toolchain manager only when multiple supported compiler versions make that depth worthwhile. |

Primary references: [Go compiler architecture](https://go.dev/src/cmd/compile/README), [rustc overview](https://rustc-dev-guide.rust-lang.org/overview.html), [rustc query evaluation](https://rustc-dev-guide.rust-lang.org/queries/query-evaluation-model-in-detail.html), [rustc backends](https://rustc-dev-guide.rust-lang.org/part-5-intro.html), [LLVM new pass manager](https://llvm.org/docs/NewPassManager.html), [MLIR dialect conversion](https://mlir.llvm.org/docs/DialectConversion/), [MLIR interfaces](https://mlir.llvm.org/docs/Interfaces/), [Cranelift documentation](https://github.com/bytecodealliance/wasmtime/blob/main/cranelift/docs/index.md), [Clang driver internals](https://clang.llvm.org/docs/DriverInternals.html), [GCC passes](https://gcc.gnu.org/onlinedocs/gccint/Passes.html), [Swift compiler performance](https://github.com/swiftlang/swift/blob/main/docs/CompilerPerformance.md), [CPack](https://cmake.org/cmake/help/latest/manual/cpack.1.html), and [rustup concepts](https://rust-lang.github.io/rustup/concepts/toolchains.html).

## Target compiler shape

```text
CLI/editor/build tool
        │
        ▼
embedding session ───── invocation/action plan
        │
        ▼
source → lexing → syntax → resolution → type checking
                                      │
                                      ▼
                             verified HIR
                                      │
                                      ▼
                         verified MIR + pass pipeline
                              │                 │
                              ▼                 ▼
                    bytecode backend     native backend
                              │                 │
                              ▼                 ▼
                         VM/runtime       object + linker
                              └────────┬────────┘
                                       ▼
                           host capability interfaces
```

The deep interface is the embedding session: clients ask for check, inspect, run, or eventually build operations without coordinating compiler phases themselves. Backends consume verified MIR and never depend on CLI state. Runtime and compiler code reach the operating system through host adapters.

## Performance plan

Performance work follows evidence, not dependency count:

1. Add per-phase wall time, peak resident memory, allocation counts where available, cache hits, emitted bytecode size, and runtime statistics to structured profiling output.
2. Extend benchmarks with small-startup, large-module, multi-module, allocation-heavy, call-heavy, Unicode, diagnostics-heavy, and repeated-check workloads.
3. Establish release baselines for cold check, warm check, cold run, steady-state execution, peak memory, and binary/archive size.
4. Optimize representation and algorithms first: intern identifiers, avoid repeated parsing, use stable IDs, reserve known capacities, reduce tree walks, and keep hot data compact.
5. Prototype one backend or allocator at a time behind an adapter. Compare identical revisions and workloads on every supported platform.
6. Adopt only when the improvement is material, correctness tests remain green, maintenance and distribution costs are acceptable, and rollback remains simple.

An incremental query engine is the highest-leverage architectural performance feature for editor and repeated-check workloads. It should begin with immutable query results and dependency tracking for module load, parse, exports, and typecheck. Persistent caching comes later because cache compatibility, compiler versioning, target configuration, and corruption recovery are separate hard problems.

## Library evaluation

| Candidate | Potential value | Cost or risk | Decision |
| --- | --- | --- | --- |
| LLVM | Mature optimization, object emission, debug information, targets, JIT infrastructure | Large binaries/builds, distribution and version policy, complex interface | Prototype later behind a native-backend seam; do not add globally now. |
| Cranelift | Fast code generation with JIT and object-module adapters | Rust/C++ boundary, target coverage and integration stability must be proven | Benchmark against LLVM on Kyna MIR after the backend contract exists. |
| MLIR | Reusable multi-level IR, dialect conversion, rewrite infrastructure | Considerable conceptual and binary weight for the current pipeline | Defer until Kyna genuinely needs several dialects or heterogeneous targets. |
| mimalloc | Drop-in allocator with useful statistics and good general-purpose performance goals | Workload-dependent benefit; can hide ownership problems | Optional benchmark experiment for compiler/VM processes, never semantic infrastructure. |
| xxHash | Very fast non-cryptographic hashes | Collision handling is still required; not for trust decisions | Candidate for content/cache fingerprints after the query model exists. Keep SHA-256 for release integrity. |
| CLI11 | Typed C++ command grammar already integrated | CLI policy can leak into compiler code | Retain in `kyna_cli`; keep all compiler behavior below the command seam. |
| FTXUI | Interactive terminal presentation already integrated | Startup/binary cost and nondeterministic presentation in pipes | Retain only for TTY views; deterministic text/JSON stays authoritative. |
| libcurl | Portable HTTP/HTTPS client already integrated | Global/runtime policy and TLS packaging require care | Retain behind `kyna_host`; scripts never receive the library interface directly. |
| utf8proc | Unicode processing already integrated | Unicode version upgrades can change behavior | Retain behind `library/text` with conformance tests and version review. |
| toml++ / pugixml / libpq | Mature format/database implementations already integrated | Expands release surface | Retain behind codecs/host adapters; expose Kyna-owned values and errors. |

The direct `catch2` and `nlohmann-json` vcpkg dependencies were removed because repository source does not use them. Reducing unused dependencies improves clean setup and supply-chain surface without changing language behavior.

## “Quantum compiler” boundary

For this architecture, “quantum compiler” is treated as an ambition for unusually strong performance and design quality, not as a claim that Kyna compiles quantum circuits. A real quantum target would require a language semantic model for qubits, measurement, target constraints, circuit or quantum IR, simulation/hardware adapters, and correctness tests. That work must enter as a separate backend and standard-library proposal rather than being implied by native-code optimization.

## Adoption order

1. Enforce module ownership and dependency direction. This is active through `spec/architecture/modules.json` and the repository verifier.
2. Finish semantic type, HIR, MIR, and bytecode vertical slices so the compatibility interpreter can be removed.
3. Add structured phase profiling and broaden compiler/runtime benchmarks.
4. Design the incremental query interface and invalidate only from explicit inputs.
5. Define the native-backend interface over verified MIR and run LLVM/Cranelift prototypes.
6. Add object emission, linker/toolchain discovery, ABI documentation, and native debugging support.
7. Expand distribution into package-manager manifests; introduce a toolchain manager only for real multi-version needs.

Each stage has a narrow seam, produces independent value, and can be verified before the next investment.
