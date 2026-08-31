# Diagnostics

Diagnostics carry severity, a stable code, message, byte-accurate primary `SourceSpan`, secondary labels, notes, help, and runtime-frame storage. The lexer and parser recover at statement and declaration boundaries and can report multiple independent mistakes. Invalid trees are not analyzed or executed.

Text output prints the source line and an underline. `--diagnostic-format json` emits schema version 1 with the same code, severity, file, and range data; this is the contract used by the VS Code extension. Errors produce exit status `1`, warnings do not prevent checking or execution, and CLI usage or module/file resolution failures produce status `2`.

The semantic best-practice pass currently reports:

- `K2601`: a fallible network, filesystem, JSON, or database operation is outside a protecting `try` block.
- `K2602`: an empty `catch` block silently hides a failure.
- `K2603`: a literal network URL uses unencrypted `http://`.
- `K2604`: a host shell command requires trusted input.
- `K2605`: the result of a value-producing operation is ignored.
- `K2606`: `fetch` relies on the default timeout instead of selecting a backend-specific timeout.
- `K2610`: SQL text is dynamic instead of using constant SQL and bound parameters.

Runtime families include `KNET2001` for phase-specific network failures, `KDB2001` for PostgreSQL failures with SQLSTATE/native causes, `KRT2001`–`KRT2004` for member access, `KRT2101`–`KRT2105` for array/object indexing, and `KRT2200`–`KRT2204` for numeric operations. These codes are stable across the tree-walk compatibility engine and bytecode VM: `KRT2201` covers division/remainder by zero, `KRT2202` covers non-integer remainder operands, and `KRT2204` covers checked integer overflow. VM-only defects and invalid bytecode retain `KVM`/`KBC` codes. Runtime diagnostics include source mappings and VM call frames.

Semantic declaration and call validation uses `KSEM1101`–`KSEM1104` for duplicate declarations, bindings, declaration conflicts, and parameters. `KSEM1201` reports an argument-count mismatch and `KSEM1202` points at the exact argument whose type is incompatible. Semantic null and index misuse uses `KSEM2401`–`KSEM2403` before execution when the type is known.

`KSEM1203` rejects calls on statically non-callable values. The VM uses `KVM2010` when a dynamically typed value is not callable and `KVM2011` when an indirect function call supplies the wrong number of arguments.

Loop-control validation uses `KSEM1301` for `break`/`continue` outside a loop, `KSEM1302` for an unknown target label, `KSEM1303` for an ambiguous duplicate active label, and `KSEM1304` for a non-boolean loop condition.

Match validation uses `KSEM1401` for a non-exhaustive expression, `KSEM1402` for arms made unreachable by a wildcard, `KSEM1403` for duplicate literal patterns, and `KSEM1404` for a pattern incompatible with the subject type.

These diagnostics are emitted by the compiler rather than reconstructed by editor tooling, so CLI text, CLI JSON, and VS Code agree on the rule and source span.
