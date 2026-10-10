# Safety and native-library upgrade status

Checkpoint: 2026-10-11. This inventory distinguishes implemented work from the
remaining approved plan. It is not a release-completion claim.

## Implemented

- Research comparison and owner/test seams for Go, TypeScript, and Rust FFI.
- Debug, Release, and Address/Undefined Behavior sanitizer baselines: all 46
  existing tests passed before compiler changes. The original Debug primes
  benchmark exceeded the existing Kyna/C++ ratio threshold (251 versus 50).
- Aliases, ordinary-file exported interfaces, named/aliased/namespace type
  imports, type-only import/export erasure, imported checked signatures and
  inferred bindings, generic interface contracts, recursive inference errors,
  type-as-value errors, and source/dependency/version-aware checked export cache.
- Constant/compatible/duplicate switch checks and function-local control-flow
  targets; no fallthrough remains the existing execution policy.
- Tree-walk checked integer operations and exact integer comparisons; numeric
  parity and closure-based evaluation-order/short-circuit tests through HIR,
  MIR, bytecode, and tree-walk execution.
- Grouped CLI help, schema-preserving usage errors, specific native errors,
  stdout-aware console color policy, and explicit color/environment tests.
- Modular extension diagnostics/executable discovery/completion catalogs;
  imported-file Problems entries, related locations, Unicode ranges, stale
  process cancellation, dependency rechecks, failure handling, and type syntax.
  VSIX 1.0.16 packages successfully. Full project-view/command extraction remains open; unsaved dependency overlays now
  pass source text through TOML overlays rather than reading stale disk text.
- Typed host native functions shared by checker, inspection, VM, and tree-walk;
  runtime argument/result checks including calls through `any`, rooted arguments,
  synchronous host callback services, and C++ exception translation.
- Versioned C ABI shared-library loading for scalar/array contracts, module
  ownership, integer-to-float conversion, result cleanup, ABI rejection, and
  `--native-library`. A C++ host invokes Kyna and Kyna imports a source wrapper
  around a C++ export in tests.
- Additional type, cache, numeric, native ABI/conversion/cleanup, extension, and
  CLI tests; switch/callback/type-module benchmarks; baseline-relative comparison
  with dependency/C++ source and result fingerprints.

## Remaining gates

1. Complete nullability/callback/recursive inference and mathematical audit
   beyond the added focused regressions; broaden cross-engine control-flow tests.
2. Finish extension module extraction, type hover
   and navigation coverage, and CLI/extension agreement tests.
3. Retained roots, callback ownership, opaque leases, and C ABI 2 are implemented.
   Callback GC, runtime/thread affinity, foreign/stale resources, and shutdown
   are tested through VM and tree-walk.
4. Native contract/checksum installation and `ky build` are implemented and smoke
   tested locally. Verified tar acquisition and per-file rechecking are implemented. Dependency relocation/deployment and
   transactional Git/native lock updates remain release gates.
5. Qt Quick GUI, CGAL 2D/mesh inspection, and mlpack regression adapters execute
   real applications. Desktop smoke has passed on macOS ARM64 offscreen. Android
   shell passed an actual Android 14 ARM64 emulator installation, launch, timer,
   background/foreground lifecycle, GC, and shutdown smoke. Physical devices and
   other Android versions remain unverified.
6. Direct C++ regression parity and repeated conversion/dispatch measurements
   are recorded. Dependency/license inventory and desktop/Android CI are added;
   clean hosted-runner platform validation and broader geometry/GUI benchmarks
   remain pending.
7. Produce native application release archives/checksums and verify Bash,
   PowerShell, VSIX, and npm together against a chosen release tag. npm publication
   remains with the owner's account.

Rams quick_review was requested with `via: rules`; the service reports all 15
workspace review credits exhausted, so the required design review did not run.
Local extension tests and the repository extension verifier do not replace it.

## Performance interpretation

Benchmarks measure process wall time, phase timings, per-process RSS where
available, output hashes, compiler/binary size, and C++ compilation time. Debug primes measurements remain above the informational threshold. The
separate Release checkpoint passes all output comparisons and ratio checks;
comparing Debug with Release is not evidence of a change-related improvement.
Results collected while
other builds are active are not evidence of a baseline-relative regression or
improvement. Use matching configurations on an idle machine and repeated runs.

## Verified builds and artifacts

- All 56 CTests passed in Debug, Release, and sanitizer configurations after
  archive installation and process-launch changes.
- Architecture, extension, language-example, installer, and npm adapter checks
  pass. The extension diagnostic tests pass, and VSIX 1.0.16 was produced locally.
- [Original Debug benchmark](performance/safety-debug-baseline.json) preserves
  the original report schema fields; it predates the new input fingerprint and
  mode fields and cannot be fed directly into the stricter baseline comparator.
- [Release checkpoint](performance/safety-release-checkpoint.json) records five
  measured repetitions after two warmups and verifies C++ output parity. It is
  an informational checkpoint for subsequent matching Release runs.
- [Checking checkpoint](performance/safety-release-checking.json) times the
  checker after separately validating executable output against C++ companions;
  C++ execution timings are not compared to Kyna checking timings.

## Native implementation checkpoint

Real optional adapters live in `library/gui`, `library/geometry`, and `library/ml`.
See [Libraries](libraries.md), [Android](android.md), and
[dependency inventory](native-dependencies.md). The direct regression oracle
checks `1e-12` output parity and reports repeated prediction conversion overhead
in [the checkpoint](performance/native-regression-checkpoint.json).

Optional application/install/build tests passed against Release and sanitizer
CLIs, including archive source wrappers, file tampering, and link rejection.
Extension overlay temporary-file tests pass. Reproducible local native archives
with SHA256SUMS are generated by `tools/native-packager/package.py`; runtime
dependencies remain external until deployment/relocation is verified. Android
provisioning initially encountered `No space left on device`; authorized cleanup
removed roughly 17.2 GiB of generated caches, node_modules, virtual environments,
and framework build output, preserving project source and lockfiles. Qt Android
and host 6.7.3, SDK platform/build tools 35, and NDK 26.1 are installed. Android
Release APK build and Android 14 ARM64 emulator application smoke passed. Unused
Qt toolchain debug symbols were also removed to keep enough build space.
Android validation covers that emulator configuration; no completed portable native release claim is made.


## Latest lifetime and Android verification

After callback contract, nested-activation, and GUI ownership changes, all 56
Release and Address/Undefined Behavior sanitizer CTests and real optional package
tests passed. LeakSanitizer is unavailable on this macOS configuration; the run
uses `detect_leaks=0`. Callback arity and invocation argument/result
contracts are enforced through `any`; a live GUI child retains its native parent.
Android native loading now resolves the adapter beside the application library
and loader failures include their underlying platform error. The rebuilt Release
APK passed the lifecycle smoke again. Hosted platform CI is running through draft PR #6. Its initial Android setup
failed because the setup action requested the retired SDK `tools` package; the
workflow now explicitly requests `platform-tools`. Other platform results remain
pending. All 58 CTests passed in the local optional-adapter configuration.


Generated Android Qt/NDK and sanitizer build directories have verified local tar
backups under `build-release/verification`; restore them before rebuilding those
configurations. Source, Release CLI, native adapters, APKs, and test logs remain.
