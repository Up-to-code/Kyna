#!/usr/bin/env python3
"""Verify the repository's source-layout and naming policy.

The checks are deliberately mechanical: domain ownership remains a human
decision, while this verifier enforces the boundaries that can be checked
without parsing C++.
"""

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
    "vcpkg",
}
SKIPPED_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".ico", ".vsix", ".zip", ".gz"}
AMBIGUOUS_STEMS = {"behavior", "functions", "helpers", "runtime", "types", "validation", "words"}
IMPLEMENTATION_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
MODULE_FACADE_PATHS = {
    pathlib.Path("library/core/src/format_value_codec.cpp"),
    pathlib.Path("library/core/src/json_value_codec.cpp"),
}
GLOB_SOURCE_PATTERN = re.compile(r"\bGLOB(?:_RECURSE)?\b", re.IGNORECASE)
EMPTY_REGISTER_MARKER = re.compile(
    r"REGISTRATION TEMPLATE|INTENTIONALLY EMPTY|intentionally empty", re.IGNORECASE
)
# .ky and .d.ky are supported alternative suffixes alongside .kyna/.kyna.d.
LEGACY_PATTERN = re.compile(r"\b(?:Kyma|kyma)\b")


def is_skipped(path: pathlib.Path) -> bool:
    relative = path.relative_to(ROOT)
    return any(part in SKIPPED_DIRECTORIES or part.startswith("build-") for part in relative.parts)


def strip_cpp_comments(content: str) -> str:
    """Remove comments sufficiently to recognize an empty registration stub."""
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", content)


def is_empty_register_stub(path: pathlib.Path, content: str) -> bool:
    if not path.name.startswith("register_") or path.suffix.lower() != ".cpp":
        return False
    if EMPTY_REGISTER_MARKER.search(content):
        return True

    code = strip_cpp_comments(content)
    # Allow a real registration implementation, but identify the usual empty
    # namespace/function skeleton even when its comments do not use our marker.
    code = re.sub(r"\bnamespace\s+[A-Za-z_]\w*\s*\{", "", code)
    code = re.sub(r"\bvoid\s+register[A-Za-z_]\w*\s*\([^)]*\)\s*\{\s*\}", "", code)
    code = re.sub(r"[{};\s]", "", code)
    return not code


def source_is_directly_under_src(path: pathlib.Path) -> bool:
    parts = path.relative_to(ROOT).parts
    try:
        src_index = parts.index("src")
    except ValueError:
        return False
    return src_index == len(parts) - 2


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
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        if path.suffix.lower() in IMPLEMENTATION_SUFFIXES and source_is_directly_under_src(path):
            if relative not in MODULE_FACADE_PATHS:
                failures.append(f"{relative}: implementation must live in a domain subfolder under src/")
        if is_empty_register_stub(path, content):
            failures.append(f"{relative}: empty register_*.cpp placeholder; remove the stub")
        if path.name == "CMakeLists.txt" and relative.parts[0] in owned_roots:
            if GLOB_SOURCE_PATTERN.search(content):
                failures.append(f"{relative}: source discovery must use explicit CMake lists, not GLOB")
        if relative.parts[0] in owned_roots and path.stem in AMBIGUOUS_STEMS:
            failures.append(f"{relative}: ambiguous filename '{path.name}'")
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
