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
npm i -g @kyna-language/cli@preview
ky new hello --template minimal
cd hello && ky run
```

Without Node.js: `curl -fsSL https://github.com/Up-to-code/Kyna/releases/latest/download/install.sh | sh`

Windows PowerShell without Node.js: `irm https://github.com/Up-to-code/Kyna/releases/latest/download/install.ps1 | iex`

> Kyna is moving toward 1.0. The bytecode VM runs the lowered subset; a tree-walk engine still covers the rest. Read [status](docs/implementation-status.md) and the [roadmap](ROADMAP.md) before production use.

## Why

- Familiar syntax: `var` / `const`, functions, classes, modules, `match`
- Types that stay out of the way, and `?` when something can be null
- Inspect the compiler: tokens → AST → HIR → MIR → bytecode
- `ky new`, `ky check`, `ky fmt`, `ky dev` for scripts and HTTP backends
- VS Code: highlight, diagnostics, Run / Check

## Language

```kyna
fn greet(name: str): str {
    return "Hello " + name;
}

var visits: int = 1;  # mutable, with a locked type
visits = visits + 1;

const audience = if (visits > 1) {  # immutable
    "returning visitor"
} else {
    "new visitor"
};

console.log(greet("Kyna"), audience);
```

Kyna uses `var` for mutable bindings and `const` for immutable bindings; the legacy spellings `let`, `set`, and `func` remain accepted as aliases. Types are inferred when safe, can be written explicitly, and are non-nullable by default. Add `?` when a value may be `null`:

```kyna
var nickname: str? = null;
```

Modules expose only explicitly exported declarations:

```kyna
import "./math.kyna" as math;
console.log(math.add(20, 22));
```

The language also supports first-class functions, mutable and transitive lexical captures, recursion, single inheritance, structural interfaces, exhaustive `match`, `switch`/`case` arms, `await` expressions, and typed `try`/`catch`/`finally`. Browse the runnable [`examples/language/`](examples/language/) programs (start with `syntax_overview.kyna`) or read the complete [language specification](docs/language-spec.md).

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

Kyna is Ahmed’s language. The agents below pair on the public repo — review stays with him.

<table>
  <tr>
    <td align="center" valign="top" width="140">
      <a href="https://github.com/Up-to-code">
        <img src="https://github.com/Up-to-code.png?size=120" width="80" alt="Ahmed Mansour" /><br />
        <strong>Ahmed Mansour</strong>
      </a><br />
      Creator
    </td>
    <td align="center" valign="top" width="140">
      <a href="https://cursor.com">
        <img src="https://github.com/cursor.png?size=120" width="80" alt="Cursor" /><br />
        <strong>Cursor</strong>
      </a><br />
      Pair programmer
    </td>
    <td align="center" valign="top" width="140">
      <a href="https://cursor.com/agents">
        <img src="https://github.com/cursor.png?size=120" width="80" alt="Cursor Cloud" /><br />
        <strong>Cursor Cloud</strong>
      </a><br />
      Cloud agent
    </td>
    <td align="center" valign="top" width="140">
      <a href="https://openai.com/codex">
        <img src="https://github.com/openai.png?size=120" width="80" alt="OpenAI Codex" /><br />
        <strong>Codex</strong>
      </a><br />
      OpenAI
    </td>
  </tr>
</table>

Ahmed owns the language, the CLI, and the merge button. Cursor, Cursor Cloud, and Codex help write and review. Dependabot keeps GitHub Actions and C++ pins current.

## Docs

[Spec](docs/language-spec.md) · [Architecture](docs/architecture.md) · [Stdlib](docs/stdlib.md) · [Runtime](docs/runtime.md) · [Projects](docs/projects.md) · [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)

MIT. See [LICENSE](LICENSE).
