#!/usr/bin/env python3
"""Reject compatibility leftovers and ambiguous subsystem filenames."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIPPED_DIRECTORIES = {
    ".git",
    ".dependencies",
    ".vscode",
    "_CPack_Packages",
    "build",
    "node_modules",
}
SKIPPED_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".ico", ".vsix", ".zip", ".gz"}
AMBIGUOUS_STEMS = {"behavior", "functions", "helpers", "runtime", "types", "validation", "words"}
# .ky and .d.ky are supported alternative suffixes alongside .kyna/.kyna.d.
LEGACY_PATTERN = re.compile(r"\b(?:Kyma|kyma)\b")


def is_skipped(path: pathlib.Path) -> bool:
    relative = path.relative_to(ROOT)
    return any(part in SKIPPED_DIRECTORIES or part.startswith("build-") for part in relative.parts)


def main() -> int:
    failures: list[str] = []
    owned_roots = ("compiler", "runtime", "library", "sdk", "tools")
    for path in ROOT.rglob("*"):
        if is_skipped(path) or not path.is_file():
            continue
        relative = path.relative_to(ROOT)
        if relative == pathlib.Path("build_tools/verify_repository_architecture.py"):
            continue
        if path.suffix.lower() in SKIPPED_SUFFIXES:
            continue
        if relative.parts[0] in owned_roots and path.stem in AMBIGUOUS_STEMS:
            failures.append(f"{relative}: ambiguous filename '{path.name}'")
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for line_number, line in enumerate(content.splitlines(), start=1):
            if LEGACY_PATTERN.search(line):
                failures.append(f"{relative}:{line_number}: legacy Kyna compatibility name")
    if failures:
        print("Kyna repository architecture verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print("Kyna repository architecture verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
