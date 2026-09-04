# Architecture

Kyna 1.0 is a collection of deep CMake modules with subsystem-owned headers. A caller sees only the interface of the module it links; implementation headers are not shared through a repository-wide include path.

The active compiler direction is:

```text
source → diagnostics/types → lexing → syntax → parsing
       → module resolution → type checking → HIR → MIR → bytecode → VM
       → standard library → embedding session → CLI/editor
```

`kyna_hir` owns stable typed expression, statement, and local IDs. Its syntax-lowering interface resolves local bindings and translates parser-specific operators into a compiler-owned vocabulary. `kyna_mir` owns temporaries, basic blocks, explicit return/goto/branch terminators, verification, and source mappings. `kyna_bytecode` consumes only verified MIR; it no longer depends on syntax nodes.

The bytecode seam consists of a versioned register instruction model, MIR lowering, validation, disassembly, and source mappings. Bytecode v4 records parameter counts, validated call-site argument tables, closure construction, and local/parent capture descriptors. The VM refuses malformed modules before execution and uses an explicit frame stack for calls and recursion rather than the C++ call stack. Nested functions are lifted in HIR and close over heap-owned mutable cells, including transitive and recursive captures. Language constructs not yet lowered continue through the characterized tree-walk engine; `kyna hir`, `kyna mir`, and `kyna bytecode` report a specific `KHIR1201` migration diagnostic instead of silently changing semantics.

Host effects cross injected interfaces. Production filesystem, process, network, PostgreSQL, and clock adapters live in `runtime/kyna_host`; deterministic adapters exercise the same interfaces in tests. Network and database interfaces return typed requests, results, and failure phases rather than opaque error strings. Standard-library modules translate those host-neutral results into managed Kyna values.

Expected source and runtime problems return structured diagnostics. CLI11 owns argument grammar. FTXUI renders diagnostics only on an interactive terminal; pipes, CI, and the VS Code extension receive deterministic text or `kyna.diagnostic/v1` JSON.

See [source-layout.md](source-layout.md) for module ownership and [cpp-api-migration.md](cpp-api-migration.md) for embedding changes.

## The right architectural view

Kyna adopts proven language-engineering principles rather than copying another language's surface syntax indiscriminately. Go is the primary reference because its compiler, package model, standard library, documentation, and tooling form one coherent system. A useful design from Go or another language enters Kyna only when it preserves Kyna's established semantics, has a clear owner, crosses the compiler through explicit representations, and can be verified end to end.

The architecture is viewed in three directions at once:

```text
                         LANGUAGE CAPABILITY
                                  │
                                  ▼
source → syntax → meaning → typed IR → executable IR → runtime behavior
  │         │         │          │             │              │
  └─────────┴─────────┴──────────┴─────────────┴──────────────┘
                                  │
                                  ▼
                    diagnostics + tests + documentation

public interface ─────── seam ─────── private implementation
       small                                  deep

research evidence → Kyna decision → implementation rule → automated check
```

The horizontal path prevents a feature from existing in only one compiler phase. The vertical seam keeps callers independent of implementation details. The research path turns an external lesson into a Kyna-owned rule instead of leaving it as an essay.

## Architectural principles

### Deep modules

A module owns substantial behavior behind a small interface. Its interface includes not only declarations, but also invariants, ordering requirements, error modes, ownership rules, and performance expectations. The implementation may contain several private helpers, but callers cross one deliberate seam.

A module is deep when removing it would force its complexity back into many callers. A pass-through wrapper that only renames another interface is shallow and should normally be removed. An adapter is justified when behavior actually varies at a seam, such as deterministic and production filesystem adapters implementing the same host interface.

### Local ownership

Every fact has one authoritative owner:

