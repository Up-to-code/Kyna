#!/usr/bin/env python3
"""Assert that representative runtime failures retain structured diagnostics."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys


EXPECTED_CODES = {
    "array_out_of_bounds.kyna": "KRT2104",
    "cyclic_json.kyna": "K5101",
    "division_by_zero.kyna": "KRT2201",
    "empty_replace_needle.kyna": "KTEXT2003",
    "empty_split_separator.kyna": "KTEXT2004",
    "explicit_error.kyna": "KRT2300",
    "integer_overflow.kyna": "KRT2204",
    "invalid_fetch_timeout.kyna": "KNET1002",
    "invalid_header_container.kyna": "KNET1003",
    "invalid_header_value.kyna": "KNET1003",
    "invalid_json.kyna": "K5100",
    "invalid_json_file.kyna": "K5100",
    "invalid_toml.kyna": "KFORMAT1001",
    "invalid_xml.kyna": "KFORMAT1101",
    "missing_file.kyna": "KFS2001",
    "negative_sleep.kyna": "KTIME2002",
    "remainder_by_zero.kyna": "KRT2201",
    "text_slice_out_of_bounds.kyna": "KTEXT2002",
    "write_missing_parent.kyna": "KFS2002",
}


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    fixtures = pathlib.Path(sys.argv[2]).resolve()
    failures: list[str] = []

    actual_files = {path.name for path in fixtures.glob("*.kyna")}
    if actual_files != set(EXPECTED_CODES):
        missing = sorted(set(EXPECTED_CODES) - actual_files)
        unregistered = sorted(actual_files - set(EXPECTED_CODES))
        failures.append(f"failure-fixture manifest mismatch: missing={missing}, unregistered={unregistered}")

    for name, expected_code in EXPECTED_CODES.items():
        source = fixtures / name
        result = subprocess.run(
            [
                str(binary),
                "run",
                str(source),
                "--diagnostic-format",
                "json",
                "--no-color",
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if result.returncode != 1:
            failures.append(f"{name}: expected exit 1, got {result.returncode}")
            continue
        try:
            document = json.loads(result.stderr)
        except json.JSONDecodeError as error:
            failures.append(f"{name}: invalid diagnostic JSON: {error}")
            continue
        if document.get("schema") != "kyna.diagnostic/v1":
            failures.append(f"{name}: unexpected schema {document.get('schema')!r}")
            continue
        diagnostics = document.get("diagnostics", [])
        errors = [item for item in diagnostics if item.get("severity") == "error"]
        if not errors:
            failures.append(f"{name}: no error diagnostic")
            continue
        diagnostic = errors[-1]
        if diagnostic.get("code") != expected_code:
            failures.append(
                f"{name}: expected {expected_code}, got {diagnostic.get('code')!r}"
            )
        if diagnostic.get("category") != "runtime":
            failures.append(f"{name}: category is not runtime")
        start = diagnostic.get("range", {}).get("start", {})
        if start.get("line", 0) < 1 or start.get("column", 0) < 1:
            failures.append(f"{name}: missing one-based source location")
        if not diagnostic.get("callFrames"):
            failures.append(f"{name}: missing runtime call frame")

    if failures:
        print("Kyna runtime-failure verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    print(f"verified {len(EXPECTED_CODES)} structured runtime failures")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
