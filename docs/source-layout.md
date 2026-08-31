# Source layout

Kyna uses subsystem-owned CMake modules. Each module owns its `include/kyna/...` interface, its `src/` implementation, and focused unit tests. Root tests exercise behavior across module interfaces.

Run `make architecture-check` to reject compatibility leftovers and ambiguous filenames before review. The same check runs under CTest and therefore in CI.

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

The dependency direction is declared by the root CMake subdirectory order and each module's explicit target links. Public headers cannot use a repository-wide include directory, so accidental reverse dependencies fail during compilation.

Files are grouped by language construct or owned invariant, not by arbitrary function count. For example, expression parsing belongs in `expression_parser.cpp`, while VM value tracing stays with the VM because splitting it from heap ownership would create a shallow circular interface.
