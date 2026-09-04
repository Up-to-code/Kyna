# Kyna

<p align="center">
  <img src="editors/vscode-kyna/assets/kyna-k.svg" width="96" alt="Kyna logo">
</p>

<p align="center">
  <strong>A small, strongly typed programming language with a real compiler and a practical CLI.</strong>
</p>

<p align="center">
  <a href="https://www.npmjs.com/package/@kyna-language/cli">npm</a>
  ·
  <a href="https://github.com/Up-to-code/Kyna/releases">Releases</a>
  ·
  <a href="https://up-to-code.github.io/kyna-docs/">Documentation</a>
  ·
  <a href="docs/language-spec.md">Language specification</a>
  ·
  <a href="editors/vscode-kyna/README.md">VS Code</a>
</p>

## Install

Install the current preview from the official npm organization:

```sh
npm install --global @kyna-language/cli@preview
ky --version
```

The package supports macOS and Linux on x64/ARM64, and Windows on x64. It
downloads the matching native release, verifies its SHA-256 checksum, and
provides both `ky` and `kyna` commands.

Create and run a project:

```sh
ky new hello --template minimal
cd hello
ky run
```

> Kyna is preview software. Read the [implementation status](docs/implementation-status.md)
> and [roadmap](ROADMAP.md) before using it in production.

## The language

```kyna
fn greet(name: str): str {
    return "Hello, " + name;
}

var visits: int = 1;
visits = visits + 1;

const audience = if (visits > 1) {
    "returning visitor"
} else {
    "new visitor"
};

console.log(greet("Kyna"), audience);
```

Kyna includes:

- inferred or explicit static types, with nullable types written as `T?`;
- mutable `var` and immutable `const` bindings;
- arrays, objects, functions, closures, classes, and structural interfaces;
- modules with explicit exports;
- exhaustive `match`, `switch`, and typed error handling;
- HIR, MIR, bytecode, and tree-walk execution paths.

Runnable examples live in [`examples/language/`](examples/language/). For the
complete syntax and behavior, read the [language specification](docs/language-spec.md).

## CLI

| Command | Purpose |
| --- | --- |
| `ky new` / `ky init` | Create a project from a template |
| `ky run` | Run a source file or project |
| `ky check` | Type-check without running |
| `ky fmt` | Format source code |
| `ky dev` / `ky serve` | Develop or serve an HTTP backend |
| `ky repl` | Open the interactive shell |
| `ky doctor` | Check the installation and environment |
| `ky tokens`, `ast`, `hir`, `mir`, `bytecode` | Inspect compiler stages |

`ky hello.kyna` is shorthand for `ky run hello.kyna`.

## Architecture

```text
source
  → lexer
  → parser / syntax tree
  → type checker
  → HIR
  → MIR
  → bytecode
  → virtual machine
```

The repository keeps compiler stages, runtime components, standard libraries,
the embedding SDK, and developer tools in separate modules:

```text
compiler/   lexing, parsing, types, checking, HIR, MIR, bytecode
runtime/    virtual machine, interpreter, and host capabilities
library/    core and standard-library modules
sdk/        embedding API
tools/      CLI and benchmarks
tests/      unit, integration, and architecture verification
docs/       specifications, research, and design records
```

Read the [architecture guide](docs/architecture.md), [source-layout rules](docs/source-layout.md),
and [compiler research](docs/compiler-architecture-research.md) for the complete design.

## Build from source

Requirements: CMake 3.25+, Python 3.10+, a C++23 compiler, and libcurl.

```sh
git clone https://github.com/Up-to-code/Kyna.git
cd Kyna
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

See [CONTRIBUTING.md](CONTRIBUTING.md) before changing the compiler or adding a
language feature.

## Documentation

- [Language specification](docs/language-spec.md)
- [Type system](docs/type-system.md)
- [Compiler architecture](docs/architecture.md)
- [Runtime](docs/runtime.md)
- [Standard library](docs/stdlib.md)
- [Installation and releases](docs/distribution.md)
- [Security](SECURITY.md)

Created and maintained by Ahmed Mansour with help from open-source contributors
and coding agents.

Kyna is available under the [MIT License](LICENSE).