| Fact | Owner |
| --- | --- |
| Token identity and spelling | `kyna_lexing` |
| Source-language node shape | `kyna_syntax` |
| Grammar and recovery | `kyna_parsing` |
| Semantic type identity and assignability | `kyna_types` |
| Names, scopes, contracts, and diagnostics | `kyna_typecheck` |
| Resolved language meaning | `kyna_hir` |
| Explicit control and data flow | `kyna_mir` |
| Executable instruction encoding | `kyna_bytecode` |
| Values, frames, allocation, and execution | `kyna_vm` |
| Operating-system effects | `kyna_host` |
| User-facing reusable behavior | `library/*` |
| Product-level orchestration | `kyna_embedding` and `kyna_cli` |

Duplicating an authoritative fact is a design defect. Derived descriptions may exist for diagnostics or rendering, but they must be generated from or tested against their owner.

### Explicit phase contracts

Each compiler phase consumes only the verified output of the preceding phase. Syntax preserves what the user wrote. Semantic analysis resolves what it means. HIR removes parser-specific ambiguity. MIR makes control flow and evaluation order explicit. Bytecode contains only validated executable operations. The runtime does not reinterpret source-level syntax.

A phase conversion is a real seam and therefore has three parts:

1. A narrow input/output interface.
2. A validator for the produced representation.
3. Focused tests that use the same interface as production callers.

### Value behavior is part of the type

Type architecture records both shape and behavior. Fixed arrays and structs are value aggregates. Kyna arrays currently have dynamic slice-like reference semantics. Maps and channels are reference descriptors. Named types own nominal identity and method sets. Interfaces describe structural method requirements. Aliases preserve the target's identity, while definitions create a new identity.

These distinctions must survive parsing, checking, lowering, and execution. A type name stored as an unexamined string is a migration adapter, not the final semantic representation.

### Methods and interfaces

Methods belong to the named type that defines their identity. A method records a full `SignatureType`: parameter types, result type, receiver type, and variadic behavior. Receiver behavior is explicit because it changes mutation, copying, and interface satisfaction.

Kyna follows Go's method-set lesson while retaining Kyna syntax:

- A value method is available on the value and its pointer/reference form.
- A pointer/reference method is not silently added to the value method set.
- An interface is satisfied structurally by a compatible method set.
- Producers do not need to import or name every consumer interface.
- Small interfaces are preferred at the point of use.
- Interface names describe behavior; implementation names describe what they are.

An `implements` clause may remain useful as an explicit assertion and diagnostic site, but structural compatibility is the semantic rule. Method lookup, interface satisfaction, and invocation must use semantic symbols and signatures rather than formatted strings.

## Manufacturing a language capability

“Implemented” means that a capability works through every applicable layer. Adding an enum member, AST node, semantic type, runtime value, or documentation paragraph alone does not complete a language feature.

Use this sequence for every new type, operator, declaration, control-flow form, method rule, builtin, or runtime facility:

1. **Write the semantic contract.** Define behavior, invariants, zero value, mutability, identity, assignability, failure modes, and observable evaluation order.
2. **Choose the owner.** Place the authoritative representation in one module and identify each downstream consumer.
3. **Design the interface.** Keep the seam small enough that callers do not need to understand the implementation.
4. **Add lexical and syntax representation.** Register tokens and keywords, add the smallest syntax node that preserves source intent, and extend visitors.
5. **Parse with recovery.** Accept the intended grammar, reject malformed forms with stable diagnostics, and resume at a documented synchronization point.
6. **Resolve and check meaning.** Bind names to symbols, construct semantic types, apply identity and assignability rules, and reject invalid programs before lowering.
7. **Lower once per abstraction.** Syntax lowers to HIR, HIR to MIR, and MIR to bytecode. Do not reproduce semantic decisions independently in every phase.
8. **Implement runtime behavior.** Define representation, allocation, equality, indexing/member access, calls, cleanup, and garbage-collector tracing where applicable.
9. **Register integrations.** Update explicit CMake lists, catalogs, renderers, disassemblers, editor grammar, examples, and builtin registries that own part of the capability.
10. **Verify the vertical slice.** Test valid use, invalid use, edge cases, both execution engines while both exist, module boundaries, diagnostics, and representative examples.
11. **Document status precisely.** Distinguish semantic representation, accepted syntax, lowered execution, standard-library exposure, and production readiness.

