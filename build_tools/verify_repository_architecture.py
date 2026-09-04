#!/usr/bin/env python3
"""Verify the repository's source-layout and naming policy.

The checks are deliberately mechanical: domain ownership remains a human
decision, while this verifier enforces the boundaries that can be checked
without parsing C++.
"""

from __future__ import annotations

import json
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
MODULE_GRAPH_PATH = ROOT / "spec" / "architecture" / "modules.json"
MODULE_CALL_PATTERN = re.compile(
    r"\bkyna_add_module\s*\(\s*([A-Za-z0-9_]+)(.*?)\)",
    re.DOTALL,
)
TARGET_LINK_PATTERN = re.compile(
    r"\btarget_link_libraries\s*\(\s*([A-Za-z0-9_]+)(.*?)\)",
    re.DOTALL,
)
INTERNAL_TARGET_PATTERN = re.compile(r"\bkyna(?:_[A-Za-z0-9_]+)?\b")
OWNED_AREAS = ("compiler", "runtime", "library", "sdk", "tools")
TARGET_DECLARATION_PATTERN = re.compile(
    r"\b(?:kyna_add_module|add_library|add_executable)\s*\(\s*"
    r"(kyna(?:_[A-Za-z0-9_]+)?)\b"
)


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


def declared_internal_dependencies(path: pathlib.Path, target: str) -> set[str]:
    """Return Kyna targets linked by target in its owning CMake file."""
    content = re.sub(r"#[^\n]*", "", path.read_text(encoding="utf-8"))
    dependencies: set[str] = set()
    for match in MODULE_CALL_PATTERN.finditer(content):
        if match.group(1) != target:
            continue
        dependency_sections = re.findall(
            r"(?:PUBLIC|PRIVATE)_DEPENDENCIES\s+(.*?)"
            r"(?=\b(?:SOURCES|PUBLIC_DEPENDENCIES|PRIVATE_DEPENDENCIES)\b|$)",
            match.group(2),
            re.DOTALL,
        )
        for section in dependency_sections:
            dependencies.update(INTERNAL_TARGET_PATTERN.findall(section))
    for match in TARGET_LINK_PATTERN.finditer(content):
        if match.group(1) != target:
            continue
        dependencies.update(INTERNAL_TARGET_PATTERN.findall(match.group(2)))
    dependencies.discard(target)
    return dependencies


def verify_module_graph(failures: list[str]) -> None:
    """Verify exact internal links and reject cycles in the module graph."""
    try:
        manifest = json.loads(MODULE_GRAPH_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        failures.append(f"spec/architecture/modules.json: cannot read module graph: {error}")
        return

    if manifest.get("schema") != "kyna.module-graph/v1":
        failures.append("spec/architecture/modules.json: unsupported or missing schema")
        return

    modules = manifest.get("modules")
    if not isinstance(modules, list):
        failures.append("spec/architecture/modules.json: modules must be an array")
        return

    graph: dict[str, set[str]] = {}
    paths: set[pathlib.Path] = set()
    for module in modules:
        if not isinstance(module, dict):
            failures.append("spec/architecture/modules.json: every module must be an object")
            continue
        target = module.get("target")
        cmake = module.get("cmake")
        expected = module.get("dependencies")
        if not isinstance(target, str) or not isinstance(cmake, str) or not isinstance(expected, list):
            failures.append("spec/architecture/modules.json: module needs target, cmake, and dependencies")
            continue
        if not all(isinstance(dependency, str) for dependency in expected):
            failures.append(f"spec/architecture/modules.json: {target} dependencies must be strings")
            continue
        path = pathlib.Path(cmake)
        if target in graph:
            failures.append(f"spec/architecture/modules.json: duplicate target {target}")
            continue
        if path in paths:
            failures.append(f"spec/architecture/modules.json: duplicate CMake owner {cmake}")
            continue
        paths.add(path)
        graph[target] = set(expected)
        absolute_path = ROOT / path
        if not absolute_path.is_file():
            failures.append(f"{cmake}: module graph owner does not exist")
            continue
        actual = declared_internal_dependencies(absolute_path, target)
        if actual != graph[target]:
            missing = sorted(graph[target] - actual)
            unexpected = sorted(actual - graph[target])
            details = []
            if missing:
                details.append("missing links " + ", ".join(missing))
            if unexpected:
                details.append("unexpected links " + ", ".join(unexpected))
            failures.append(f"{cmake}: {target} dependency graph mismatch ({'; '.join(details)})")

    for target, dependencies in graph.items():
        unknown = sorted(dependencies - graph.keys())
        if unknown:
            failures.append(
                f"spec/architecture/modules.json: {target} references unknown targets "
                + ", ".join(unknown)
            )

    declared_targets: dict[str, pathlib.Path] = {}
    for area in OWNED_AREAS:
        for cmake_path in (ROOT / area).rglob("CMakeLists.txt"):
            content = re.sub(r"#[^\n]*", "", cmake_path.read_text(encoding="utf-8"))
            for match in TARGET_DECLARATION_PATTERN.finditer(content):
                declared_targets[match.group(1)] = cmake_path.relative_to(ROOT)
    missing_targets = sorted(declared_targets.keys() - graph.keys())
    for target in missing_targets:
        failures.append(
            f"{declared_targets[target]}: {target} is missing from spec/architecture/modules.json"
        )
    stale_targets = sorted(graph.keys() - declared_targets.keys())
    for target in stale_targets:
        failures.append(f"spec/architecture/modules.json: {target} has no CMake target declaration")

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(target: str, path: list[str]) -> None:
        if target in visiting:
            start = path.index(target)
            cycle = " -> ".join(path[start:] + [target])
            failures.append(f"spec/architecture/modules.json: dependency cycle {cycle}")
            return
        if target in visited:
            return
        visiting.add(target)
        path.append(target)
        for dependency in sorted(graph.get(target, set())):
            if dependency in graph:
                visit(dependency, path)
        path.pop()
        visiting.remove(target)
        visited.add(target)

    for target in sorted(graph):
        visit(target, [])


def main() -> int:
    failures: list[str] = []
    verify_module_graph(failures)
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
        if path.name == "CMakeLists.txt" and relative.parts[0] in OWNED_AREAS:
            if GLOB_SOURCE_PATTERN.search(content):
                failures.append(f"{relative}: source discovery must use explicit CMake lists, not GLOB")
        if relative.parts[0] in OWNED_AREAS and path.stem in AMBIGUOUS_STEMS:
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
