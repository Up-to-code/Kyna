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
import hashlib
import json
import pathlib
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time


def median(values: list[float]) -> float:
    return statistics.median(values)


def measure(binary: str, source: pathlib.Path, reps: int, root: pathlib.Path,
            warmups: int = 2, metrics: bool = False, check: bool = False) -> dict:
    """Run `binary source` `reps` times, returning wall-clock seconds per run."""
    timings: list[float] = []
    memory: list[int | None] = []
    phases: list[dict] = []
    expected = None
    with tempfile.TemporaryDirectory(prefix="kyna-sample-") as tmp:
        metric_path = pathlib.Path(tmp) / "phases.json"
        rss_path = pathlib.Path(tmp) / "rss.txt"
        for index in range(warmups + reps):
            command = [binary, str(source)]
            if check:
                command.insert(1, "check")
            if metrics:
                metric_path.unlink(missing_ok=True)
                command += ["--metrics-file", str(metric_path)]
            rss_supported = pathlib.Path("/usr/bin/time").is_file() and sys.platform in {"darwin", "linux"}
            if rss_supported:
                command = ["/usr/bin/time", "-l" if sys.platform == "darwin" else "-v",
                           "-o", str(rss_path), *command]
            start = time.perf_counter_ns()
            result = subprocess.run(command, cwd=root, capture_output=True, text=True)
            elapsed = (time.perf_counter_ns() - start) / 1e9
            if result.returncode != 0:
                raise RuntimeError(f"{source.name} exited {result.returncode}: {result.stderr.strip()}")
            if expected is None:
                expected = result.stdout
            elif result.stdout != expected:
                raise RuntimeError(f"{source.name} output changed between repetitions")
            if index < warmups:
                continue
            timings.append(elapsed)
            peak = None
            if rss_supported:
                raw = rss_path.read_text()
                match = re.search(r"(\d+)\s+maximum resident set size", raw) if sys.platform == "darwin" else re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", raw)
                if match:
                    peak = int(match.group(1)) * (1 if sys.platform == "darwin" else 1024)
            memory.append(peak)
            if metrics:
                payload = json.loads(metric_path.read_text())
                if payload.get("schema") != "kyna.metrics/v1":
                    raise RuntimeError("unsupported phase metrics schema")
                phases.append(payload)
    return {"wall_seconds": timings, "median_seconds": median(timings),
            "peak_rss_bytes": memory, "phases": phases}


