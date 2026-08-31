# Kyna

<p align="center">
  <img src="editors/vscode-kyna/assets/kyna-k.svg" width="112" alt="Kyna logo">
</p>

<p align="center"><strong>A small, strongly typed language for readable scripts and language-tooling experiments.</strong></p>

<p align="center">
  <a href="https://github.com/Up-to-code/Kyna/releases">Releases</a> ·
  <a href="docs/language-spec.md">Language specification</a> ·
  <a href="editors/vscode-kyna/README.md">VS Code extension</a>
</p>

Kyna is a brace-delimited programming language and C++23 implementation. It combines familiar scripting syntax with static types, structured diagnostics, modules, classes, structural interfaces, lexical closures, and an inspectable compiler pipeline.

> [!IMPORTANT]
> Kyna is under active development toward 1.0. The validated bytecode VM runs the supported lowered subset; a temporary tree-walk compatibility engine handles constructs that have not yet moved to bytecode. See the [implementation status](docs/implementation-status.md) and [roadmap](ROADMAP.md) before depending on Kyna for production work.

## Why Kyna?

- **Readable language basics:** explicit bindings, functions, classes, interfaces, modules, exceptions, and familiar control flow.
- **Useful scripting capabilities:** Unicode text, JSON, collections, files, processes, HTTP(S), and parameterized PostgreSQL queries.
- **Compiler visibility:** inspect tokens, syntax trees, HIR, MIR, and validated register bytecode from the CLI.
- **Actionable errors:** stable diagnostic codes, precise source spans, recoverable parsing, and text or JSON output.
- **Embeddable runtime:** host operations are injected behind capability interfaces, with managed memory and explicit GC statistics.
- **Editor support:** syntax highlighting, completion, navigation, live diagnostics, CodeLens, and Run/Check actions for VS Code.

## Quick start

### Requirements

- CMake 3.25 or newer
- Python 3.10 or newer
- A C++23 compiler
- libcurl development headers
- Git and an internet connection when CMake needs to fetch a missing pinned dependency

PostgreSQL support is enabled when libpq is available. For a fully pinned dependency build, use the included [`vcpkg.json`](vcpkg.json); the default developer preset can otherwise fetch CLI11, FTXUI, and utf8proc when they are missing.

### Build and test

```sh
git clone https://github.com/Up-to-code/Kyna.git
cd Kyna
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

CI runs the project on Linux, macOS, and Windows. Release and sanitizer presets are also available:

```sh
cmake --preset release
cmake --build --preset release

cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
```

### Run Kyna

```sh
./build-debug/bin/kyna run examples/hello.kyna
./build-debug/bin/kyna check examples/hello.kyna
./build-debug/bin/kyna repl
```

On Windows, use the executable produced in the selected preset's build directory. Prebuilt platform archives are available on the [Releases page](https://github.com/Up-to-code/Kyna/releases); verify their checksums before running them.

## Language tour

```kyna
func greet(name: str): str {
    return "Hello " + name;
}

let visits: int = 1;  # mutable, with a locked type
visits = visits + 1;

set audience = if (visits > 1) {  # immutable
    "returning visitor"
} else {
    "new visitor"
};

console.log(greet("Kyna"), audience);
```

Kyna deliberately uses `let` for mutable bindings and `set` for immutable bindings. Types are inferred when safe, can be written explicitly, and are non-nullable by default. Add `?` when a value may be `null`:

```kyna
let nickname: str? = null;
```

Modules expose only explicitly exported declarations:

```kyna
import "./math.kyna" as math;

