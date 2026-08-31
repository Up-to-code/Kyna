#!/usr/bin/env python3
"""Verify deterministic source-byte inspection for clean and suspicious inputs."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


def inspect(binary: pathlib.Path, path: pathlib.Path, *options: str):
    return subprocess.run(
        [str(binary), "inspect", str(path), *options, "--no-color"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    clean_source = pathlib.Path(sys.argv[2]).resolve()
    failures: list[str] = []

    clean = inspect(binary, clean_source, "--format", "json")
    if clean.returncode != 0:
        failures.append(f"clean source returned {clean.returncode}: {clean.stderr.strip()}")
    else:
        document = json.loads(clean.stdout)
        if document.get("schema") != "kyna.source-inspection/v1":
            failures.append("source inspector emitted the wrong schema")
        if document.get("suspicious") or document.get("nulBytes") != 0:
            failures.append("clean source was marked suspicious")

    with tempfile.TemporaryDirectory(prefix="kyna-source-inspection-") as directory:
        suspicious_path = pathlib.Path(directory) / "suspicious.kyna"
        suspicious_path.write_bytes(b"\xef\xbb\xbfprint(\"Kyna\");\x00\r\n")
        suspicious = inspect(binary, suspicious_path, "--format", "json")
        if suspicious.returncode != 1:
            failures.append(f"suspicious source returned {suspicious.returncode}, expected 1")
        else:
            document = json.loads(suspicious.stdout)
            if not document.get("utf8Bom") or document.get("nulBytes") != 1:
                failures.append("BOM or NUL byte was not detected")
            if document.get("crlf") != 1 or not document.get("suspicious"):
                failures.append("CRLF or suspicious status was not reported")

    if failures:
        print("Kyna source inspector verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print("verified clean and suspicious source-byte inspection")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
