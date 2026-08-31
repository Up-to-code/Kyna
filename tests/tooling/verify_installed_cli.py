#!/usr/bin/env python3
"""Exercise a release-shaped archive through the native per-user installer."""

from __future__ import annotations

import functools
import hashlib
import http.server
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import zipfile


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def run(*arguments: str | pathlib.Path, cwd: pathlib.Path | None = None,
        env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(argument) for argument in arguments],
        cwd=cwd,
        env=env,
        text=True,
        capture_output=True,
        timeout=60,
        check=False,
    )


def target_name() -> tuple[str, str]:
    operating_system = {
        "darwin": "darwin",
        "linux": "linux",
        "win32": "windows",
    }.get(sys.platform)
    require(operating_system is not None, f"unsupported test operating system: {sys.platform}")
    machine = platform.machine().lower()
    architecture = "x86_64" if machine in {"x86_64", "amd64"} else "arm64" if machine in {"arm64", "aarch64"} else None
    require(architecture is not None, f"unsupported test architecture: {machine}")
    return operating_system, architecture


def stage_package(cli: pathlib.Path, repository: pathlib.Path, package: pathlib.Path) -> None:
    binary_directory = package / "bin"
    binary_directory.mkdir(parents=True)
    alias = cli.with_name("kyna" + cli.suffix)
    require(alias.is_file(), f"compatibility executable is missing: {alias}")
    shutil.copy2(cli, binary_directory / cli.name)
    shutil.copy2(alias, binary_directory / alias.name)
    if os.name == "nt":
        for dependency in cli.parent.glob("*.dll"):
            shutil.copy2(dependency, binary_directory / dependency.name)
    share = package / "share" / "kyna"
    share.mkdir(parents=True)
    for filename in ("README.md", "LICENSE"):
        shutil.copy2(repository / filename, share / filename)
    for directory in ("templates", "docs", "examples"):
        shutil.copytree(repository / directory, share / directory)


def create_release(cli: pathlib.Path, repository: pathlib.Path, release: pathlib.Path) -> pathlib.Path:
    operating_system, architecture = target_name()
    package = release / "package"
    stage_package(cli, repository, package)
    if operating_system == "windows":
        asset = release / f"kyna-{operating_system}-{architecture}.zip"
        with zipfile.ZipFile(asset, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in package.rglob("*"):
                if path.is_file():
                    archive.write(path, path.relative_to(package))
    else:
        asset = release / f"kyna-{operating_system}-{architecture}.tar.gz"
        with tarfile.open(asset, "w:gz") as archive:
            for path in package.iterdir():
                archive.add(path, arcname=path.name)
    digest = hashlib.sha256(asset.read_bytes()).hexdigest()
    (release / "SHA256SUMS").write_text(f"{digest}  {asset.name}\n", encoding="utf-8")
    return asset


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format: str, *_arguments: object) -> None:
        pass


def installer_command(repository: pathlib.Path, prefix: pathlib.Path) -> list[str]:
    if os.name == "nt":
        powershell = shutil.which("pwsh") or shutil.which("powershell")
        require(powershell is not None, "PowerShell is required for the Windows installer test")
        return [powershell, "-NoProfile", "-File", str(repository / "install.ps1"),
                "-Prefix", str(prefix), "-NonInteractive", "-NoPathUpdate"]
    return ["sh", str(repository / "install.sh"), "--prefix", str(prefix), "--no-interactive"]


def main() -> int:
    cli = pathlib.Path(sys.argv[1]).resolve()
    repository = pathlib.Path(sys.argv[2]).resolve()
    require(cli.is_file(), f"CLI does not exist: {cli}")

    with tempfile.TemporaryDirectory(prefix="kyna-install-") as temporary:
        root = pathlib.Path(temporary)
        release = root / "release"
        release.mkdir()
        asset = create_release(cli, repository, release)
        handler = functools.partial(QuietHandler, directory=str(release))
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            environment = os.environ.copy()
            environment["KYNA_RELEASE_BASE_URL"] = f"http://127.0.0.1:{server.server_port}"
            prefix = root / "installed"
            command = installer_command(repository, prefix)

            installed = run(*command, env=environment)
            require(installed.returncode == 0, installed.stderr or installed.stdout)
            suffix = ".exe" if os.name == "nt" else ""
            installed_cli = prefix / "bin" / f"ky{suffix}"
            installed_alias = prefix / "bin" / f"kyna{suffix}"
            require(installed_cli.is_file(), "installer did not create the ky executable")
            require(installed_alias.is_file(), "installer did not create the kyna compatibility alias")
            require((prefix / "share" / "kyna" / "README.md").is_file(), "installer omitted packaged documentation")
            require((prefix / "share" / "kyna" / "templates" / "minimal" / "kyna.toml").is_file(),
                    "installer omitted the project templates")
            require((prefix / "share" / "kyna" / "install-manifest.txt").is_file(),
                    "installer did not record its managed files")
            require(run(installed_cli, "--version").returncode == 0, "installed ky --version failed")
            require(run(installed_alias, "--version").returncode == 0, "installed kyna --version failed")

            project = root / "hello"
            created = run(installed_cli, "--no-color", "--no-interactive", "new", project,
                          "--template", "minimal", "--no-git")
            require(created.returncode == 0, created.stderr)
            checked = run(installed_cli, "--no-color", "--no-interactive", "check", cwd=project)
            require(checked.returncode == 0, checked.stderr)

            reinstalled = run(*command, env=environment)
            require(reinstalled.returncode == 0, reinstalled.stderr or reinstalled.stdout)
            require((prefix / "bin" / f"ky{suffix}.previous").is_file(), "reinstall did not preserve the previous ky")
            require((prefix / "bin" / f"kyna{suffix}.previous").is_file(), "reinstall did not preserve the previous kyna alias")

            installed_digest = hashlib.sha256(installed_cli.read_bytes()).hexdigest()
            (release / "SHA256SUMS").write_text(f"{'0' * 64}  {asset.name}\n", encoding="utf-8")
            rejected = run(*command, env=environment)
            require(rejected.returncode != 0, "installer accepted a bad archive checksum")
            require(hashlib.sha256(installed_cli.read_bytes()).hexdigest() == installed_digest,
                    "checksum failure replaced the working installation")

            removed = run(installed_cli, "--no-color", "--no-interactive", "self", "uninstall",
                          "--prefix", prefix)
            require(removed.returncode == 0, removed.stderr)
            if os.name == "nt":
                for _ in range(100):
                    if not installed_cli.exists() and not installed_alias.exists():
                        break
                    time.sleep(0.05)
            require(not installed_cli.exists() and not installed_alias.exists(), "self uninstall left executables behind")
        finally:
            server.shutdown()
            thread.join(timeout=5)
            server.server_close()

    print("Kyna installed-CLI verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
