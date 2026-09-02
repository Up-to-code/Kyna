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
- JavaScript-style imports bind each imported name (or the whole namespace for `import * as`) to the module's exported symbols;
- resolution uses the importing file's directory first, followed by configured library roots;
- cycles are diagnosed with an import stack;
- module initialization runs once and in dependency order.

Imports must precede other top-level declarations. Only named top-level declarations can be exported. Namespace reads are live, namespace writes are forbidden, and private declarations are not visible. Resolution canonicalizes the importer-relative candidate before checking repeated `--module-path` roots. Cycles report the full filename chain. Dependencies initialize once in postorder.

Source files may use the `.kyna`/`.ky` extension for program modules and `.kyna.d`/`.d.ky`/`.ky.d` for ambient type-definition files. Type-definition files contribute compile-time interfaces only and are never executed.
