# Kyna

<p align="center">
  <img src="editors/vscode-kyna/assets/kyna-k.svg" width="96" alt="Kyna">
</p>

<p align="center">
  <strong>A small, strongly typed language.</strong><br />
  Readable scripts, a real compiler, and a <code>ky</code> CLI.
</p>

<p align="center">
  <a href="https://github.com/Up-to-code/Kyna/releases">Releases</a>
  ·
  <a href="https://up-to-code.github.io/kyna-docs/">Docs</a>
  ·
  <a href="docs/language-spec.md">Spec</a>
  ·
  <a href="editors/vscode-kyna/README.md">VS Code</a>
</p>

```sh
curl -fsSL https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh | sh
ky new hello --template minimal
cd hello && ky run
```

Windows: `irm https://github.com/Up-to-code/Kyna/releases/latest/download/install.ps1 | iex`

> Kyna is moving toward 1.0. The bytecode VM runs the lowered subset; a tree-walk engine still covers the rest. Read [status](docs/implementation-status.md) and the [roadmap](ROADMAP.md) before production use.

## Why

- Familiar syntax: `let` / `set`, functions, classes, modules, `match`
- Types that stay out of the way, and `?` when something can be null
- Inspect the compiler: tokens → AST → HIR → MIR → bytecode
- `ky new`, `ky check`, `ky fmt`, `ky dev` for scripts and HTTP backends
- VS Code: highlight, diagnostics, Run / Check

## Language

```kyna
func greet(name: str): str {
    return "Hello " + name;
}

let visits: int = 1;
visits = visits + 1;

set audience = if (visits > 1) {
    "returning visitor"
} else {
    "new visitor"
};

console.log(greet("Kyna"), audience);
```

`let` is mutable. `set` is not. Types are non-nullable unless you write `str?`.

```kyna
import "./math.kyna" as math;
console.log(math.add(20, 22));
```

More in [`examples/`](examples/) and the [language spec](docs/language-spec.md).

## `ky`

| | |
| --- | --- |
| `ky new` / `ky init` | project from `minimal` or `backend` |
| `ky run` / `ky check` / `ky fmt` | execute, typecheck, format |
| `ky dev` / `ky serve` | watch or serve an HTTP backend |
| `ky repl` | interactive shell |
| `ky doctor` | PATH, editor, project |
| `ky tokens` `ast` `hir` `mir` `bytecode` | compiler stages |

`ky hello.kyna` means `ky run hello.kyna`.

HTTP starter:

```sh
ky new hello-api --template backend
cd hello-api && ky dev
```

## Build from source

CMake 3.25+, Python 3.10+, a C++23 compiler, libcurl.

```sh
git clone https://github.com/Up-to-code/Kyna.git
cd Kyna
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
make install-all
```

That installs `ky` (and the `kyna` alias) plus the VS Code extension.

## Contributors

<table>
  <tr>
    <td align="center" valign="top" width="160">
      <a href="https://github.com/Up-to-code">
        <img src="https://github.com/Up-to-code.png?size=120" width="88" alt="Ahmed Mansour" /><br />
        <strong>Ahmed Mansour</strong>
      </a><br />
      Creator
    </td>
    <td align="center" valign="top" width="160">
      <a href="https://cursor.com">
        <img src="https://www.cursor.com/favicon.ico" width="88" alt="Cursor" /><br />
        <strong>Cursor</strong>
      </a><br />
      AI pair programmer
    </td>
  </tr>
</table>

Kyna is built by [Ahmed Mansour](https://github.com/Up-to-code), with [Cursor](https://cursor.com) as an AI pair programmer on the public repo.

## Docs

[Spec](docs/language-spec.md) · [Architecture](docs/architecture.md) · [Stdlib](docs/stdlib.md) · [Runtime](docs/runtime.md) · [Projects](docs/projects.md) · [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)

MIT. See [LICENSE](LICENSE).