The feature is complete only when the applicable cells in this matrix are covered:

| Layer | Completion evidence |
| --- | --- |
| Lexing | Token spelling and source span tests |
| Syntax | Node and visitor coverage |
| Parsing | Valid, invalid, and recovery tests |
| Semantic types | Identity, underlying type, assignability, and method-set tests |
| Type checking | Positive and negative program diagnostics |
| HIR | Stable resolved representation and renderer output |
| MIR | Explicit flow, verifier acceptance, and renderer output |
| Bytecode | Opcode validation and disassembly |
| Runtime | Correct values, effects, failures, and memory tracing |
| Tooling | CLI, embedding, editor, and inspection behavior |
| Documentation | Specification, example, and implementation-status entry |

## Canonical file anatomy

Go's source is predictable because declarations appear in a stable conceptual order and comments state contracts. Kyna applies that lesson to C++ without pretending C++ files are Go packages.

Every implementation file follows this order:

```cpp
// Owns one behavior and names that behavior directly.

#include <kyna/module/public_contract.hpp>

#include "private_detail.hpp"

#include <standard_library_header>

namespace kyna::domain {
namespace {

// File-local constants, data, and helpers.

} // namespace

// Public or domain-entry functions, ordered by conceptual importance.

// Private member implementations and secondary helpers.

} // namespace kyna::domain
```

The ordering rules are:

1. Start with a purpose comment stating what the file owns, not when it changed.
2. Include the file's public contract first, then private project headers, then standard-library or third-party headers in stable groups.
3. Put file-local helpers in an unnamed namespace and keep them out of public headers.
4. Present entry functions before secondary implementation details.
5. Keep one responsibility per implementation file and one domain per folder.
6. Split a growing visitor or dispatcher by owned behavior, such as operators, calls, members, control flow, or construction.
7. Keep public declarations under `include/kyna/...`; keep implementation details under the owning module's `src/<domain>/` folder.
8. List every implementation file explicitly in CMake. Source discovery by glob is forbidden.

A public header follows a similarly stable shape: include guard, minimal dependencies, namespace, supporting value types, the main interface, then small free functions. It must not expose a private dependency merely to save an implementation include.

## Comment and documentation architecture

Comments explain ownership, invariants, reasons, and consequences. They do not narrate obvious syntax and do not preserve a changelog inside source files.

Kyna adopts these documentation rules from Go's documentation discipline:

- Every exported public type or function receives a complete-sentence contract comment.
- The first sentence begins with the declared name when natural: `ArrayType represents ...` or `typeFromRef converts ...`.
- A module-level comment explains what the module owns, what it deliberately does not own, and the main entry interface.
- Field comments explain units, ownership, lifetime, nullability, ordering, and sentinel meanings when the declaration cannot communicate them.
- Comments name invariants close to the code enforcing them.
- References use exact symbol and file names so search and generated documentation remain useful.
- Examples demonstrate observable behavior and belong in executable tests or verified language examples whenever possible.
- `TODO` comments require a concrete missing behavior or issue reference; vague aspirations belong in a plan, not beside production code.
- Generated files and tool directives are clearly marked and never edited as if they were authoritative source.

Public documentation describes the interface. Private comments explain the implementation's non-obvious reasoning. Tests describe behavior through names and assertions. Architecture documents explain ownership and relationships. Keeping those roles separate prevents comments from becoming a second, conflicting specification.

## Research becomes architecture

Research is accepted only after it passes through a repeatable conversion process:

