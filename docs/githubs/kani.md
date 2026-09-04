# Kani architecture study

Kani is a bit-precise model checker for Rust. It is useful to Kyna as an architecture reference for separating a language driver, compiler integration, intermediate representation, verification backend, and user-facing proof harnesses. Kani is not a C++ compiler backend and is not proposed as an embedded Kyna runtime dependency.

## Observed architecture

Kani's documented pipeline separates three substantial responsibilities:

1. `cargo-kani` provides the user-facing driver and Cargo integration.
2. `kani-compiler` integrates with the Rust compiler and translates a selected program into the representation expected by the verification backend.
3. CBMC performs bounded model checking and returns verification results.

This separation lets the driver coordinate user intent without owning compiler semantics, and lets the verification backend remain distinct from normal execution. Kani's StableMIR work also demonstrates the cost of depending directly on private compiler representations: a stable, tool-oriented IR seam can reduce coupling between a verifier and compiler internals. Its RFC process reserves explicit design review for substantial or hard-to-reverse changes.

Primary references: [Kani repository](https://github.com/model-checking/kani), [Kani architecture article](https://model-checking.github.io/kani-verifier-blog/2023/08/03/turbocharging-rust-code-verification.html), [StableMIR integration](https://model-checking.github.io/kani/stable-mir.html), [RFC process](https://model-checking.github.io/kani/rfc/), and [Kani usage](https://model-checking.github.io/kani/usage.html).

## Kyna decisions

| Kani lesson | Kyna application | Verification |
| --- | --- | --- |
| Thin user driver | `kyna_cli` parses and presents; `kyna_embedding` owns reusable operations. | CLI and embedding tests must observe the same diagnostics and results. |
| Compiler/verifier separation | A future verifier is a sibling consumer of versioned checked HIR/MIR, not code inside parsing or the VM. | The verifier accepts only validated IR and cannot import CLI implementation headers. |
| Stable IR seam | If external tools need compiler data, publish a versioned serialization instead of exposing private C++ objects. | Schema fixtures, compatibility tests, malformed-input rejection, and compiler-version metadata. |
| Proof harnesses | Verification entry points and assumptions must be explicit source constructs or manifest declarations. | Positive, counterexample, timeout, and unsupported-feature tests. |
| RFC discipline | Require a focused decision record for an ABI, backend, cache format, package registry, or verification semantics. | The record names alternatives, consequences, migration, and rollback. |

## What Kyna does not copy

- Kani's Rust compiler coupling: Kyna owns its own syntax, semantic types, HIR, and MIR.
- CBMC as an automatic runtime dependency: model checking is specialized tooling with separate scale and distribution constraints.
- Rust proof syntax without a Kyna semantic design: assumptions, nondeterminism, unwinding bounds, and undefined behavior need Kyna-owned meaning.
- A verification feature represented only in the CLI: editor, embedding, diagnostics, serialization, and tests must use the same deep interface.

## Possible future verifier seam

```text
Kyna source → check → verified HIR/MIR → versioned verification model
                                             │
                                             ▼
                                  verifier backend adapter
                                             │
                                             ▼
                         proved | counterexample | unknown | unsupported
```

The result is structured data containing source spans, properties, backend identity, bounds, elapsed time, and reproducibility metadata. Text, JSON, editor, and CI presentation are renderers of that result. No verifier should become part of the normal compile or run path unless the user requests it.
