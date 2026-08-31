#!/usr/bin/env python3
"""Compile network examples and run deterministic network checkpoints."""

from __future__ import annotations

import pathlib
import subprocess
import sys


EXPECTED_OUTPUT = {
    "caught_transport_error.kyna": "caught network KNET2001 true\n",
    "checkpoints.kyna": "checkpoints 200 3 Ada\n",
    "fetch_result.kyna": "safe fetch 200 true\nsafe failure KNET2001 true\n",
    "response_text.kyna": "text response 200 114 Grace\n",
}

REQUIRED_SNIPPETS = {
    "api_key_header.kyna": ["X-API-Key", "processEnv", "catch"],
    "bearer_authorization.kyna": ["Authorization", "Bearer ", "processEnv", "catch"],
    "cookies_and_csrf.kyna": ["Cookie", "X-CSRF-Token", "catch"],
    "fetch_result.kyna": ["http.tryFetch", "fetchResult", ".error.code"],
    "http_status_error.kyna": ["response.ok", "response.status", "catch"],
    "post_json.kyna": ['method: "POST"', "jsonStringify", "Content-Type", "catch"],
    "query_parameters.kyna": ["?latitude=", "timeout", "catch"],
}


def invoke(binary: pathlib.Path, command: str, source: pathlib.Path):
    return subprocess.run(
        [str(binary), command, str(source), "--no-color"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    examples = pathlib.Path(sys.argv[2]).resolve()
    failures: list[str] = []
    sources = sorted(examples.glob("*.kyna"))

    for source in sources:
        for command in ("check", "tokens", "ast", "hir", "mir", "bytecode"):
            result = invoke(binary, command, source)
            if result.returncode != 0:
                failures.append(
                    f"{source.name}: {command} exited {result.returncode}: {result.stderr.strip()}"
                )

    for name, snippets in REQUIRED_SNIPPETS.items():
        source = examples / name
        if not source.is_file():
            failures.append(f"missing network example: {name}")
            continue
        contents = source.read_text()
        for snippet in snippets:
            if snippet not in contents:
                failures.append(f"{name}: missing network checkpoint {snippet!r}")

    for name, expected in EXPECTED_OUTPUT.items():
        result = invoke(binary, "run", examples / name)
        if result.returncode != 0:
            failures.append(f"{name}: run exited {result.returncode}: {result.stderr.strip()}")
        elif result.stdout != expected:
            failures.append(
                f"{name}: output mismatch; expected={expected!r}, actual={result.stdout!r}"
            )

    if failures:
        print("Kyna network-example verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print(
        f"compiled {len(sources)} network examples through bytecode; "
        f"ran {len(EXPECTED_OUTPUT)} deterministic checkpoints"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