```text
primary source
    ↓
observed mechanism and invariant
    ↓
problem in Kyna that the mechanism addresses
    ↓
adopt / adapt / reject decision
    ↓
owner + interface + implementation location
    ↓
test, verifier, or registry rule
    ↓
implementation-status evidence
```

The research record must separate facts from conclusions:

| Record | Required content |
| --- | --- |
| Evidence | Primary source, exact package/file, and observed behavior |
| Mechanism | How the design works, including constraints and failure cases |
| Kyna problem | The concrete duplication, ambiguity, safety issue, or missing capability |
| Decision | Adopt directly, adapt to Kyna semantics, defer, or reject |
| Ownership | Module, interface seam, implementation folder, and downstream consumers |
| Verification | Test, architecture check, benchmark, or compatibility fixture |
| Status | Researched, designed, represented, parsed, checked, lowered, executable, or released |

External code is not copied merely because its architecture is valuable. The behavior and invariants are re-expressed through Kyna's types, module seams, naming, diagnostics, memory model, and compatibility promises. Hard-to-reverse choices with real alternatives and surprising consequences receive an architectural decision record; ordinary local design belongs beside its owning module.

The Go study under [go_architecture_study](go_architecture_study/README.md), the [cross-compiler research](compiler-architecture-research.md), and focused [open-source studies](githubs/README.md) form the evidence library. This document is the adopted Kyna doctrine. [AGENTS.md](../AGENTS.md) is the executable contributor guide. The machine-readable [module graph](../spec/architecture/modules.json), repository verifiers, and tests are the enforcement layer.

## What Kyna takes from Go

| Go lesson | Kyna application |
| --- | --- |
| Directory/package ownership | One domain per folder inside explicit CMake modules |
| `internal` visibility | Module resolution rejects imports outside an allowed ancestor scope |
| Small behavioral interfaces | Host capabilities and structural language interfaces |
| Concrete type objects | `kyna_types` semantic graph instead of string comparison |
| Value and pointer method sets | Explicit receiver behavior in named semantic types |
| Zero-value usability | Valid default states where Kyna's non-null rules permit them |
| Errors as inspectable values | Structured diagnostics and typed runtime `Error` values |
| Explicit compiler phases | Syntax → type checking → HIR → verified MIR → validated bytecode |
| Export data | Module export cache containing checked public facts |
| `cmd` separation | Thin CLI command adapters over embedding/compiler modules |
| `gofmt` predictability | Stable file anatomy and automated repository formatting/checks |
| Godoc discipline | Searchable contract comments on public declarations |
| Standard-library depth | Small reusable contracts with substantial, tested behavior |

Go is a reference, not a ceiling. Designs from Rust, Swift, Kotlin, TypeScript, Python, functional languages, systems languages, and research compilers may be adopted when they pass the same evidence and ownership process. A feature's popularity is not sufficient; it must improve correctness, clarity, performance, safety, or expressive power without weakening the coherence of Kyna.

Primary Go references for this doctrine are the [Go compiler architecture](https://go.dev/src/cmd/compile/README), [module layout guidance](https://go.dev/doc/modules/layout), [Go documentation conventions](https://go.dev/doc/comment), [Go code-review guidance](https://go.dev/wiki/CodeReviewComments), and [Go language specification](https://go.dev/ref/spec).

## Beyond Go

Rust contributes demand-driven queries, explicit compiler representations, and backend separation. LLVM contributes pass-analysis preservation and invalidation. MLIR contributes legality-driven conversion. Clang contributes a thin driver over reusable compiler libraries. Kani contributes verifier separation and stable tool-facing IR lessons. Swift and GCC reinforce phase-specific performance measurement and named transformation pipelines.

Kyna adopts these mechanisms in this order: enforce module boundaries; finish verified IR coverage; measure phases and representative workloads; add incremental queries; define a native-backend seam; then benchmark backend candidates. The detailed evidence, tradeoffs, library scorecard, and staged plan live in [compiler-architecture-research.md](compiler-architecture-research.md).
