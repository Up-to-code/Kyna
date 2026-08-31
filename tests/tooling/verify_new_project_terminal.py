#!/usr/bin/env python3
"""Exercise the interactive `ky new` wizard through a pseudo-terminal."""

from __future__ import annotations

import os
import pathlib
import select
import signal
import sys
import tempfile
import time


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    if os.name == "nt":
        print("ky new terminal verification skipped on Windows")
        return 0

    import pty

    cli = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="ky-new-") as temporary:
        root = pathlib.Path(temporary)
        pid, terminal = pty.fork()
        if pid == 0:
            os.chdir(root)
            os.execv(str(cli), [str(cli), "new", "--no-git"])

        captured = bytearray()

        def drain(duration: float) -> None:
            deadline = time.monotonic() + duration
            while time.monotonic() < deadline:
                readable, _, _ = select.select([terminal], [], [], 0.05)
                if not readable:
                    continue
                try:
                    chunk = os.read(terminal, 65536)
                except OSError:
                    return
                if not chunk:
                    return
                captured.extend(chunk)

        try:
            drain(0.6)
            os.write(terminal, b"hello-professional\r")
            drain(0.8)
            os.write(terminal, b"\r")  # Select the default minimal template.

            deadline = time.monotonic() + 8
            status = None
            while time.monotonic() < deadline:
                drain(0.1)
                completed, candidate = os.waitpid(pid, os.WNOHANG)
                if completed == pid:
                    status = candidate
                    break
            if status is None:
                os.kill(pid, signal.SIGTERM)
                os.waitpid(pid, 0)
                raise AssertionError("ky new wizard did not finish")
        finally:
            os.close(terminal)

        rendered = captured.decode("utf-8", errors="replace")
        project = root / "hello-professional"
        require(os.waitstatus_to_exitcode(status) == 0, rendered[-3000:])
        require((project / "kyna.toml").is_file(), "wizard did not create the manifest")
        require((project / "src/main.kyna").is_file(), "wizard did not create the entry file")
        require("Created hello-professional" in rendered, "creation summary was not rendered")
        require("ky run" in rendered, "next-step command was not rendered")

    print("ky new terminal verification passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