def run_once(binary: str, source: pathlib.Path, root: pathlib.Path, check: bool = False) -> str:
    result = subprocess.run(
        [binary, "check", str(source)] if check else [binary, str(source)],
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
    parser.add_argument("--ky", help="path to the ky binary (default: repo build-release/bin/ky)")
    parser.add_argument("--programs", default=str(pathlib.Path(__file__).resolve().parent / "programs"))
    parser.add_argument("--reps", type=int, default=10)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--json-output", type=pathlib.Path)
    parser.add_argument("--phase-metrics", action="store_true")
    parser.add_argument("--check", action="store_true", help="measure repeated checking without execution")
    parser.add_argument("--threshold", type=float, default=50.0,
                        help="warn when ky/cpp ratio exceeds this value")
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--fail-on-regression", action="store_true",
                        help="return a non-zero exit code when a ratio exceeds --threshold")
    args = parser.parse_args()
    if args.reps < 1 or args.warmups < 0:
        parser.error("--reps must be positive and --warmups nonnegative")

    repo_root = pathlib.Path(__file__).resolve().parents[2]
    ky = (pathlib.Path(args.ky) if args.ky else repo_root / "build-release/bin/ky").resolve()
    if not ky.is_file():
        print(f"error: ky binary not found at {ky}", file=sys.stderr)
        return 2

    programs_dir = pathlib.Path(args.programs)
    pairs: list[tuple[pathlib.Path, pathlib.Path | None]] = []
    for kyna in sorted(programs_dir.glob("*.kyna")):
        cpp = programs_dir / (kyna.stem + ".cpp")
        pairs.append((kyna.resolve(), cpp.resolve() if cpp.is_file() and not args.check else None))
    if not pairs:
        print(f"error: no .kyna workloads found under {programs_dir}", file=sys.stderr)
        return 2

    def command_text(command):
        result = subprocess.run(command, cwd=repo_root, capture_output=True, text=True)
        return result.stdout.strip() if result.returncode == 0 else None

    cache = ky.parent.parent / "CMakeCache.txt"
    cache_text = cache.read_text() if cache.is_file() else ""
    configuration = re.search(r"^CMAKE_BUILD_TYPE:STRING=(.*)$", cache_text, re.MULTILINE)
    report = {"schema": "kyna.benchmark/v1", "commit": command_text(["git", "rev-parse", "HEAD"]),
              "dirty": bool(command_text(["git", "status", "--porcelain"])),
              "platform": platform.platform(), "architecture": platform.machine(),
              "python": platform.python_version(),
              "cpp_compiler": command_text([args.compiler, "--version"]),
              "build_configuration": configuration.group(1) if configuration else None,
              "binary_sha256": hashlib.sha256(ky.read_bytes()).hexdigest(),
              "binary_bytes": ky.stat().st_size, "repetitions": args.reps,
              "warmups": args.warmups, "phase_metrics": args.phase_metrics,
              "command": "check" if args.check else "run", "cache_policy": "warmups reuse export caches",
              "measurement": "process wall time including launcher; per-process peak RSS",
              "workloads": []}

    with tempfile.TemporaryDirectory(prefix="kyna-bench-") as tmp:
        workdir = pathlib.Path(tmp)
        print(f"{'program':<18}{'ky (ms)':>12}{'cpp (ms)':>10}{'ratio':>10}  verdict")
        print("-" * 62)
        errored = False
        regressed = False
        for kyna, cpp in pairs:
            try:
                cpp_binary = compile_cpp(cpp, args.compiler, workdir) if cpp else None
                kyma_out = run_once(str(ky), kyna, repo_root, args.check)
                cpp_out = run_once(str(cpp_binary), cpp, workdir) if cpp else kyma_out
                if kyma_out != cpp_out:
                    print(f"{kyna.stem:<18}OUTPUT MISMATCH (ky={kyma_out!r}, cpp={cpp_out!r})")
                    errored = True
                    continue

                kyma_times = measure(str(ky), kyna, args.reps, repo_root, args.warmups, args.phase_metrics, args.check)
                cpp_times = measure(str(cpp_binary), cpp, args.reps, workdir, args.warmups) if cpp else None
                report["workloads"].append({"name": kyna.stem,
                    "source_sha256": hashlib.sha256(kyna.read_bytes()).hexdigest(),
                    "output_sha256": hashlib.sha256(kyma_out.encode()).hexdigest(),
                    "kyna": kyma_times, "cpp": cpp_times,
                    "cpp_binary_bytes": cpp_binary.stat().st_size if cpp_binary else None})
                kyma_ms = kyma_times["median_seconds"] * 1e3
                cpp_ms = cpp_times["median_seconds"] * 1e3 if cpp_times else 0
                ratio = kyma_ms / cpp_ms if cpp_ms > 0 else 0
                if ratio > args.threshold:
                    regressed = True
                verdict = "RATIO ABOVE THRESHOLD" if ratio > args.threshold else "ok"
                if cpp_times:
                    print(f"{kyna.stem:<18}{kyma_ms:>12.2f}{cpp_ms:>10.2f}{ratio:>10.2f}  {verdict}")
                else:
                    print(f"{kyna.stem:<18}{kyma_ms:>12.2f}{'—':>10}{'—':>10}  {verdict}")
            except RuntimeError as exc:
                print(f"{kyna.stem:<18}ERROR: {exc}")
                report["workloads"].append({"name": kyna.stem, "error": str(exc)})
                errored = True
        print("-" * 62)
        if args.json_output:
            args.json_output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
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
