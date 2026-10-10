# Libraries

The standard library is included. Optional packages use typed C ABI 2 exports;
C++ objects stay inside adapters and Kyna owns opaque resource leases. Native
callbacks retain traced Kyna values and run synchronously on the VM/UI thread.

| Package | Implementation | Local verification |
|---|---|---|
| `gui` | Qt Quick windows, text, buttons, inputs, row/column, paths, timers, unsubscribe, lifecycle | macOS ARM64 offscreen application and retained callback smoke passed |
| `geometry` | CGAL orientation, distance, segment intersection, hull; OFF triangle mesh inspection and volume | 2D fixtures and tetrahedron measurements passed |
| `ml` | mlpack linear regression, typed feature arrays, opaque models | predictions match direct C++ within `1e-12`; dimension and stale-handle failures tested |

Build only the packages you need:

```bash
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release \
  -DKYNA_WITH_GUI=ON -DKYNA_WITH_GEOMETRY=ON -DKYNA_WITH_ML=ON
cmake --build build-native --parallel
```

Install Qt Quick, CGAL, or mlpack development dependencies for enabled packages.
The default CLI does not require them. See [license inventory](native-dependencies.md).
A `.ky` wrapper exports checked functions; example wrappers are under
`examples/native/packages`. Application code remains Kyna.

```kyna
import { train, predict } from "./ml.ky";
var model = train([[1.0, 2.0, 3.0]], [3.0, 5.0, 7.0]);
console.log(predict(model, [[4.0, 5.0]]));
```

Matrix layout is feature rows × observation columns. Responses contain one value
per observation. Both training and prediction reject non-finite inputs and
inconsistent dimensions. `dispose(model)` releases the model immediately;
subsequent uses report `KNATIVE1006`. Garbage collection and session shutdown
also release resources.

CGAL uses `Exact_predicates_inexact_constructions_kernel`: predicate decisions
are exact; constructed points, distances, area, and volume are approximate.
`intersection` returns no points, one crossing point, or two overlap endpoints.
`inspectMesh` returns `[vertices, faces, closed, selfIntersecting, area, signedVolume,
minX, minY, minZ, maxX, maxY, maxZ]`. `volume` rejects open/self-intersecting meshes;
orientation determines volume sign. OFF input must be a valid triangle mesh.

GUI `button(parent, label, fn(): void)` and `after(milliseconds, handler)` retain
handlers. `run()` enters the UI loop; `quit()` ends it. A handler failure ends the
loop and becomes a native diagnostic. `onLifecycle(fn(str): void)` delivers
`active`, `inactive`, `hidden`, and `suspended` while the loop runs. `after` and
`onLifecycle` return IDs accepted by `unsubscribe`. See `gui_smoke.ky`,
`gui_hull.ky`, and `mesh.ky` under `examples/native/apps`.

## Verified native installation

Declare a trusted native binary alongside its source wrapper:

```toml
[dependencies.ml]
path = "./packages/ml"
[dependencies.ml.native]
version = "1.0.16"
target = "darwin-arm64"
abi = 2
path = "./artifacts/libkyna_ml.so"
sha256 = "<64 lowercase hexadecimal characters>"
```

`url = "https://..."` can replace the artifact path. For a verified tar archive,
set `archive = true` and `library = "native/libkyna_ml.so"` (the exact relative
entry). SHA-256 then covers the whole archive. Only directories and regular files
are accepted; links, traversal paths, and special files are rejected. Installed
file hashes are recorded individually and checked before loading. Put `ml.ky`
at the archive root to allow `import { train } from "ml"` automatically. `ky install` checks target,
ABI, descriptor version, and SHA-256 before registering a package. Installed
artifacts live under `.kyna/native`; their contracts appear in `.kyna/native.lock`
and `kyna.lock`. `ky install --locked` rejects changed contracts. Run/check verify
installed checksums again and automatically load those bindings. No dependency
install scripts execute. ABI validation requires loading trusted native code;
checksums verify integrity, not publisher identity.

`ky build main.ky --output dist/my-app` checks and stages application modules,
installed native artifacts, the CLI, launchers, and `SHA256SUMS` in a fresh folder.
Runtime dependencies of native libraries must also be installed on the target;
this staging command does not yet relocate Qt frameworks or CGAL/mlpack dynamic
dependencies. `ky build ./my-binding --native --output build-binding` explicitly
runs an authored CMake build. See [Android](android.md) for the ARM64 shell.

Only locally passed targets are verified here. Desktop CI and Android emulator
gates must pass before their targets are advertised as supported.

Local archive preparation is documented in [the native packager](../tools/native-packager/README.md). The prepared macOS archives contain adapters and checked wrappers; their dependency metadata explicitly identifies external runtime libraries. Portable deployment remains a release gate.
