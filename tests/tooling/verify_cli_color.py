#!/usr/bin/env python3
"""Verify deterministic ANSI color behavior for the Kyna CLI."""

from __future__ import annotations

import pathlib
import subprocess
import sys


def run(binary: pathlib.Path, *color_arguments: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [str(binary), "check", "-", *color_arguments],
        input=b"set broken = ;\n",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    cases = {
        "auto": (run(binary), False),
        "always": (run(binary, "--color", "always"), True),
        "never": (run(binary, "--color", "never"), False),
        "no-color": (run(binary, "--no-color"), False),
    }
    failures: list[str] = []
    for name, (result, expects_color) in cases.items():
        has_color = b"\x1b[" in result.stderr
        if result.returncode != 1:
            failures.append(f"{name}: expected exit 1, received {result.returncode}")
        if has_color != expects_color:
            failures.append(f"{name}: ANSI color present={has_color}, expected={expects_color}")
    if failures:
        print("Kyna CLI color verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print("Kyna CLI color policy verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
