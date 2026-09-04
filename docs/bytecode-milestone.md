# Bytecode consolidation milestone

This is an implementation checkpoint, not a completed VM-only release. The
language remains compatible with existing programs through pre-execution engine
selection where lowering is incomplete. Once VM execution starts, no diagnostic
can restart the program in another engine.

## Current-source baseline

Debug and Release were configured and built with their CMake presets. The first
Debug CTest run passed 44 of 45 tests. Its architecture failure came from generated
`.kyc` export caches containing absolute checkout paths; these generated files
are now excluded from source naming checks. No source naming rule was removed.
The initial Release suite passed all 45 tests after that verifier correction.

## Capability inventory

“VM” means lowering and execution are exercised by existing or added tests; it
does not imply every combination of features has been exhaustively verified.

| Capability | Front end | HIR/MIR/bytecode | Execution and remaining work |
| --- | --- | --- | --- |
| Literals, arithmetic, local bindings, array/object indexing | Tested | Tested | VM; negative bounds and arithmetic diagnostics covered |
| Branches, loops, labeled control flow, literal match | Tested | Tested | VM; existing control-flow examples |
| Functions, recursive calls, lexical closures | Tested | Tested | VM; native callbacks share frames, captures, and heap |
| Global collection functions | Tested | Tested | VM `map`, `filter`, `reduce`, `find`, `any`, `all`; optional index, nested calls, exceptions, and GC roots |
| Sorting | Tested | Tested | Copy-returning VM sort with optional should-swap callback; arbitrary collection methods still require audit |
| Dynamic call and timing | Tested | Tested | VM `call` and `measure` use the same native callback interface |
| Explicit collection and heap statistics | Tested | Tested | VM roots include native temporaries and active callback frames |
| Exceptions | Tested | Tested | VM catches callback failures at native call sites and preserves uncaught callback locations/call stacks |
| Classes and inherited methods | Tested baseline | Existing lowering | Constructors, static fields, field initializers, and all combinations need a dedicated parity audit |
| Top-level variables captured by functions | Accepted | Incomplete | Still selects tree walk before execution |
| Module initialization, namespaces, imports/exports | Tested baseline | Incomplete graph lowering | Multi-module execution remains tree walk |
| Native object methods and API store | Accepted | Partial | API-store methods and full native-object dispatch remain pending |
| Files, processes, text, JSON, document formats, HTTP client | Tested baseline | Existing direct-native support | VM; effects-once failure regression uses injected adapters |
| Database and HTTP server orchestration | Tested baseline | Incomplete | Existing compatibility path retained |
| REPL state across submissions | Tested | No persistent VM session | Existing interpreter retained; replaying prior submissions is not an acceptable migration |
| Await | Accepted | Synchronous lowering | Existing synchronous semantics retained |

Native callback re-entry is bounded to 64 native levels to protect the host
stack; ordinary language calls retain the existing 4096-frame limit. Native
adapters must not retain the callback interface after invocation. They must root
native-held managed values across calls that may collect. Collection callback
iteration observes the current input length. Removing the current element from
inside a successful filter/find predicate produces `KRT2104`, rather than an
out-of-bounds native read.

## Public-interface migration inventory

| Interface | Current contract | Migration still required |
| --- | --- | --- |
| `LanguageSession` | Existing check/run/source/inspection entry points retained | Persistent VM state, module compilation, and server execution |
| `TreeWalkInterpreter` and `Interpreter` | Public syntax-driven execution and environment access | Cannot delete until equivalent adapters or an explicit breaking contract exist |
| `RuntimeValue` / `Value`, `Function`, `Class`, `Environment` | Expose syntax-owned declarations and interpreter state | Separate stable value handles from compatibility internals |
| `BytecodeNativeAdapter` | Existing `invoke` still works; optional `invokeWithCallbacks` provides VM re-entry | Future async/native object interfaces remain separate work |
| `LanguageResult` | Adds optional `metrics`; existing fields retained | Source-compatible extension; no binary ABI promise |
| CLI | Adds `--metrics-file`; `kyna` remains the supported 1.x alias | No alias removal in this milestone |

## Performance evidence

Enable `LanguageSessionOptions.collectMetrics` to collect monotonic phase times.
The CLI writes `kyna.metrics/v1` JSON to `--metrics-file PATH`, separately from
program stdout and diagnostics. Source runs split lexing, parsing, checking,
HIR, MIR, bytecode, native setup, and VM execution. File-based loading currently
combines `load_lex_parse` and `resolve_check`, reflecting the existing module
interfaces. VM timing includes validation, heap setup, and collection.

The benchmark harness defaults to Release, two warmups, and ten repetitions.
`--json-output` records raw wall samples, per-process RSS where available, binary
size/hash, source hashes, commit/dirty state, host, compiler, and build type.
`--phase-metrics` additionally records each run's phase output. A missing RSS
measurement is null, never zero. Kyna-only workloads check repeatability; paired
C++ workloads additionally compare output. These are local discovery reports:
background builds and uncontrolled host load mean they are not release budgets.

Regression gates remain pending until runner equivalence, baseline comparison,
and VM-only parity are established. Native backend, source generics, threading,
generational collection, and broader 1.0 library/tooling work remain pending.

## Checkpoint verification and reports

The expanded full suite passed 46/46 tests in Debug and Release. The final
callback/metrics checks also passed after the last SDK instrumentation adjustment.
ASan/UBSan passed the phase-metrics, language-session, bytecode, and executable
example suites, including forced GC in nested callbacks and sort callbacks.
Repository architecture and VS Code extension verification passed.

Published local reports:

- [Initial Release baseline](performance/bytecode-baseline.json).
- [Implementation checkpoint](performance/bytecode-checkpoint.json).
- [Compiler and execution phases](performance/bytecode-phases.json).
- [Repeated checks, including modules](performance/bytecode-checking.json).

The Release executable increased from 7,112,512 to 7,180,608 bytes (about 1%).
Median end-to-end collection latency was 15.62 ms initially and 15.16 ms at the
checkpoint; primes measured 77.46 ms and 72.82 ms respectively. These small local
differences are not a demonstrated speedup: startup/launcher costs and host noise
are significant. The multi-module workload was added after the baseline and has
no before/after comparison. Phase reports explicitly identify its remaining
tree-walk execution.
