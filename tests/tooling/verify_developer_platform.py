#!/usr/bin/env python3
"""End-to-end checks for the public ky developer-platform contract."""

from __future__ import annotations

import http.client
import json
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
    require("Kyna 1.0.0" in repl.stdout and "interactive playground" in repl.stdout,
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
                         "src/routes/index.kyna", "src/routes/home.kyna", "src/routes/health.kyna"):
            require((backend / expected).is_file(), f"backend scaffold omitted {expected}")
        require(not (backend / "src/config/server.kyna").exists(),
                "backend duplicated manifest server configuration in source")
        app_source = (backend / "src/app.kyna").read_text(encoding="utf-8")
        require("http.server()" in app_source and "serverConfig" not in app_source,
                "backend application does not defer to kyna.toml server settings")
        initial_routes = (backend / "src/routes/index.kyna").read_text(encoding="utf-8")
        require('app.get("/", homeRoute.show);' in initial_routes,
                "backend scaffold omitted its homepage route")
        duplicate_home = invoke(cli, "generate", "route", "another-home", "--method", "get",
                                "--path", "/", cwd=backend)
        require(duplicate_home.returncode == 2 and "already registered" in duplicate_home.stderr,
                "route generator accepted a duplicate method/path registration")
        generated = invoke(cli, "generate", "route", "users", "--method", "post",
                           "--path", "/api/users", cwd=backend)
        require(generated.returncode == 0, generated.stderr)
        require((backend / "src/routes/users.kyna").is_file(), "route generator omitted route module")
        route_index = (backend / "src/routes/index.kyna").read_text(encoding="utf-8")
        require('import "./users.kyna" as usersRoute;' in route_index and
                'app.post("/api/users", usersRoute.index);' in route_index,
                "route generator did not wire the route registry")
        generated_detail = invoke(cli, "generate", "route", "user-detail", "--method", "get",
                                  "--path", "/teams/:team/users/:user", cwd=backend)
        require(generated_detail.returncode == 0, generated_detail.stderr)
        detail_source = (backend / "src/routes/user-detail.kyna").read_text(encoding="utf-8")
        require('# ky:route method=get path="/teams/:team/users/:user" handler=index' in detail_source,
                "generated route omitted machine-readable route identity")
        require("params: request.params" in detail_source and "query: request.query" in detail_source,
                "generated route did not expose path and query data")
        duplicate_slug = invoke(cli, "generate", "route", "duplicate", "--method", "get",
                                "--path", "/:id/:id", cwd=backend)
        require(duplicate_slug.returncode == 2 and "duplicate parameter" in duplicate_slug.stderr,
                "route generator accepted duplicate path-parameter names")
        require(invoke(cli, "check", cwd=backend).returncode == 0,
                "generated Express-style backend did not type-check")

        # Importing a symbol the target module does not export is a module-level
        # error (K4004), checked across the real module loader in this project.
        modules = backend / "src" / "modules"
        modules.mkdir(parents=True, exist_ok=True)
        (modules / "lib.kyna").write_text(
            'export func greet(name: str): str { return name; }\n', encoding="utf-8")
        consumer = modules / "consumer.kyna"
        consumer.write_text(
            'import { greet } from "./lib.kyna"; print(greet("Kyna"));\n', encoding="utf-8")
        require(invoke(cli, "check", str(consumer), cwd=backend).returncode == 0,
                "valid named import did not type-check")
        consumer.write_text(
            'import { missing } from "./lib.kyna"; print(missing);\n', encoding="utf-8")
        missing_import = invoke(cli, "check", str(consumer), cwd=backend)
        require(missing_import.returncode != 0, "nonexported named import did not fail to type-check")
        require("has no exported member 'missing'" in missing_import.stderr,
                f"nonexported import lacked a module diagnostic: {missing_import.stderr}")
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
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
            connection.request("GET", "/")
            response = connection.getresponse()
            home_body = response.read().decode()
            connection.close()
            require(response.status == 200 and '"status":"ready"' in home_body,
                    f"homepage route was not ready: {response.status} {home_body}")
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
            connection.request("GET", "/teams/acme/users/alice%20smith?include=posts+comments&limit=10")
            response = connection.getresponse()
            route_body = response.read().decode()
            connection.close()
            require(response.status == 200, f"dynamic route returned {response.status}: {route_body}")
            route_result = json.loads(route_body)
            require(route_result["params"] == {"team": "acme", "user": "alice smith"},
                    f"multiple route parameters were not captured: {route_result}")
            require(route_result["query"] == {"include": "posts comments", "limit": "10"},
                    f"query parameters were not decoded: {route_result}")
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
