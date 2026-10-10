# Modules

Kyna supports namespace imports and JavaScript-style named/default imports:

```kyna
// legacy namespace import (still supported)
import "./math.kyna" as math;

// JavaScript-style named imports
import { add, sub } from "./math.kyna";

// default export import
import greet from "./math.kyna";

// namespace import
import * as math from "./math.kyna";

export fn add(a: int, b: int): int { return a + b; }
export default fn greet(name: str): str { return "hi " + name; }
export { sub, someValue };
```

- one source file defines a module identity;
- `export` adds a declaration to the module's public table; `export default` marks the default export; `export { a, b }` re-exports named declarations;
- `import "path" as name` loads and caches a module, analyzes it once, then exposes only exports;
- after a successful check, a dependency writes a `*.kyna.kyc` stamp beside the source; the next compile skips re-checking that dependency's body when the compiler contract version and exact source bytes of the module and its transitive dependencies still match. Checked function and binding contracts are serialized with the stamp. The entry module is always analyzed. Set `KYNA_DISABLE_EXPORT_CACHE` to turn this off. Class shapes still come from the parsed AST;
- JavaScript-style imports bind each imported name (or the whole namespace for `import * as`) to the module's exported symbols;
- resolution uses the importing file's directory first, followed by configured library roots;
- cycles are diagnosed with an import stack;
- module initialization runs once and in dependency order.

Imports must precede other top-level declarations. Only named top-level declarations can be exported. Namespace reads are live, namespace writes are forbidden, and private declarations are not visible. Resolution canonicalizes the importer-relative candidate before checking repeated `--module-path` roots. Cycles report the full filename chain. Dependencies initialize once in postorder.

A directory passed to `ky check` or `ky run` is a **package**: every `*.kyna` file in that folder (except `*_test.kyna`) shares one namespace without sibling imports. Packages nested under an `internal/` directory may only be imported from the parent tree of that `internal` folder (diagnostic `KSEM1042`).

Source files may use the `.kyna`/`.ky` extension for program modules and `.kyna.d`/`.d.ky`/`.ky.d` for ambient type-definition files. Type-definition files contribute compile-time interfaces only and are never executed.

## Type imports and exports

`t.ky` is an ordinary module filename. Exported interfaces and aliases can live
in any `.ky` or `.kyna` file:

```kyna
// types.ky
export type Count = int;
export intf Box<T> { value: T; }

// app.ky
import type { Count, Box } from "./types.ky";
var count: Count = 2;
var box: Box<int> = { value: count };
```

`import type * as types from "./types.ky"` provides qualified annotations such
as `types.Count`. `export type { Count }` exports a local or imported type.
Type-only imports do not initialize their module or its runtime dependencies.
A normal import of an interface or alias also contributes no runtime value.
Using either as a value is an error. Imported function parameters, inferred
results, and generic interface contracts remain checked across module boundaries.
Type re-export lists name existing bindings; direct `export ... from` syntax is
not supported.

For explicit C++ module loading, see [Native bindings](native-bindings.md).
