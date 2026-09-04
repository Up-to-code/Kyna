# Open-source architecture studies

This directory records focused studies of external open-source systems. Each study identifies an observed mechanism, the problem it solves, what Kyna adopts or rejects, the owning Kyna seam, and a verification requirement.

- [Kani](kani.md) — compiler/verifier separation, stable IR boundaries, and RFC discipline.
- [Go architecture study](../go_architecture_study/README.md) — compiler, source layout, language types, documentation, tooling, runtime, and standard-library lessons.
- [Cross-compiler research](../compiler-architecture-research.md) — Rust, LLVM, MLIR, Cranelift, Clang, GCC, Swift, distribution, and library decisions.

These records are evidence, not vendored designs. Kyna's adopted rules live in [architecture.md](../architecture.md), module relationships live in [source-layout.md](../source-layout.md), and mechanical enforcement lives in `AGENTS.md` and the repository verifier.
