# Compiler safety and native-library research

Research checkpoint: 2026-10-10. Implementation status is recorded separately;
an architectural recommendation is not evidence that a feature works.

## Language comparisons and decisions

| Mechanism | Primary reference | Kyna decision and owner | Verification |
| --- | --- | --- | --- |
| Assignability and interfaces | [Go specification](https://go.dev/ref/spec#Assignability) | `kyna_types` owns directional compatibility; `kyna_typecheck` owns structural conformance. Preserve explicit `any`, non-null defaults, and distinct integer/float contracts. | Positive and negative assignments, interface generics, nullability, callbacks |
| Type/value module separation | [TypeScript modules](https://www.typescriptlang.org/docs/handbook/modules/reference.html) | `kyna_parsing` records type-only imports/exports; `kyna_resolution` resolves them; runtime must erase their initialization edges. `t.ky` is an ordinary filename. | Named/aliased/namespace types; declaration imports; a type-only dependency with observable initialization |
| Compatibility limits | [TypeScript compatibility](https://www.typescriptlang.org/docs/handbook/type-compatibility.html) | Do not adopt bivariance or accidental inferred `any`; carry complete checked signatures across module boundaries. | Incorrect imported arity, parameter type, result assignment, function-value calls |
| Control-flow targets | [Go statements](https://go.dev/ref/spec#Statements) | Checker tracks function-local loop/switch targets. Kyna keeps constant switch cases and no fallthrough. | Duplicate/default cases; nested functions; labeled break/continue |
| Native boundaries | [Rust FFI](https://doc.rust-lang.org/nomicon/ffi.html) | Host/native adapter owns a versioned C ABI, ownership, callback roots, conversions, and exception translation. C++ implementation alone does not make headers importable. | ABI mismatch, stale handles, retained callbacks, exception translation |

## Mathematical behavior

The preservation target is: lowering preserves each accepted program's result,
observable evaluation order, and failure category. Type safety alone does not
prove this property, and test coverage must not be described as a formal proof.

Audit checked integer arithmetic at signed limits, division/remainder by zero,
minimum integer divided by minus one, numeric promotion, floating comparisons,
short-circuiting, side effects, and exception cleanup. Compare direct VM and
tree-walk execution where both support the same input. Compare C++ workloads
only when their numeric semantics and outputs agree; C++ signed overflow is not
a valid oracle for Kyna's checked overflow.

[CGAL's kernel documentation](https://doc.cgal.org/latest/Kernel_23/index.html)
explains why rounded floating-point predicates can invalidate geometric
decisions. Geometry adapters should use exact predicates, identify approximate
constructions/measurements explicitly, reject non-finite input, and test
collinear, coincident, degenerate, and extreme-scale data.

## Native-library seams

Qt Quick, CGAL, and mlpack remain optional adapters, outside the default CLI
dependency set. Their public contract is Kyna types, not C++ templates or raw
pointers. GUI callbacks execute on the UI/VM thread. Model/mesh/widget lifetime
must be explicit and retained callbacks must remain rooted until unsubscribe or
shutdown.

- [Qt platforms](https://doc.qt.io/qt-6/supported-platforms.html) and
  [licensing](https://doc.qt.io/qt-6/licensing.html): desktop and Android require
  separate application/deployment smoke tests and dependency notices.
- [CGAL licensing](https://doc.cgal.org/latest/Manual/license.html): record the
  license of each selected package; do not describe all CGAL packages as LGPL.
- [mlpack regression](https://www.mlpack.org/doc/user/methods/linear_regression.html):
  demonstrate training/prediction against the same direct C++ data and record
  conversion overhead separately from algorithm time.

Before accepting a native dependency, record its pinned version, supported
targets, compilation/execution latency, peak memory, output and archive sizes,
correctness results, distribution cost, and rollback (disable the optional
adapter without changing core language behavior).
