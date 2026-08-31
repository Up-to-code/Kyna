#!/usr/bin/env python3
"""Verify that the repository contains one canonical Kyna VS Code extension."""

from __future__ import annotations

import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile


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
    language_ids = {language.get("id") for language in languages}
    if language_ids != {"kyna", "kyna-manifest"}:
        failures.append("extension must register Kyna source and kyna.toml manifest languages")
    source_languages = [language for language in languages if language.get("id") == "kyna"]
    if not source_languages or source_languages[0].get("extensions") != [".kyna", ".ky", ".kyna.d", ".d.ky", ".ky.d"]:
        failures.append("extension must register canonical .kyna and .ky suffixes plus declaration forms")
    manifest_languages = [language for language in languages if language.get("id") == "kyna-manifest"]
    if not manifest_languages or manifest_languages[0].get("filenames") != ["kyna.toml"]:
        failures.append("extension must register kyna.toml as the project manifest")

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
    for required_completion in ("fetchResult", "tryFetch", "responseBinding", "resultBinding"):
        if required_completion not in implementation:
            failures.append(f"missing advanced completion support: {required_completion}")
    if "vscode.window.terminals.filter" not in implementation or "existing.dispose()" not in implementation:
        failures.append("Run/Check must replace stale terminals from older extension versions")
    first_preset = implementation.find("'build-debug'")
    first_legacy = implementation.find("'build', 'bin', name")
    if first_preset < 0 or first_legacy < 0 or first_preset > first_legacy:
        failures.append("preset build directories must be preferred over the legacy build directory")
    if "['ky.exe', 'kyna.exe']" not in implementation or "['ky', 'kyna']" not in implementation:
        failures.append("CLI discovery must prefer ky and retain the kyna compatibility alias")
    if "registerDocumentFormattingEditProvider" not in implementation or "['fmt', '-'" not in implementation:
        failures.append("extension must register document formatting backed by ky fmt stdin")
    for project_feature in (
        "kyna.runProject", "kyna.devProject", "kyna.installDependencies",
        "kyna.generateRoute", "kyna.configureProject", "registerTreeDataProvider",
        "registerFileDecorationProvider", "manifestCompletionProvider",
        "KynaRoutesProvider", "kyna.routes", "readRoutes", "routeMethodPresentation",
        "validateRoutePath", "kyna.openRoutesFolder", "kyna.openRouteEndpoint",
        "simpleBrowser.show", "vscode.workspace.saveAll(false)",
    ):
        if project_feature not in implementation:
            failures.append(f"missing project workflow support: {project_feature}")

    for language in languages:
        for icon in language.get("icon", {}).values():
            if not (EXTENSION / icon.removeprefix("./")).is_file():
                failures.append(f"missing language icon: {icon}")
    icon = manifest.get("icon")
    if not icon or not (EXTENSION / icon).is_file():
        failures.append("missing marketplace icon")

    readme = (EXTENSION / "README.md").read_text(encoding="utf-8")
    for artwork in (
        "assets/readme/kyna-vscode-hero.png",
        "assets/readme/kyna-vscode-features.png",
    ):
        if not (EXTENSION / artwork).is_file():
            failures.append(f"missing README artwork: {artwork}")
        if f"]({artwork})" not in readme:
            failures.append(f"README does not embed artwork: {artwork}")

    if len(sys.argv) > 1:
        archive = pathlib.Path(sys.argv[1])
        if not archive.is_file():
            failures.append(f"VSIX does not exist: {archive}")
        else:
            with zipfile.ZipFile(archive) as package:
                names = set(package.namelist())
                required = {
                    "extension/readme.md",
                    "extension/package.json",
                    "extension/extension.js",
                    "extension/syntaxes/kyna.tmLanguage.json",
                    "extension/syntaxes/kyna-manifest.tmLanguage.json",
                    "extension/manifest-language-configuration.json",
                    "extension/snippets/kyna.json",
                    "extension/assets/kyna-k.png",
                    "extension/assets/kyna-file-light.svg",
                    "extension/assets/kyna-file-dark.svg",
                    "extension/assets/manifest-light.svg",
                    "extension/assets/manifest-dark.svg",
                    "extension/assets/route-light.svg",
                    "extension/assets/route-dark.svg",
                    "extension/assets/routes-view.svg",
                }
                for item in sorted(required - names):
                    failures.append(f"VSIX is missing required file: {item}")
                if not any(name.startswith("extension/examples/") and name.endswith(".kyna") for name in names):
                    failures.append("VSIX is missing packaged Kyna examples")
                packaged_readme = package.read("extension/readme.md").decode("utf-8") if "extension/readme.md" in names else ""
                images = re.findall(r"!\[[^]]*\]\(([^)]+)\)", packaged_readme)
                if not images:
                    failures.append("packaged README has no images")
                for image in images:
                    if not image.startswith("https://"):
                        failures.append(f"packaged README image is not HTTPS: {image}")
                    elif os.environ.get("KYNA_VERIFY_REMOTE_IMAGES") == "1":
                        try:
                            request = urllib.request.Request(image, method="HEAD", headers={"User-Agent": "Kyna-VSIX-validator"})
                            with urllib.request.urlopen(request, timeout=15) as response:
                                if response.status != 200:
                                    failures.append(f"README image returned HTTP {response.status}: {image}")
                        except Exception as error:
                            curl = shutil.which("curl")
                            fallback = subprocess.run(
                                [curl, "-fsSIL", "--max-time", "15", image],
                                capture_output=True,
                                text=True,
                                check=False,
                            ) if curl else None
                            if not fallback or fallback.returncode != 0:
                                detail = fallback.stderr.strip() if fallback else str(error)
                                failures.append(
                                    f"README image could not be resolved: {image}: {detail}"
                                )

    if failures:
        print("Kyna VS Code extension verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print(f"Kyna VS Code extension {manifest['version']} verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
