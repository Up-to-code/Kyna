# Implementation status

Kyna 1.0 is an active compatibility-reset implementation. This table distinguishes working, tested behavior from remaining release gates; it is not a promise that every roadmap item has landed.

| Area | Current status |
|---|---|
| Source spans, lexer, parser recovery | implemented and tested |
| Name/type analysis and class contracts | implemented baseline; standard-library symbols and basic call contracts are centralized in `kyna_symbols` |
| Namespace modules and cycle diagnostics | implemented baseline |
| HIR | stable typed IDs, resolved locals/functions/native calls, lexical capture analysis, nested-function lifting, array/object/index/member expressions, typed exceptions, renderer, and lowering for literals, bindings, calls, operators, assignment, blocks, conditionals, exhaustive matches, loops, and labeled control flow |
| MIR | explicit temporaries/basic blocks/terminators/calls, exception regions, closure/capture operations, native/member/index/collection operations, verifier, short-circuit boolean lowering, cleanup routing, and labeled `break`/`continue` lowering |
| Register bytecode model, validator, disassembler | bytecode v5 with exception tables, arrays/objects/indexing, typed Error member reads, native-call slots, function values, heap closures, capture descriptors, direct/indirect calls, parameter counts, call arguments, and MIR lowering |
| Bytecode VM | explicit iterative call frames, cross-frame exception unwinding, catchable runtime failures, first-class functions, mutable/transitive lexical captures, injected native adapter, checked integer arithmetic, stable runtime codes, source call stacks, and validated execution for the lowered subset |
| Tree-walk compatibility engine | retained temporarily for constructs not yet lowered |
| Structured text/JSON diagnostics | `kyna.diagnostic/v1` implemented |
| CLI11 command grammar | run/check/repl/tokens/ast/hir/mir/bytecode implemented |
| FTXUI terminal diagnostics | implemented for interactive terminals |
| Managed heap | iterative tracing, temporary roots, heap-owned VM closures/capture cells, VM allocation safepoints, cycle reclamation, and per-execution statistics implemented; generational collection remains open |
| Text, JSON, collections, filesystem, process | Unicode code-point length/slice/search/case operations, JSON parse/stringify, collection literals/indexing/mutation/default sort/unique/push/pop/keys, filesystem primitives, JSON files, process execution/environment, and clocks execute through VM adapters; callback collection algorithms remain open |
| HTTP/HTTPS | linked libcurl adapter, TLS verification, timeouts, bounded retry, typed failure phases, and VM-native `fetch` responses with status/headers/JSON/text |
| PostgreSQL | parameterized libpq query adapter, typed scalar/null mapping, SQLSTATE diagnostics; pooling/transactions/ORM remain open |
| Async/await and event loop | not implemented |
| Full HIR/MIR coverage | exceptions, collection literals, indexing/mutation, core text/JSON, fetch responses, and direct production natives are implemented; classes, modules, callback collection algorithms, and async operations remain open |
| HTTP server, sockets, DAP, LSP | not implemented |
| Formatter, package manager, documentation generator | not implemented |
| VS Code extension | 1.0.1: `.kyna`, comments, completion, imports, symbols, definitions, hover, CodeLens, live diagnostics, Run/Check, compiler inspection, purple assets |
| Cross-platform archives | CI matrix and CPack archives configured; signed native installers remain a release gate |

Kyna 1.0 must not be tagged until every mandatory gate in [ROADMAP.md](../ROADMAP.md) and [release-policy.md](release-policy.md) is complete.
