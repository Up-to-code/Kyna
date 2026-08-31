# Learn Kyna by running code

The numbered folders form an executable curriculum. Every `.kyna` lesson is compiled by the normal test suite and deterministic lessons are run with exact output checkpoints.

1. `01_basics`: values, types, immutable `set`, mutable `let`, operators, nullability, and Unicode text.
2. `02_control_flow`: conditional expressions, loops, and exhaustive `match` expressions.
3. `03_functions`: parameters, returns, recursion, and functions as values.
4. `04_data`: arrays, objects, JSON, TOML, XML, mutation, and collection operations.
5. `05_errors_and_network`: typed errors, cleanup, throwing `fetch`, and non-throwing `http.tryFetch`.
6. `06_system`: environment variables, injected OS/terminal facts, and cleanup-safe JSON, TOML, XML, and text-file work.

Run a lesson with `kyna run examples/learning/01_basics/variables_and_types.kyna` and inspect its compiler representations with `kyna tokens`, `kyna ast`, `kyna hir`, `kyna mir`, or `kyna bytecode`.
