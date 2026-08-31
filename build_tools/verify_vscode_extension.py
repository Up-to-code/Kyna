#!/usr/bin/env python3
"""Verify that the repository contains one canonical Kyna VS Code extension."""

from __future__ import annotations

import json
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
EXTENSION = ROOT / "editors" / "vscode-kyna"


def main() -> int:
    failures: list[str] = []
    manifests = list((ROOT / "editors").glob("*/package.json"))
    if manifests != [EXTENSION / "package.json"]:
        failures.append("editors/ must contain exactly one extension manifest: vscode-kyna/package.json")

    manifest = json.loads((EXTENSION / "package.json").read_text(encoding="utf-8"))
    if manifest.get("publisher") != "kyna-lang" or manifest.get("name") != "kyna-language-support":
        failures.append("extension identity must be kyna-lang.kyna-language-support")
    languages = manifest.get("contributes", {}).get("languages", [])
    if len(languages) != 1 or languages[0].get("id") != "kyna":
        failures.append("extension must register exactly the kyna language")
    if languages and languages[0].get("extensions") != [".kyna"]:
        failures.append("extension must register only the canonical .kyna suffix")

    commands = {
        command["command"]
        for command in manifest.get("contributes", {}).get("commands", [])
    }
    implementation = (EXTENSION / "extension.js").read_text(encoding="utf-8")
    registered = set(re.findall(r"registerCommand\('([^']+)'", implementation))
    if commands != registered:
        failures.append(
            f"manifest commands and registered commands differ: manifest={sorted(commands)}, "
            f"implementation={sorted(registered)}"
        )
    if "new vscode.ProcessExecution(program, arguments" not in implementation:
        failures.append("Run/Check must launch Kyna through direct process execution")
    if "vscode.tasks.executeTask(task)" not in implementation:
        failures.append("Run/Check must retain process output in a managed task terminal")
    if "[command, editor.document.fileName, '--color', 'always']" not in implementation:
        failures.append("Run/Check must request colored terminal diagnostics")
    if "terminal.sendText(" in implementation:
        failures.append("Run/Check must not route commands through the user's login shell")
    if "vscode.window.terminals.filter" not in implementation or "existing.dispose()" not in implementation:
        failures.append("Run/Check must replace stale terminals from older extension versions")
    first_preset = implementation.find("'build-debug'")
    first_legacy = implementation.find("'build', 'bin', 'kyna'")
    if first_preset < 0 or first_legacy < 0 or first_preset > first_legacy:
        failures.append("preset build directories must be preferred over the legacy build directory")

    for language in languages:
        for icon in language.get("icon", {}).values():
            if not (EXTENSION / icon.removeprefix("./")).is_file():
                failures.append(f"missing language icon: {icon}")
    icon = manifest.get("icon")
    if not icon or not (EXTENSION / icon).is_file():
        failures.append("missing marketplace icon")

    if failures:
        print("Kyna VS Code extension verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print(f"Kyna VS Code extension {manifest['version']} verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
