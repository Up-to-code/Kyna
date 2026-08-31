#!/usr/bin/env python3
"""End-to-end checks for the public ky developer-platform contract."""

from __future__ import annotations

import http.client
import os
import pathlib
import signal
import socket
import subprocess
import sys
import tempfile
import time


def invoke(cli: pathlib.Path, *arguments: str, cwd: pathlib.Path | None = None, stdin: str | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(cli), *arguments, "--no-color", "--no-interactive"],
        cwd=cwd,
        input=stdin,
        text=True,
        capture_output=True,
        timeout=30,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def free_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def main() -> int:
    cli = pathlib.Path(sys.argv[1]).resolve()
    alias = cli.with_name("kyna" + cli.suffix)
    require(cli.name.startswith("ky"), f"primary executable is not ky: {cli}")
    require(alias.is_file(), "kyna compatibility executable is missing")
    require(invoke(cli, "--version").returncode == 0, "ky --version failed")
    require(invoke(alias, "--version").returncode == 0, "kyna --version failed")
    unnamed = invoke(cli, "new")
    require(unnamed.returncode == 2 and "project name is required" in unnamed.stderr,
            "non-interactive ky new did not require a deterministic project name")

    repl_source = """:keys
set fixed = 10;
let changing: int = 10;
changing = changing + 5;
print("bindings", fixed, changing)
set text = "test"; print(text)
fixed = 20;
changing = "wrong";
print("after-error", changing);
:history
:quit
"""
    repl = invoke(cli, "repl", stdin=repl_source)
    require(repl.returncode == 0, "REPL process failed")
    require("Kyna 1.0.0 · interactive playground" in repl.stdout,
            "REPL banner does not match the CLI version")
    require("Left/Right" in repl.stdout and "Left click" in repl.stdout,
            "REPL did not expose its keyboard and mouse control list")
    require("bindings 10 15" in repl.stdout, "REPL did not preserve declarations and mutable state")
    require("test" in repl.stdout, "REPL did not complete a one-line expression without a semicolon")
    require("after-error 15" in repl.stdout, "REPL did not continue after a failed submission")
    require("immutable binding 'fixed'" in repl.stderr, "REPL did not preserve immutable binding state")
    require("cannot assign str to int" in repl.stderr, "REPL did not preserve mutable binding type state")
    require("undefined name" not in repl.stderr, f"REPL lost a previous declaration: {repl.stderr}")

    with tempfile.TemporaryDirectory(prefix="kyna-platform-") as temporary:
        root = pathlib.Path(temporary)
        minimal = root / "hello"
        created = invoke(cli, "new", str(minimal), "--template", "minimal", "--no-git")
        require(created.returncode == 0, created.stderr)
        require((minimal / "kyna.toml").is_file(), "minimal manifest missing")
        checked = invoke(cli, "check", cwd=minimal / "src")
        require(checked.returncode == 0, checked.stderr)

        messy = "func  plus( value:int){# preserved\nreturn value+1;\n}\n"
        formatted = invoke(cli, "fmt", "-", stdin=messy)
        require(formatted.returncode == 0, formatted.stderr)
        require("    return value + 1;" in formatted.stdout, "formatter spacing/indentation failed")
        require("# preserved" in formatted.stdout, "formatter discarded a comment")
        stable = invoke(cli, "fmt", "-", stdin=formatted.stdout)
        require(stable.stdout == formatted.stdout, "formatter is not idempotent")

        dependency = root / "shared"
        require(invoke(cli, "new", str(dependency), "--template", "minimal", "--no-git").returncode == 0, "dependency scaffold failed")
        added = invoke(cli, "add", "shared", "--path", "../shared", cwd=minimal)
        require(added.returncode == 0, added.stderr)
        require((minimal / "kyna.lock").is_file(), "lockfile was not generated")
        require(invoke(cli, "install", "--locked", cwd=minimal).returncode == 0, "locked install rejected a current lockfile")
        with (minimal / "kyna.toml").open("a", encoding="utf-8") as manifest:
            manifest.write("\n[dependencies.extra]\npath = \"../shared\"\n")
        require(invoke(cli, "install", "--locked", cwd=minimal).returncode == 2, "--locked failed to detect manifest drift")

        backend = root / "api"
        require(invoke(cli, "new", str(backend), "--template", "backend", "--no-git").returncode == 0, "backend scaffold failed")
        for expected in ("src/app.kyna", "src/middleware/request_logger.kyna",
                         "src/routes/index.kyna", "src/routes/health.kyna"):
            require((backend / expected).is_file(), f"backend scaffold omitted {expected}")
        require(not (backend / "src/config/server.kyna").exists(),
                "backend duplicated manifest server configuration in source")
        app_source = (backend / "src/app.kyna").read_text(encoding="utf-8")
        require("http.server()" in app_source and "serverConfig" not in app_source,
                "backend application does not defer to kyna.toml server settings")
        generated = invoke(cli, "generate", "route", "users", "--method", "post",
                           "--path", "/api/users", cwd=backend)
        require(generated.returncode == 0, generated.stderr)
        require((backend / "src/routes/users.kyna").is_file(), "route generator omitted route module")
        route_index = (backend / "src/routes/index.kyna").read_text(encoding="utf-8")
        require('import "./users.kyna" as usersRoute;' in route_index and
                'app.post("/api/users", usersRoute.index);' in route_index,
                "route generator did not wire the route registry")
        require(invoke(cli, "check", cwd=backend).returncode == 0,
                "generated Express-style backend did not type-check")
        port = free_port()
        manifest_path = backend / "kyna.toml"
        manifest_source = manifest_path.read_text(encoding="utf-8")
        manifest_path.write_text(manifest_source.replace("port = 3000", f"port = {port}"),
                                 encoding="utf-8")
        server = subprocess.Popen(
            [str(cli), "run", "--quiet", "--no-color", "--no-interactive"],
            cwd=backend,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            for _ in range(100):
                try:
                    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=1)
                    connection.request("GET", "/health")
                    response = connection.getresponse()
                    body = response.read().decode()
                    connection.close()
                    if response.status == 200:
                        break
                except OSError:
                    time.sleep(0.05)
            else:
                raise AssertionError("backend server did not start on the kyna.toml port")
            require(body == '{"status":"ok"}', f"unexpected health response: {body}")
        finally:
            if os.name == "nt":
                server.terminate()
            else:
                server.send_signal(signal.SIGINT)
            server.wait(timeout=10)
        if os.name != "nt":
            require(server.returncode == 130, f"Ctrl-C exit code was {server.returncode}, expected 130")

            watch_port = free_port()
            manifest_path.write_text(
                manifest_path.read_text(encoding="utf-8").replace(
                    f"port = {port}", f"port = {watch_port}"),
                encoding="utf-8",
            )
            watched_route = backend / "src/routes/health.kyna"
            good_route = watched_route.read_text(encoding="utf-8")
            dev = subprocess.Popen(
                [str(cli), "run", "dev", "--no-color", "--no-interactive"],
                cwd=backend,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            try:
                for _ in range(100):
                    try:
                        connection = http.client.HTTPConnection("127.0.0.1", watch_port, timeout=1)
                        connection.request("GET", "/health")
                        response = connection.getresponse()
                        response.read()
                        connection.close()
                        if response.status == 200:
                            break
                    except OSError:
                        time.sleep(0.05)
                else:
                    raise AssertionError("ky run dev did not start the watched server")

                watched_route.write_text("export func broken( {\n", encoding="utf-8")
                time.sleep(0.6)
                connection = http.client.HTTPConnection("127.0.0.1", watch_port, timeout=2)
                connection.request("GET", "/health")
                response = connection.getresponse()
                response.read()
                connection.close()
                require(response.status == 200,
                        "ky dev stopped the last good server after a failed check")
            finally:
                watched_route.write_text(good_route, encoding="utf-8")
                dev.send_signal(signal.SIGINT)
                dev.wait(timeout=10)
            require(dev.returncode == 130,
                    f"ky run dev Ctrl-C exit code was {dev.returncode}, expected 130")

    print("Kyna developer-platform verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
