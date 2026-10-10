# Native bindings

Kyna remains a C++ compiler and VM implementation. A native binding supplies a
checked signature and a runtime implementation; it does not change Kyna into a
native-code compiler.

## Embedding a C++ host

Register `kyna::NativeFunction` values in
`LanguageSessionOptions::nativeFunctions`. The parameter and result `TypeRef`
contracts feed checking, module analysis, compiler inspection, VM execution,
and the tree-walk engine. The runtime repeats arity and value checks, including
calls through explicit `any`. Duplicate or standard-library names are rejected.

`invoke` receives rooted arguments, a heap, and synchronous callback services.
Arguments, callback services, and spans cannot be retained after the invocation.
Attach shared `NativeLifetime` and `NativeResources` registries to functions
that retain callbacks or return opaque handles. `Heap::retain` supplies traced
roots; expired roots and foreign-thread access are rejected. `NativeLifetime`
retains callback IDs, enforces an active owning runtime, and permits explicit
release. Session shutdown closes registries and breaks retained-value cycles. Native C++
exceptions become `KNATIVE1005` failures. See
[the host tests](../tests/native/test_native_functions.cpp) for a minimal example.

## Loading a C ABI module

The public header is
[`native_abi.h`](../sdk/kyna_embedding/include/kyna/language/native_abi.h).
Export `kyna_native_module_v1` and return an immutable module descriptor with ABI
version 1, `sizeof(kyna_native_module)`, a package name/version, and function
signatures. Scalars and homogeneous, recursively typed arrays are supported.
Strings use byte lengths and can contain embedded nulls. Integer arguments are
converted to floating-point values when the declared contract requires float.

Inputs are borrowed for the call. Result buffers remain valid until the host
calls `release(context)` exactly once, including conversion failures. The module
must catch C++ exceptions inside its adapter: neither `invoke` nor `release` may
throw across the C boundary. Returned functions keep the shared library loaded
until the final owner is destroyed. ABI mismatches and malformed contracts are
rejected before registration. A native library is executable code and must be
trusted by the application author.

```bash
ky check app.ky --native-library ./my_module.so
ky run app.ky --native-library ./my_module.so
```

Use `.dylib` on macOS and `.dll` on Windows. The option is repeatable. The
[fixture](../tests/native/native_fixture.cpp) demonstrates a complete adapter:

```bash
c++ -std=c++20 -shared -fPIC -I sdk/kyna_embedding/include \
  tests/native/native_fixture.cpp -o /tmp/kyna_fixture.so
printf 'log(nativeTwice(21));\n' | ky run - --native-library /tmp/kyna_fixture.so
```

To present the host function through an ordinary import, export a wrapper from a
`.ky` or `.kyna` module and launch with the same native-library option. This is
explicit local loading. `ky install` also accepts verified native artifacts and
source wrappers with ABI, target, version, and checksum metadata. See
[Libraries](libraries.md) for manifests and `ky build` application staging.

Desktop adapters have been tested on macOS ARM64. The Android ARM64 application
shell has passed its emulator lifecycle smoke. Portable native dependency
relocation and the other desktop CI targets remain release gates.

## ABI 2 resource and callback modules

[`native_abi_v2.h`](../sdk/kyna_embedding/include/kyna/language/native_abi_v2.h)
adds per-session `create`/`close`, nominal resource contracts, resource validation
and destruction, and host callback services. Export `kyna_native_module_v2`; the
loader continues accepting ABI 1 modules. Version 2 is a separate symbol, not an
in-place ABI 1 layout change.

Callbacks passed into a function are borrowed IDs. Call `retain_callback` before
storing an ID, then `release_callback` on unsubscribe or shutdown. Callbacks can
only be invoked on the owning runtime/UI thread inside an active native call,
such as `guiRun`. Their argument/result buffers obey ordinary ABI conversion and
release rules. Callback arity is checked when passed into native code; retained ABI 2 callbacks also validate arguments and results against their declared contracts at invocation, including calls through `any`. An old execution's callback cannot run in a replacement VM.

Resource IDs are module-local and must not be recycled into a live stale ID.
`resource_valid` checks ID and nominal type before each conversion. Explicit
destruction invalidates the ID; GC releases the final lease; module close frees
remaining resources. `create` must catch exceptions and return null on failure;
all other C entry points must translate exceptions to structured results.

The ABI-only C++ helper `kyna/native/adapter.hpp` owns result buffers, checked
finite numbers, and monotonic resource registries. Examples in `library/gui`,
`library/geometry`, and `library/ml` demonstrate complete adapters. Install the
ABI headers with `cmake --install`, then explicitly build an authored adapter with
`ky build ./binding --native --output build-binding`. Native package acquisition
and application staging are described in [Libraries](libraries.md).

Load a fresh native module for each `LanguageSession`; its returned function set
shares session-owned module state and must not be reused across sessions.
