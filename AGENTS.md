# AGENTS.md — clean-code architecture & registry guide

This file is the source of truth for how source code is organised and how new
things are registered. Read it before adding, splitting, or renaming files.

## Repo layout

```
compiler/   kyna_* modules (lexing, syntax, parsing, types, typecheck, hir, mir, bytecode, ...)
runtime/    kyna_vm (bytecode VM + tree-walk interpreter), kyna_host
library/    standard libraries (core, collections, database, formats, text)
sdk/        kyna_embedding (language session / embedding API)
tools/      kyna_cli, benchmark harness
tests/      ctest suites + tooling verifiers
docs/       specifications (language-spec, type-system, stdlib, ...)
```

Each compiler module is a static library built by `kyna_add_module(...)` (see
`cmake/KynaModule.cmake`). CMake uses **explicit source lists** (no globs), so
every new or moved source file must be listed in that module's `CMakeLists.txt`.

## Clean folder architecture

Goal: **one thing per file, one domain per folder.** A domain owns the source
files for the work it performs; add a member to that domain rather than to a
"god file" or a catch-all source directory.

The structure is applied to **every** module across `compiler/`, `library/`,
`runtime/`, `sdk/`, and `tools/`, not just a single reference module. Each
module's private implementation under `src/` is split into domain subfolders
whose names describe the work they own. Empty `register_*.cpp` registration
templates are not part of the architecture and must be removed; registration
instructions belong here and in the relevant module documentation.

Canonical vocabulary used per domain (see `compiler/kyna_lexing` for the full
`types` / `validators` / `checkers` / `testers` working reference):

```
src/
  types/        pure type/value definitions + their formatting/description
  validators/   rules that accept/reject input (validators, modifier rules, catalogs)
  checkers/     the actual work (scanners, parsers, type checkers, analyzers)
  testers/      focused tests for this domain
  lowering/     IR passes (syntax->HIR, HIR->MIR)
  rendering/    human/JSON renderers and disassemblers
  catalog/      registry tables (stdlib symbols, stdlib natives)
  codecs/       value codecs (json, format)
  generators/   emitters (bytecode)
  loading/      module/file loading
  graph/        module graph construction
  io/           source I/O
  entry/        program entry points
  parsing/      CLI/argument parsing
  dispatcher/   CLI command dispatch
  command/      CLI subcommand implementations
```

All of these already exist; this list is guidance for naming new domain folders,
not a checklist every module must contain. A module only owns the domains that
match its work, and a single-file module places its one file under the single
domain that names it (e.g. `text/src/text/unicode_text.cpp`).

Rules:
- Every `.cpp` file has exactly **one responsibility**.
- Public headers under `include/kyna/<module>/...` form the stable public API.
  Do **not** rename or move public header include paths used by other modules
  (e.g. `kyna/lexing/token.hpp`) without updating every downstream include.
- Private implementation files live in domain subfolders under `src/`; a
  `.cpp`, `.cc`, or `.cxx` directly under a module's `src/` is misplaced.
- A module facade is an intentional, documented exception when a small
  module-level entry point coordinates a public API. The current exceptions are
  `library/core/src/format_value_codec.cpp` and
  `library/core/src/json_value_codec.cpp`; do not add another exception
  silently. If a facade grows a distinct implementation responsibility, move
  that implementation into an appropriate domain folder.
- Include paths use the public `<module>/...` prefix; same-dir private headers
  may use relative includes. A file in a subfolder that needs a header at the
  `src/` root uses a relative include (`../header.hpp`).
- One shared private header may stay at a `src/` root when it is used across
  the whole module's subfolders and is explicitly named/documented as shared;
  e.g. `tools/kyna_cli/src/cli_commands.hpp` is the shared command contract,
  included from subfolders as `../cli_commands.hpp`. Current module-wide
  contracts also include the `*_private.hpp` headers at the roots of the
  collections, database, text, host, and embedding modules. Root private
  headers do not make root-level implementation files permissible.
- CMake source lists are explicit. Every implementation added or moved must be
  added to the owning module's `CMakeLists.txt`; do not use `file(GLOB ...)` or
  `file(GLOB_RECURSE ...)` to discover sources.

### C++ file anatomy (Go-inspired)

Implementation files follow a stable top-to-bottom order so a reader can open
any checker or catalog and find the same shape:

1. Purpose comment (what this file owns, not a changelog).
2. Includes.
3. File-local helpers.
4. Public/entry functions.
5. Private helpers.

Split a pass when a single `std::visit` or `invoke()` grows past one
responsibility (operators vs calls vs members). Do not flatten the C++ tree
into a Go-style `src/cmd`; keep `compiler/kyna_*` modules and split *inside*
`src/<domain>/`.

### How to register new things

New token type  -> declare `TokenKind` in `kyna/lexing/token.hpp`, name it in
                   `lexing/src/types/token_kind_description.cpp`, scan it in
                   `lexing/src/checkers/token_scanner.cpp`, test under `testers/`.
New keyword     -> add entry in `lexing/src/validators/keyword_catalog.cpp`.
New checker     -> add a file under `<module>/src/checkers/` and list it in CMake.
New tester      -> add a file under `<module>/src/testers/` and list it in CMake.

Do not create or copy `register_*.cpp` templates. The architecture verifier
rejects empty registration stubs. To add a new member, create its real source
file in the owning domain, list it explicitly in CMake, and update the relevant
registry/catalog and tests described below.

### How to register a new standard-library native / builtin

A callable builtin must be registered in **all four** places or it will not work
end to end:

1. `library/core/src/bytecode/bytecode_standard_library.cpp` — implement in
   `invoke()` and add the name to `bytecodeStandardLibraryFunctionNames()`.
2. `library/core/src/catalog/standard_library_catalog.cpp` — define the
   tree-walk native via `global->define(...)`.
3. `compiler/kyna_symbols/src/catalog/standard_library_symbols.cpp` — add a
   `StandardLibrarySymbol{...}` so `ky check` knows its arity and return type.
4. `tests/tooling/verify_language_examples.py` — add a `BUILTIN_COVERAGE` entry
   and make an example call the builtin (the verifier enforces coverage).

Library modules live under `library/<name>/` with `include/kyna/stdlib/...` and
are wired into `CMakeLists.txt` via `add_subdirectory`.

## Timing / performance (embedded libraries)

Builtins that take a significant amount of time should be measurable. Use the
timing API (see `measure` in the stdlib catalogs): wrap the work in a thunk and
call `measure(thunk)` to record elapsed time, and `profileLog(...)` to emit it.
Keep timing out of hot paths unless profiling is enabled.

## Build / test / verify

```
cmake --build build-debug
./build-debug/bin/ky check <file.kyna>   # semantic analysis
ctest --test-dir build-debug             # all C++ unit + tooling tests
python3 build_tools/verify_repository_architecture.py
python3 build_tools/verify_vscode_extension.py
python3 tests/tooling/verify_language_examples.py ./build-debug/bin/ky <repo_root>
```

Benchmarks: `tools/benchmark/run_benchmark.py` compiles identical programs in this
language and in C++, runs both, and reports a time-diff to detect performance
regressions
(see `tools/benchmark/README.md`).