console.log(math.add(20, 22));
```

The language also supports first-class functions, mutable and transitive lexical captures, recursion, single inheritance, structural interfaces, exhaustive `match`, and typed `try`/`catch`/`finally`. Browse the runnable [`examples/language/`](examples/language/) programs or read the complete [language specification](docs/language-spec.md).

## Command-line tools

```text
kyna run <file|->
kyna check <file|->
kyna repl
kyna tokens <file|-> [--format text|json]
kyna ast <file|-> [--format text|json]
kyna hir <file|-> [--format text|json]
kyna mir <file|-> [--format text|json]
kyna bytecode <file|-> [--format text|json]
```

`kyna program.kyna` is shorthand for `kyna run program.kyna`. Common options include repeatable `--module-path <dir>`, `--diagnostic-format text|json`, and `--color auto|always|never`; `--no-color` is an alias for `--color never`.

## Standard library and host capabilities

Kyna includes APIs for:

- Unicode-aware text and mutable collection operations
- JSON parsing, serialization, and file storage
- Filesystem and process access
- HTTP and HTTPS requests through libcurl
- In-memory CRUD stores
- Parameterized PostgreSQL queries through libpq
- Explicit garbage collection and heap statistics

Filesystem, process, network, database, and timing operations cross injected host-capability boundaries, allowing embedders and tests to replace them. Network requests verify TLS certificates and apply timeouts and bounded retries. Review the [standard-library guide](docs/stdlib.md), [runtime guide](docs/runtime.md), and [database guide](docs/database.md) for the available APIs and security considerations.

End-to-end examples are organized under [`examples/`](examples/):

- [`examples/language/`](examples/language/) covers bindings, nullability, closures, exceptions, objects, collections, interfaces, inheritance, and Unicode text.
- [`examples/modules/`](examples/modules/) demonstrates explicit imports and exports.
- [`examples/weather_api.kyna`](examples/weather_api.kyna) calls the keyless Open-Meteo API.
- [`examples/fake_api_store.kyna`](examples/fake_api_store.kyna) demonstrates HTTP CRUD and JSON persistence.
- [`examples/backend/`](examples/backend/) shows parameterized PostgreSQL access and a repository-module seam.

## VS Code extension

Package and install the local extension from the repository root:

```sh
make vscode-package
code --install-extension editors/vscode-kyna/kyna-language-support-1.0.4.vsix --force
```

The extension recognizes `.kyna` files and provides highlighting, snippets, completion, symbols, same-file definitions, hover information, import completion, live diagnostics, CodeLens, and commands for running, checking, and inspecting compiler output. Set `kyna.executable` if the CLI is not installed or cannot be found in a recognized CMake build directory.

See the [extension README](editors/vscode-kyna/README.md) for development and privacy details.

## Documentation

| Area | Documentation |
| --- | --- |
| Language | [Specification](docs/language-spec.md) · [Grammar](docs/grammar.md) · [Type system](docs/type-system.md) · [Object model](docs/object-model.md) |
| Compiler and runtime | [Architecture](docs/architecture.md) · [Source layout](docs/source-layout.md) · [Runtime](docs/runtime.md) · [Garbage collection](docs/garbage-collection.md) |
| Using Kyna | [Modules](docs/modules.md) · [Standard library](docs/stdlib.md) · [Database](docs/database.md) · [Diagnostics](docs/diagnostics.md) |
| Development | [Testing](docs/testing.md) · [Distribution](docs/distribution.md) · [Release policy](docs/release-policy.md) |
| Project status | [Implementation status](docs/implementation-status.md) · [Roadmap](ROADMAP.md) · [Changelog](CHANGELOG.md) |

## Repository layout

```text
compiler/   lexer, parser, analysis, HIR, MIR, and bytecode
runtime/    host-capability interfaces and bytecode VM
library/    core, collection, text, and database support
sdk/        public C++ embedding API
tools/      CLI and development utilities
editors/    VS Code extension
examples/   runnable Kyna programs
tests/      compiler, runtime, standard-library, and tooling tests
docs/       language and architecture documentation
```

## Contributing

Read [`CONTRIBUTING.md`](CONTRIBUTING.md) before opening a pull request. Please report security vulnerabilities privately by following [`SECURITY.md`](SECURITY.md), not through a public issue.

## License

Kyna is available under the [MIT License](LICENSE).
