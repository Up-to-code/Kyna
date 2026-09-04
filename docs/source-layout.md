# Source layout

Kyna uses subsystem-owned CMake modules. Each module owns its `include/kyna/...` interface, its `src/` implementation, and focused unit tests. Root tests exercise behavior across module interfaces.

Run `make architecture-check` to reject compatibility leftovers, ambiguous filenames, unregistered modules, dependency drift, and dependency cycles before review. The same check runs under CTest and therefore in CI.

| Repository area | Responsibility |
| --- | --- |
| `compiler/kyna_source` | Source identity, byte spans, and line indexes |
| `compiler/kyna_diagnostics` | Versioned diagnostic data plus text and JSON rendering |
| `compiler/kyna_lexing` | Tokens, keywords, token descriptions, and recoverable tokenization |
| `compiler/kyna_syntax` | Syntax node families and syntax-tree ownership |
| `compiler/kyna_parsing` | Declaration, statement, expression, type, and recovery parsing |
| `compiler/kyna_symbols` | Standard-library symbol identities, call signatures, and argument contracts |
| `compiler/kyna_resolution` | Module paths, dependency graphs, caching, and cycle reports |
| `compiler/kyna_typecheck` | Names, types, control flow, class contracts, interfaces, and lints |
| `compiler/kyna_hir` | Resolved locals, compiler-owned operators, stable IDs, and syntax lowering |
| `compiler/kyna_mir` | Temporaries, basic blocks, terminators, source mappings, and MIR verification |
| `compiler/kyna_bytecode` | Register instructions, MIR lowering, validation, and disassembly |
| `runtime/kyna_host` | Filesystem, process, network, database, and clock capability adapters |
| `runtime/kyna_vm` | Runtime values, frames, bytecode execution, legacy execution, and tracing heap |
| `library/collections` | Collection transforms and predicate algorithms |
| `library/database` | Kyna database values, query results, and `db` namespace installation |
| `library/text` | UTF-8 validation and Unicode-aware text operations |
| `library/core` | Trusted standard-library catalog and JSON value codec |
| `sdk/kyna_embedding` | The high-level language-session interface |
| `tools/kyna_cli` | CLI11 command parsing and command adapters |
| `tools/npm-installer` | Version-locked npm adapter for verified native release archives |

The authoritative dependency graph is [`spec/architecture/modules.json`](../spec/architecture/modules.json). Each module's explicit CMake links must match that manifest exactly. The architecture verifier rejects a missing target, a stale target, an undeclared link, a missing link, an unknown dependency, or a cycle. Public headers cannot use a repository-wide include directory, so accidental reverse dependencies also fail during compilation.

The intended relation between layers is:

```text
source ──► diagnostics ──► lexing ──► syntax ──► parsing ──► resolution
   │                           │          │                       │
   └───────────────────────────┘          ├──► HIR ─► MIR ─► bytecode
types ──► symbols ────────────────────────└──► typecheck          │
  │          │                                  │                ▼
  └──────────┴──────────────────────────────────┴──────────────► VM
host ──────────────────────────────────────────────────────────► VM
library modules ──► standard library ──► embedding ──► CLI
```

Arrows mean “may depend on,” not data flow in every command. The manifest records the exact build graph; this view explains the architectural direction. Higher layers orchestrate lower layers. Lower compiler layers never import the CLI, embedding session, or product policy.

## Folder relation

Every module has the same ownership shape even when it needs only a subset of the folders:

```text
<module>/
  include/kyna/<module>/   stable public interface
  src/<domain>/            private implementation, one responsibility per file
  CMakeLists.txt           explicit sources and declared dependencies
```

Folders name owned work (`types`, `validators`, `checkers`, `lowering`, `rendering`, `catalog`, `codecs`, `loading`, `graph`, `io`, `entry`, `parsing`, `dispatcher`, or `command`). They are not mandatory layers and must not become empty scaffolding. Tests live beside a module only when they test its private domain; cross-module behavior belongs under root `tests/`.

Files are grouped by language construct or owned invariant, not by arbitrary function count. For example, expression parsing belongs in `expression_parser.cpp`, while VM value tracing stays with the VM because splitting it from heap ownership would create a shallow circular interface.
