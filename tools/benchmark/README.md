# Benchmark harness

The default binary is now `build-release/bin/ky`, with two warmups and ten
measured repetitions. Add `--json-output report.json` for `kyna.benchmark/v1`
results, and `--phase-metrics` for per-run `kyna.metrics/v1` compiler/VM timings.
Use `--check --phase-metrics` for repeated checking, including the multi-module
workload. Export caches are reused after warmups.
Kyna-only `.kyna` workloads also run; `.cpp` companions add output parity checks.
RSS is measured per process on macOS/Linux and reported as unavailable elsewhere.
See [the milestone inventory](../../docs/bytecode-milestone.md) for limitations.

Compiles identical workloads written once in this language and once in C++,
runs each several times, and reports per-program median wall-clock time plus a
`ky / cpp` ratio so a performance regression is easy to spot.

## Layout

```
tools/benchmark/
  run_benchmark.py        harness (python3, no dependencies beyond the stdlib)
  programs/
    primes.kyna/.cpp      workload pair: count primes below 50000
    nested_loops.kyna/.cpp  workload pair: nested-loop integer accumulation
```

A "benchmark" is a same-stem pair under `programs/` (e.g. `primes.kyna` and
`primes.cpp`). Every pair runs both binaries several times and cross-checks
that their stdout matches, so an implementation drift is reported, not trusted.

## Usage

```sh
# from the repo root; uses build-debug/bin/ky and clang++ by default
python3 tools/benchmark/run_benchmark.py

# custom binary / compiler / repetitions
python3 tools/benchmark/run_benchmark.py --ky ./build-debug/bin/ky --compiler g++ --reps 10
```

Options:

| flag | default | meaning |
| --- | --- | --- |
| `--ky PATH` | `build-release/bin/ky` | path to the language CLI |
| `--programs DIR` | `tools/benchmark/programs` | directory of `.kyna`/`.cpp` pairs |
| `--reps N` | `10` | repetitions per binary per program (median is reported) |
| `--warmups N` | `2` | unrecorded warmups per workload |
| `--json-output PATH` | off | write metadata, samples, RSS, and artifact sizes |
| `--phase-metrics` | off | record compiler/VM phase metrics for each Kyna run |
| `--threshold RATIO` | `50` | warn when `ky/cpp` exceeds this |
| `--compiler` | `clang++` | C++ compiler used for the `.cpp` side |
| `--fail-on-regression` | off | exit non-zero when any ratio exceeds the threshold |

## Interpreting the results

`ratio` is `ky median / cpp median`. This language currently runs through a
tree-walking/bytecode interpreter while C++ is natively compiled with `-O2`, so a
large ratio on CPU-bound loops is expected. A long-term goal is to narrow that
gap; a ratio that jumps sharply between commits is a signal worth investigating.

The harness exits `0` unless a program errors or (with `--fail-on-regression`) a
ratio exceeds `--threshold`, so it can be wired into CI as a soft detector of
performance regressions.

## Adding a benchmark

Create `programs/<name>.kyna` and `programs/<name>.cpp` with identical output
for the same input. Both files will be picked up automatically. Keep the
workload CPU-bound and single-threaded for a fair comparison, and keep the C++
side deterministic (no allocations beyond what the language does is required,
but both must print the same value).
