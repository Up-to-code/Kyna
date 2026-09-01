#!/usr/bin/env python3
"""ky-vs-C++ benchmark harness.

Compiles identical workloads written once in this language and once in C++,
runs each several times, and reports per-program median wall-clock times plus a
`ky / cpp` ratio so a performance regression is easy to spot.

A "benchmark" is a same-stem pair under `--programs`:
    primes.kyna        (workload in this language)
    primes.cpp         (identical workload in C++)

Both must print identical stdout so the harness can cross-check correctness;
mismatched output is reported as a failure, not silently trusted.

Usage:
    python3 tools/benchmark/run_benchmark.py [--ky PATH] [--programs DIR]
                                             [--reps N] [--threshold RATIO]
                                             [--compiler clang++|g++]
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import statistics
import subprocess
import sys
import tempfile
import time


def median(values: list[float]) -> float:
    return statistics.median(values)


def measure(binary: str, source: pathlib.Path, reps: int, root: pathlib.Path) -> list[float]:
    """Run `binary source` `reps` times, returning wall-clock seconds per run."""
    timings: list[float] = []
    for _ in range(reps):
        start = time.perf_counter_ns()
        result = subprocess.run(
            [binary, str(source)],
            cwd=root,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        elapsed = (time.perf_counter_ns() - start) / 1e9
        timings.append(elapsed)
        if result.returncode != 0:
            raise RuntimeError(f"{source.name} exited {result.returncode}: {result.stderr.strip()}")
    return timings


def run_once(binary: str, source: pathlib.Path, root: pathlib.Path) -> str:
    result = subprocess.run(
        [binary, str(source)],
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"{source.name} exited {result.returncode}: {result.stderr.strip()}")
    return result.stdout.strip()


def compile_cpp(source: pathlib.Path, compiler: str, workdir: pathlib.Path) -> pathlib.Path:
    binary = workdir / source.stem
    result = subprocess.run(
        [compiler, "-O2", "-std=c++17", str(source), "-o", str(binary)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"failed to compile {source.name}: {result.stderr.strip()}")
    return binary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ky", help="path to the ky binary (default: repo build-debug/bin/ky)")
    parser.add_argument("--programs", default=str(pathlib.Path(__file__).resolve().parent / "programs"))
    parser.add_argument("--reps", type=int, default=5)
    parser.add_argument("--threshold", type=float, default=50.0,
                        help="warn when ky/cpp ratio exceeds this value")
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--fail-on-regression", action="store_true",
                        help="return a non-zero exit code when a ratio exceeds --threshold")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parents[2]
    ky = pathlib.Path(args.ky) if args.ky else repo_root / "build-debug/bin/ky"
    if not ky.is_file():
        print(f"error: ky binary not found at {ky}", file=sys.stderr)
        return 2

    programs_dir = pathlib.Path(args.programs)
    pairs: list[tuple[pathlib.Path, pathlib.Path]] = []
    for kyna in sorted(programs_dir.glob("*.kyna")):
        cpp = programs_dir / (kyna.stem + ".cpp")
        if cpp.is_file():
            pairs.append((kyna, cpp))
    if not pairs:
        print(f"error: no .kyna/.cpp pairs found under {programs_dir}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="kyna-bench-") as tmp:
        workdir = pathlib.Path(tmp)
        print(f"{'program':<18}{'ky (ms)':>12}{'cpp (ms)':>10}{'ratio':>10}  verdict")
        print("-" * 62)
        errored = False
        regressed = False
        for kyna, cpp in pairs:
            try:
                cpp_binary = compile_cpp(cpp, args.compiler, workdir)
                kyma_out = run_once(str(ky), kyna, repo_root)
                cpp_out = run_once(str(cpp_binary), cpp, workdir)
                if kyma_out != cpp_out:
                    print(f"{kyna.stem:<18}OUTPUT MISMATCH (ky={kyma_out!r}, cpp={cpp_out!r})")
                    errored = True
                    continue

                kyma_times = measure(str(ky), kyna, args.reps, repo_root)
                cpp_times = measure(str(cpp_binary), cpp, args.reps, workdir)
                kyma_ms = median(kyma_times) * 1e3
                cpp_ms = median(cpp_times) * 1e3
                ratio = kyma_ms / cpp_ms if cpp_ms > 0 else float("inf")
                if ratio > args.threshold:
                    regressed = True
                verdict = "REGRESSION" if ratio > args.threshold else "ok"
                print(f"{kyna.stem:<18}{kyma_ms:>12.2f}{cpp_ms:>10.2f}{ratio:>10.2f}  {verdict}")
            except RuntimeError as exc:
                print(f"{kyna.stem:<18}ERROR: {exc}")
                errored = True
        print("-" * 62)
        if errored:
            print("one or more benchmarks errored", file=sys.stderr)
            return 1
        if regressed:
            summary = "one or more benchmarks exceeded the ratio threshold"
            if args.fail_on_regression:
                print(summary, file=sys.stderr)
                return 1
            print(summary + " (--fail-on-regression to fail)")
        else:
            print("all benchmarks passed")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
