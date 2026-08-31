#!/usr/bin/env python3
"""Check every Kyna example and verify deterministic examples end to end."""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys


ANSI = re.compile(r"\x1b\[[0-9;]*m")

EXPECTED_OUTPUT = {
    "examples/hello.kyna": "Hello Kyna\nadult\n",
    "examples/classes.kyna": "woof Rex\n",
    "examples/match.kyna": "one\n",
    "examples/modules/main.kyna": "42\n",
    "examples/language/advanced_control_flow.kyna": "visits 12\nshortCircuit false true\n",
    "examples/language/bindings_and_nullability.kyna": (
        "bindings 42 2\nnullable true true\nequality true true\n"
    ),
    "examples/language/closures.kyna": (
        "addForty 42\nfirstCounter 1 2\nsecondCounter 11\nfactorial 120\nnestedAdder 42\n"
    ),
    "examples/language/collection_algorithms.kyna": (
        "mapped [2,4,4,6]\nreduced 8 first 2\nfiltered [2,2]\n"
        "distinct [1,2,3]\nordered [1,2,3]\npredicates true true\n"
    ),
    "examples/language/exceptions.kyna": (
        "recovered returned KVM2301:boom:boom:cleanup-first:cleanup-second:return-cleanup\n"
    ),
    "examples/language/first_class_functions.kyna": "sum 42\nproduct 42\n",
    "examples/language/interfaces_and_inheritance.kyna": (
        "interface Rex\noverride animal Rex says woof\n"
    ),
    "examples/language/json_and_objects.kyna": (
        "decoded 22 true\nproject Kyna 42\nheader application/json\n"
        "keys [\"name\",\"version\"]\nencoded {\"items\":[20,22],\"ready\":true}\n"
    ),
    "examples/language/recursive_functions.kyna": "factorial(6) 720\nfibonacci(10) 55\n",
    "examples/language/unicode_text.kyna": (
        "length 12\nslice Héllo\nfind 8\ncontains true\nreplace Héllo Kyna\n"
        "case äbc KYNA\nsplit 3 two\n"
    ),
}

# These examples must compile through every production compiler representation.
# Modules, database adapters, and callback-driven collection algorithms still have
# explicitly tracked bytecode migration work and continue through the compatibility engine.
BYTECODE_EXAMPLES = {
    relative
    for relative in EXPECTED_OUTPUT
    if relative
    not in {
        "examples/modules/main.kyna",
        "examples/language/collection_algorithms.kyna",
    }
}


def invoke(binary: pathlib.Path, root: pathlib.Path, command: str, source: pathlib.Path):
    return subprocess.run(
        [str(binary), command, str(source), "--no-color"],
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    root = pathlib.Path(sys.argv[2]).resolve()
    failures: list[str] = []
    examples = sorted((root / "examples").rglob("*.kyna"))

    for source in examples:
        relative = source.relative_to(root).as_posix()
        for command in ("check", "tokens", "ast"):
            result = invoke(binary, root, command, source)
            if result.returncode != 0:
                failures.append(
                    f"{relative}: {command} exited {result.returncode}: {result.stderr.strip()}"
                )

    for relative in sorted(BYTECODE_EXAMPLES):
        for command in ("hir", "mir", "bytecode"):
            result = invoke(binary, root, command, root / relative)
            if result.returncode != 0:
                failures.append(
                    f"{relative}: {command} exited {result.returncode}: {result.stderr.strip()}"
                )

    for relative, expected in EXPECTED_OUTPUT.items():
        executed = invoke(binary, root, "run", root / relative)
        actual = ANSI.sub("", executed.stdout)
        if executed.returncode != 0:
            failures.append(f"{relative}: run exited {executed.returncode}: {executed.stderr.strip()}")
        elif actual != expected:
            failures.append(f"{relative}: output mismatch\nexpected={expected!r}\nactual={actual!r}")

    missing = sorted(set(EXPECTED_OUTPUT) - {path.relative_to(root).as_posix() for path in examples})
    failures.extend(f"missing deterministic example: {relative}" for relative in missing)

    if failures:
        print("Kyna language example verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print(
        f"checked {len(examples)} examples; compiled {len(BYTECODE_EXAMPLES)} through bytecode; "
        f"ran {len(EXPECTED_OUTPUT)} deterministic examples"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
