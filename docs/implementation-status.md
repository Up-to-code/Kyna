# Implementation status

Kyna 1.0 is an active compatibility-reset implementation. This table distinguishes working, tested behavior from remaining release gates; it is not a promise that every roadmap item has landed.

| Area | Current status |
|---|---|
| Repository architecture | domain-owned implementation folders and explicit source lists enforced; `spec/architecture/modules.json` is the authoritative internal target graph and verification rejects missing/stale targets, link drift, unknown dependencies, and cycles |
| Source spans, lexer, parser recovery | implemented and tested |
| Name/type analysis and class contracts | implemented baseline; standard-library symbols and basic call contracts are centralized in `kyna_symbols`; interfaces support `extends` inheritance, generic type parameters substituted at `implements`/`extends` sites, optional properties, call signatures, and index signatures |
| Namespace modules and cycle diagnostics | implemented baseline; additionally supports JavaScript-style `import { a, b } from`, `import default from`, `import * as ns from`, `export default`, and `export { list }`, plus `.ky`/`.d.ky`/`.ky.d` file-extension aliases for program and ambient type-definition modules |
| HIR | stable typed IDs, resolved locals/functions/native calls, lexical capture analysis, nested-function lifting, array/object/index/member expressions, typed exceptions, renderer, and lowering for literals, bindings, calls, operators, assignment, blocks, conditionals, exhaustive matches, loops, and labeled control flow |
| MIR | explicit temporaries/basic blocks/terminators/calls, exception regions, closure/capture operations, native/member/index/collection operations, verifier, short-circuit boolean lowering, cleanup routing, and labeled `break`/`continue` lowering |
| Register bytecode model, validator, disassembler | bytecode v7 with class metadata, instance creation, exact parent-method binding, exception tables, arrays/objects/indexing, typed Error member reads, native-call slots, function values, heap closures, capture descriptors, direct/indirect calls, parameter counts, call arguments, and MIR lowering |
| Bytecode VM | explicit iterative call frames, cross-frame exception unwinding, catchable runtime failures, first-class functions, mutable/transitive lexical captures, injected native adapter, checked integer arithmetic, stable runtime codes, source call stacks, and validated execution for the lowered subset |
| Tree-walk compatibility engine | retained temporarily for constructs not yet lowered |
| Structured text/JSON diagnostics | `kyna.diagnostic/v1` implemented |
| CLI11 command grammar | primary `ky` plus the `kyna` 1.x alias; projects, route generation, run/check, formatter, dependency locking, serve/dev, doctor, self-management, REPL, and compiler inspection implemented |
| FTXUI terminal experience | interactive template selector with arrows or j/k, Kyna-purple presentation, terminal-aware diagnostics and execution animation; broader phase progress reporting remains open |
| Managed heap | iterative tracing, temporary roots, heap-owned VM closures/capture cells, VM allocation safepoints, cycle reclamation, and per-execution statistics implemented; generational collection remains open |
| Text, JSON, TOML, XML, collections, filesystem, process | Unicode text, JSON, native toml++/pugixml document conversion, collection operations, filesystem primitives, file editing, process execution/environment, and clocks execute through VM adapters; namespace calls lower to canonical VM natives; callback collection algorithms remain open |
| HTTP/HTTPS | libcurl client plus an injected Boost.Asio/Beast server capability; router methods, middleware, parameters/query/headers/body, JSON/text helpers, loopback-safe defaults, body limits, timeouts, overrides, and graceful interruption are implemented |
| PostgreSQL | parameterized libpq query adapter, typed scalar/null mapping, SQLSTATE diagnostics; pooling/transactions/ORM remain open |
| Async/await and event loop | not implemented |
| Full HIR/MIR coverage | exceptions, collection literals, indexing/mutation, core text/JSON, fetch responses, and direct production natives are implemented; classes, modules, callback collection algorithms, and async operations remain open |
| Incremental compilation and native backend | researched and architecturally staged; query engine, persistent cache, native backend, object emission, and linker integration are not implemented |
| Raw sockets, DAP, LSP | not implemented |
| Formatter and package manager | comment/string-preserving formatter with stdin/check/recursive modes and Git/path dependency locking implemented; a documentation generator and central registry are intentionally absent |
| VS Code extension | 1.0.12: `.kyna` plus `kyna.toml`, compact Project and method-grouped Routes views, colored route icons, homepage/static/slug route wizard, click-to-open handlers and live endpoints, safe save-before-generation behavior, clear manifest CodeLens actions, stale Run/Dev cleanup, manifest-authoritative server settings, Express-style HTTP completions/snippets, symbols, definitions, hover, live diagnostics, CLI-backed formatting, `ky`/`kyna` discovery, task-backed terminals, compiler inspection, theme-aware assets, and official VSCE packaging |
| Cross-platform distribution | zero-clone macOS/Linux/Windows archives, per-user shell/PowerShell installers, published `@kyna-language/cli` npm installer, self-management, GHCR image, Dev Container, signing/notarization gates, checksums, and provenance workflows implemented; clean hosted-runner release validation completed for `v1.0.0-preview.2`; other package-manager manifests and multi-version toolchain management are deferred |

Kyna 1.0 must not be tagged until every mandatory gate in [ROADMAP.md](../ROADMAP.md) and [release-policy.md](release-policy.md) is complete.
