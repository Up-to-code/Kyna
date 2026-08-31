# Testing architecture

Tests are grouped by the module seam they exercise:

- `tests/lexer` checks tokenization and keyword classification;
- `tests/parser` checks AST construction;
- `tests/runtime` checks environments, object lifetime, and collection;
- `tests/stdlib` checks host adapters;
- `tests/runtime/failures` contains executable programs that must fail with a specific runtime diagnostic;
- `tests/tooling/verify_language_examples.py` checks every example through `check`, `tokens`, and `ast`, verifies the supported HIR/MIR/bytecode subset, and compares deterministic output;
- `tests/tooling/verify_runtime_failures.py` verifies exit status, the `kyna.diagnostic/v1` JSON schema, stable error codes, runtime categories, source locations, and call frames;
- `tests/tooling/verify_network_examples.py` compiles every network example through bytecode and runs the deterministic mock-network checkpoints;
- `tests/tooling/verify_developer_platform.py` exercises `ky`/`kyna`, project discovery and scaffolding, formatter idempotence, lockfile enforcement, a real loopback HTTP service, and Ctrl-C exit behavior;
- `tests/tooling/verify_installed_cli.py` builds a release-shaped archive, serves it locally, runs the native Unix or Windows installer, verifies both command names, creates and checks a project, tests reinstall backups, rejects a corrupt checksum without replacing the working binary, and exercises self-uninstall;
- `tests/tooling/verify_repl_terminal.py` drives the rich REPL through a real pseudo-terminal, edits a misspelled identifier with Left Arrow, recalls the corrected command with Up Arrow, pastes a multi-line program with a comment, completes `:project`, and verifies editing, source boundaries, workspace context, and session state;
- `tests/tooling/verify_new_project_terminal.py` drives `ky new` through a real pseudo-terminal, supplies a project name, selects the default template, and verifies the generated project and next-step summary;
- `tests/v03` checks result interfaces, byte spans, recovery, modules, structural interfaces, access control, and source overlays;
- `tests/test_kyna.cpp` is the end-to-end language suite.

Tests link public domain targets through the compatibility aggregate; no test reaches into a private helper. The release and sanitizer configurations run the same CTest inventory.

The example verifier also reads the compiler's standard-library symbol catalog. Every callable builtin must have a registered example that actually invokes it. Adding a builtin without adding coverage therefore fails CTest immediately.

Filesystem examples and installer releases run from disposable temporary directories. The installer test uses a loopback HTTP server and never changes the machine-wide installation or Windows user PATH. Public HTTP examples are syntax-checked during ordinary CI, while deterministic injected network adapters exercise HTTP success and failure behavior without depending on an external service.
