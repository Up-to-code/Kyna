# Kyna

<p align="center">
  <img src="editors/vscode-kyna/assets/kyna-k.svg" width="112" alt="Kyna logo">
</p>

<p align="center"><strong>A small, typed language for readable scripts and language-tool experiments.</strong></p>

<p align="center">
  <a href="https://github.com/Up-to-code/Kyna/releases">Releases</a> ·
  <a href="docs/language-spec.md">Language specification</a> ·
  <a href="editors/vscode-kyna/README.md">VS Code support</a>
</p>

Kyna is a brace-delimited, strongly typed programming language implemented in C++23. Kyna 1.0 is moving through a validated register-bytecode pipeline while retaining the characterized tree-walk path for constructs still being lowered. It includes structured diagnostics, recoverable parsing, modules, structural interfaces, class contracts, a tracing heap, injected host capabilities, and a persistent REPL.

The bytecode path supports first-class functions and lexical closures with independent mutable state, transitive captures, and recursive nested functions. See [`examples/language/closures.kyna`](examples/language/closures.kyna).

## Highlights

- Clear `let` and `set` bindings, functions, classes, interfaces, modules, and `try`/`catch`.
- Python-style `#` comments and JavaScript-style `console.log` output.
- JSON, HTTP(S), filesystem, process, collection, parameterized PostgreSQL, and in-memory CRUD helpers.
- Compiler diagnostics with stable codes, source spans, text/JSON output, and best-practice warnings.
- A VS Code extension with syntax highlighting, snippets, declarations, import completion, live checking, and Run/Check buttons.
- Non-moving managed heap with explicit collection and runtime statistics.

## Quick start

Requirements: CMake 3.25 or newer, a C++23 compiler, Git, and libcurl development headers. Missing CLI11 sources are fetched at their pinned release; the complete pinned dependency set is available through `vcpkg.json`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a program:

```sh
./build/bin/kyna run examples/hello.kyna --no-color
./build/bin/kyna check examples/hello.kyna --no-color
./build/bin/kyna run examples/language/recursive_functions.kyna --no-color
./build/bin/kyna run examples/language/advanced_control_flow.kyna --no-color
./build/bin/kyna run examples/language/first_class_functions.kyna --no-color
./build/bin/kyna repl
```

The CLI also supports `tokens`, `ast`, resolved `hir`, verified `mir`, validated `bytecode` disassembly, repeated `--module-path`, `--diagnostic-format text|json`, and `kyna file.kyna` as a `run` alias. Global options work before or after the input path. See [distribution.md](docs/distribution.md) for Linux, Windows, and macOS archives.

## A small Kyna program

```kyna
func greet(name: str): str {
    return "Hello " + name;
}

let age = 21;
set category = if (age >= 18) {
    "adult"
} else {
    "minor"
};

console.log(greet("Kyna"), category);
```

Modules use explicit imports and exports:

```kyna
import "./math.kyna" as math;

console.log(math.add(2, 3));
```

Read the complete grammar and semantics in the [language specification](docs/language-spec.md), [module guide](docs/modules.md), and [type-system guide](docs/type-system.md).

## Standard library examples

```kyna
set response = fetch("https://api.open-meteo.com/v1/forecast?latitude=30.04&longitude=31.24&current=temperature_2m", { timeout: 15000 });
set weather = response.json();
console.log("temperature", weather.current.temperature_2m);

func adult(user: any): bool {
    return user.age >= 18;
}

set adults = filter(users, adult);
set ordered = sort(adults);
set names = map(adults, userName);
set hasAdministrator = any(adults, isAdministrator);
fs.createDirectory("output");
fs.writeJson("output/users.json", ordered);
```

Runnable end-to-end examples live in [`examples/`](examples/), including VM-backed recursive functions, a keyless Open-Meteo HTTPS smoke test, and a Fake Store CRUD example. Network calls use the injected runtime capability and linked libcurl with certificate verification, timeouts, bounded retries, and actionable transfer errors; they never embed credentials.

Backend examples under [`examples/backend/`](examples/backend/) demonstrate parameterized PostgreSQL CRUD and a repository-module seam suitable for ORM-style domain mapping. See the [database guide](docs/database.md) for SQL types, null handling, diagnostics, and credential safety.

## VS Code

Build and install the extension locally:

```sh
make vscode-package
code --install-extension editors/vscode-kyna/kyna-language-support-1.0.1.vsix --force
```

The extension registers only the canonical `.kyna` extension, plus `#` comments, completions for Kyna words/declarations/imports, live diagnostics, symbols, definitions, hover, CodeLens, Run/Check, and compiler-inspection commands. Its primary icon is purple; the black-and-white branding assets remain available in [`editors/vscode-kyna/assets/legacy/`](editors/vscode-kyna/assets/legacy/).

## Documentation

| Topic | Guide |
| --- | --- |
| Language syntax and semantics | [language-spec.md](docs/language-spec.md), [grammar.md](docs/grammar.md) |
| Architecture and source layout | [architecture.md](docs/architecture.md), [source-layout.md](docs/source-layout.md) |
| Runtime, modules, and memory | [runtime.md](docs/runtime.md), [modules.md](docs/modules.md), [garbage-collection.md](docs/garbage-collection.md) |
| PostgreSQL and repository modules | [database.md](docs/database.md) |
| Diagnostics and tooling | [diagnostics.md](docs/diagnostics.md), [distribution.md](docs/distribution.md) |
| Safe installation | [installation-security.md](docs/installation-security.md) |
| C++ embedding migration | [cpp-api-migration.md](docs/cpp-api-migration.md) |
| Delivery status and roadmap | [implementation-status.md](docs/implementation-status.md), [ROADMAP.md](ROADMAP.md) |

## Project layout

Public headers are owned by modules under [`compiler/`](compiler/), [`runtime/`](runtime/), [`library/`](library/), and [`sdk/`](sdk/). CMake targets enforce the forward compiler pipeline and prevent tools from reaching through module interfaces.

## Contributing and security

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. To report a security vulnerability privately, follow [SECURITY.md](SECURITY.md) rather than opening a public issue. The macOS “Move to Bin” warning is covered in the [safe installation guide](docs/installation-security.md).

## License

Kyna is released under the [MIT License](LICENSE).
